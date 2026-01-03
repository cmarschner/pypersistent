"""
Persistent Hash Map implementation in Python, similar to Clojure's PersistentHashMap.

This implementation uses a Hash Array Mapped Trie (HAMT) for efficient immutable operations
with structural sharing. Operations like assoc and dissoc return new maps that share most
of their structure with the original.
"""

from typing import Any, Iterator, Tuple, Optional
import sys


HASH_BITS = 5
HASH_MASK = (1 << HASH_BITS) - 1  # 0b11111
MAX_BITMAP_SIZE = 1 << HASH_BITS  # 32


def hash_value(key: Any) -> int:
    """Get hash for a key, ensuring it's a positive integer."""
    h = hash(key)
    return h if h >= 0 else -h


def popcount(n: int) -> int:
    """Count the number of set bits in an integer."""
    return bin(n).count('1')


class BitmapNode:
    """
    A node in the HAMT that uses a bitmap to indicate which slots are occupied.
    Each node can have up to 32 children (corresponding to 5 bits of the hash).
    """

    __slots__ = ('bitmap', 'array')

    def __init__(self, bitmap: int, array: list):
        self.bitmap = bitmap
        self.array = array

    def get(self, shift: int, key_hash: int, key: Any, not_found: Any) -> Any:
        """Get a value from this node."""
        bit_pos = 1 << ((key_hash >> shift) & HASH_MASK)

        if (self.bitmap & bit_pos) == 0:
            return not_found

        idx = popcount(self.bitmap & (bit_pos - 1)) * 2

        # Check if it's a key-value pair or a child node
        key_or_node = self.array[idx]
        val_or_none = self.array[idx + 1]

        if key_or_node is None:
            # It's a child node
            return val_or_none.get(shift + HASH_BITS, key_hash, key, not_found)
        elif key_or_node == key or (key_or_node is not None and key_or_node == key):
            # It's a key-value pair and keys match
            return val_or_none
        else:
            return not_found

    def assoc(self, shift: int, key_hash: int, key: Any, val: Any) -> 'BitmapNode':
        """Associate a key-value pair, returning a new node."""
        bit_pos = 1 << ((key_hash >> shift) & HASH_MASK)
        idx = popcount(self.bitmap & (bit_pos - 1)) * 2

        if (self.bitmap & bit_pos) != 0:
            # Slot is occupied
            key_or_node = self.array[idx]
            val_or_none = self.array[idx + 1]

            if key_or_node is None:
                # It's a child node, recurse
                new_child = val_or_none.assoc(shift + HASH_BITS, key_hash, key, val)
                if new_child is val_or_none:
                    return self
                new_array = self.array[:]
                new_array[idx + 1] = new_child
                return BitmapNode(self.bitmap, new_array)

            # It's a key-value pair
            if key_or_node == key:
                # Same key, update value
                if val_or_none is val:
                    return self
                new_array = self.array[:]
                new_array[idx + 1] = val
                return BitmapNode(self.bitmap, new_array)
            else:
                # Different key, same hash slot - create a sub-node
                new_child = self._create_node(
                    shift + HASH_BITS,
                    key_or_node, val_or_none,
                    key_hash, key, val
                )
                new_array = self.array[:]
                new_array[idx] = None
                new_array[idx + 1] = new_child
                return BitmapNode(self.bitmap, new_array)
        else:
            # Slot is empty, insert new entry
            new_array = self.array[:idx] + [key, val] + self.array[idx:]
            return BitmapNode(self.bitmap | bit_pos, new_array)

    def _create_node(self, shift: int, key1: Any, val1: Any,
                     key2_hash: int, key2: Any, val2: Any):
        """Create a new node containing two key-value pairs."""
        key1_hash = hash_value(key1)

        if shift >= 64:
            # Too deep, use collision node
            return CollisionNode(key1_hash, [key1, val1, key2, val2])

        idx1 = (key1_hash >> shift) & HASH_MASK
        idx2 = (key2_hash >> shift) & HASH_MASK

        if idx1 == idx2:
            # Same index at this level, recurse
            child = self._create_node(shift + HASH_BITS, key1, val1,
                                     key2_hash, key2, val2)
            return BitmapNode(1 << idx1, [None, child])
        else:
            # Different indices, create node with both entries
            if idx1 < idx2:
                return BitmapNode((1 << idx1) | (1 << idx2),
                                [key1, val1, key2, val2])
            else:
                return BitmapNode((1 << idx1) | (1 << idx2),
                                [key2, val2, key1, val1])

    def dissoc(self, shift: int, key_hash: int, key: Any) -> Optional['BitmapNode']:
        """Remove a key, returning a new node or None if empty."""
        bit_pos = 1 << ((key_hash >> shift) & HASH_MASK)

        if (self.bitmap & bit_pos) == 0:
            return self

        idx = popcount(self.bitmap & (bit_pos - 1)) * 2
        key_or_node = self.array[idx]
        val_or_none = self.array[idx + 1]

        if key_or_node is None:
            # It's a child node
            new_child = val_or_none.dissoc(shift + HASH_BITS, key_hash, key)

            if new_child is val_or_none:
                return self

            if new_child is None:
                # Child is empty, remove this entry
                if popcount(self.bitmap) == 1:
                    return None
                new_array = self.array[:idx] + self.array[idx + 2:]
                return BitmapNode(self.bitmap & ~bit_pos, new_array)
            else:
                new_array = self.array[:]
                new_array[idx + 1] = new_child
                return BitmapNode(self.bitmap, new_array)

        # It's a key-value pair
        if key_or_node == key:
            # Found it, remove
            if popcount(self.bitmap) == 1:
                return None
            new_array = self.array[:idx] + self.array[idx + 2:]
            return BitmapNode(self.bitmap & ~bit_pos, new_array)
        else:
            return self

    def iterate(self) -> Iterator[Tuple[Any, Any]]:
        """Iterate over all key-value pairs in this node."""
        for i in range(0, len(self.array), 2):
            key_or_node = self.array[i]
            val_or_none = self.array[i + 1]

            if key_or_node is None:
                # Child node
                yield from val_or_none.iterate()
            else:
                # Key-value pair
                yield (key_or_node, val_or_none)


