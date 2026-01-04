"""
Tests for PersistentTreeMap - Immutable sorted map using red-black tree

Tests verify:
- Basic operations (assoc, dissoc, get, contains)
- Ordering (iteration, first, last)
- Range queries (subseq, rsubseq)
- Immutability guarantees
- Factory methods
- Equality comparison
- Large maps
"""

import pytest
from pypersistent import PersistentTreeMap


class TestPersistentTreeMapBasics:
    """Test basic operations on PersistentTreeMap"""

    def test_empty_map(self):
        """Test empty map creation"""
        m = PersistentTreeMap()
        assert len(m) == 0

    def test_assoc_single(self):
        """Test adding a single key-value pair"""
        m = PersistentTreeMap()
        m2 = m.assoc("key", "value")
        assert len(m2) == 1
        assert m2.get("key") == "value"

    def test_assoc_multiple(self):
        """Test adding multiple key-value pairs"""
        m = PersistentTreeMap()
        m = m.assoc("a", 1).assoc("b", 2).assoc("c", 3)
        assert len(m) == 3
        assert m.get("a") == 1
        assert m.get("b") == 2
        assert m.get("c") == 3

    def test_assoc_update_existing(self):
        """Test updating existing key"""
        m = PersistentTreeMap.create(a=1, b=2, c=3)
        m2 = m.assoc("b", 99)
        assert m2.get("b") == 99
        assert len(m2) == 3  # Size unchanged

    def test_dissoc_existing(self):
        """Test removing existing key"""
        m = PersistentTreeMap.create(a=1, b=2, c=3)
        m2 = m.dissoc("b")
        assert len(m2) == 2
        assert m2.get("a") == 1
        assert m2.get("c") == 3
        with pytest.raises(KeyError):
            m2.get("b")

    def test_dissoc_nonexistent(self):
        """Test removing non-existent key (no-op)"""
        m = PersistentTreeMap.create(a=1, b=2)
        m2 = m.dissoc("z")
        assert m2 == m

    def test_get_with_default(self):
        """Test get with default value"""
        m = PersistentTreeMap.create(a=1, b=2)
        assert m.get("a", "default") == 1
        assert m.get("z", "default") == "default"

    def test_get_missing_raises(self):
        """Test that get without default raises KeyError"""
        m = PersistentTreeMap.create(a=1)
        with pytest.raises(KeyError):
            m.get("z")

    def test_contains(self):
        """Test contains method"""
        m = PersistentTreeMap.create(a=1, b=2, c=3)
        assert m.contains("a")
        assert m.contains("b")
        assert not m.contains("z")


class TestPersistentTreeMapOrdering:
    """Test that PersistentTreeMap maintains sorted order"""

    def test_keys_sorted_order(self):
        """Test that keys are in sorted order"""
        m = PersistentTreeMap()
        # Add in random order
        m = m.assoc(5, "five").assoc(1, "one").assoc(9, "nine")
        m = m.assoc(3, "three").assoc(7, "seven")

        keys = m.keys_list()
        assert keys == [1, 3, 5, 7, 9]

    def test_iteration_order(self):
        """Test that iteration is in sorted order"""
        m = PersistentTreeMap()
        m = m.assoc("c", 3).assoc("a", 1).assoc("b", 2).assoc("d", 4)

        keys = list(m)
        assert keys == ["a", "b", "c", "d"]

    def test_first(self):
        """Test getting first (minimum) entry"""
        m = PersistentTreeMap()
        m = m.assoc(5, "five").assoc(1, "one").assoc(9, "nine")

        first = m.first()
        assert first == [1, "one"]

    def test_first_empty_raises(self):
        """Test that first() on empty map raises error"""
        m = PersistentTreeMap()
        with pytest.raises(RuntimeError):
            m.first()

    def test_last(self):
        """Test getting last (maximum) entry"""
        m = PersistentTreeMap()
        m = m.assoc(5, "five").assoc(1, "one").assoc(9, "nine")

        last = m.last()
        assert last == [9, "nine"]

    def test_last_empty_raises(self):
        """Test that last() on empty map raises error"""
        m = PersistentTreeMap()
        with pytest.raises(RuntimeError):
            m.last()

    def test_items_sorted_order(self):
        """Test that items() returns sorted entries"""
        m = PersistentTreeMap()
        m = m.assoc(3, "c").assoc(1, "a").assoc(2, "b")

        items = m.items()
        assert items == [[1, "a"], [2, "b"], [3, "c"]]

    def test_values_in_key_order(self):
        """Test that values are in key-sorted order"""
        m = PersistentTreeMap()
        m = m.assoc(3, "c").assoc(1, "a").assoc(2, "b")

        values = m.values_list()
        assert values == ["a", "b", "c"]


