# PyPersistent

A high-performance collection of persistent (immutable) data structures for Python, written in C++.

## Features

- **Immutable**: All operations return new collections, leaving originals unchanged
- **Structural Sharing**: New versions share most structure (O(log n) changes instead of O(n) copies)
- **Fast**: 3-5x faster than pyrsistent, with merge operations up to 176x faster
- **Thread-Safe**: Immutability makes concurrent access safe without locks
- **Python 3.13+ Free-Threading Ready**: Lock-free design with atomic reference counting for true parallelism
- **Memory Efficient**: Structural sharing minimizes memory overhead
- **Dual Interface**: Both functional (Clojure-style) and Pythonic APIs

## Data Structures

PyPersistent provides five core persistent data structures:

### PersistentDict
**Unordered key-value map** based on Hash Array Mapped Trie (HAMT).

- **Use for**: General-purpose dictionary needs with immutability
- **Time complexity**: O(log₃₂ n) ≈ 6 steps for 1M elements
- **Features**: Fast lookups, structural sharing, bulk merge operations
- **Example**:
  ```python
  from pypersistent import PersistentDict

  m = PersistentDict.create(name='Alice', age=30)
  m2 = m.set('city', 'NYC')
  m3 = m | {'role': 'developer'}  # Merge
  ```

### PersistentSortedDict
**Sorted key-value map** based on Left-Leaning Red-Black Tree.

- **Use for**: Ordered data, range queries, min/max operations
- **Time complexity**: O(log₂ n) for all operations
- **Features**: Sorted iteration, range queries (subseq/rsubseq), first/last
- **Example**:
  ```python
  from pypersistent import PersistentSortedDict

  m = PersistentSortedDict.from_dict({3: 'c', 1: 'a', 2: 'b'})
  list(m.keys())  # [1, 2, 3] - always sorted

  # Range queries
  sub = m.subseq(start=1, end=2, start_inclusive=True, end_inclusive=False)
  list(sub.keys())  # [1]

  # Min/max
  m.first()  # (1, 'a')
  m.last()   # (3, 'c')
  ```

### PersistentList
**Indexed sequence** based on bit-partitioned vector trie (RRB-Tree variant).

- **Use for**: Ordered sequences with efficient random access and append
- **Time complexity**: O(log₃₂ n) for get/set, O(1) for append
- **Features**: Fast indexed access, efficient append, slicing
- **Example**:
  ```python
  from pypersistent import PersistentList

  v = PersistentList.create(1, 2, 3)
  v2 = v.conj(4)  # Append
  v3 = v2.assoc(0, 10)  # Update index 0
  v[1]  # 2 - indexed access
  ```

### PersistentSet
**Unordered set** based on HAMT (same as PersistentDict).

- **Use for**: Unique collection of items, set operations
- **Time complexity**: O(log₃₂ n) for add/remove/contains
- **Features**: Fast membership testing, set operations (union, intersection, difference)
- **Example**:
  ```python
  from pypersistent import PersistentSet

  s = PersistentSet.create(1, 2, 3)
  s2 = s.conj(4)  # Add
  s3 = s.disj(2)  # Remove
  2 in s  # True
  ```

### PersistentArrayMap
**Small map optimization** using array of key-value pairs.

- **Use for**: Maps with < 8 entries (automatic optimization)
- **Time complexity**: O(n) linear scan, but faster than HAMT for tiny maps
- **Features**: Lower memory overhead, faster for very small maps
- **Note**: Typically used internally; PersistentDict automatically uses this for small maps

## Choosing the Right Data Structure

| Need | Use | Why |
|------|-----|-----|
| General key-value storage | **PersistentDict** | Fastest for unordered data |
| Sorted keys / range queries | **PersistentSortedDict** | Maintains sort order, supports ranges |
| Indexed sequence | **PersistentList** | Fast random access and append |
| Unique items / set operations | **PersistentSet** | Membership testing, set algebra |
| Very small dicts (< 8 items) | **PersistentArrayMap** | Lower overhead for tiny dicts |

## Performance

### Why Use Persistent Data Structures?

The key advantage of persistent data structures is **structural sharing**: when you create a modified version, the new and old versions share most of their internal structure rather than duplicating everything.

**Example:**
```python
# Python dict - must copy everything
d1 = {i: f'val{i}' for i in range(10000)}
d2 = d1.copy()  # Copies all 10,000 entries
d2['new_key'] = 'new_value'

# PersistentDict - shares structure
from pypersistent import PersistentDict
m1 = PersistentDict.from_dict({i: f'val{i}' for i in range(10000)})
m2 = m1.set('new_key', 'new_value')  # Shares 99.99% of structure with m1
```

