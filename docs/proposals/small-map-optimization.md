# Small Map Optimization Proposal: PersistentArrayMap

**Date**: January 3, 2026
**Status**: Proposal
**Priority**: HIGH - 60x performance opportunity for common case

---

## Problem: HAMT Overhead for Tiny Maps

Our current implementation uses HAMT (Hash Array Mapped Trie) for **all** map sizes, even single-element maps. This incurs unnecessary overhead for small maps:

### Performance Data (vs Python dict)

| Size | from_dict | Overhead | Incremental | Overhead |
|------|-----------|----------|-------------|----------|
| 1    | 583 ns    | **4.7x** | 1.08 µs     | **8.7x** |
| 8    | 1.25 µs   | **6.0x** | 4.33 µs     | **20.9x** |
| 16   | 2.42 µs   | **8.3x** | 9.00 µs     | **31.0x** |
| 32   | 6.04 µs   | **20.7x** | 19.00 µs    | **65.2x** |
| 64   | 19.71 µs  | **52.6x** | 45.58 µs    | **121.8x** |
| 100  | 29.92 µs  | **60x** | 69.00 µs    | **138x** |

**Key observation**: Overhead **grows exponentially** with size due to increasing HAMT tree depth.

### Why HAMT is Slow for Small Maps

For a 100-element HAMT:
1. **Hash computation**: 100 calls to `PyObject_Hash()` (~50-100ns each)
2. **Tree construction**: 3-4 levels of BitmapNodes, each requiring:
   - Vector allocation
   - Bitmap calculation
   - Pointer chasing
3. **Memory overhead**: Each BitmapNode = 32+ bytes, 100 entries ≈ 20-30 nodes
4. **Cache misses**: Scattered allocations, poor locality

For a simple flat array of 100 entries:
1. **No hashing needed** (linear scan with equality check)
2. **Single allocation**: One contiguous array
3. **Perfect cache locality**: Sequential access
4. **At 8-16 elements, O(n) linear scan beats O(log n) tree lookup**

---

## Solution: PersistentArrayMap (Clojure-style)

Implement a **hybrid approach** like Clojure:

### Design

```cpp
// For maps with ≤ 8 entries (tunable threshold)
class PersistentArrayMap : public NodeBase {
private:
    std::shared_ptr<std::vector<Entry*>> entries_;  // [k1,v1, k2,v2, ...]

public:
    // O(n) lookup - but n ≤ 8, so fast!
    py::object get(const py::object& key) const {
        for (auto* entry : *entries_) {
            if (entry->key.equal(key)) {
                return entry->value;
            }
        }
        return NOT_FOUND;
    }

    // O(n) assoc - returns new ArrayMap or upgrades to HAMT
    PersistentMap assoc(const py::object& key, const py::object& val) const {
        // If key exists, replace
        for (size_t i = 0; i < entries_->size(); ++i) {
            if ((*entries_)[i]->key.equal(key)) {
                // COW: copy array, replace entry
                auto new_entries = std::make_shared<std::vector<Entry*>>(*entries_);
                (*new_entries)[i] = new Entry(key, val);
                return PersistentArrayMap(new_entries);
            }
        }

        // Key not found - add it
        if (entries_->size() < ARRAY_MAP_THRESHOLD) {
            // Stay as ArrayMap
            auto new_entries = std::make_shared<std::vector<Entry*>>(*entries_);
            new_entries->push_back(new Entry(key, val));
            return PersistentArrayMap(new_entries);
        } else {
            // Upgrade to HAMT
            return upgradeToHAMT()->assoc(key, val);
        }
    }
};
```

### Threshold Selection

**Clojure uses 8 entries** as the threshold. Why?

1. **Cache line**: 8 entries × 16 bytes = 128 bytes (fits in 2 cache lines)
2. **Linear scan cost**: 8 comparisons = ~80-160ns (comparable to 1 HAMT lookup)
3. **Simplicity**: Power of 2, easy to reason about

**Our recommendation**: Start with **8 entries**, benchmark, potentially increase to **16**.

---

## Expected Performance Improvement

### Small Maps (≤8 elements)

| Operation | Current (HAMT) | With ArrayMap | Speedup |
|-----------|---------------|---------------|---------|
| from_dict (8 elem) | 1.25 µs | **~250 ns** | **5x faster** ✅ |
| lookup | 250 ns | **~100 ns** | **2.5x faster** ✅ |
| assoc | ~500 ns | **~200 ns** | **2.5x faster** ✅ |
| iteration | 25 µs | **~5 µs** | **5x faster** ✅ |

### Medium Maps (9-100 elements)

Transition to HAMT at 9 elements:
- **One-time upgrade cost**: ~2-3 µs (convert ArrayMap → HAMT)
- **After upgrade**: Same HAMT performance

**Net result**: Small maps much faster, no regression for larger maps.

---

## Implementation Plan

### Phase 1: PersistentArrayMap Core (2-3 days)

**Files to create**:
- `src/array_map.hpp` - PersistentArrayMap class definition
- `src/array_map.cpp` - Implementation

**Core operations**:
```cpp
class PersistentArrayMap {
    py::object get(const py::object& key) const;  // O(n) linear scan
    PersistentMap assoc(key, val) const;          // O(n), upgrades at threshold
    PersistentMap dissoc(const py::object& key) const;  // O(n), never downgrades
    bool contains(const py::object& key) const;    // O(n)
    size_t size() const;                           // O(1)

private:
    PersistentMap upgradeToHAMT() const;  // Convert to HAMT when size > threshold
};
```