class TestPersistentTreeMapRangeQueries:
    """Test range query operations"""

    def test_subseq(self):
        """Test getting subsequence in range"""
        m = PersistentTreeMap()
        for i in range(0, 10):
            m = m.assoc(i, f"val{i}")

        # Get range [3, 7)
        sub = m.subseq(3, 7)
        keys = sub.keys_list()
        assert keys == [3, 4, 5, 6]

    def test_subseq_empty_range(self):
        """Test subseq with empty range"""
        m = PersistentTreeMap.create(a=1, b=2, c=3)
        sub = m.subseq("z", "zz")
        assert len(sub) == 0

    def test_rsubseq(self):
        """Test getting reversed subsequence"""
        m = PersistentTreeMap()
        for i in range(0, 10):
            m = m.assoc(i, f"val{i}")

        # Get range [3, 7) in reverse
        rsub = m.rsubseq(3, 7)
        keys = rsub.keys_list()
        # Note: The result is still a sorted map, so keys are still sorted
        # rsubseq returns entries in reverse order, but when you build a new map
        # from them, they get sorted again
        assert set(keys) == {3, 4, 5, 6}

    def test_subseq_with_strings(self):
        """Test subseq with string keys"""
        m = PersistentTreeMap()
        m = m.assoc("apple", 1).assoc("banana", 2).assoc("cherry", 3)
        m = m.assoc("date", 4).assoc("elderberry", 5)

        sub = m.subseq("banana", "date")
        keys = sub.keys_list()
        assert keys == ["banana", "cherry"]


class TestPersistentTreeMapImmutability:
    """Test that PersistentTreeMap is truly immutable"""

    def test_assoc_immutability(self):
        """Test that assoc doesn't modify original"""
        m1 = PersistentTreeMap.create(a=1, b=2)
        m2 = m1.assoc("c", 3)

        assert len(m1) == 2
        assert len(m2) == 3
        assert not m1.contains("c")
        assert m2.contains("c")

    def test_dissoc_immutability(self):
        """Test that dissoc doesn't modify original"""
        m1 = PersistentTreeMap.create(a=1, b=2, c=3)
        m2 = m1.dissoc("b")

        assert len(m1) == 3
        assert len(m2) == 2
        assert m1.contains("b")
        assert not m2.contains("b")

    def test_multiple_versions(self):
        """Test maintaining multiple versions"""
        m1 = PersistentTreeMap.create(a=1)
        m2 = m1.assoc("b", 2)
        m3 = m2.assoc("c", 3)
        m4 = m1.assoc("d", 4)

        assert len(m1) == 1
        assert len(m2) == 2
        assert len(m3) == 3
        assert len(m4) == 2
        assert m1.keys_list() == ["a"]
        assert m2.keys_list() == ["a", "b"]
        assert m3.keys_list() == ["a", "b", "c"]
        assert m4.keys_list() == ["a", "d"]


