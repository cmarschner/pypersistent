"""
Property-based integrity tests for PersistentDict.

Tests verify correctness by performing operations on both PersistentDict and
Python dict in parallel, then comparing results to ensure they match.
"""

import pytest
from pypersistent import PersistentDict
import random


class TestIntegrityBasic:
    """Basic integrity tests comparing PersistentDict to dict."""

    def _assert_maps_equal(self, pmap, pydict, msg=""):
        """Assert PersistentDict and dict have identical contents."""
        # Convert PersistentDict to dict for comparison
        pmap_dict = dict(pmap.items())
        assert pmap_dict == pydict, f"{msg}\nPersistentDict: {pmap_dict}\nPython dict: {pydict}"
        assert len(pmap) == len(pydict), f"{msg}\nLength mismatch"

    def test_single_assoc(self):
        """Test single key insertion."""
        pmap = PersistentDict()
        pydict = {}

        pmap = pmap.assoc('key1', 'value1')
        pydict['key1'] = 'value1'

        self._assert_maps_equal(pmap, pydict, "After single assoc")

    def test_multiple_assoc(self):
        """Test multiple key insertions."""
        pmap = PersistentDict()
        pydict = {}

        # Add 100 keys
        for i in range(100):
            pmap = pmap.assoc(f'key{i}', i)
            pydict[f'key{i}'] = i

        self._assert_maps_equal(pmap, pydict, "After 100 assoc operations")

    def test_assoc_update(self):
        """Test updating existing keys."""
        pmap = PersistentDict()
        pydict = {}

        # Add initial keys
        for i in range(50):
            pmap = pmap.assoc(f'key{i}', i)
            pydict[f'key{i}'] = i

        # Update half of them
        for i in range(0, 50, 2):
            pmap = pmap.assoc(f'key{i}', i * 100)
            pydict[f'key{i}'] = i * 100

        self._assert_maps_equal(pmap, pydict, "After updates")

    def test_dissoc(self):
        """Test key removal."""
        pmap = PersistentDict()
        pydict = {}

        # Add keys
        for i in range(100):
            pmap = pmap.assoc(f'key{i}', i)
            pydict[f'key{i}'] = i

        # Remove every 3rd key
        for i in range(0, 100, 3):
            pmap = pmap.dissoc(f'key{i}')
            del pydict[f'key{i}']

        self._assert_maps_equal(pmap, pydict, "After dissoc operations")

    def test_dissoc_nonexistent(self):
        """Test removing nonexistent keys."""
        pmap = PersistentDict()
        pydict = {}

        # Add some keys
        for i in range(10):
            pmap = pmap.assoc(f'key{i}', i)
            pydict[f'key{i}'] = i

        # Try to remove nonexistent keys (should be no-op)
        for i in range(100, 110):
            pmap = pmap.dissoc(f'key{i}')
            pydict.pop(f'key{i}', None)  # No-op if key doesn't exist

        self._assert_maps_equal(pmap, pydict, "After dissoc nonexistent")

    def test_from_dict(self):
        """Test creating from dict."""
        pydict = {f'key{i}': i for i in range(1000)}
        pmap = PersistentDict.from_dict(pydict)

        self._assert_maps_equal(pmap, pydict, "After from_dict")


