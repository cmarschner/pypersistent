"""Unit tests for PersistentDict implementation."""

import pytest
import sys
from pathlib import Path

# Add scripts directory to path to import pure Python implementation
sys.path.insert(0, str(Path(__file__).parent.parent / "scripts"))
from persistent_map import PersistentDict


class TestPersistentDictBasics:
    """Test basic operations on PersistentDict."""

    def test_empty_map_creation(self):
        """Test creating an empty PersistentDict."""
        m = PersistentDict()
        assert len(m) == 0
        assert repr(m) == "PersistentDict({})"

    def test_basic_assoc(self):
        """Test basic association of key-value pairs."""
        m = PersistentDict()
        m2 = m.assoc('name', 'Alice').assoc('age', 30).assoc('city', 'NYC')

        assert len(m2) == 3
        assert m2.get('name') == 'Alice'
        assert m2.get('age') == 30
        assert m2['age'] == 30  # Test bracket notation
        assert m2['city'] == 'NYC'

    def test_contains(self):
        """Test membership checking with 'in' operator."""
        m = PersistentDict()
        m2 = m.assoc('a', 1).assoc('b', 2).assoc('c', 3)

        assert 'a' in m2
        assert 'b' in m2
        assert 'c' in m2
        assert 'd' not in m2
        assert 'x' not in m2

        # Test after dissoc
        m3 = m2.dissoc('b')
        assert 'b' not in m3
        assert 'a' in m3
        assert 'c' in m3

    def test_get_with_default(self):
        """Test get method with default values."""
        m = PersistentDict().assoc('key', 'value')

        assert m.get('key') == 'value'
        assert m.get('nonexistent') is None
        assert m.get('nonexistent', 'default') == 'default'


class TestPersistentDictImmutability:
    """Test immutability guarantees."""

    def test_immutability(self):
        """Test that original map remains unchanged after creating new versions."""
        m1 = PersistentDict()
        m2 = m1.assoc('name', 'Alice').assoc('age', 30).assoc('city', 'NYC')

        # m1 should still be empty
        assert len(m1) == 0
        assert 'name' not in m1

        # m2 should have the values
        assert len(m2) == 3
        assert 'name' in m2

    def test_update_existing_key(self):
        """Test updating an existing key creates a new map."""
        m2 = PersistentDict().assoc('name', 'Alice').assoc('age', 30).assoc('city', 'NYC')
        m3 = m2.assoc('age', 31).assoc('country', 'USA')

        # New map has updated value
        assert m3.get('age') == 31
        assert m3.get('country') == 'USA'
        assert len(m3) == 4

        # Original map unchanged
        assert m2.get('age') == 30
        assert 'country' not in m2
        assert len(m2) == 3

    def test_dissoc(self):
        """Test that dissoc removes a key and creates a new map."""
        m3 = PersistentDict().assoc('name', 'Alice').assoc('age', 31).assoc('city', 'NYC').assoc('country', 'USA')
        m4 = m3.dissoc('city')

        # New map doesn't have the key
        assert 'city' not in m4
        assert len(m4) == 3

        # Original map still has the key
        assert 'city' in m3
        assert m3.get('city') == 'NYC'
        assert len(m3) == 4

    def test_dissoc_nonexistent_key(self):
        """Test that dissoc on nonexistent key returns same map."""
        m = PersistentDict().assoc('a', 1)
        m2 = m.dissoc('nonexistent')

        assert len(m2) == 1
        assert 'a' in m2


class TestPersistentDictIteration:
    """Test iteration methods."""

    def test_iteration_over_keys(self):
        """Test iterating over keys."""
        m = PersistentDict().assoc('name', 'Alice').assoc('age', 31).assoc('city', 'NYC').assoc('country', 'USA')

        keys = set(m)
        assert keys == {'name', 'age', 'city', 'country'}

        # Test that we can access values during iteration
        for key in m:
            value = m[key]
            assert value is not None

    def test_items_iteration(self):
        """Test iterating over items (key-value pairs)."""
        m = PersistentDict().assoc('name', 'Alice').assoc('age', 31).assoc('city', 'NYC')

        items = dict(m.items())
        assert items == {'name': 'Alice', 'age': 31, 'city': 'NYC'}

        # Test tuple unpacking
        for key, val in m.items():
            assert m[key] == val

    def test_keys_method(self):
        """Test keys() method."""
        m = PersistentDict().assoc('x', 1).assoc('y', 2).assoc('z', 3)

        keys = set(m.keys())
        assert keys == {'x', 'y', 'z'}

    def test_values_method(self):
        """Test values() method."""
        m = PersistentDict().assoc('x', 1).assoc('y', 2).assoc('z', 3)

        values = sorted(m.values())
        assert values == [1, 2, 3]


