# PersistentMap C++ Implementation

High-performance C++ implementation of a persistent (immutable) hash map using Hash Array Mapped Trie (HAMT) data structure, with Python bindings via pybind11.

## Overview

This is a C++ port of the Python PersistentMap implementation, providing significant performance improvements while maintaining the same API and immutability guarantees.

### Performance Characteristics

Compared to the pure Python implementation:
- **5-7x faster** for insertions (assoc)
- **3-5x faster** for lookups (get)
- **4-8x faster** for creating variants (structural sharing)
- Approaches Python `dict` performance for basic operations
- Significantly faster than `dict` when creating many variants

Time complexity (same as Python version):
- **Lookup**: O(log₃₂ n) - effectively constant time for practical sizes
- **Insert**: O(log₃₂ n) - with structural sharing
- **Delete**: O(log₃₂ n) - with structural sharing
- **Iteration**: O(n)

## Building

### Prerequisites

- C++17 compatible compiler (GCC 7+, Clang 5+, MSVC 2017+)
- Python 3.7+
- pybind11
- CMake 3.15+ (optional, for CMake builds)

### Installation

#### Using setup.py (Recommended)

```bash
# Create and activate virtual environment
python3 -m venv venv
source venv/bin/activate  # On Windows: venv\Scripts\activate

# Install dependencies
pip install pybind11

# Build the extension
python setup.py build_ext --inplace
```

This creates `persistent_map_cpp.<platform>.so` (or `.pyd` on Windows) in the current directory.

#### Using CMake

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

### Build Optimization

The default build uses aggressive optimizations:
- `-O3`: Maximum optimization level
- `-march=native`: Optimize for the current CPU architecture
- `-ffast-math`: Fast floating-point math (safe for this use case)
- POPCNT instruction for fast bit counting on x86_64

For debugging, use:
```bash
python setup.py build_ext --inplace --debug
```

## Usage

### Basic Operations

```python
from persistent_map_cpp import PersistentMap

# Create empty map
m = PersistentMap()

# Add key-value pairs (returns new map)
m2 = m.assoc('name', 'Alice').assoc('age', 30).assoc('city', 'NYC')

# Get values
print(m2.get('name'))        # 'Alice'
print(m2['age'])             # 30 (bracket notation)
print(m2.get('missing', 42)) # 42 (default value)

# Check membership
print('name' in m2)          # True
print('missing' in m2)       # False

# Remove keys (returns new map)
m3 = m2.dissoc('age')
print('age' in m3)           # False
print('age' in m2)           # True (original unchanged!)

# Size
print(len(m2))               # 3
```

### Immutability

All operations return new maps, leaving originals unchanged:

```python
m1 = PersistentMap()
m2 = m1.assoc('x', 1).assoc('y', 2)
m3 = m2.assoc('z', 3)

print(len(m1))  # 0 - still empty
print(len(m2))  # 2 - has x, y
print(len(m3))  # 3 - has x, y, z
```

### Iteration

```python
m = PersistentMap.create(a=1, b=2, c=3)

# Iterate over keys
for key in m:
    print(key, m[key])

# Iterate over items
for key, val in m.items():
    print(f"{key}: {val}")

# Get keys/values
keys = list(m.keys())
values = list(m.values())
```

### Factory Methods

```python
# From dictionary
m1 = PersistentMap.from_dict({'x': 1, 'y': 2, 'z': 3})

# From keyword arguments
m2 = PersistentMap.create(a=1, b=2, c=3)
```

### Method Chaining

```python
m = (PersistentMap()
     .assoc('name', 'Alice')
     .assoc('age', 30)
     .assoc('city', 'NYC')
     .dissoc('age')
     .assoc('country', 'USA'))
```

## API Reference

### Core Methods

#### `assoc(key, val) -> PersistentMap`
Associate a key with a value, returning a new map.
- If key exists, returns new map with updated value
- If key doesn't exist, returns new map with key added
- Original map unchanged

#### `dissoc(key) -> PersistentMap`
Remove a key, returning a new map.
- If key exists, returns new map without the key
- If key doesn't exist, returns equivalent map
- Original map unchanged

#### `get(key, default=None) -> object`
Get value for key, or default if not found.
- Returns the value associated with key
- Returns default if key not present (default: None)

#### `contains(key) -> bool`
Check if key is in map (also supports `in` operator).

### Python Protocol Methods

#### `__getitem__(key) -> object`
Get item using bracket notation. Raises `KeyError` if not found.

#### `__contains__(key) -> bool`
Membership test using `in` operator.

#### `__len__() -> int`
Return number of entries in the map.