class CollisionNode:
    """
    A node for handling hash collisions. Contains a linear array of key-value pairs
    that all have the same hash code.
    """

    __slots__ = ('hash', 'array')

    def __init__(self, hash_code: int, array: list):
        self.hash = hash_code
        self.array = array

    def get(self, shift: int, key_hash: int, key: Any, not_found: Any) -> Any:
        """Get a value from this collision node."""
        for i in range(0, len(self.array), 2):
            if self.array[i] == key:
                return self.array[i + 1]
        return not_found

    def assoc(self, shift: int, key_hash: int, key: Any, val: Any) -> 'CollisionNode':
        """Associate a key-value pair in this collision node."""
        for i in range(0, len(self.array), 2):
            if self.array[i] == key:
                if self.array[i + 1] is val:
                    return self
                new_array = self.array[:]
                new_array[i + 1] = val
                return CollisionNode(self.hash, new_array)

        # Key not found, append
        return CollisionNode(self.hash, self.array + [key, val])

    def dissoc(self, shift: int, key_hash: int, key: Any) -> Optional['CollisionNode']:
        """Remove a key from this collision node."""
        for i in range(0, len(self.array), 2):
            if self.array[i] == key:
                if len(self.array) == 2:
                    return None
                new_array = self.array[:i] + self.array[i + 2:]
                return CollisionNode(self.hash, new_array)
        return self

    def iterate(self) -> Iterator[Tuple[Any, Any]]:
        """Iterate over all key-value pairs in this collision node."""
        for i in range(0, len(self.array), 2):
            yield (self.array[i], self.array[i + 1])


