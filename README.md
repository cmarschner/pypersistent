# PyPersistent

A high-performance collection of persistent (immutable) data structures for Python, written in C++.

## Features

- **Immutable**: All operations return new maps, leaving originals unchanged
- **Structural Sharing**: New versions share most structure with old versions (O(log n) copies instead of O(n))
- **Fast**: 38% faster than pure Python implementation for insertions, ~10x slower than mutable dict
- **Thread-Safe**: Immutability makes concurrent access safe without locks
- **Python 3.13+ Free-Threading Ready**: Lock-free design with atomic reference counting for true parallelism
- **Memory Efficient**: Structural sharing minimizes memory overhead
- **Dual Interface**: Both functional (Clojure-style) and Pythonic APIs

## Data Structures

PyPersistent provides five core persistent data structures:

### PersistentMap
**Unordered key-value map** based on Hash Array Mapped Trie (HAMT).

- **Use for**: General-purpose dictionary needs with immutability
- **Time complexity**: O(log₃₂ n) ≈ 6 steps for 1M elements
- **Features**: Fast lookups, structural sharing, bulk merge operations
- **Example**:
  ```python
  from pypersistent import PersistentMap

  m = PersistentMap.create(name='Alice', age=30)
  m2 = m.set('city', 'NYC')
  m3 = m | {'role': 'developer'}  # Merge
  ```

### PersistentTreeMap
**Sorted key-value map** based on Left-Leaning Red-Black Tree.

- **Use for**: Ordered data, range queries, min/max operations
- **Time complexity**: O(log₂ n) for all operations
- **Features**: Sorted iteration, range queries (subseq/rsubseq), first/last
- **Example**:
  ```python
  from pypersistent import PersistentTreeMap

  m = PersistentTreeMap.from_dict({3: 'c', 1: 'a', 2: 'b'})
  list(m.keys())  # [1, 2, 3] - always sorted

  # Range queries
  sub = m.subseq(start=1, end=2, start_inclusive=True, end_inclusive=False)
  list(sub.keys())  # [1]

  # Min/max
  m.first()  # (1, 'a')
  m.last()   # (3, 'c')
  ```

### PersistentVector
**Indexed sequence** based on bit-partitioned vector trie (RRB-Tree variant).

- **Use for**: Ordered sequences with efficient random access and append
- **Time complexity**: O(log₃₂ n) for get/set, O(1) for append
- **Features**: Fast indexed access, efficient append, slicing
- **Example**:
  ```python
  from pypersistent import PersistentVector

  v = PersistentVector.create(1, 2, 3)
  v2 = v.conj(4)  # Append
  v3 = v2.assoc(0, 10)  # Update index 0
  v[1]  # 2 - indexed access
  ```

### PersistentHashSet
**Unordered set** based on HAMT (same as PersistentMap).

- **Use for**: Unique collection of items, set operations
- **Time complexity**: O(log₃₂ n) for add/remove/contains
- **Features**: Fast membership testing, set operations (union, intersection, difference)
- **Example**:
  ```python
  from pypersistent import PersistentHashSet

  s = PersistentHashSet.create(1, 2, 3)
  s2 = s.conj(4)  # Add
  s3 = s.disj(2)  # Remove
  2 in s  # True
  ```

### PersistentArrayMap
**Small map optimization** using array of key-value pairs.

- **Use for**: Maps with < 8 entries (automatic optimization)
- **Time complexity**: O(n) linear scan, but faster than HAMT for tiny maps
- **Features**: Lower memory overhead, faster for very small maps
- **Note**: Typically used internally; PersistentMap automatically uses this for small maps

## Choosing the Right Data Structure

| Need | Use | Why |
|------|-----|-----|
| General key-value storage | **PersistentMap** | Fastest for unordered data |
| Sorted keys / range queries | **PersistentTreeMap** | Maintains sort order, supports ranges |
| Indexed sequence | **PersistentVector** | Fast random access and append |
| Unique items / set operations | **PersistentHashSet** | Membership testing, set algebra |
| Very small maps (< 8 items) | **PersistentArrayMap** | Lower overhead for tiny maps |

## Performance

### Quick Summary

pypersistent provides **6-8% faster** bulk operations compared to baseline and is **3-150x faster** than pure Python pyrsistent for most operations. The real value is structural sharing: creating variants is **3000x faster** than copying dicts.

### Benchmark Results