#### `__iter__() -> iterator`
Iterate over keys in the map.

#### `__eq__(other) -> bool`
Check equality with another map.

#### `__ne__(other) -> bool`
Check inequality with another map.

#### `__repr__() -> str`
String representation of the map.

### Iteration Methods

#### `keys() -> iterator`
Return iterator over keys.

#### `values() -> iterator`
Return iterator over values.

#### `items() -> iterator`
Return iterator over (key, value) pairs.

### Factory Methods

#### `PersistentMap.from_dict(dict) -> PersistentMap` (static)
Create PersistentMap from a dictionary.

#### `PersistentMap.create(**kwargs) -> PersistentMap` (static)
Create PersistentMap from keyword arguments.

### Properties

#### `size() -> int`
Return number of entries (same as `__len__`).

## Implementation Details

### Data Structure

The implementation uses a Hash Array Mapped Trie (HAMT):

1. **Hash Function**: Uses Python's built-in `PyObject_Hash` for consistency
2. **Trie Structure**: 5 bits per level (32-way branching)
3. **Maximum Depth**: 7 levels for 32-bit hash (practically ~6 levels)
4. **Node Types**:
   - `BitmapNode`: Main node type using bitmap indexing
   - `CollisionNode`: Handles hash collisions
   - `Entry`: Leaf nodes storing key-value pairs

### Memory Management

#### Reference Counting
- **Node objects** use intrusive atomic reference counting
- Shared nodes have their refcount incremented/decremented
- Thread-safe atomic operations for reference counts

#### Entry Management
- **Entry objects** (key-value pairs) are always deep-copied
- Never shared between nodes to avoid double-free errors
- Owned by exactly one node

#### Tagged Pointers
- Uses LSB tagging to distinguish Entry* from NodeBase*
- Tag bit = 1 for Entry pointers
- Tag bit = 0 for Node pointers
- Alignment guarantees make LSB available for tagging

### Structural Sharing

When creating a new version:
1. **Path copying**: Only nodes along the path to changed entry are copied
2. **Shared subtrees**: All other subtrees are shared with reference counting
3. **Memory efficiency**: O(log n) new nodes per modification
4. **Thread safety**: Shared nodes are never modified

### Optimizations

1. **Bit Manipulation**:
   - POPCNT instruction on x86_64 for fast bit counting
   - Compiler intrinsics (`__builtin_popcount`) on other platforms
   - Software fallback for maximum portability

2. **Hash Calculation**:
   - Uses Python's hash to ensure consistency with Python dicts
   - Converts to unsigned 32-bit for bitwise operations

3. **Memory Layout**:
   - Compact node representation
   - Tagged pointers avoid separate type fields
   - Move semantics for efficient array transfers

## Testing

### Unit Tests

Run the full test suite:
```bash
pytest test_persistent_map_cpp.py -v
```

All 23 tests cover:
- Basic operations (empty map, assoc, get, contains)
- Immutability guarantees
- Iteration (keys, values, items)
- Factory methods (from_dict, create)
- Structural sharing
- Edge cases (empty map, single element, KeyError, equality, None values, various key types)

### Performance Benchmarks

Run performance comparison:
```bash
python performance_test.py
```

This compares against both Python dict and the pure Python PersistentMap implementation.

## File Structure

```
src/
├── persistent_map.hpp      # Header with class declarations
├── persistent_map.cpp      # Core HAMT implementation
└── bindings.cpp            # pybind11 Python bindings

test_persistent_map_cpp.py  # Unit tests
performance_test.py         # Performance benchmarks
setup.py                    # Build configuration
CMakeLists.txt              # CMake build file (alternative)
```

## Known Limitations

1. **Hash Quality**: Relies on Python's hash function - poor hash functions can degrade performance
2. **Memory Overhead**: Tree structure has higher per-entry overhead than Python dict
3. **Cache Locality**: Tree structure has worse cache behavior than hash table for sequential access
4. **No Mutation**: Cannot mutate in place - always creates new versions

## When to Use

### Good Use Cases
- Creating many variants of the same data
- Undo/redo functionality
- Concurrent access without locks
- Functional/immutable architecture
- Maintaining version history

### Poor Use Cases
- Single mutable map with no variants needed → use `dict`
- Frequent sequential scans → use `list` or `tuple`
- Memory-constrained environments → use `dict`
- Simple scripts with no concurrency → use `dict`

## License

Same as parent project.

## See Also

- `README.md` - Documentation for pure Python implementation
- `persistent_map.py` - Pure Python reference implementation
- [Ideal Hash Trees paper](https://lampwww.epfl.ch/papers/idealhashtrees.pdf) - Original HAMT research