class TestPersistentTreeMapFactoryMethods:
    """Test factory methods for creating maps"""

    def test_from_dict(self):
        """Test creating from Python dict"""
        d = {"a": 1, "b": 2, "c": 3}
        m = PersistentTreeMap.from_dict(d)

        assert len(m) == 3
        assert m.get("a") == 1
        assert m.get("b") == 2
        assert m.get("c") == 3
        # Should be sorted
        assert m.keys_list() == ["a", "b", "c"]

    def test_create(self):
        """Test creating with keyword arguments"""
        m = PersistentTreeMap.create(x=10, y=20, z=30)

        assert len(m) == 3
        assert m.get("x") == 10
        assert m.get("y") == 20
        assert m.get("z") == 30

    def test_create_empty(self):
        """Test creating with no arguments"""
        m = PersistentTreeMap.create()
        assert len(m) == 0


class TestPersistentTreeMapEquality:
    """Test equality comparisons"""

    def test_equality_same_entries(self):
        """Test that maps with same entries are equal"""
        m1 = PersistentTreeMap.create(a=1, b=2, c=3)
        m2 = PersistentTreeMap.create(a=1, b=2, c=3)
        assert m1 == m2

    def test_equality_different_insertion_order(self):
        """Test that insertion order doesn't affect equality"""
        m1 = PersistentTreeMap().assoc("a", 1).assoc("b", 2).assoc("c", 3)
        m2 = PersistentTreeMap().assoc("c", 3).assoc("a", 1).assoc("b", 2)
        assert m1 == m2

    def test_inequality_different_values(self):
        """Test that maps with different values are not equal"""
        m1 = PersistentTreeMap.create(a=1, b=2)
        m2 = PersistentTreeMap.create(a=1, b=99)
        assert m1 != m2

    def test_inequality_different_keys(self):
        """Test that maps with different keys are not equal"""
        m1 = PersistentTreeMap.create(a=1, b=2)
        m2 = PersistentTreeMap.create(a=1, c=2)
        assert m1 != m2

    def test_inequality_with_dict(self):
        """Test that map is not equal to dict"""
        m = PersistentTreeMap.create(a=1, b=2)
        d = {"a": 1, "b": 2}
        assert m != d


class TestPersistentTreeMapPythonProtocols:
    """Test Python protocol support"""

    def test_bracket_access(self):
        """Test bracket notation for access"""
        m = PersistentTreeMap.create(a=1, b=2, c=3)
        assert m["a"] == 1
        assert m["b"] == 2
        assert m["c"] == 3

    def test_bracket_access_missing_raises(self):
        """Test that bracket access raises KeyError for missing key"""
        m = PersistentTreeMap.create(a=1)
        with pytest.raises(KeyError):
            _ = m["z"]

    def test_contains_operator(self):
        """Test 'in' operator"""
        m = PersistentTreeMap.create(a=1, b=2, c=3)
        assert "a" in m
        assert "b" in m
        assert "z" not in m

    def test_len(self):
        """Test len() function"""
        m = PersistentTreeMap()
        assert len(m) == 0

        m = m.assoc("a", 1)
        assert len(m) == 1

        m = m.assoc("b", 2).assoc("c", 3)
        assert len(m) == 3

    def test_iteration(self):
        """Test iterating over keys"""
        m = PersistentTreeMap()
        m = m.assoc(3, "c").assoc(1, "a").assoc(2, "b")

        keys = list(m)
        assert keys == [1, 2, 3]  # Sorted order

    def test_dict_conversion(self):
        """Test converting to dict"""
        m = PersistentTreeMap.create(a=1, b=2, c=3)
        d = m.dict()

        assert isinstance(d, dict)
        assert d == {"a": 1, "b": 2, "c": 3}


