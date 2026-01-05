"""
Tests for PersistentArrayMap - Small map optimization

Tests verify:
- Basic operations (assoc, dissoc, get, contains)
- Immutability guarantees
- Iteration methods
- Factory methods
- Size limit enforcement (8 entries max)
- Equality comparison
- Edge cases
"""

import pytest
from pypersistent import PersistentArrayMap


class TestPersistentArrayMapBasics:
    """Test basic operations on PersistentArrayMap"""

    def test_empty_map(self):
        """Test empty map creation"""
        m = PersistentArrayMap()
        assert len(m) == 0
        assert 'key' not in m

    def test_assoc_single(self):
        """Test adding a single key-value pair"""
        m = PersistentArrayMap()
        m2 = m.assoc('key', 'value')
        assert len(m2) == 1
        assert m2.get('key') == 'value'
        assert m2['key'] == 'value'
        assert 'key' in m2

    def test_assoc_multiple(self):
        """Test adding multiple entries"""
        m = PersistentArrayMap()
        m = m.assoc('a', 1).assoc('b', 2).assoc('c', 3)
        assert len(m) == 3
        assert m.get('a') == 1
        assert m.get('b') == 2
        assert m.get('c') == 3

    def test_assoc_update_existing(self):
        """Test updating an existing key"""
        m = PersistentArrayMap.create(a=1, b=2)
        m2 = m.assoc('a', 10)
        assert len(m2) == 2
        assert m2.get('a') == 10
        assert m2.get('b') == 2

    def test_dissoc(self):
        """Test removing a key"""
        m = PersistentArrayMap.create(a=1, b=2, c=3)
        m2 = m.dissoc('b')
        assert len(m2) == 2
        assert 'b' not in m2
        assert m2.get('a') == 1
        assert m2.get('c') == 3

    def test_dissoc_nonexistent(self):
        """Test removing a key that doesn't exist"""
        m = PersistentArrayMap.create(a=1)
        m2 = m.dissoc('b')
        assert m2 == m  # Should return same map

    def test_get_default(self):
        """Test get with default value"""
        m = PersistentArrayMap.create(a=1)
        assert m.get('a') == 1
        assert m.get('b') is None
        assert m.get('b', 'default') == 'default'

    def test_getitem_keyerror(self):
        """Test that __getitem__ raises KeyError for missing keys"""
        m = PersistentArrayMap.create(a=1)
        with pytest.raises(KeyError):
            _ = m['b']

    def test_contains(self):
        """Test membership testing"""
        m = PersistentArrayMap.create(a=1, b=2)
        assert m.contains('a')
        assert not m.contains('c')
        assert 'a' in m
        assert 'c' not in m


class TestPersistentArrayMapSizeLimit:
    """Test size limit enforcement (max 8 entries)"""

    def test_max_size_8(self):
        """Test that we can add up to 8 entries"""
        m = PersistentArrayMap()
        for i in range(8):
            m = m.assoc(f'key{i}', i)
        assert len(m) == 8

    def test_exceed_max_size(self):
        """Test that adding 9th entry raises RuntimeError"""
        m = PersistentArrayMap()
        for i in range(8):
            m = m.assoc(f'key{i}', i)

        with pytest.raises(RuntimeError, match="max size exceeded"):
            m.assoc('key8', 8)

    def test_from_dict_too_large(self):
        """Test that from_dict rejects dicts with >8 entries"""
        d = {f'key{i}': i for i in range(9)}
        with pytest.raises(RuntimeError, match="too large"):
            PersistentArrayMap.from_dict(d)

    def test_create_too_many_kwargs(self):
        """Test that create rejects >8 keyword arguments"""
        with pytest.raises(RuntimeError, match="Too many"):
            PersistentArrayMap.create(a=1, b=2, c=3, d=4, e=5, f=6, g=7, h=8, i=9)


class TestPersistentArrayMapImmutability:
    """Test that PersistentArrayMap is truly immutable"""

    def test_assoc_immutability(self):
        """Test that assoc doesn't modify original"""
        m1 = PersistentArrayMap.create(a=1, b=2)
        m2 = m1.assoc('c', 3)

        assert len(m1) == 2
        assert len(m2) == 3
        assert 'c' not in m1
        assert 'c' in m2

    def test_dissoc_immutability(self):
        """Test that dissoc doesn't modify original"""
        m1 = PersistentArrayMap.create(a=1, b=2, c=3)
        m2 = m1.dissoc('b')

        assert len(m1) == 3
        assert len(m2) == 2
        assert 'b' in m1
        assert 'b' not in m2

    def test_update_immutability(self):
        """Test that update doesn't modify original"""
        m1 = PersistentArrayMap.create(a=1, b=2)
        m2 = m1.update({'c': 3})

        assert len(m1) == 2
        assert len(m2) == 3
        assert 'c' not in m1
        assert 'c' in m2