class TestPersistentDictFactoryMethods:
    """Test factory methods and creation."""

    def test_from_dict(self):
        """Test creating from a dictionary."""
        m = PersistentDict.from_dict({'x': 1, 'y': 2, 'z': 3})

        assert len(m) == 3
        assert m.get('x') == 1
        assert m.get('y') == 2
        assert m.get('z') == 3

    def test_create_factory_method(self):
        """Test create() factory method with keyword arguments."""
        m = PersistentDict.create(a=1, b=2, c=3)

        assert len(m) == 3
        assert m.get('a') == 1
        assert m.get('b') == 2
        assert m.get('c') == 3


class TestPersistentDictStructuralSharing:
    """Test structural sharing and efficiency."""

    def test_structural_sharing_large_scale(self):
        """
        Test structural sharing with large maps.
        Creating variants should be efficient due to structural sharing.
        """
        # Create base map with 1000 entries
        base = PersistentDict()
        for i in range(1000):
            base = base.assoc(f'key{i}', i)

        assert len(base) == 1000

        # Create variations that share most structure
        var1 = base.assoc('extra1', 'value1')
        var2 = base.assoc('extra2', 'value2')

        assert len(var1) == 1001
        assert len(var2) == 1001
        assert len(base) == 1000  # Base unchanged

        # Variants are independent
        assert 'extra1' in var1
        assert 'extra1' not in var2
        assert 'extra2' in var2
        assert 'extra2' not in var1
        assert 'extra1' not in base
        assert 'extra2' not in base


class TestPersistentDictEdgeCases:
    """Test edge cases and error conditions."""

    def test_getitem_keyerror(self):
        """Test that bracket notation raises KeyError for missing keys."""
        m = PersistentDict().assoc('a', 1)

        with pytest.raises(KeyError) as exc_info:
            _ = m['nonexistent']

        assert 'nonexistent' in str(exc_info.value)

    def test_equality(self):
        """Test equality between PersistentDicts."""
        m1 = PersistentDict().assoc('a', 1).assoc('b', 2)
        m2 = PersistentDict().assoc('b', 2).assoc('a', 1)
        m3 = PersistentDict().assoc('a', 1).assoc('b', 3)

        # Same content, equal
        assert m1 == m2

        # Different content, not equal
        assert m1 != m3

        # Different types, not equal
        assert m1 != {'a': 1, 'b': 2}

    def test_empty_map_operations(self):
        """Test operations on empty map."""
        m = PersistentDict()

        assert len(m) == 0
        assert list(m) == []
        assert list(m.items()) == []
        assert list(m.keys()) == []
        assert list(m.values()) == []
        assert m.get('any') is None
        assert 'any' not in m

    def test_single_element_map(self):
        """Test map with single element."""
        m = PersistentDict().assoc('only', 'value')

        assert len(m) == 1
        assert 'only' in m
        assert m['only'] == 'value'

        # Removing the only element
        m2 = m.dissoc('only')
        assert len(m2) == 0
        assert 'only' not in m2

    def test_chaining_operations(self):
        """Test chaining multiple operations."""
        m = (PersistentDict()
             .assoc('a', 1)
             .assoc('b', 2)
             .assoc('c', 3)
             .dissoc('b')
             .assoc('d', 4))

        assert len(m) == 3
        assert 'a' in m
        assert 'b' not in m
        assert 'c' in m
        assert 'd' in m

    def test_none_as_value(self):
        """Test that None can be used as a value."""
        m = PersistentDict().assoc('key', None)

        assert 'key' in m
        assert m['key'] is None
        assert m.get('key') is None
        assert m.get('key', 'default') is None

    def test_various_key_types(self):
        """Test different types of keys."""
        m = (PersistentDict()
             .assoc('string', 1)
             .assoc(42, 2)
             .assoc(3.14, 3)
             .assoc((1, 2), 4))

        assert m['string'] == 1
        assert m[42] == 2
        assert m[3.14] == 3
        assert m[(1, 2)] == 4

    def test_repr_with_content(self):
        """Test string representation with content."""
        m = PersistentDict().assoc('a', 1)
        repr_str = repr(m)

        assert 'PersistentDict' in repr_str
        assert "'a'" in repr_str or '"a"' in repr_str
        assert '1' in repr_str


