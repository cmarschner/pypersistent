# PyPersistent

A high-performance persistent (immutable) hash map implementation for Python, written in C++.

## Features

- **Immutable**: All operations return new maps, leaving originals unchanged
- **Structural Sharing**: New versions share most structure with old versions (O(log n) copies instead of O(n))
- **Fast**: 38% faster than pure Python implementation for insertions, ~10x slower than mutable dict
- **Thread-Safe**: Immutability makes concurrent access safe without locks
- **Memory Efficient**: Structural sharing minimizes memory overhead
- **Dual Interface**: Both functional (Clojure-style) and Pythonic APIs

## Performance

Compared to Python's built-in `dict` (for 1,000,000 elements):

| Operation | PersistentMap | dict | Ratio |
|-----------|---------------|------|-------|
| Insertion | 2.90s | 299ms | 9.7x slower |
| Lookup | 1.02s | 427ms | 2.4x slower |
| Update | 402ms | 80ms | 5.0x slower |
| Structural Sharing | **0.48ms** | 1.57s | **3270x FASTER** ✨ |

When you need to create multiple versions of data (undo/redo, time-travel, etc.), PersistentMap is **orders of magnitude faster** than copying dicts.

## Installation

```bash
pip install pypersistent
```

Or build from source:

```bash
git clone https://github.com/cmarschner/pypersistent.git
cd pypersistent
python setup.py install
```

## Usage

### Functional Style (Clojure-inspired)

```python
from persistent_map_cpp import PersistentMap

# Create empty map
m = PersistentMap()

# Add entries (returns new map)
m2 = m.assoc('name', 'Alice').assoc('age', 30)

# Remove entries
m3 = m2.dissoc('age')

# Get values
name = m2.get('name')  # 'Alice'
age = m2.get('age', 0)  # 30

# Check membership
'name' in m2  # True

# Original unchanged
len(m)   # 0
len(m2)  # 2
```

### Pythonic Style

```python
from persistent_map_cpp import PersistentMap

# Create from dict or kwargs
m = PersistentMap.create(name='Alice', age=30)
m = PersistentMap.from_dict({'name': 'Alice', 'age': 30})

# Dict-like operations (but returns new map!)
m2 = m.set('city', 'NYC')
m3 = m.delete('age')
m4 = m.update({'height': 165, 'weight': 60})
m5 = m | {'extra': 'data'}  # Merge operator

# Access like dict
name = m['name']  # Raises KeyError if not found
name = m.get('name', 'default')  # With default

# Iterate
for key in m:
    print(key, m[key])

for key, value in m.items():
    print(key, value)
```

### Complete API

**Core operations (functional style):**
- `assoc(key, val)` - Add/update key
- `dissoc(key)` - Remove key
- `get(key, default=None)` - Get value
- `contains(key)` - Check membership

**Pythonic aliases:**
- `set(key, val)` - Alias for `assoc`
- `delete(key)` - Alias for `dissoc`
- `update(mapping)` - Bulk merge
- `merge(mapping)` - Alias for `update`
- `m1 | m2` - Merge operator
- `clear()` - Empty map
- `copy()` - Returns self (immutable)

**Standard dict protocol:**
- `m[key]` - Get item (raises KeyError)
- `key in m` - Membership test
- `len(m)` - Size
- `for k in m` - Iterate keys
- `keys()`, `values()`, `items()` - Iterators
- `==`, `!=` - Equality

## Use Cases

PersistentMap is ideal when you need:

- **Multiple versions of data**: Undo/redo, time-travel debugging, version history
- **Concurrent access**: Share data across threads without locks or defensive copying
- **Functional programming**: Natural fit for immutable data structures
- **Copy-on-write semantics**: When creating modified copies is common

Use regular `dict` when:
- You only need one mutable map
- Maximum raw performance is critical
- Memory per entry is a constraint

## How It Works

PyPersistent implements a **Hash Array Mapped Trie (HAMT)**, the data structure used by:
- Clojure's persistent maps
- Scala's immutable maps
- Haskell's `Data.HashMap`

Key innovations in this implementation:

1. **Entry sharing with `shared_ptr`**: Entries are shared between map versions using shared pointers, reducing INCREF/DECREF operations by 44x
2. **Inline storage with `std::variant`**: Entries stored by value in node arrays for cache-friendly access
3. **O(log n) memory iteration**: Tree-walking iterator uses O(log n) stack space instead of materializing all entries

## Technical Details

- **Time Complexity**: O(log₃₂ n) for all operations (~6 steps for 1M elements)
- **Space Complexity**: O(n) with structural sharing across versions
- **Implementation**: C++ with pybind11 bindings
- **Thread Safety**: Fully thread-safe for reading (immutable)

## Development

```bash
# Install development dependencies
pip install pytest pybind11

# Build extension
python setup.py build_ext --inplace

# Run tests
pytest test_persistent_map_cpp.py -v

# Run performance benchmarks
python performance_test.py
```

## License

MIT License - see LICENSE file for details

## Credits

Inspired by Clojure's persistent data structures and the HAMT paper by Bagwell (2001).

Implementation by Clemens Marschner.
