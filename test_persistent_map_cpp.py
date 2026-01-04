"""Unit tests for PersistentMap C++ implementation."""

import pytest
from pypersistent import PersistentMap


class TestPersistentMapBasics:
    """Test basic operations on PersistentMap."""

    def test_empty_map_creation(self):
        """Test creating an empty PersistentMap."""
        m = PersistentMap()
        assert len(m) == 0
        assert repr(m) == "PersistentMap({})"

    def test_basic_assoc(self):
        """Test basic association of key-value pairs."""
        m = PersistentMap()
        m2 = m.assoc('name', 'Alice').assoc('age', 30).assoc('city', 'NYC')

        assert len(m2) == 3
        assert m2.get('name') == 'Alice'
        assert m2.get('age') == 30
        assert m2['age'] == 30  # Test bracket notation
        assert m2['city'] == 'NYC'

    def test_contains(self):
        """Test membership checking with 'in' operator."""
        m = PersistentMap()
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
        m = PersistentMap().assoc('key', 'value')

        assert m.get('key') == 'value'
        assert m.get('nonexistent') is None
        assert m.get('nonexistent', 'default') == 'default'


class TestPersistentMapImmutability:
    """Test immutability guarantees."""

    def test_immutability(self):
        """Test that original map remains unchanged after creating new versions."""
        m1 = PersistentMap()
        m2 = m1.assoc('name', 'Alice').assoc('age', 30).assoc('city', 'NYC')

        # m1 should still be empty
        assert len(m1) == 0
        assert 'name' not in m1

        # m2 should have the values
        assert len(m2) == 3
        assert 'name' in m2

    def test_update_existing_key(self):
        """Test updating an existing key creates a new map."""
        m2 = PersistentMap().assoc('name', 'Alice').assoc('age', 30).assoc('city', 'NYC')
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
        m3 = PersistentMap().assoc('name', 'Alice').assoc('age', 31).assoc('city', 'NYC').assoc('country', 'USA')
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
        m = PersistentMap().assoc('a', 1)
        m2 = m.dissoc('nonexistent')

        assert len(m2) == 1
        assert 'a' in m2


class TestPersistentMapIteration:
    """Test iteration methods."""

    def test_iteration_over_keys(self):
        """Test iterating over keys."""
        m = PersistentMap().assoc('name', 'Alice').assoc('age', 31).assoc('city', 'NYC').assoc('country', 'USA')

        keys = set(m)
        assert keys == {'name', 'age', 'city', 'country'}

        # Test that we can access values during iteration
        for key in m:
            value = m[key]
            assert value is not None

    def test_items_iteration(self):
        """Test iterating over items (key-value pairs)."""
        m = PersistentMap().assoc('name', 'Alice').assoc('age', 31).assoc('city', 'NYC')

        items = dict(m.items())
        assert items == {'name': 'Alice', 'age': 31, 'city': 'NYC'}

        # Test tuple unpacking
        for key, val in m.items():
            assert m[key] == val

    def test_keys_method(self):
        """Test keys() method."""
        m = PersistentMap().assoc('x', 1).assoc('y', 2).assoc('z', 3)

        keys = set(m.keys())
        assert keys == {'x', 'y', 'z'}

    def test_values_method(self):
        """Test values() method."""
        m = PersistentMap().assoc('x', 1).assoc('y', 2).assoc('z', 3)

        values = sorted(m.values())
        assert values == [1, 2, 3]


class TestPersistentMapFactoryMethods:
    """Test factory methods and creation."""

    def test_from_dict(self):
        """Test creating from a dictionary."""
        m = PersistentMap.from_dict({'x': 1, 'y': 2, 'z': 3})

        assert len(m) == 3
        assert m.get('x') == 1
        assert m.get('y') == 2
        assert m.get('z') == 3

    def test_create_factory_method(self):
        """Test create() factory method with keyword arguments."""
        m = PersistentMap.create(a=1, b=2, c=3)

        assert len(m) == 3
        assert m.get('a') == 1
        assert m.get('b') == 2
        assert m.get('c') == 3


class TestPersistentMapStructuralSharing:
    """Test structural sharing and efficiency."""

    def test_structural_sharing_large_scale(self):
        """
        Test structural sharing with large maps.
        Creating variants should be efficient due to structural sharing.
        """
        # Create base map with 1000 entries
        base = PersistentMap()
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


class TestPersistentMapEdgeCases:
    """Test edge cases and error conditions."""

    def test_getitem_keyerror(self):
        """Test that bracket notation raises KeyError for missing keys."""
        m = PersistentMap().assoc('a', 1)

        with pytest.raises(KeyError) as exc_info:
            _ = m['nonexistent']

        assert 'nonexistent' in str(exc_info.value)

    def test_equality(self):
        """Test equality between PersistentMaps."""
        m1 = PersistentMap().assoc('a', 1).assoc('b', 2)
        m2 = PersistentMap().assoc('b', 2).assoc('a', 1)
        m3 = PersistentMap().assoc('a', 1).assoc('b', 3)

        # Same content, equal
        assert m1 == m2

        # Different content, not equal
        assert m1 != m3

        # Different types, not equal
        assert m1 != {'a': 1, 'b': 2}

    def test_empty_map_operations(self):
        """Test operations on empty map."""
        m = PersistentMap()

        assert len(m) == 0
        assert list(m) == []
        assert list(m.items()) == []
        assert list(m.keys()) == []
        assert list(m.values()) == []
        assert m.get('any') is None
        assert 'any' not in m

    def test_single_element_map(self):
        """Test map with single element."""
        m = PersistentMap().assoc('only', 'value')

        assert len(m) == 1
        assert 'only' in m
        assert m['only'] == 'value'

        # Removing the only element
        m2 = m.dissoc('only')
        assert len(m2) == 0
        assert 'only' not in m2

    def test_chaining_operations(self):
        """Test chaining multiple operations."""
        m = (PersistentMap()
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
        m = PersistentMap().assoc('key', None)

        assert 'key' in m
        assert m['key'] is None
        assert m.get('key') is None
        assert m.get('key', 'default') is None

    def test_various_key_types(self):
        """Test different types of keys."""
        m = (PersistentMap()
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
        m = PersistentMap().assoc('a', 1)
        repr_str = repr(m)

        assert 'PersistentMap' in repr_str
        assert "'a'" in repr_str or '"a"' in repr_str
        assert '1' in repr_str

    def test_iterator_from_temporary_merge(self):
        """
        Regression test for iterator dangling pointer bug.

        The bug: creating an iterator from a temporary PersistentMap
        (e.g., (pm1 | pm2).items()) caused a segfault because the
        temporary was destroyed before the iterator was consumed,
        leaving the iterator with a dangling pointer to the root node.

        The fix: MapIterator now holds a reference to the root node
        by incrementing its refcount.
        """
        d = {x: x for x in range(10000)}
        d2 = {y: y for y in range(5000, 15000, 1)}
        pm1 = PersistentMap.from_dict(d)
        pm2 = PersistentMap.from_dict(d2)

        # This used to crash with exit code 139 (SIGSEGV)
        a = sorted((d | d2).items())
        b = sorted((pm1 | pm2).items())
        assert a == b

        # Also test with keys() and values()
        merged_keys = sorted((pm1 | pm2).keys())
        merged_values = set((pm1 | pm2).values())
        assert len(merged_keys) == 15000
        assert len(merged_values) == 15000
