"""
Tests for PersistentList - Immutable indexed sequence

Tests verify:
- Basic operations (conj, nth, assoc, pop)
- Indexing (positive, negative, slicing)
- Immutability guarantees
- Factory methods
- Equality comparison
- Iteration
- Tail optimization
- Large vectors (structural sharing)
"""

import pytest
from pypersistent import PersistentList


class TestPersistentListBasics:
    """Test basic operations on PersistentList"""

    def test_empty_vector(self):
        """Test empty vector creation"""
        v = PersistentList()
        assert len(v) == 0

    def test_conj_single(self):
        """Test appending a single element"""
        v = PersistentList()
        v2 = v.conj(1)
        assert len(v2) == 1
        assert v2.nth(0) == 1

    def test_conj_multiple(self):
        """Test appending multiple elements"""
        v = PersistentList()
        v = v.conj(1).conj(2).conj(3)
        assert len(v) == 3
        assert v.nth(0) == 1
        assert v.nth(1) == 2
        assert v.nth(2) == 3

    def test_nth_access(self):
        """Test nth() method for element access"""
        v = PersistentList.create(10, 20, 30, 40, 50)
        assert v.nth(0) == 10
        assert v.nth(2) == 30
        assert v.nth(4) == 50

    def test_nth_out_of_range(self):
        """Test that nth() raises error for out of range"""
        v = PersistentList.create(1, 2, 3)
        with pytest.raises(IndexError):
            v.nth(5)

    def test_assoc_update(self):
        """Test updating element at index"""
        v = PersistentList.create(1, 2, 3, 4, 5)
        v2 = v.assoc(2, 99)
        assert v2.nth(2) == 99
        assert v2.nth(0) == 1
        assert v2.nth(4) == 5

    def test_assoc_out_of_range(self):
        """Test that assoc() raises error for out of range"""
        v = PersistentList.create(1, 2, 3)
        with pytest.raises(IndexError):
            v.assoc(5, 99)

    def test_pop_single(self):
        """Test removing last element"""
        v = PersistentList.create(1, 2, 3)
        v2 = v.pop()
        assert len(v2) == 2
        assert v2.nth(0) == 1
        assert v2.nth(1) == 2

    def test_pop_empty(self):
        """Test that pop() raises error on empty vector"""
        v = PersistentList()
        with pytest.raises(RuntimeError):
            v.pop()

    def test_get_with_default(self):
        """Test get() method with default value"""
        v = PersistentList.create(1, 2, 3)
        assert v.get(1) == 2
        assert v.get(10, "default") == "default"

    def test_append_alias(self):
        """Test that append() is alias for conj()"""
        v = PersistentList()
        v1 = v.conj(1)
        v2 = v.append(1)
        assert v1 == v2

    def test_set_alias(self):
        """Test that set() is alias for assoc()"""
        v = PersistentList.create(1, 2, 3)
        v1 = v.assoc(1, 99)
        v2 = v.set(1, 99)
        assert v1 == v2


class TestPersistentListIndexing:
    """Test Python-style indexing"""

    def test_bracket_access(self):
        """Test bracket notation for element access"""
        v = PersistentList.create(10, 20, 30, 40, 50)
        assert v[0] == 10
        assert v[2] == 30
        assert v[4] == 50

    def test_negative_indexing(self):
        """Test negative indices"""
        v = PersistentList.create(10, 20, 30, 40, 50)
        assert v[-1] == 50
        assert v[-2] == 40
        assert v[-5] == 10

    def test_index_out_of_range(self):
        """Test that indexing raises error for out of range"""
        v = PersistentList.create(1, 2, 3)
        with pytest.raises(IndexError):
            _ = v[5]
        with pytest.raises(IndexError):
            _ = v[-5]

    def test_slicing(self):
        """Test slicing support"""
        v = PersistentList.create(0, 1, 2, 3, 4, 5, 6, 7, 8, 9)

        # Basic slice
        v2 = v[2:5]
        assert len(v2) == 3
        assert v2.list() == [2, 3, 4]

        # Slice to end
        v3 = v[7:]
        assert v3.list() == [7, 8, 9]

        # Slice from start
        v4 = v[:3]
        assert v4.list() == [0, 1, 2]

    def test_negative_slice_indices(self):
        """Test slicing with negative indices"""
        v = PersistentList.create(0, 1, 2, 3, 4, 5)
        v2 = v[-3:-1]
        assert v2.list() == [3, 4]

    def test_empty_slice(self):
        """Test that empty slice returns empty vector"""
        v = PersistentList.create(1, 2, 3)
        v2 = v[2:2]
        assert len(v2) == 0

    def test_slice_method(self):
        """Test explicit slice() method"""
        v = PersistentList.create(0, 1, 2, 3, 4, 5)
        v2 = v.slice(2, 5)
        assert v2.list() == [2, 3, 4]