#### vs Python dict (1M elements)

| Operation | pypersistent | dict | Notes |
|-----------|--------------|------|-------|
| **Construction** | 2.74s | 299ms | 9x slower (immutability cost) |
| **Lookup (1K ops)** | 776ms | 427ms | 1.8x slower |
| **Update (single)** | 147µs | ~80ns | Comparable for single ops |
| **Structural Sharing (100 variants)** | **0.48ms** | 1.57s | **3000x FASTER** |

**Key insight**: For creating multiple versions, pypersistent is orders of magnitude faster.

#### vs pyrsistent (pure Python) - Apples-to-Apples

| Size | Operation | pypersistent | pyrsistent | Speedup |
|------|-----------|--------------|------------|---------|
| **1M** | **Merge (500K+500K)** | **11.5ms** | **1.60s** | **139x faster** |
| **1M** | **Construction** | **185ms** | **813ms** | **4.4x faster** |
| **1M** | **Lookup (1M ops)** | **726ms** | **796ms** | **1.1x faster** |
| **1M** | **Iteration** | 452ms | 167ms | 2.7x slower |
| **1K** | **Merge (500+500)** | **8.7µs** | **1.22ms** | **141x faster** |
| **1K** | **Construction** | **76µs** | **192µs** | **2.5x faster** |
| **100** | **Merge (50+50)** | **17.8µs** | **120µs** | **6.7x faster** |
| **100** | **Construction** | 30µs | 18.5µs | 1.6x slower |

**Iteration note**: Using `items_list()` instead of `items()` makes iteration 1.7-3x faster for maps < 100K

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

### PersistentMap - Hash Map

```python
from pypersistent import PersistentMap

# Create
m = PersistentMap.create(name='Alice', age=30)
m = PersistentMap.from_dict({'name': 'Alice', 'age': 30})

# Add/update (functional style)
m2 = m.assoc('city', 'NYC')

# Add/update (Pythonic style)
m2 = m.set('city', 'NYC')
m3 = m | {'role': 'developer'}  # Merge

# Get
name = m['name']  # Raises KeyError if missing
name = m.get('name', 'default')

# Remove
m4 = m.dissoc('age')  # Functional
m4 = m.delete('age')  # Pythonic

# Check membership
'name' in m  # True

# Iterate
for key, value in m.items():
    print(key, value)
```

### PersistentTreeMap - Sorted Map

```python
from pypersistent import PersistentTreeMap

# Create (automatically sorted by keys)
m = PersistentTreeMap.from_dict({3: 'c', 1: 'a', 2: 'b'})
list(m.keys())  # [1, 2, 3]

# Same API as PersistentMap
m2 = m.assoc(4, 'd')
m3 = m.dissoc(1)

# Sorted-specific operations
first_entry = m.first()  # (1, 'a')
last_entry = m.last()    # (3, 'c')

# Range queries
sub = m.subseq(start=1, end=3)  # Keys 1-2 (end exclusive)
list(sub.keys())  # [1, 2]

rsub = m.rsubseq(start=3, end=1)  # Reverse order
list(rsub.keys())  # [3, 2]
```

### PersistentVector - Indexed Sequence

```python
from pypersistent import PersistentVector

# Create
v = PersistentVector.create(1, 2, 3)
v = PersistentVector.from_list([1, 2, 3])

# Append (functional)
v2 = v.conj(4)  # [1, 2, 3, 4]

# Update by index
v3 = v.assoc(0, 10)  # [10, 2, 3]

# Access by index
first = v[0]  # 1
last = v[-1]  # 3

# Slice
sub = v[1:3]  # PersistentVector([2, 3])

# Iterate
for item in v:
    print(item)

# Length
len(v)  # 3
```

### PersistentHashSet - Set

```python
from pypersistent import PersistentHashSet

# Create
s = PersistentHashSet.create(1, 2, 3)
s = PersistentHashSet.from_set({1, 2, 3})

# Add
s2 = s.conj(4)  # {1, 2, 3, 4}

# Remove
s3 = s.disj(2)  # {1, 3}

# Membership
2 in s  # True

# Set operations
s1 = PersistentHashSet.create(1, 2, 3)
s2 = PersistentHashSet.create(3, 4, 5)

union = s1 | s2  # {1, 2, 3, 4, 5}
intersection = s1 & s2  # {3}
difference = s1 - s2  # {1, 2}

# Iterate
for item in s:
    print(item)
```