### When to Use pypersistent

**✅ Use pypersistent when:**
- **Creating multiple versions** - Undo/redo, snapshots, version history
- **Concurrent access** - Multiple threads safely reading shared data without locks
- **Functional programming** - Immutable data flow, pure functions
- **Building up collections incrementally** - Each addition shares structure with previous version

**❌ Use standard Python dict/list when:**
- You only need one mutable version
- Maximum raw construction speed is critical
- Extremely memory-constrained environments

### Performance vs Python Standard Library

#### PersistentDict vs dict

| Operation | pypersistent | Python dict | Speedup |
|-----------|--------------|-------------|---------|
| **Structural Sharing (100 variants of 10K dict)** | **74µs** | **1.79ms** | **24x faster** 🚀 |
| **Structural Sharing (100 variants of 1M dict)** | **158µs** | **157s** | **3000x faster** 🚀 |
| Single lookup | 776µs (1K ops) | 427µs | 1.8x slower |
| Construction (1M elements) | 2.74s | 299ms | 9x slower |

**Key insight**: Creating multiple versions is where pypersistent excels - up to **3000x faster** than copying dicts.

#### PersistentList vs list

| Operation | pypersistent | Python list | Speedup |
|-----------|--------------|-------------|---------|
| **Structural Sharing (100 variants of 10K list)** | **74µs** | **1.79ms** | **24x faster** 🚀 |
| Append 100 elements | 42µs | 2.3µs | 18x slower |
| Random access (100 ops) | 13µs | 2.7µs | 5x slower |

**Key insight**: If you need multiple versions (undo, history), PersistentList is dramatically faster.

### Performance vs pyrsistent

