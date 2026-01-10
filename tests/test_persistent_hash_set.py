"""
Tests for PersistentSet - Immutable set implementation

Tests verify:
- Basic operations (conj, disj, contains)
- Set operations (union, intersection, difference, symmetric_difference)
- Set operators (|, &, -, ^)
- Set predicates (issubset, issuperset, isdisjoint)
- Immutability guarantees
- Factory methods
- Equality comparison
"""

import pytest
from pypersistent import PersistentSet


class TestPersistentSetBasics:
    """Test basic operations on PersistentSet"""

    def test_empty_set(self):
        """Test empty set creation"""
        s = PersistentSet()
        assert len(s) == 0
        assert 'elem' not in s

    def test_conj_single(self):
        """Test adding a single element"""
        s = PersistentSet()
        s2 = s.conj(1)
        assert len(s2) == 1
        assert 1 in s2

    def test_conj_multiple(self):
        """Test adding multiple elements"""
        s = PersistentSet()
        s = s.conj(1).conj(2).conj(3)
        assert len(s) == 3
        assert 1 in s
        assert 2 in s
        assert 3 in s

    def test_conj_duplicate(self):
        """Test that adding duplicate doesn't increase size"""
        s = PersistentSet.create(1, 2, 3)
        s2 = s.conj(2)
        assert len(s2) == 3

    def test_disj(self):
        """Test removing an element"""
        s = PersistentSet.create(1, 2, 3)
        s2 = s.disj(2)
        assert len(s2) == 2
        assert 2 not in s2
        assert 1 in s2
        assert 3 in s2

    def test_disj_nonexistent(self):
        """Test removing element that doesn't exist"""
        s = PersistentSet.create(1, 2, 3)
        s2 = s.disj(4)
        assert s2 == s  # Should return same set

    def test_contains(self):
        """Test membership testing"""
        s = PersistentSet.create(1, 2, 3)
        assert s.contains(1)
        assert not s.contains(4)
        assert 1 in s
        assert 4 not in s


class TestPersistentSetOperations:
    """Test set operations"""

    def test_union(self):
        """Test set union"""
        s1 = PersistentSet.create(1, 2, 3)
        s2 = PersistentSet.create(3, 4, 5)
        s3 = s1.union(s2)
        assert len(s3) == 5
        assert set(s3.list()) == {1, 2, 3, 4, 5}

    def test_intersection(self):
        """Test set intersection"""
        s1 = PersistentSet.create(1, 2, 3)
        s2 = PersistentSet.create(2, 3, 4)
        s3 = s1.intersection(s2)
        assert len(s3) == 2
        assert set(s3.list()) == {2, 3}

    def test_difference(self):
        """Test set difference"""
        s1 = PersistentSet.create(1, 2, 3)
        s2 = PersistentSet.create(2, 3, 4)
        s3 = s1.difference(s2)
        assert len(s3) == 1
        assert 1 in s3

    def test_symmetric_difference(self):
        """Test symmetric difference"""
        s1 = PersistentSet.create(1, 2, 3)
        s2 = PersistentSet.create(2, 3, 4)
        s3 = s1.symmetric_difference(s2)
        assert len(s3) == 2
        assert set(s3.list()) == {1, 4}


class TestPersistentSetOperators:
    """Test set operators"""

    def test_or_operator(self):
        """Test | operator for union"""
        s1 = PersistentSet.create(1, 2, 3)
        s2 = PersistentSet.create(3, 4, 5)
        s3 = s1 | s2
        assert len(s3) == 5
        assert set(s3.list()) == {1, 2, 3, 4, 5}

    def test_and_operator(self):
        """Test & operator for intersection"""
        s1 = PersistentSet.create(1, 2, 3)
        s2 = PersistentSet.create(2, 3, 4)
        s3 = s1 & s2
        assert len(s3) == 2
        assert set(s3.list()) == {2, 3}

    def test_sub_operator(self):
        """Test - operator for difference"""
        s1 = PersistentSet.create(1, 2, 3)
        s2 = PersistentSet.create(2, 3, 4)
        s3 = s1 - s2
        assert len(s3) == 1
        assert 1 in s3

    def test_xor_operator(self):
        """Test ^ operator for symmetric difference"""
        s1 = PersistentSet.create(1, 2, 3)
        s2 = PersistentSet.create(2, 3, 4)
        s3 = s1 ^ s2
        assert len(s3) == 2
        assert set(s3.list()) == {1, 4}