class TestIntegrityMerge:
    """Test merge operation integrity."""

    def _assert_maps_equal(self, pmap, pydict, msg=""):
        """Assert PersistentDict and dict have identical contents."""
        pmap_dict = dict(pmap.items())
        assert pmap_dict == pydict, f"{msg}\nPersistentDict: {pmap_dict}\nPython dict: {pydict}"
        assert len(pmap) == len(pydict), f"{msg}\nLength mismatch"

    def test_merge_disjoint(self):
        """Test merging maps with no overlapping keys."""
        # Create first map
        pmap1 = PersistentDict()
        dict1 = {}
        for i in range(500):
            pmap1 = pmap1.assoc(f'a{i}', i)
            dict1[f'a{i}'] = i

        # Create second map (disjoint keys)
        pmap2 = PersistentDict()
        dict2 = {}
        for i in range(500):
            pmap2 = pmap2.assoc(f'b{i}', i)
            dict2[f'b{i}'] = i

        # Merge using update()
        pmap_merged = pmap1.update(dict(pmap2.items()))
        dict_merged = {**dict1, **dict2}

        self._assert_maps_equal(pmap_merged, dict_merged, "After merge (disjoint)")

    def test_merge_overlapping(self):
        """Test merging maps with overlapping keys."""
        # Create first map
        pmap1 = PersistentDict()
        dict1 = {}
        for i in range(1000):
            pmap1 = pmap1.assoc(f'key{i}', f'old{i}')
            dict1[f'key{i}'] = f'old{i}'

        # Create second map (overlapping keys)
        pmap2 = PersistentDict()
        dict2 = {}
        for i in range(500, 1500):
            pmap2 = pmap2.assoc(f'key{i}', f'new{i}')
            dict2[f'key{i}'] = f'new{i}'

        # Merge using | operator
        pmap_merged = pmap1 | dict(pmap2.items())
        dict_merged = {**dict1, **dict2}

        self._assert_maps_equal(pmap_merged, dict_merged, "After merge (overlapping)")

    def test_merge_complete_overlap(self):
        """Test merging maps with complete key overlap (all updates)."""
        # Create first map
        pmap1 = PersistentDict()
        dict1 = {}
        for i in range(500):
            pmap1 = pmap1.assoc(f'key{i}', i)
            dict1[f'key{i}'] = i

        # Create second map (same keys, different values)
        pmap2 = PersistentDict()
        dict2 = {}
        for i in range(500):
            pmap2 = pmap2.assoc(f'key{i}', i * 100)
            dict2[f'key{i}'] = i * 100

        # Merge
        pmap_merged = pmap1.merge(dict(pmap2.items()))
        dict_merged = {**dict1, **dict2}

        self._assert_maps_equal(pmap_merged, dict_merged, "After merge (complete overlap)")

    def test_merge_empty(self):
        """Test merging with empty maps."""
        # Non-empty with empty
        pmap1 = PersistentDict.from_dict({f'key{i}': i for i in range(100)})
        dict1 = {f'key{i}': i for i in range(100)}

        pmap2 = PersistentDict()
        dict2 = {}

        pmap_merged = pmap1 | dict(pmap2.items())
        dict_merged = {**dict1, **dict2}

        self._assert_maps_equal(pmap_merged, dict_merged, "After merge with empty")

        # Empty with non-empty
        pmap_merged2 = pmap2 | dict(pmap1.items())
        dict_merged2 = {**dict2, **dict1}

        self._assert_maps_equal(pmap_merged2, dict_merged2, "After merge empty with non-empty")

    def test_merge_large(self):
        """Test merging large maps (1M total elements)."""
        # Create first map (500K elements)
        pmap1 = PersistentDict.from_dict({i: i * 2 for i in range(500000)})
        dict1 = {i: i * 2 for i in range(500000)}

        # Create second map (500K elements, 250K overlap)
        pmap2 = PersistentDict.from_dict({i: i * 3 for i in range(250000, 750000)})
        dict2 = {i: i * 3 for i in range(250000, 750000)}

        # Merge
        pmap_merged = pmap1 | dict(pmap2.items())
        dict_merged = {**dict1, **dict2}

        self._assert_maps_equal(pmap_merged, dict_merged, "After large merge")