class TestPersistentListImmutability:
    """Test that PersistentList is truly immutable"""

    def test_conj_immutability(self):
        """Test that conj doesn't modify original"""
        v1 = PersistentList.create(1, 2, 3)
        v2 = v1.conj(4)
        assert len(v1) == 3
        assert len(v2) == 4
        assert v1.list() == [1, 2, 3]
        assert v2.list() == [1, 2, 3, 4]

    def test_assoc_immutability(self):
        """Test that assoc doesn't modify original"""
        v1 = PersistentList.create(1, 2, 3)
        v2 = v1.assoc(1, 99)
        assert v1.nth(1) == 2
        assert v2.nth(1) == 99

    def test_pop_immutability(self):
        """Test that pop doesn't modify original"""
        v1 = PersistentList.create(1, 2, 3, 4)
        v2 = v1.pop()
        assert len(v1) == 4
        assert len(v2) == 3
        assert v1.list() == [1, 2, 3, 4]
        assert v2.list() == [1, 2, 3]


class TestPersistentListFactoryMethods:
    """Test factory methods for creating vectors"""

    def test_from_list(self):
        """Test creating from Python list"""
        l = [1, 2, 3, 4, 5]
        v = PersistentList.from_list(l)
        assert len(v) == 5
        assert v.list() == [1, 2, 3, 4, 5]

    def test_from_iterable(self):
        """Test creating from iterable"""
        r = range(5)
        v = PersistentList.from_iterable(r)
        assert len(v) == 5
        assert v.list() == [0, 1, 2, 3, 4]

    def test_create(self):
        """Test creating with arguments"""
        v = PersistentList.create(1, 2, 3, 4, 5)
        assert len(v) == 5
        assert v.list() == [1, 2, 3, 4, 5]

    def test_create_empty(self):
        """Test creating with no arguments"""
        v = PersistentList.create()
        assert len(v) == 0


class TestPersistentListEquality:
    """Test equality comparisons"""

    def test_equality_same_elements(self):
        """Test that vectors with same elements are equal"""
        v1 = PersistentList.create(1, 2, 3)
        v2 = PersistentList.create(1, 2, 3)
        assert v1 == v2

    def test_inequality_different_elements(self):
        """Test that vectors with different elements are not equal"""
        v1 = PersistentList.create(1, 2, 3)
        v2 = PersistentList.create(1, 2, 4)
        assert v1 != v2

    def test_inequality_different_lengths(self):
        """Test that vectors with different lengths are not equal"""
        v1 = PersistentList.create(1, 2)
        v2 = PersistentList.create(1, 2, 3)
        assert v1 != v2

    def test_inequality_with_non_vector(self):
        """Test that vector is not equal to non-vector"""
        v = PersistentList.create(1, 2, 3)
        assert v != [1, 2, 3]
        assert v != (1, 2, 3)


class TestPersistentListIteration:
    """Test iteration"""

    def test_iteration(self):
        """Test iterating over vector"""
        v = PersistentList.create(1, 2, 3, 4, 5)
        elements = list(v)
        assert elements == [1, 2, 3, 4, 5]

    def test_list_method(self):
        """Test list() method"""
        v = PersistentList.create(1, 2, 3)
        l = v.list()
        assert isinstance(l, list)
        assert l == [1, 2, 3]

    def test_contains(self):
        """Test membership testing"""
        v = PersistentList.create(1, 2, 3, 4, 5)
        assert 3 in v
        assert 10 not in v


class TestPersistentListEdgeCases:
    """Test edge cases and special scenarios"""

    def test_none_as_element(self):
        """Test that None can be an element"""
        v = PersistentList.create(None, 1, 2)
        assert len(v) == 3
        assert v.nth(0) is None

    def test_tail_boundary(self):
        """Test vector at tail boundary (32 elements)"""
        # Create vector with exactly 32 elements (fills one tail)
        elements = list(range(32))
        v = PersistentList.from_list(elements)
        assert len(v) == 32
        for i in range(32):
            assert v.nth(i) == i

        # Add one more to trigger tree creation
        v2 = v.conj(32)
        assert len(v2) == 33
        assert v2.nth(32) == 32

    def test_large_vector(self):
        """Test with large vector (>1000 elements)"""
        elements = list(range(1000))
        v = PersistentList.from_list(elements)
        assert len(v) == 1000

        # Verify random access
        assert v.nth(0) == 0
        assert v.nth(500) == 500
        assert v.nth(999) == 999

        # Verify structural sharing by updating
        v2 = v.assoc(500, 9999)
        assert v.nth(500) == 500  # Original unchanged
        assert v2.nth(500) == 9999  # Updated in new vector

    def test_very_large_vector(self):
        """Test with very large vector (>10000 elements)"""
        # Build incrementally to test tail optimization
        v = PersistentList()
        for i in range(10000):
            v = v.conj(i)

        assert len(v) == 10000
        assert v.nth(0) == 0
        assert v.nth(5000) == 5000
        assert v.nth(9999) == 9999

    def test_repr(self):
        """Test string representation"""
        v = PersistentList.create(1, 2, 3)
        r = repr(v)
        assert 'PersistentList' in r

    def test_repr_large_vector(self):
        """Test string representation truncates for large vectors"""
        v = PersistentList.from_list(list(range(100)))
        r = repr(v)
        assert 'PersistentList' in r
        # Should show truncation
        assert '...' in r or 'more' in r

    def test_mixed_types(self):
        """Test vector with mixed element types"""
        v = PersistentList.create(1, "hello", 3.14, None, [1, 2])
        assert v.nth(0) == 1
        assert v.nth(1) == "hello"
        assert v.nth(2) == 3.14
        assert v.nth(3) is None
        assert v.nth(4) == [1, 2]

    def test_nested_vectors(self):
        """Test vector containing vectors"""
        v1 = PersistentList.create(1, 2, 3)
        v2 = PersistentList.create(4, 5, 6)
        v3 = PersistentList.create(v1, v2)

        assert len(v3) == 2
        assert v3.nth(0) == v1
        assert v3.nth(1) == v2