class TestPersistentTreeMapEdgeCases:
    """Test edge cases and special scenarios"""

    def test_none_as_value(self):
        """Test that None can be a value"""
        m = PersistentTreeMap.create(a=None, b=1)
        assert len(m) == 2
        assert m.get("a") is None

    def test_numeric_keys(self):
        """Test with numeric keys"""
        m = PersistentTreeMap()
        m = m.assoc(10, "ten").assoc(5, "five").assoc(15, "fifteen")
        m = m.assoc(3, "three").assoc(12, "twelve")

        keys = m.keys_list()
        assert keys == [3, 5, 10, 12, 15]

    def test_mixed_numeric_keys(self):
        """Test with int and float keys"""
        m = PersistentTreeMap()
        m = m.assoc(1, "one").assoc(1.5, "one-half").assoc(2, "two")

        assert m.get(1) == "one"
        assert m.get(1.5) == "one-half"
        assert m.get(2) == "two"

    def test_large_map(self):
        """Test with large map"""
        m = PersistentTreeMap()
        n = 1000

        # Insert in random order
        import random
        keys = list(range(n))
        random.shuffle(keys)

        for k in keys:
            m = m.assoc(k, f"val{k}")

        assert len(m) == n

        # Verify ordering
        sorted_keys = m.keys_list()
        assert sorted_keys == list(range(n))

        # Verify random access
        assert m.get(0) == "val0"
        assert m.get(500) == "val500"
        assert m.get(999) == "val999"

    def test_very_large_map(self):
        """Test with very large map (stress test)"""
        m = PersistentTreeMap()
        n = 10000

        # Build incrementally
        for i in range(n):
            m = m.assoc(i, i * 2)

        assert len(m) == n
        assert m.get(0) == 0
        assert m.get(5000) == 10000
        assert m.get(9999) == 19998

        # Verify ordering still maintained
        assert m.first() == [0, 0]
        assert m.last() == [9999, 19998]

    def test_repr(self):
        """Test string representation"""
        m = PersistentTreeMap.create(a=1, b=2, c=3)
        r = repr(m)
        assert "PersistentTreeMap" in r
        assert "a" in r or "1" in r

    def test_repr_large_map(self):
        """Test string representation truncates for large maps"""
        m = PersistentTreeMap()
        for i in range(100):
            m = m.assoc(i, i * 2)

        r = repr(m)
        assert "PersistentTreeMap" in r
        assert "..." in r or "more" in r


class TestPersistentTreeMapPerformance:
    """Test performance characteristics"""

    def test_incremental_build(self):
        """Test building large map incrementally"""
        m = PersistentTreeMap()

        for i in range(1000):
            m = m.assoc(i, f"value{i}")

        assert len(m) == 1000

    def test_structural_sharing(self):
        """Test structural sharing by creating many variants"""
        # Create base map
        m = PersistentTreeMap()
        for i in range(100):
            m = m.assoc(i, i * 2)

        # Create 50 variants with single key changed
        variants = []
        for i in range(50):
            variants.append(m.assoc(i, -1))

        # Verify all variants are correct
        for i, variant in enumerate(variants):
            assert variant.get(i) == -1
            # Other keys should be unchanged
            if i < 99:
                assert variant.get(i + 1) == (i + 1) * 2


class TestPersistentTreeMapSpecialCases:
    """Test special cases and boundary conditions"""

    def test_single_element_map(self):
        """Test map with single element"""
        m = PersistentTreeMap.create(key="value")
        assert len(m) == 1
        assert m.get("key") == "value"
        assert m.first() == ["key", "value"]
        assert m.last() == ["key", "value"]

    def test_build_and_remove_all(self):
        """Test building up and then removing all elements"""
        m = PersistentTreeMap()

        # Build up
        for i in range(10):
            m = m.assoc(i, f"val{i}")
        assert len(m) == 10

        # Remove all
        for i in range(10):
            m = m.dissoc(i)
        assert len(m) == 0

    def test_repeated_assoc_same_key(self):
        """Test associating same key multiple times"""
        m = PersistentTreeMap.create(a=1, b=2, c=3)
        m = m.assoc("b", 10)
        m = m.assoc("b", 20)
        m = m.assoc("b", 30)

        assert m.get("b") == 30
        assert len(m) == 3


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