class TestCollisionNode:
    """Test CollisionNode optimization (Phase 5) - hash collision scenarios."""

    class CollidingKey:
        """Helper class to create keys with identical hashes."""
        def __init__(self, value, hash_value):
            self.value = value
            self._hash = hash_value

        def __hash__(self):
            return self._hash

        def __eq__(self, other):
            if not isinstance(other, TestCollisionNode.CollidingKey):
                return False
            return self.value == other.value

        def __repr__(self):
            return f"CollidingKey({self.value}, hash={self._hash})"

    def test_single_collision(self):
        """Test two keys with the same hash (simplest collision case)."""
        # Create two keys with same hash
        key1 = self.CollidingKey('key1', 12345)
        key2 = self.CollidingKey('key2', 12345)

        # Add both to map
        m = PersistentDict().assoc(key1, 'value1').assoc(key2, 'value2')

        # Both keys should be present
        assert len(m) == 2
        assert m[key1] == 'value1'
        assert m[key2] == 'value2'
        assert key1 in m
        assert key2 in m

    def test_multiple_collisions(self):
        """Test many keys with the same hash (stress test for CollisionNode)."""
        hash_value = 99999
        collision_size = 10

        # Create base map with colliding keys
        m = PersistentDict()
        for i in range(collision_size):
            key = self.CollidingKey(f'key{i}', hash_value)
            m = m.assoc(key, i)

        # Verify all keys are present
        assert len(m) == collision_size
        for i in range(collision_size):
            key = self.CollidingKey(f'key{i}', hash_value)
            assert m[key] == i
            assert key in m

    def test_collision_update_existing(self):
        """Test updating an existing key in a CollisionNode (COW semantics)."""
        key1 = self.CollidingKey('key1', 42)
        key2 = self.CollidingKey('key2', 42)
        key3 = self.CollidingKey('key3', 42)

        # Create map with collisions
        m1 = PersistentDict().assoc(key1, 'v1').assoc(key2, 'v2').assoc(key3, 'v3')

        # Update middle key
        m2 = m1.assoc(key2, 'v2_updated')

        # m2 should have updated value
        assert m2[key2] == 'v2_updated'
        assert m2[key1] == 'v1'
        assert m2[key3] == 'v3'

        # m1 should be unchanged (immutability)
        assert m1[key2] == 'v2'
        assert m1[key1] == 'v1'
        assert m1[key3] == 'v3'

    def test_collision_dissoc(self):
        """Test removing keys from a CollisionNode."""
        hash_value = 777
        keys = [self.CollidingKey(f'key{i}', hash_value) for i in range(5)]

        # Build map with 5 colliding keys
        m = PersistentDict()
        for i, key in enumerate(keys):
            m = m.assoc(key, i)

        assert len(m) == 5

        # Remove middle key
        m2 = m.dissoc(keys[2])
        assert len(m2) == 4
        assert keys[2] not in m2
        assert keys[0] in m2
        assert keys[1] in m2
        assert keys[3] in m2
        assert keys[4] in m2

        # Original unchanged
        assert len(m) == 5
        assert keys[2] in m

        # Remove all keys one by one
        m3 = m
        for key in keys:
            m3 = m3.dissoc(key)
        assert len(m3) == 0

    def test_collision_dissoc_nonexistent(self):
        """Test dissoc on nonexistent key in CollisionNode."""
        key1 = self.CollidingKey('key1', 555)
        key2 = self.CollidingKey('key2', 555)
        key_missing = self.CollidingKey('missing', 555)

        m1 = PersistentDict().assoc(key1, 'v1').assoc(key2, 'v2')
        m2 = m1.dissoc(key_missing)

        # Map unchanged
        assert len(m2) == 2
        assert m2[key1] == 'v1'
        assert m2[key2] == 'v2'

    def test_collision_iteration(self):
        """Test iterating over a CollisionNode."""
        hash_value = 333
        expected_items = {}

        m = PersistentDict()
        for i in range(8):
            key = self.CollidingKey(f'key{i}', hash_value)
            m = m.assoc(key, i * 10)
            expected_items[key] = i * 10

        # Iterate and verify all items present
        actual_items = {k: v for k, v in m.items()}
        assert len(actual_items) == 8
        assert actual_items == expected_items

        # Test keys() and values()
        assert len(list(m.keys())) == 8
        assert len(list(m.values())) == 8

    def test_collision_structural_sharing(self):
        """Test that CollisionNode uses structural sharing (shared_ptr optimization)."""
        hash_value = 111
        keys = [self.CollidingKey(f'key{i}', hash_value) for i in range(20)]

        # Build large collision node
        base = PersistentDict()
        for i, key in enumerate(keys):
            base = base.assoc(key, i)

        # Create variant with one additional collision
        new_key = self.CollidingKey('new_key', hash_value)
        variant = base.assoc(new_key, 999)

        # Both should have correct values
        assert len(base) == 20
        assert len(variant) == 21
        assert new_key not in base
        assert new_key in variant
        assert variant[new_key] == 999

        # All original keys should be in both
        for i, key in enumerate(keys):
            assert base[key] == i
            assert variant[key] == i

    def test_collision_then_dissoc_all(self):
        """Test creating collisions then removing all entries."""
        hash_value = 888
        keys = [self.CollidingKey(f'key{i}', hash_value) for i in range(6)]

        # Build collision node
        m = PersistentDict()
        for i, key in enumerate(keys):
            m = m.assoc(key, i)

        assert len(m) == 6

        # Remove all but one
        m2 = m
        for key in keys[:-1]:
            m2 = m2.dissoc(key)

        # Should have only last key
        assert len(m2) == 1
        assert keys[-1] in m2
        assert m2[keys[-1]] == 5

        # Remove last key
        m3 = m2.dissoc(keys[-1])
        assert len(m3) == 0
        assert keys[-1] not in m3

    def test_mixed_collision_and_normal_keys(self):
        """Test map with both colliding and non-colliding keys."""
        # Some colliding keys
        col1 = self.CollidingKey('col1', 100)
        col2 = self.CollidingKey('col2', 100)
        col3 = self.CollidingKey('col3', 100)

        # Normal keys (likely different hashes)
        normal_keys = ['normal1', 'normal2', 'normal3', 'normal4']

        # Build mixed map
        m = PersistentDict()
        m = m.assoc(col1, 'c1').assoc(col2, 'c2').assoc(col3, 'c3')
        for nk in normal_keys:
            m = m.assoc(nk, f'val_{nk}')

        # Verify all keys present
        assert len(m) == 7
        assert m[col1] == 'c1'
        assert m[col2] == 'c2'
        assert m[col3] == 'c3'
        for nk in normal_keys:
            assert m[nk] == f'val_{nk}'

        # Iterate over all
        items = dict(m.items())
        assert len(items) == 7

    def test_collision_with_none_values(self):
        """Test CollisionNode with None as values."""
        hash_value = 222
        keys = [self.CollidingKey(f'key{i}', hash_value) for i in range(4)]

        m = PersistentDict()
        for key in keys:
            m = m.assoc(key, None)

        # All keys should be present with None values
        assert len(m) == 4
        for key in keys:
            assert key in m
            assert m[key] is None

    def test_large_collision_node_performance(self):
        """Test performance with large collision (100 colliding keys)."""
        hash_value = 55555
        collision_size = 100

        # Build large collision node
        m = PersistentDict()
        for i in range(collision_size):
            key = self.CollidingKey(f'key{i}', hash_value)
            m = m.assoc(key, i)

        # Verify correctness
        assert len(m) == collision_size
        for i in range(collision_size):
            key = self.CollidingKey(f'key{i}', hash_value)
            assert m[key] == i

        # Test updates on large collision
        key_to_update = self.CollidingKey('key50', hash_value)
        m2 = m.assoc(key_to_update, 9999)
        assert m2[key_to_update] == 9999
        assert m[key_to_update] == 50  # Original unchanged

        # Test removals
        m3 = m
        for i in range(0, collision_size, 10):  # Remove every 10th
            key = self.CollidingKey(f'key{i}', hash_value)
            m3 = m3.dissoc(key)

        assert len(m3) == 90  # 100 - 10 removed