class TestPersistentArrayMapIteration:
    """Test iteration methods"""

    def test_keys_iteration(self):
        """Test iterating over keys"""
        m = PersistentArrayMap.create(a=1, b=2, c=3)
        keys = set(m.keys())
        assert keys == {'a', 'b', 'c'}

    def test_values_iteration(self):
        """Test iterating over values"""
        m = PersistentArrayMap.create(a=1, b=2, c=3)
        values = set(m.values())
        assert values == {1, 2, 3}

    def test_items_iteration(self):
        """Test iterating over items"""
        m = PersistentArrayMap.create(a=1, b=2, c=3)
        items = set(m.items())
        assert items == {('a', 1), ('b', 2), ('c', 3)}

    def test_keys_list(self):
        """Test keys_list returns a list"""
        m = PersistentArrayMap.create(a=1, b=2, c=3)
        keys = m.keys_list()
        assert isinstance(keys, list)
        assert set(keys) == {'a', 'b', 'c'}

    def test_values_list(self):
        """Test values_list returns a list"""
        m = PersistentArrayMap.create(a=1, b=2, c=3)
        values = m.values_list()
        assert isinstance(values, list)
        assert set(values) == {1, 2, 3}

    def test_items_list(self):
        """Test items_list returns a list"""
        m = PersistentArrayMap.create(a=1, b=2, c=3)
        items = m.items_list()
        assert isinstance(items, list)
        assert set(items) == {('a', 1), ('b', 2), ('c', 3)}

    def test_iter_default(self):
        """Test that __iter__ iterates over keys"""
        m = PersistentArrayMap.create(a=1, b=2, c=3)
        keys = set(m)
        assert keys == {'a', 'b', 'c'}


class TestPersistentArrayMapFactoryMethods:
    """Test factory methods for creating maps"""

    def test_from_dict(self):
        """Test creating from dictionary"""
        d = {'a': 1, 'b': 2, 'c': 3}
        m = PersistentArrayMap.from_dict(d)
        assert len(m) == 3
        assert m.get('a') == 1
        assert m.get('b') == 2
        assert m.get('c') == 3

    def test_from_dict_empty(self):
        """Test creating from empty dictionary"""
        m = PersistentArrayMap.from_dict({})
        assert len(m) == 0

    def test_create(self):
        """Test creating with keyword arguments"""
        m = PersistentArrayMap.create(a=1, b=2, c=3)
        assert len(m) == 3
        assert m.get('a') == 1
        assert m.get('b') == 2
        assert m.get('c') == 3

    def test_create_empty(self):
        """Test creating with no arguments"""
        m = PersistentArrayMap.create()
        assert len(m) == 0


class TestPersistentArrayMapUpdate:
    """Test update/merge operations"""

    def test_update_with_dict(self):
        """Test updating with a dictionary"""
        m1 = PersistentArrayMap.create(a=1, b=2)
        m2 = m1.update({'c': 3, 'd': 4})
        assert len(m2) == 4
        assert m2.get('c') == 3
        assert m2.get('d') == 4

    def test_update_with_arraymap(self):
        """Test updating with another PersistentArrayMap"""
        m1 = PersistentArrayMap.create(a=1, b=2)
        m2 = PersistentArrayMap.create(c=3, d=4)
        m3 = m1.update(m2)
        assert len(m3) == 4
        assert m3.get('c') == 3
        assert m3.get('d') == 4

    def test_update_override(self):
        """Test that update overrides existing keys"""
        m1 = PersistentArrayMap.create(a=1, b=2)
        m2 = m1.update({'a': 10, 'c': 3})
        assert len(m2) == 3
        assert m2.get('a') == 10
        assert m2.get('b') == 2
        assert m2.get('c') == 3

    def test_merge_alias(self):
        """Test that merge is an alias for update"""
        m1 = PersistentArrayMap.create(a=1)
        m2 = m1.merge({'b': 2})
        assert len(m2) == 2
        assert m2.get('b') == 2

    def test_or_operator(self):
        """Test | operator for merging"""
        m1 = PersistentArrayMap.create(a=1, b=2)
        m2 = m1 | {'c': 3}
        assert len(m2) == 3
        assert m2.get('c') == 3