### API Summary

**Common operations (all data structures):**
- `create(*args)` - Create from elements
- `from_X(...)` - Create from Python collection
- Immutability - all operations return new instances
- Thread-safe reads - safe to share across threads

**PersistentMap / PersistentTreeMap:**
- `assoc(k, v)` / `set(k, v)` - Add/update
- `dissoc(k)` / `delete(k)` - Remove
- `get(k, default=None)` - Get value
- `m[k]` - Get (raises KeyError)
- `k in m` - Membership
- `keys()`, `values()`, `items()` - Iterators
- `m1 | m2` - Merge

**PersistentTreeMap only:**
- `first()` - Min entry
- `last()` - Max entry
- `subseq(start, end)` - Range query
- `rsubseq(start, end)` - Reverse range

**PersistentVector:**
- `conj(item)` - Append
- `assoc(idx, val)` - Update by index
- `v[idx]` - Get by index
- `v[start:end]` - Slice
- `len(v)` - Length

**PersistentHashSet:**
- `conj(item)` - Add
- `disj(item)` - Remove
- `item in s` - Membership
- `s1 | s2` - Union
- `s1 & s2` - Intersection
- `s1 - s2` - Difference

## Use Cases

**Use persistent data structures when:**
- Creating multiple versions of data (undo/redo, time-travel, version history)
- Sharing data across threads without locks or defensive copying
- Functional programming patterns (immutability, pure functions)
- Creating modified copies is frequent (structural sharing makes this fast)
- Python 3.13+ free-threading for true parallelism

**Use mutable collections when:**
- Single mutable instance is sufficient
- Maximum raw construction speed is critical
- Memory per entry is highly constrained

**Specific use cases:**
- **Config management**: Share base config, each thread creates customized version
- **Event sourcing**: Maintain history of all states efficiently
- **Reactive programming**: Pass immutable state between components
- **Concurrent caching**: Multiple threads read/update cache without locks
- **Functional data pipelines**: Transform data through pipeline stages

## How It Works

PyPersistent implements multiple classic persistent data structures:

### PersistentMap - Hash Array Mapped Trie (HAMT)
Based on the HAMT data structure used by Clojure, Scala, and Haskell:
- 32-way branching tree indexed by hash bits
- Path copying for immutability
- Structural sharing between versions
- `std::shared_ptr` for entry sharing (44x fewer INCREF/DECREF)
- Inline storage with `std::variant` for cache-friendly access

### PersistentTreeMap - Left-Leaning Red-Black Tree
Self-balancing binary search tree with:
- Path copying for immutability
- Sorted order maintenance (O(log₂ n))
- Atomic reference counting for node sharing
- Range query support via tree traversal

### PersistentVector - Bit-Partitioned Trie
32-way branching tree for indexed access:
- Path copying for updates
- Tail optimization for fast append
- O(log₃₂ n) random access (~6 steps for 1M elements)
- Efficient slicing via structural sharing

### PersistentHashSet - HAMT-based Set
Uses PersistentMap internally with:
- Keys as set elements
- Same O(log₃₂ n) complexity
- Set algebra operations

### PersistentArrayMap - Simple Array
Linear array for tiny maps (< 8 entries):
- Lower overhead than HAMT for small sizes
- O(n) operations but faster than tree for n < 8
- Automatically used by PersistentMap when beneficial

## Technical Details

**Complexity:**
- PersistentMap/HashSet: O(log₃₂ n) ≈ 6 steps for 1M elements
- PersistentTreeMap: O(log₂ n) for all operations
- PersistentVector: O(log₃₂ n) get/set, O(1) append
- PersistentArrayMap: O(n) but fast for n < 8

**Implementation:**
- C++ with pybind11 bindings
- Atomic reference counting (`std::atomic<int>`)
- Structural sharing for memory efficiency
- Thread-safe reads (fully immutable)

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

# Run all tests
pytest -v

# Run specific data structure tests
pytest test_persistent_map.py -v
pytest test_persistent_tree_map.py -v
pytest test_persistent_vector.py -v
pytest test_persistent_hash_set.py -v

# Run performance benchmarks
python performance_test.py
python performance_vector.py
```

## License

MIT License - see LICENSE file for details

## Credits

Inspired by Clojure's persistent data structures and the HAMT paper by Bagwell (2001).

Implementation by Clemens Marschner.