### Phase 2: Integration with PersistentMap (1 day)

**Modify `src/persistent_map.cpp`**:

```cpp
PersistentMap PersistentMap::fromDict(const py::dict& d) {
    size_t n = d.size();

    // Use ArrayMap for small maps
    if (n <= ARRAY_MAP_THRESHOLD) {
        return PersistentArrayMap::fromDict(d);
    }

    // Use HAMT for larger maps (existing code)
    // ...
}
```

### Phase 3: Testing (1 day)

1. **Unit tests**: Ensure ArrayMap has same semantics as HAMT
2. **Transition tests**: Verify upgrade from ArrayMap → HAMT works correctly
3. **Performance tests**: Confirm 5x speedup for small maps
4. **Stress tests**: Test with 1M operations on tiny maps

---

## Design Decisions

### Q: Why not always use ArrayMap?

**A**: O(n) linear scan becomes too slow:
- 8 elements: ~8 comparisons = 80ns (fast!)
- 100 elements: ~50 comparisons average = 500ns (2x slower than HAMT)
- 1000 elements: ~500 comparisons = 5µs (20x slower than HAMT)

### Q: Should we downgrade from HAMT to ArrayMap on dissoc?

**A**: **No**. Reasons:
1. **Complexity**: Adds state transitions, harder to reason about
2. **Uncommon pattern**: Most code grows maps or keeps them stable
3. **Clojure doesn't downgrade**: Proven design decision

Once a map upgrades to HAMT, it stays HAMT.

### Q: What threshold should we use?

**A**: **Start with 8, benchmark, potentially increase to 16**.

Rationale:
- Clojure uses 8 (battle-tested)
- Cache-friendly (128 bytes = 2 cache lines)
- Can tune based on benchmarks

### Q: How to handle structural sharing between ArrayMap and HAMT?

**A**: They're **different types**, no sharing needed:
- ArrayMap uses `std::shared_ptr<vector<Entry*>>` (COW)
- HAMT uses tree nodes
- Upgrade creates new HAMT from scratch (one-time cost)

---

## Alternative: Inline Small Maps

For **very tiny maps** (1-4 elements), we could inline entries directly:

```cpp
class PersistentMap {
private:
    enum class StorageType { INLINE, ARRAY, HAMT };

    StorageType type_;

    union {
        struct {  // INLINE (≤4 entries)
            Entry* entries[4];
            uint8_t count;
        } inline_storage;

        PersistentArrayMap* array_map;  // ARRAY (5-16 entries)
        NodeBase* hamt_root;            // HAMT (17+ entries)
    };
};
```

**Trade-off**:
- ✅ Even faster for 1-4 element maps (no heap allocation)
- ❌ Adds complexity (3 storage types instead of 2)
- ❌ Larger sizeof(PersistentMap) (64 bytes vs 16 bytes)

**Recommendation**: **Start with ArrayMap only**, add inline optimization later if benchmarks justify it.

---

## Risks and Mitigations

### Risk 1: API Compatibility

**Risk**: Users might rely on implementation details (e.g., type checks)

**Mitigation**:
- PersistentArrayMap inherits from same base as HAMT
- All public APIs remain unchanged
- Python layer sees same PersistentMap type

### Risk 2: Upgrade Cost

**Risk**: Upgrading from ArrayMap → HAMT at 9th element might be expensive

**Mitigation**:
- Upgrade cost is one-time (~2-3 µs)
- Only happens once per map
- Far outweighed by 5x speedup for first 8 operations

### Risk 3: Testing Complexity

**Risk**: Need to test all operations for both ArrayMap and HAMT

**Mitigation**:
- Comprehensive test suite
- Property-based testing (ensure ArrayMap ≡ HAMT semantics)
- Fuzz testing at transition boundary (7-10 elements)

---

## Expected Timeline

| Phase | Task | Duration | Dependencies |
|-------|------|----------|--------------|
| 1 | Implement PersistentArrayMap core | 2-3 days | None |
| 2 | Integrate with PersistentMap | 1 day | Phase 1 |
| 3 | Write comprehensive tests | 1 day | Phase 2 |
| 4 | Benchmark and tune threshold | 0.5 day | Phase 3 |
| 5 | Documentation and examples | 0.5 day | Phase 4 |

**Total**: **5-6 days**

---

## Success Metrics

✅ **Primary**: Small maps (≤8 elements) are **5x faster** for from_dict and assoc
✅ **Secondary**: Lookup on small maps is **2-3x faster**
✅ **Safety**: All existing tests pass without modification
✅ **Semantics**: ArrayMap behaves identically to HAMT (property tests)

---

## Recommendation

**Implement PersistentArrayMap** - High value, well-understood, proven design.

### Why This is Worth It

1. **Common case**: Many applications use small maps frequently (config, metadata, small data)
2. **60x overhead** for 100-element maps is unacceptable
3. **Proven design**: Clojure has used this successfully for 15+ years
4. **Clean implementation**: Clear separation, easy to test
5. **No breaking changes**: Pure performance optimization

### Why NOT to Do It

Only skip if:
- Small maps are not used in target applications
- 5-6 days is too long for current priorities
- Want to ship current optimizations first (valid staged approach)

**My recommendation**: **Implement this**. It's the single biggest performance win for typical usage patterns.