class TestPersistentArrayMapEquality:
    """Test equality comparisons"""

    def test_equality_same_entries(self):
        """Test that maps with same entries are equal"""
        m1 = PersistentArrayMap.create(a=1, b=2, c=3)
        m2 = PersistentArrayMap.create(a=1, b=2, c=3)
        assert m1 == m2

    def test_equality_different_order(self):
        """Test that order doesn't matter for equality"""
        m1 = PersistentArrayMap.create(a=1, b=2, c=3)
        m2 = PersistentArrayMap.create(c=3, a=1, b=2)
        assert m1 == m2

    def test_inequality_different_values(self):
        """Test that maps with different values are not equal"""
        m1 = PersistentArrayMap.create(a=1, b=2)
        m2 = PersistentArrayMap.create(a=1, b=3)
        assert m1 != m2

    def test_inequality_different_keys(self):
        """Test that maps with different keys are not equal"""
        m1 = PersistentArrayMap.create(a=1, b=2)
        m2 = PersistentArrayMap.create(a=1, c=2)
        assert m1 != m2

    def test_inequality_different_sizes(self):
        """Test that maps with different sizes are not equal"""
        m1 = PersistentArrayMap.create(a=1, b=2)
        m2 = PersistentArrayMap.create(a=1, b=2, c=3)
        assert m1 != m2

    def test_inequality_with_non_map(self):
        """Test that map is not equal to non-map"""
        m = PersistentArrayMap.create(a=1)
        assert m != {'a': 1}
        assert m != [('a', 1)]


class TestPersistentArrayMapEdgeCases:
    """Test edge cases and special scenarios"""

    def test_none_as_value(self):
        """Test that None can be used as a value"""
        m = PersistentArrayMap.create(a=None)
        assert m.get('a') is None
        assert 'a' in m

    def test_none_as_key(self):
        """Test that None can be used as a key"""
        m = PersistentArrayMap()
        m = m.assoc(None, 'value')
        assert m.get(None) == 'value'
        assert None in m

    def test_various_key_types(self):
        """Test various hashable key types"""
        m = PersistentArrayMap()
        m = m.assoc(1, 'int')
        m = m.assoc('str', 'string')
        m = m.assoc((1, 2), 'tuple')
        m = m.assoc(3.14, 'float')

        assert len(m) == 4
        assert m.get(1) == 'int'
        assert m.get('str') == 'string'
        assert m.get((1, 2)) == 'tuple'
        assert m.get(3.14) == 'float'

    def test_clear(self):
        """Test clear method"""
        m = PersistentArrayMap.create(a=1, b=2, c=3)
        m2 = m.clear()
        assert len(m2) == 0
        assert len(m) == 3  # Original unchanged

    def test_copy(self):
        """Test copy method (should return self for immutable)"""
        m = PersistentArrayMap.create(a=1, b=2)
        m2 = m.copy()
        assert m2 == m

    def test_repr(self):
        """Test string representation"""
        m = PersistentArrayMap.create(a=1, b=2)
        r = repr(m)
        assert 'PersistentArrayMap' in r
        assert 'a' in r or "'a'" in r
        assert '1' in r

    def test_pythonic_aliases(self):
        """Test Pythonic method aliases"""
        m = PersistentArrayMap()

        # set is alias for assoc
        m1 = m.set('a', 1)
        m2 = m.assoc('a', 1)
        assert m1 == m2

        # delete is alias for dissoc
        m3 = m1.delete('a')
        m4 = m1.dissoc('a')
        assert m3 == m4


class TestPersistentArrayMapPerformance:
    """Performance-related tests (verify behavior, not actual timing)"""

    def test_linear_scan_correctness(self):
        """Verify linear scan finds all entries"""
        # Create map with 8 entries
        entries = {f'key{i}': i for i in range(8)}
        m = PersistentArrayMap.from_dict(entries)

        # Verify all entries are found
        for key, value in entries.items():
            assert m.get(key) == value
            assert key in m

    def test_assoc_preserves_existing(self):
        """Verify assoc preserves all existing entries"""
        m = PersistentArrayMap.create(a=1, b=2, c=3)
        m = m.assoc('d', 4)

        assert m.get('a') == 1
        assert m.get('b') == 2
        assert m.get('c') == 3
        assert m.get('d') == 4

    def test_dissoc_preserves_remaining(self):
        """Verify dissoc preserves all non-removed entries"""
        m = PersistentArrayMap.create(a=1, b=2, c=3, d=4)
        m = m.dissoc('b')

        assert m.get('a') == 1
        assert m.get('c') == 3
        assert m.get('d') == 4
        assert 'b' not in m


if __name__ == '__main__':
    pytest.main([__file__, '-v'])