class TestIntegritySequences:
    """Test complex sequences of operations."""

    def _assert_maps_equal(self, pmap, pydict, msg=""):
        """Assert PersistentDict and dict have identical contents."""
        pmap_dict = dict(pmap.items())
        assert pmap_dict == pydict, f"{msg}\nPersistentDict: {pmap_dict}\nPython dict: {pydict}"
        assert len(pmap) == len(pydict), f"{msg}\nLength mismatch"

    def test_random_operations(self):
        """Test random sequence of operations."""
        random.seed(42)  # Reproducible
        pmap = PersistentDict()
        pydict = {}

        # Perform 1000 random operations
        for i in range(1000):
            op = random.choice(['assoc', 'dissoc', 'update'])

            if op == 'assoc':
                key = f'key{random.randint(0, 99)}'
                value = random.randint(0, 1000)
                pmap = pmap.assoc(key, value)
                pydict[key] = value

            elif op == 'dissoc':
                if pydict:
                    key = random.choice(list(pydict.keys()))
                    pmap = pmap.dissoc(key)
                    del pydict[key]

            elif op == 'update':
                # Add a few keys at once
                updates = {f'batch{random.randint(0, 50)}': random.randint(0, 100)
                          for _ in range(random.randint(1, 10))}
                pmap = pmap.update(updates)
                pydict.update(updates)

        self._assert_maps_equal(pmap, pydict, "After 1000 random operations")

    def test_build_modify_rebuild(self):
        """Test building, modifying, and rebuilding a map."""
        pmap = PersistentDict()
        pydict = {}

        # Phase 1: Build
        for i in range(500):
            pmap = pmap.assoc(f'key{i}', i)
            pydict[f'key{i}'] = i

        self._assert_maps_equal(pmap, pydict, "After initial build")

        # Phase 2: Modify (update some, remove some)
        for i in range(0, 500, 2):
            pmap = pmap.assoc(f'key{i}', i * 100)
            pydict[f'key{i}'] = i * 100

        for i in range(0, 500, 5):
            pmap = pmap.dissoc(f'key{i}')
            del pydict[f'key{i}']

        self._assert_maps_equal(pmap, pydict, "After modifications")

        # Phase 3: Rebuild with new keys
        for i in range(500, 1000):
            pmap = pmap.assoc(f'key{i}', i)
            pydict[f'key{i}'] = i

        self._assert_maps_equal(pmap, pydict, "After rebuild")

    def test_multiple_versions(self):
        """Test creating multiple versions and verifying each."""
        # Base map
        base_pmap = PersistentDict()
        base_dict = {}
        for i in range(100):
            base_pmap = base_pmap.assoc(f'base{i}', i)
            base_dict[f'base{i}'] = i

        # Create 10 variants
        variants_pmap = []
        variants_dict = []

        for v in range(10):
            variant_pmap = base_pmap
            variant_dict = base_dict.copy()

            # Each variant adds unique keys
            for i in range(20):
                variant_pmap = variant_pmap.assoc(f'var{v}_{i}', i)
                variant_dict[f'var{v}_{i}'] = i

            variants_pmap.append(variant_pmap)
            variants_dict.append(variant_dict)

        # Verify each variant
        for v in range(10):
            self._assert_maps_equal(variants_pmap[v], variants_dict[v],
                                   f"Variant {v}")

        # Verify base unchanged
        self._assert_maps_equal(base_pmap, base_dict, "Base after creating variants")


class TestIntegrityEdgeCases:
    """Test edge cases and special values."""

    def _assert_maps_equal(self, pmap, pydict, msg=""):
        """Assert PersistentDict and dict have identical contents."""
        pmap_dict = dict(pmap.items())
        assert pmap_dict == pydict, f"{msg}\nPersistentDict: {pmap_dict}\nPython dict: {pydict}"
        assert len(pmap) == len(pydict), f"{msg}\nLength mismatch"

    def test_none_values(self):
        """Test None as values."""
        pmap = PersistentDict()
        pydict = {}

        for i in range(50):
            pmap = pmap.assoc(f'key{i}', None)
            pydict[f'key{i}'] = None

        self._assert_maps_equal(pmap, pydict, "With None values")

    def test_mixed_types(self):
        """Test mixed key and value types."""
        pmap = PersistentDict()
        pydict = {}

        # String keys
        pmap = pmap.assoc('str_key', 'str_value')
        pydict['str_key'] = 'str_value'

        # Int keys
        pmap = pmap.assoc(42, 'int_key')
        pydict[42] = 'int_key'

        # Float keys
        pmap = pmap.assoc(3.14, 'float_key')
        pydict[3.14] = 'float_key'

        # Tuple keys
        pmap = pmap.assoc((1, 2, 3), 'tuple_key')
        pydict[(1, 2, 3)] = 'tuple_key'

        # None as value
        pmap = pmap.assoc('none_value', None)
        pydict['none_value'] = None

        # List as value
        pmap = pmap.assoc('list_value', [1, 2, 3])
        pydict['list_value'] = [1, 2, 3]

        self._assert_maps_equal(pmap, pydict, "With mixed types")

    def test_large_values(self):
        """Test with large string values."""
        pmap = PersistentDict()
        pydict = {}

        # Add keys with large values
        for i in range(100):
            large_value = 'x' * 10000  # 10KB string
            pmap = pmap.assoc(f'key{i}', large_value)
            pydict[f'key{i}'] = large_value

        self._assert_maps_equal(pmap, pydict, "With large values")

    def test_empty_strings(self):
        """Test empty string keys and values."""
        pmap = PersistentDict()
        pydict = {}

        # Empty string as key
        pmap = pmap.assoc('', 'empty_key')
        pydict[''] = 'empty_key'

        # Empty string as value
        pmap = pmap.assoc('empty_value', '')
        pydict['empty_value'] = ''

        self._assert_maps_equal(pmap, pydict, "With empty strings")

    def test_unicode_keys(self):
        """Test Unicode keys and values."""
        pmap = PersistentDict()
        pydict = {}

        unicode_keys = ['日本語', 'русский', 'العربية', '🔥emoji🎉', 'Ñoño']
        for i, key in enumerate(unicode_keys):
            pmap = pmap.assoc(key, f'value_{i}')
            pydict[key] = f'value_{i}'

        self._assert_maps_equal(pmap, pydict, "With Unicode keys")


