# PyPersistent

A high-performance persistent (immutable) hash map implementation for Python, written in C++.

## Features

- **Immutable**: All operations return new maps, leaving originals unchanged
- **Structural Sharing**: New versions share most structure with old versions (O(log n) copies instead of O(n))
- **Fast**: 38% faster than pure Python implementation for insertions, ~10x slower than mutable dict
- **Thread-Safe**: Immutability makes concurrent access safe without locks
- **Python 3.13+ Free-Threading Ready**: Lock-free design with atomic reference counting for true parallelism
- **Memory Efficient**: Structural sharing minimizes memory overhead
- **Dual Interface**: Both functional (Clojure-style) and Pythonic APIs

## Performance

### Quick Summary

pypersistent provides **6-8% faster** bulk operations compared to baseline and is **3-150x faster** than pure Python pyrsistent for most operations. The real value is structural sharing: creating variants is **3000x faster** than copying dicts.

### Benchmark Results (1M elements)

#### vs Python dict

| Operation | pypersistent | dict | Notes |
|-----------|--------------|------|-------|
| **Construction** | 2.74s | 299ms | 9x slower (immutability cost) |
| **Lookup** | 776ms | 427ms | 1.8x slower |
| **Update (single)** | 147µs | ~80ms | Comparable for single ops |
| **Structural Sharing** | **0.48ms** | 1.57s | **3000x FASTER** |

**Key insight**: For creating multiple versions, pypersistent is orders of magnitude faster.

#### vs pyrsistent (pure Python)

| Operation | pypersistent | pyrsistent | Speedup |
|-----------|--------------|------------|---------|
| **Merge** | 10.5ms | 1.57s | **150x faster** |
| **Construction** | 196ms | 788ms | **4x faster** |
| **Lookup** | 776ms | 806ms | ~1x (neutral) |
| **Iteration** | 169ms* | 169ms | **1x** (with items_list()) |

\* Using fast `items_list()` method; iterator is 2.9x slower

### Performance by Map Size

| Size | Construction | Lookup (1K ops) | Sharing (100 variants) |
|------|--------------|-----------------|------------------------|
| 100 | 73µs | 19µs | 77µs |
| 1K | 765µs | 205µs | 95µs |
| 10K | 9.7ms | 228µs | 108µs |
| 100K | 110ms | 255µs | 133µs |
| 1M | 2.74s | 776ms | 158µs |

### Fast Iteration Methods

For complete iteration over small-medium maps, use materialized list methods:

```python
m = PersistentMap.from_dict({...})

# Fast methods (1.7-3x faster for maps < 100K)
items = m.items_list()   # Returns list of (key, value) tuples
keys = m.keys_list()     # Returns list of keys
values = m.values_list() # Returns list of values

# Lazy iterators (better for very large maps or early exit)
for k, v in m.items():   # Generator, O(log n) memory
    ...
```

**Performance**:
- Maps ≤ 10K: **3x faster** with `items_list()`
- Maps ≤ 100K: **1.7x faster** with `items_list()`
- Maps > 100K: Use iterator (lazy, memory-efficient)

### When to Use pypersistent

**Use pypersistent when**:
- Creating multiple versions of data (undo/redo, time-travel)
- Concurrent access across threads (lock-free reads)
- Functional programming patterns
- Merging large maps frequently

**Use dict when**:
- Single mutable map is sufficient
- Maximum raw construction speed is critical
- Memory per entry is constrained

### Technical Details

**Implementation**: C++ HAMT (Hash Array Mapped Trie) with:
- Bottom-up bulk construction for from_dict()
- Arena allocator for fast node allocation
- Structural tree merging for merge()
- COW semantics for collision nodes
- Fast iteration with pre-allocated lists

**Time Complexity**: O(log₃₂ n) ≈ 6 steps for 1M elements
**Space Complexity**: O(n) with structural sharing across versions

For detailed performance analysis and benchmarking methodology, see `docs/`.

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
from pypersistent import PersistentMap

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
from pypersistent import PersistentMap

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

## Python 3.13+ Free-Threading Support

PyPersistent is **fully compatible** with Python 3.13's experimental free-threading mode (nogil), making it ideal for parallel workloads:

### Why It Works

1. **Lock-Free Reads**: Immutable data structure allows concurrent reads without synchronization
2. **Atomic Reference Counting**: Internal C++ reference counting uses `std::atomic` operations
3. **Thread-Safe Entry Storage**: Uses `std::shared_ptr` with built-in thread-safe reference counting
4. **Independent Updates**: Each thread can create new versions without blocking others

### Usage with Free-Threading

```python
# Python 3.13+ with --disable-gil or PYTHON_GIL=0
import threading
from pypersistent import PersistentMap

# Shared base map
base_config = PersistentMap.create(
    api_url='https://api.example.com',
    timeout=30,
    retries=3
)

def process_request(thread_id, data):
    # Each thread creates its own version - no locks needed!
    my_config = base_config.set('thread_id', thread_id)
    my_config = my_config.set('request_data', data)

    # Concurrent reads are completely lock-free
    url = my_config['api_url']
    timeout = my_config['timeout']

    # Do work...
    return my_config

# Spawn threads - true parallelism without GIL!
threads = []
for i in range(100):
    t = threading.Thread(target=process_request, args=(i, f'data_{i}'))
    threads.append(t)
    t.start()

for t in threads:
    t.join()
```

### Performance Benefits

Without the GIL:
- **Parallel reads**: Multiple threads read simultaneously without contention
- **Parallel updates**: Each thread creates new versions independently
- **No lock overhead**: Zero synchronization cost for immutable operations
- **Structural sharing shines**: Creating 100 thread-local variants is 3000x faster than copying dicts

### Enable Free-Threading

```bash
# Python 3.13+
python3.13 --disable-gil your_script.py

# Or set environment variable
export PYTHON_GIL=0
python3.13 your_script.py
```

**Note**: Free-threading is experimental in Python 3.13. Some packages may not be compatible yet.

## Development

```bash
# Install development dependencies
pip install pytest pybind11

# Build extension
python setup.py build_ext --inplace

# Run tests
pytest test_pypersistent.py -v

# Run performance benchmarks
python performance_test.py
```

## License

MIT License - see LICENSE file for details

## Credits

Inspired by Clojure's persistent data structures and the HAMT paper by Bagwell (2001).

Implementation by Clemens Marschner.