class TestPersistentSetPredicates:
    """Test set predicates"""

    def test_issubset(self):
        """Test subset testing"""
        s1 = PersistentSet.create(1, 2)
        s2 = PersistentSet.create(1, 2, 3)
        assert s1.issubset(s2)
        assert not s2.issubset(s1)
        assert s1 <= s2

    def test_issuperset(self):
        """Test superset testing"""
        s1 = PersistentSet.create(1, 2, 3)
        s2 = PersistentSet.create(1, 2)
        assert s1.issuperset(s2)
        assert not s2.issuperset(s1)
        assert s1 >= s2

    def test_proper_subset(self):
        """Test proper subset (< operator)"""
        s1 = PersistentSet.create(1, 2)
        s2 = PersistentSet.create(1, 2, 3)
        s3 = PersistentSet.create(1, 2)
        assert s1 < s2
        assert not s1 < s3  # Not proper subset of equal set

    def test_proper_superset(self):
        """Test proper superset (> operator)"""
        s1 = PersistentSet.create(1, 2, 3)
        s2 = PersistentSet.create(1, 2)
        s3 = PersistentSet.create(1, 2, 3)
        assert s1 > s2
        assert not s1 > s3  # Not proper superset of equal set

    def test_isdisjoint(self):
        """Test disjoint testing"""
        s1 = PersistentSet.create(1, 2, 3)
        s2 = PersistentSet.create(4, 5, 6)
        s3 = PersistentSet.create(3, 4, 5)
        assert s1.isdisjoint(s2)
        assert not s1.isdisjoint(s3)


class TestPersistentSetImmutability:
    """Test that PersistentSet is truly immutable"""

    def test_conj_immutability(self):
        """Test that conj doesn't modify original"""
        s1 = PersistentSet.create(1, 2, 3)
        s2 = s1.conj(4)
        assert len(s1) == 3
        assert len(s2) == 4
        assert 4 not in s1
        assert 4 in s2

    def test_disj_immutability(self):
        """Test that disj doesn't modify original"""
        s1 = PersistentSet.create(1, 2, 3)
        s2 = s1.disj(2)
        assert len(s1) == 3
        assert len(s2) == 2
        assert 2 in s1
        assert 2 not in s2

    def test_union_immutability(self):
        """Test that union doesn't modify original"""
        s1 = PersistentSet.create(1, 2)
        s2 = PersistentSet.create(3, 4)
        s3 = s1.union(s2)
        assert len(s1) == 2
        assert len(s2) == 2
        assert len(s3) == 4


class TestPersistentSetFactoryMethods:
    """Test factory methods for creating sets"""

    def test_from_set(self):
        """Test creating from Python set"""
        ps = {1, 2, 3, 4, 5}
        s = PersistentSet.from_set(ps)
        assert len(s) == 5
        assert set(s.list()) == {1, 2, 3, 4, 5}

    def test_from_list(self):
        """Test creating from list (duplicates removed)"""
        l = [1, 2, 2, 3, 3, 3]
        s = PersistentSet.from_list(l)
        assert len(s) == 3
        assert set(s.list()) == {1, 2, 3}

    def test_create(self):
        """Test creating with arguments"""
        s = PersistentSet.create(1, 2, 3, 4, 5)
        assert len(s) == 5
        assert set(s.list()) == {1, 2, 3, 4, 5}

    def test_create_empty(self):
        """Test creating with no arguments"""
        s = PersistentSet.create()
        assert len(s) == 0