class TestIntegrityImmutability:
    """Test that operations preserve immutability while maintaining correctness."""

    def _assert_maps_equal(self, pmap, pydict, msg=""):
        """Assert PersistentDict and dict have identical contents."""
        pmap_dict = dict(pmap.items())
        assert pmap_dict == pydict, f"{msg}\nPersistentDict: {pmap_dict}\nPython dict: {pydict}"

    def test_original_unchanged_after_assoc(self):
        """Test that original map unchanged after assoc."""
        # Build original maps
        pmap1 = PersistentDict()
        dict1 = {}
        for i in range(100):
            pmap1 = pmap1.assoc(f'key{i}', i)
            dict1[f'key{i}'] = i

        # Make copies for comparison
        dict1_copy = dict1.copy()

        # Create modified version
        pmap2 = pmap1.assoc('new_key', 'new_value')
        dict2 = dict1.copy()
        dict2['new_key'] = 'new_value'

        # Verify modified version correct
        self._assert_maps_equal(pmap2, dict2, "Modified version")

        # Verify original unchanged
        self._assert_maps_equal(pmap1, dict1_copy, "Original after modification")

    def test_original_unchanged_after_dissoc(self):
        """Test that original map unchanged after dissoc."""
        # Build original maps
        pmap1 = PersistentDict()
        dict1 = {}
        for i in range(100):
            pmap1 = pmap1.assoc(f'key{i}', i)
            dict1[f'key{i}'] = i

        dict1_copy = dict1.copy()

        # Create modified version
        pmap2 = pmap1.dissoc('key50')
        dict2 = dict1.copy()
        del dict2['key50']

        # Verify modified version correct
        self._assert_maps_equal(pmap2, dict2, "Modified version")

        # Verify original unchanged
        self._assert_maps_equal(pmap1, dict1_copy, "Original after dissoc")

    def test_original_unchanged_after_merge(self):
        """Test that original maps unchanged after merge."""
        # Build first map
        pmap1 = PersistentDict()
        dict1 = {}
        for i in range(100):
            pmap1 = pmap1.assoc(f'a{i}', i)
            dict1[f'a{i}'] = i

        dict1_copy = dict1.copy()

        # Build second map
        pmap2 = PersistentDict()
        dict2 = {}
        for i in range(100):
            pmap2 = pmap2.assoc(f'b{i}', i)
            dict2[f'b{i}'] = i

        dict2_copy = dict2.copy()

        # Merge
        pmap_merged = pmap1 | dict(pmap2.items())
        dict_merged = {**dict1, **dict2}

        # Verify merged correct
        self._assert_maps_equal(pmap_merged, dict_merged, "Merged version")

        # Verify originals unchanged
        self._assert_maps_equal(pmap1, dict1_copy, "First map after merge")
        self._assert_maps_equal(pmap2, dict2_copy, "Second map after merge")


if __name__ == '__main__':
    pytest.main([__file__, '-v'])