class PersistentMap:
    """
    An immutable, persistent hash map implementation using HAMT.

    Similar to Clojure's PersistentHashMap, this provides O(log32 n) operations
    with structural sharing between versions.

    Example:
        m1 = PersistentMap()
        m2 = m1.assoc('a', 1).assoc('b', 2)
        m3 = m2.assoc('c', 3)

        # m1, m2, and m3 are all independent immutable maps
        # but they share most of their internal structure
    """

    __slots__ = ('_root', '_count')

    def __init__(self, root=None, count=0):
        self._root = root
        self._count = count

    def get(self, key: Any, default=None) -> Any:
        """Get the value associated with key, or default if not present."""
        if self._root is None:
            return default

        key_hash = hash_value(key)
        result = self._root.get(0, key_hash, key, PersistentMap._NOT_FOUND)

        if result is PersistentMap._NOT_FOUND:
            return default
        return result

    def assoc(self, key: Any, val: Any) -> 'PersistentMap':
        """
        Associate a key with a value, returning a new PersistentMap.
        The original map is unchanged.
        """
        key_hash = hash_value(key)

        if self._root is None:
            return PersistentMap(BitmapNode(0, []).assoc(0, key_hash, key, val), 1)

        # Check if key already exists
        old_val = self._root.get(0, key_hash, key, PersistentMap._NOT_FOUND)
        new_root = self._root.assoc(0, key_hash, key, val)

        if new_root is self._root:
            return self

        new_count = self._count if old_val is not PersistentMap._NOT_FOUND else self._count + 1
        return PersistentMap(new_root, new_count)

    def dissoc(self, key: Any) -> 'PersistentMap':
        """
        Remove a key, returning a new PersistentMap.
        The original map is unchanged.
        """
        if self._root is None:
            return self

        key_hash = hash_value(key)
        old_val = self._root.get(0, key_hash, key, PersistentMap._NOT_FOUND)

        if old_val is PersistentMap._NOT_FOUND:
            return self

        new_root = self._root.dissoc(0, key_hash, key)
        return PersistentMap(new_root, self._count - 1)

    def __contains__(self, key: Any) -> bool:
        """Check if a key is in the map."""
        if self._root is None:
            return False
        key_hash = hash_value(key)
        return self._root.get(0, key_hash, key, PersistentMap._NOT_FOUND) is not PersistentMap._NOT_FOUND

    def __len__(self) -> int:
        """Return the number of key-value pairs in the map."""
        return self._count

    def __iter__(self) -> Iterator[Any]:
        """Iterate over keys in the map."""
        if self._root is not None:
            for key, _ in self._root.iterate():
                yield key

    def items(self) -> Iterator[Tuple[Any, Any]]:
        """Iterate over (key, value) pairs."""
        if self._root is not None:
            yield from self._root.iterate()

    def keys(self) -> Iterator[Any]:
        """Iterate over keys."""
        return iter(self)

    def values(self) -> Iterator[Any]:
        """Iterate over values."""
        if self._root is not None:
            for _, val in self._root.iterate():
                yield val

    def __repr__(self) -> str:
        """String representation of the map."""
        items = ', '.join(f'{repr(k)}: {repr(v)}' for k, v in self.items())
        return f'PersistentMap({{{items}}})'

    def __eq__(self, other) -> bool:
        """Check equality with another map."""
        if not isinstance(other, PersistentMap):
            return False

        if len(self) != len(other):
            return False

        for key, val in self.items():
            if key not in other or other.get(key) != val:
                return False

        return True

    def __getitem__(self, key: Any) -> Any:
        """Get item using bracket notation. Raises KeyError if not found."""
        result = self.get(key, PersistentMap._NOT_FOUND)
        if result is PersistentMap._NOT_FOUND:
            raise KeyError(key)
        return result

    @staticmethod
    def create(**kwargs) -> 'PersistentMap':
        """Create a PersistentMap from keyword arguments."""
        m = PersistentMap()
        for k, v in kwargs.items():
            m = m.assoc(k, v)
        return m

    @staticmethod
    def from_dict(d: dict) -> 'PersistentMap':
        """Create a PersistentMap from a dictionary."""
        m = PersistentMap()
        for k, v in d.items():
            m = m.assoc(k, v)
        return m


# Sentinel value for "not found"
PersistentMap._NOT_FOUND = object()


if __name__ == '__main__':
    # Example usage demonstrating immutability and structural sharing
    print("=== Persistent Map Demo ===\n")

    # Create an empty map
    m1 = PersistentMap()
    print(f"m1 = {m1}")
    print(f"len(m1) = {len(m1)}\n")

    # Add some entries
    m2 = m1.assoc('name', 'Alice').assoc('age', 30).assoc('city', 'NYC')
    print(f"m2 = {m2}")
    print(f"len(m2) = {len(m2)}")
    print(f"m2.get('name') = {m2.get('name')}")
    print(f"m2['age'] = {m2['age']}\n")

    # m1 is still empty - immutability
    print(f"m1 is still empty: {m1}")
    print(f"len(m1) = {len(m1)}\n")

    # Create a new version with updates
    m3 = m2.assoc('age', 31).assoc('country', 'USA')
    print(f"m3 = {m3}")
    print(f"m3.get('age') = {m3.get('age')}")
    print(f"m2.get('age') = {m2.get('age')} (unchanged)\n")

    # Remove a key
    m4 = m3.dissoc('city')
    print(f"m4 (without 'city') = {m4}")
    print(f"'city' in m4: {'city' in m4}")
    print(f"'city' in m3: {'city' in m3} (unchanged)\n")

    # Iteration
    print("Iterating over m3:")
    for key in m3:
        print(f"  {key}: {m3[key]}")
    print()

    print("Items in m3:")
    for key, val in m3.items():
        print(f"  {key}: {val}")
    print()

    # Create from dict
    m5 = PersistentMap.from_dict({'x': 1, 'y': 2, 'z': 3})
    print(f"m5 = {m5}\n")

    # Demonstrate structural sharing efficiency
    print("=== Structural Sharing Demo ===")
    base = PersistentMap()
    for i in range(1000):
        base = base.assoc(f'key{i}', i)

    # Creating variations shares most structure
    var1 = base.assoc('extra1', 'value1')
    var2 = base.assoc('extra2', 'value2')

    print(f"Base map has {len(base)} entries")
    print(f"var1 has {len(var1)} entries (shares structure with base)")
    print(f"var2 has {len(var2)} entries (shares structure with base)")
    print(f"var1 and var2 are independent but efficient")
