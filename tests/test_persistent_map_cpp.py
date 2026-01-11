"""Unit tests for PersistentDict C++ implementation."""

import pytest
from pypersistent import PersistentDict


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

    def test_iterator_from_temporary_merge(self):
        """
        Regression test for iterator dangling pointer bug.

        The bug: creating an iterator from a temporary PersistentDict
        (e.g., (pm1 | pm2).items()) caused a segfault because the
        temporary was destroyed before the iterator was consumed,
        leaving the iterator with a dangling pointer to the root node.

        The fix: MapIterator now holds a reference to the root node
        by incrementing its refcount.
        """
        d = {x: x for x in range(10000)}
        d2 = {y: y for y in range(5000, 15000, 1)}
        pm1 = PersistentDict.from_dict(d)
        pm2 = PersistentDict.from_dict(d2)

        # This used to crash with exit code 139 (SIGSEGV)
        a = sorted((d | d2).items())
        b = sorted((pm1 | pm2).items())
        assert a == b

        # Also test with keys() and values()
        merged_keys = sorted((pm1 | pm2).keys())
        merged_values = set((pm1 | pm2).values())
        assert len(merged_keys) == 15000
        assert len(merged_values) == 15000

    def test_large_merge_sorted_items_list(self):
        """
        Regression test for merge crash with items_list() + sorted().
        
        The bug: After merging large maps (>=100 entries), the count was
        incorrect (count_ + n instead of actual count), causing items_list()
        to pre-allocate a list with too many slots. This left trailing None
        elements that caused a segfault when sorted() tried to compare them.
        
        The fix: Count actual entries after merging by iterating the merged tree.
        """
        # Create maps with overlapping keys
        pm1 = PersistentDict.from_dict({x: x for x in range(10000)})
        pm2 = PersistentDict.from_dict({y: y for y in range(5000, 15000)})
        
        # Merge them
        merged = pm1 | pm2
        
        # Verify correct count (15000 not 20000)
        assert len(merged) == 15000
        
        # Get items_list - this used to create list with 20000 slots
        items = merged.items_list()
        assert len(items) == 15000
        
        # This used to crash trying to sort None elements
        sorted_items = sorted(items)
        assert len(sorted_items) == 15000
        
        # Verify all items are tuples, not None
        for item in sorted_items:
            assert isinstance(item, tuple)
            assert len(item) == 2
        
        # Verify first and last items
        assert sorted_items[0] == (0, 0)
        assert sorted_items[-1] == (14999, 14999)


class TestPersistentDictPickle:
    """Test pickle serialization for PersistentDict."""

    def test_pickle_empty(self):
        """Test pickling an empty PersistentDict."""
        import pickle
        m = PersistentDict()
        pickled = pickle.dumps(m)
        restored = pickle.loads(pickled)
        assert restored == m
        assert len(restored) == 0

    def test_pickle_small(self):
        """Test pickling a small PersistentDict."""
        import pickle
        m = PersistentDict.create(a=1, b=2, c=3)
        pickled = pickle.dumps(m)
        restored = pickle.loads(pickled)
        assert restored == m
        assert restored['a'] == 1
        assert restored['b'] == 2
        assert restored['c'] == 3

    def test_pickle_large(self):
        """Test pickling a large PersistentDict."""
        import pickle
        m = PersistentDict.from_dict({i: f'val{i}' for i in range(1000)})
        pickled = pickle.dumps(m)
        restored = pickle.loads(pickled)
        assert restored == m
        assert len(restored) == 1000
        assert restored[500] == 'val500'

    def test_pickle_nested(self):
        """Test pickling nested persistent structures."""
        import pickle
        from pypersistent import PersistentList

        m = PersistentDict.create(
            name='test',
            values=PersistentList.create(1, 2, 3),
            count=42
        )
        pickled = pickle.dumps(m)
        restored = pickle.loads(pickled)
        assert restored == m
        assert restored['name'] == 'test'
        assert restored['values'] == PersistentList.create(1, 2, 3)
        assert restored['count'] == 42

    def test_pickle_various_types(self):
        """Test pickling a dict with various value types."""
        import pickle
        m = PersistentDict.from_dict({
            'int': 42,
            'float': 3.14,
            'str': 'hello',
            'list': [1, 2, 3],
            'dict': {'nested': 'value'},
            'none': None,
            'bool': True
        })
        pickled = pickle.dumps(m)
        restored = pickle.loads(pickled)
        assert restored == m
        assert restored['int'] == 42
        assert restored['float'] == 3.14
        assert restored['str'] == 'hello'
        assert restored['list'] == [1, 2, 3]
        assert restored['dict'] == {'nested': 'value'}
        assert restored['none'] is None
        assert restored['bool'] is True
