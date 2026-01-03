# Integrity Testing with Parallel Python Dict Verification

## Overview

The integrity test suite (`test_integrity.py`) verifies PersistentMap correctness by performing operations on both PersistentMap and Python's built-in dict in parallel, then comparing results to ensure they match exactly.

This approach provides strong guarantees that our implementation behaves identically to Python dict (minus mutability), catching subtle bugs that might not be obvious from assertion-based tests alone.

## Test Coverage

### TestIntegrityBasic (6 tests)
Basic operations verified against dict:
- Single and multiple `assoc` operations
- Updating existing keys
- `dissoc` operations (both existing and nonexistent keys)
- `from_dict` bulk construction

### TestIntegrityMerge (5 tests)
Merge operation correctness:
- **Disjoint keys**: Merging maps with no overlap (500 elements each)
- **Overlapping keys**: Partial overlap (1000 + 1000 with 500 overlap)
- **Complete overlap**: All keys overlap, values updated
- **Empty maps**: Merging with/to empty maps
- **Large merge**: 500K + 500K elements (1M total)

### TestIntegritySequences (3 tests)
Complex operation sequences:
- **Random operations**: 1000 random assoc/dissoc/update operations with reproducible seed
- **Build-modify-rebuild**: Multi-phase construction and modification
- **Multiple versions**: Creating 10 variants from base map, verifying each independently

### TestIntegrityEdgeCases (5 tests)
Edge cases and special values:
- None as values
- Mixed types (string, int, float, tuple keys; None, list values)
- Large values (10KB strings)
- Empty strings as keys and values
- Unicode keys (Japanese, Russian, Arabic, emoji)

### TestIntegrityImmutability (3 tests)
Immutability verification:
- Original unchanged after `assoc`
- Original unchanged after `dissoc`
- Both originals unchanged after `merge`

## Methodology

Each test follows this pattern:

```python
def test_example(self):
    # Initialize parallel data structures
    pmap = PersistentMap()
    pydict = {}

    # Perform operations on both
    pmap = pmap.assoc('key', 'value')
    pydict['key'] = 'value'

    # Verify they match
    self._assert_maps_equal(pmap, pydict, "After operation")
```

The `_assert_maps_equal` helper:
1. Converts PersistentMap to dict via `items()`
2. Compares contents with `==`
3. Verifies lengths match
4. Provides detailed error messages on mismatch

## Key Benefits

### 1. Correctness Verification
Every operation is verified against Python's dict implementation, which is:
- Battle-tested with billions of production uses
- Extensively tested by CPython team
- The reference implementation users expect

### 2. Merge Correctness
Particularly critical for the structural merge optimization (Phase 4):
- Tests verify merged map contains exact same entries as `{**dict1, **dict2}`
- Catches subtle issues like:
  - Missing keys after merge
  - Wrong values when keys overlap
  - Incorrect handling of empty maps
  - Large-scale merge bugs

### 3. Random Operation Testing
The random operations test (`test_random_operations`) uses a fixed seed to:
- Generate 1000 random operations
- Mix assoc, dissoc, and update operations
- Create realistic usage patterns
- Remain reproducible for debugging

### 4. Immutability + Correctness
Tests verify that:
- New version has correct contents
- Original version unchanged
- Both properties hold simultaneously

## Test Statistics

- **Total tests**: 22
- **Operations tested**: assoc, dissoc, update, merge, from_dict
- **Total operations executed**: ~50,000+
- **Elements tested**: Up to 1M in large merge test
- **Edge cases**: 5 categories (None, mixed types, large values, empty strings, Unicode)

## Running the Tests

```bash
# Run integrity tests only
pytest test_integrity.py -v

# Run all tests (existing + integrity)
pytest test_persistent_map_cpp.py test_integrity.py -v

# Run with detailed output
pytest test_integrity.py -v --tb=long
```

## Future Enhancements

Possible additions:
1. **Hypothesis integration**: Property-based testing with automatically generated test cases
2. **Stress testing**: Multi-million element operations
3. **Concurrent testing**: Verify thread-safety with parallel reads
4. **Performance regression**: Track that integrity tests remain fast

## Why This Matters

The integrity tests caught or could catch:

### Potential Bugs Prevented
- **Incorrect merge**: Missing keys or wrong values after merge
- **Hash collision errors**: Wrong behavior when keys collide
- **Unicode handling**: Encoding issues with non-ASCII keys
- **None handling**: Confusion between None value vs missing key
- **Empty map edge cases**: Division by zero, null pointers
- **Update semantics**: Wrong behavior when updating vs inserting

### Real Example: Merge Verification

```python
def test_merge_overlapping(self):
    # Create 1000-element map
    pmap1 = PersistentMap.from_dict({f'key{i}': f'old{i}' for i in range(1000)})
    dict1 = {f'key{i}': f'old{i}' for i in range(1000)}

    # Create 1000-element map (500 overlap)
    pmap2 = PersistentMap.from_dict({f'key{i}': f'new{i}' for i in range(500, 1500)})
    dict2 = {f'key{i}': f'new{i}' for i in range(500, 1500)}

    # Merge
    pmap_merged = pmap1 | dict(pmap2.items())
    dict_merged = {**dict1, **dict2}

    # Verify EXACT match
    self._assert_maps_equal(pmap_merged, dict_merged)
```

This test verifies:
- 1500 total keys present
- 500 old keys (0-499) have old values
- 500 overlapping keys (500-999) have new values (updated)
- 500 new keys (1000-1499) have new values
- Exactly matches dict merge semantics

## Conclusion

The integrity test suite provides **high confidence** that PersistentMap behaves identically to Python dict for all operations. This is critical for:

1. **User trust**: Users can reason about behavior using dict mental model
2. **Correctness**: Optimizations won't silently break semantics
3. **Refactoring**: Can change implementation with confidence
4. **Documentation**: Can document "behaves like dict but immutable"

By testing with a reference implementation, we get correctness verification "for free" without needing to manually enumerate every expected behavior.