[pyrsistent](https://github.com/tobgu/pyrsistent) is a mature pure-Python library with similar persistent data structures. pypersistent offers better performance through its C++ implementation, but with a smaller feature set.

#### PersistentDict Performance Comparison

| Size | Operation | pypersistent (C++) | pyrsistent (Python) | Speedup |
|------|-----------|-------------------|---------------------|---------|
| **100K** | **Merge (50K+50K)** | **563µs** | **98.9ms** | **176x faster** 🚀 |
| **10K** | **Merge (5K+5K)** | **155µs** | **9.12ms** | **59x faster** 🚀 |
| **10K** | **Lookup (1K ops)** | **157µs** | **476µs** | **3.0x faster** |
| **10K** | **Update (100 ops)** | **64µs** | **287µs** | **4.5x faster** |
| **10K** | **Construction** | **811µs** | **1.95ms** | **2.4x faster** |
| **10K** | **Iteration** | 1.27ms | 732µs | 1.7x slower |

**Why the difference?** pypersistent's C++ implementation provides:
- Optimized tree traversal and node allocation
- Cache-friendly memory layout
- Specialized bulk merge algorithms
- Direct memory manipulation without Python object overhead

#### Feature Comparison

| Feature | pypersistent | pyrsistent |
|---------|-------------|-----------|
| PersistentDict (pmap) | ✅ | ✅ |
| PersistentList (pvector) | ✅ | ✅ |
| PersistentSet (pset) | ✅ | ✅ |
| PersistentSortedDict | ✅ | ✅ (pbag) |
| PersistentArrayMap (small dicts) | ✅ | ❌ |
| PersistentBag (multiset) | ❌ | ✅ |
| PersistentDeque | ❌ | ✅ |
| PersistentRecord/Class | ❌ | ✅ |
| Evolvers (transient mutations) | ❌ | ✅ |
| Freeze/thaw (deep conversion) | ❌ | ✅ |
| Type checking integration | ❌ | ✅ |

**Summary**: Choose pypersistent for performance-critical applications with the core data structures. Choose pyrsistent for a richer feature set and pure-Python portability.

### Performance by Map Size

| Size | Construction | Lookup (1K ops) | Sharing (100 variants) |
|------|--------------|-----------------|------------------------|
| 100 | 73µs | 19µs | 77µs |
| 1K | 765µs | 205µs | 95µs |
| 10K | 9.7ms | 228µs | 108µs |
| 100K | 110ms | 255µs | 133µs |
| 1M | 2.74s | 776ms | 158µs |

### Fast Iteration Methods

For complete iteration over small-medium dicts, use materialized list methods:

```python
m = PersistentDict.from_dict({...})

# Fast methods (1.7-3x faster for dicts < 100K)
items = m.items_list()   # Returns list of (key, value) tuples
keys = m.keys_list()     # Returns list of keys
values = m.values_list() # Returns list of values

# Lazy iterators (better for very large dicts or early exit)
for k, v in m.items():   # Generator, O(log n) memory
    ...
```

**Performance**:
- Dicts ≤ 10K: **3x faster** with `items_list()`
- Dicts ≤ 100K: **1.7x faster** with `items_list()`
- Dicts > 100K: Use iterator (lazy, memory-efficient)

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

### PersistentDict - Hash Map

```python
from pypersistent import PersistentDict

# Create
m = PersistentDict.create(name='Alice', age=30)
m = PersistentDict.from_dict({'name': 'Alice', 'age': 30})

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

### PersistentSortedDict - Sorted Map

```python
from pypersistent import PersistentSortedDict

# Create (automatically sorted by keys)
m = PersistentSortedDict.from_dict({3: 'c', 1: 'a', 2: 'b'})
list(m.keys())  # [1, 2, 3]

# Same API as PersistentDict
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

### PersistentList - Indexed Sequence

```python
from pypersistent import PersistentList

# Create
v = PersistentList.create(1, 2, 3)
v = PersistentList.from_list([1, 2, 3])

# Append (functional)
v2 = v.conj(4)  # [1, 2, 3, 4]

# Update by index
v3 = v.assoc(0, 10)  # [10, 2, 3]

# Access by index
first = v[0]  # 1
last = v[-1]  # 3

# Slice
sub = v[1:3]  # PersistentList([2, 3])

# Iterate
for item in v:
    print(item)

# Length
len(v)  # 3
```

### PersistentSet - Set

```python
from pypersistent import PersistentSet

# Create
s = PersistentSet.create(1, 2, 3)
s = PersistentSet.from_set({1, 2, 3})

# Add
s2 = s.conj(4)  # {1, 2, 3, 4}

# Remove
s3 = s.disj(2)  # {1, 3}

# Membership
2 in s  # True

# Set operations
s1 = PersistentSet.create(1, 2, 3)
s2 = PersistentSet.create(3, 4, 5)

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

**PersistentDict / PersistentSortedDict:**
- `assoc(k, v)` / `set(k, v)` - Add/update
- `dissoc(k)` / `delete(k)` - Remove
- `get(k, default=None)` - Get value
- `m[k]` - Get (raises KeyError)
- `k in m` - Membership
- `keys()`, `values()`, `items()` - Iterators
- `m1 | m2` - Merge

**PersistentSortedDict only:**
- `first()` - Min entry
- `last()` - Max entry
- `subseq(start, end)` - Range query
- `rsubseq(start, end)` - Reverse range

**PersistentList:**
- `conj(item)` - Append
- `assoc(idx, val)` - Update by index
- `v[idx]` - Get by index
- `v[start:end]` - Slice
- `len(v)` - Length

**PersistentSet:**
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

### PersistentDict - Hash Array Mapped Trie (HAMT)
Based on the HAMT data structure used by Clojure, Scala, and Haskell:
- 32-way branching tree indexed by hash bits
- Path copying for immutability
- Structural sharing between versions
- `std::shared_ptr` for entry sharing (44x fewer INCREF/DECREF)
- Inline storage with `std::variant` for cache-friendly access

### PersistentSortedDict - Left-Leaning Red-Black Tree
Self-balancing binary search tree with:
- Path copying for immutability
- Sorted order maintenance (O(log₂ n))
- Atomic reference counting for node sharing
- Range query support via tree traversal

### PersistentList - Bit-Partitioned Trie
32-way branching tree for indexed access:
- Path copying for updates
- Tail optimization for fast append
- O(log₃₂ n) random access (~6 steps for 1M elements)
- Efficient slicing via structural sharing

### PersistentSet - HAMT-based Set
Uses PersistentDict internally with:
- Keys as set elements
- Same O(log₃₂ n) complexity
- Set algebra operations

### PersistentArrayMap - Simple Array
Linear array for tiny dicts (< 8 entries):
- Lower overhead than HAMT for small sizes
- O(n) operations but faster than tree for n < 8
- Automatically used by PersistentDict when beneficial

## Technical Details

**Complexity:**
- PersistentDict/HashSet: O(log₃₂ n) ≈ 6 steps for 1M elements
- PersistentSortedDict: O(log₂ n) for all operations
- PersistentList: O(log₃₂ n) get/set, O(1) append
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
from pypersistent import PersistentDict

# Shared base dict
base_config = PersistentDict.create(
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
pytest test_persistent_dict.py -v
pytest test_persistent_sorted_dict.py -v
pytest test_persistent_list.py -v
pytest test_persistent_set.py -v

# Run performance benchmarks
python performance_test.py
python performance_vector.py
```

## License

MIT License - see LICENSE file for details

## Credits

Inspired by Clojure's persistent data structures and the HAMT paper by Bagwell (2001).

Implementation by Clemens Marschner.