class TestPersistentSetUpdate:
    """Test update operations"""

    def test_update_with_set(self):
        """Test updating with Python set"""
        s1 = PersistentSet.create(1, 2)
        s2 = s1.update({3, 4, 5})
        assert len(s2) == 5
        assert set(s2.list()) == {1, 2, 3, 4, 5}

    def test_update_with_hashset(self):
        """Test updating with another PersistentSet"""
        s1 = PersistentSet.create(1, 2)
        s2 = PersistentSet.create(3, 4)
        s3 = s1.update(s2)
        assert len(s3) == 4
        assert set(s3.list()) == {1, 2, 3, 4}

    def test_update_with_list(self):
        """Test updating with list"""
        s1 = PersistentSet.create(1, 2)
        s2 = s1.update([2, 3, 4])
        assert len(s2) == 4
        assert set(s2.list()) == {1, 2, 3, 4}


class TestPersistentSetEquality:
    """Test equality comparisons"""

    def test_equality_same_elements(self):
        """Test that sets with same elements are equal"""
        s1 = PersistentSet.create(1, 2, 3)
        s2 = PersistentSet.create(3, 2, 1)  # Different order
        assert s1 == s2

    def test_inequality_different_elements(self):
        """Test that sets with different elements are not equal"""
        s1 = PersistentSet.create(1, 2, 3)
        s2 = PersistentSet.create(1, 2, 4)
        assert s1 != s2

    def test_inequality_different_sizes(self):
        """Test that sets with different sizes are not equal"""
        s1 = PersistentSet.create(1, 2)
        s2 = PersistentSet.create(1, 2, 3)
        assert s1 != s2

    def test_inequality_with_non_set(self):
        """Test that set is not equal to non-set"""
        s = PersistentSet.create(1, 2, 3)
        assert s != {1, 2, 3}
        assert s != [1, 2, 3]


class TestPersistentSetIteration:
    """Test iteration"""

    def test_iteration(self):
        """Test iterating over set"""
        s = PersistentSet.create(1, 2, 3, 4, 5)
        elements = set(s)
        assert elements == {1, 2, 3, 4, 5}

    def test_list_method(self):
        """Test list() method"""
        s = PersistentSet.create(1, 2, 3)
        l = s.list()
        assert isinstance(l, list)
        assert set(l) == {1, 2, 3}


class TestPersistentSetEdgeCases:
    """Test edge cases and special scenarios"""

    def test_none_as_element(self):
        """Test that None can be an element"""
        s = PersistentSet.create(None, 1, 2)
        assert len(s) == 3
        assert None in s

    def test_large_set(self):
        """Test with large set"""
        elements = range(1000)
        s = PersistentSet.from_list(list(elements))
        assert len(s) == 1000
        for i in elements:
            assert i in s

    def test_clear(self):
        """Test clear method"""
        s = PersistentSet.create(1, 2, 3)
        s2 = s.clear()
        assert len(s2) == 0
        assert len(s) == 3  # Original unchanged

    def test_copy(self):
        """Test copy method (should return self for immutable)"""
        s = PersistentSet.create(1, 2, 3)
        s2 = s.copy()
        assert s2 == s

    def test_repr(self):
        """Test string representation"""
        s = PersistentSet.create(1, 2, 3)
        r = repr(s)
        assert 'PersistentSet' in r

    def test_pythonic_aliases(self):
        """Test Pythonic method aliases"""
        s = PersistentSet()

        # add is alias for conj
        s1 = s.add(1)
        s2 = s.conj(1)
        assert s1 == s2

        # remove is alias for disj
        s3 = s1.remove(1)
        s4 = s1.disj(1)
        assert s3 == s4


if __name__ == '__main__':
    pytest.main([__file__, '-v'])