class TestPersistentListPerformance:
    """Test performance characteristics"""

    def test_tail_optimization_performance(self):
        """Test that appending uses tail optimization (O(1) amortized)"""
        # Build large vector incrementally
        # This should complete quickly due to tail optimization
        v = PersistentList()
        for i in range(10000):
            v = v.conj(i)
        assert len(v) == 10000

    def test_structural_sharing(self):
        """Test structural sharing by creating many variants"""
        # Create base vector
        v = PersistentList.from_list(list(range(1000)))

        # Create 100 variants with single element changed
        # This should be fast due to structural sharing
        variants = []
        for i in range(100):
            variants.append(v.assoc(i, -1))

        # Verify all variants are correct
        for i, variant in enumerate(variants):
            assert variant.nth(i) == -1
            # Other elements should be unchanged
            if i < 999:
                assert variant.nth(i + 1) == i + 1


class TestPersistentListSpecialCases:
    """Test special cases and boundary conditions"""

    def test_single_element_vector(self):
        """Test vector with single element"""
        v = PersistentList.create(42)
        assert len(v) == 1
        assert v.nth(0) == 42
        assert v[-1] == 42

        v2 = v.pop()
        assert len(v2) == 0

    def test_build_and_reduce(self):
        """Test building up and then reducing vector"""
        v = PersistentList()

        # Build up
        for i in range(10):
            v = v.conj(i)
        assert len(v) == 10

        # Reduce back down
        for i in range(10):
            v = v.pop()
        assert len(v) == 0

    def test_multiple_updates_same_index(self):
        """Test updating same index multiple times"""
        v = PersistentList.create(1, 2, 3, 4, 5)
        v = v.assoc(2, 10)
        v = v.assoc(2, 20)
        v = v.assoc(2, 30)
        assert v.nth(2) == 30

    def test_slice_entire_vector(self):
        """Test slicing entire vector"""
        v = PersistentList.create(1, 2, 3, 4, 5)
        v2 = v[:]
        assert v2.list() == [1, 2, 3, 4, 5]


class TestPersistentListPickle:
    """Test pickle serialization for PersistentList."""

    def test_pickle_empty(self):
        """Test pickling an empty PersistentList."""
        import pickle
        v = PersistentList()
        pickled = pickle.dumps(v)
        restored = pickle.loads(pickled)
        assert restored == v
        assert len(restored) == 0

    def test_pickle_small(self):
        """Test pickling a small PersistentList."""
        import pickle
        v = PersistentList.create(1, 2, 3, 4, 5)
        pickled = pickle.dumps(v)
        restored = pickle.loads(pickled)
        assert restored == v
        assert restored.list() == [1, 2, 3, 4, 5]

    def test_pickle_large(self):
        """Test pickling a large PersistentList."""
        import pickle
        v = PersistentList.from_list(list(range(1000)))
        pickled = pickle.dumps(v)
        restored = pickle.loads(pickled)
        assert restored == v
        assert len(restored) == 1000
        assert restored[500] == 500

    def test_pickle_various_types(self):
        """Test pickling a list with various value types."""
        import pickle
        v = PersistentList.from_list([
            42,
            3.14,
            'hello',
            [1, 2, 3],
            {'nested': 'value'},
            None,
            True
        ])
        pickled = pickle.dumps(v)
        restored = pickle.loads(pickled)
        assert restored == v
        assert restored[0] == 42
        assert restored[1] == 3.14
        assert restored[2] == 'hello'
        assert restored[3] == [1, 2, 3]
        assert restored[4] == {'nested': 'value'}
        assert restored[5] is None
        assert restored[6] is True


if __name__ == '__main__':
    pytest.main([__file__, '-v'])
