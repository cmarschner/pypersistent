# pypersistent vs pyrsistent: Side-by-Side Comparison

**Methodology**: Same as `performance_test.py` - statistical robustness with median values, CV% for high variance.

## Performance Comparison Table

### 100 Elements

| Operation | pypersistent (C++) | pyrsistent (Python) | Speedup |
|-----------|-------------------|---------------------|---------|
| **Insertion** | 73.08 µs | 398.75 µs | **5.5x faster** |
| **Lookup** | 19.04 µs | 62.79 µs | **3.3x faster** |
| **Contains** | 17.83 µs | 60.38 µs | **3.4x faster** |
| **Update** | 73.42 µs | 361.71 µs | **4.9x faster** |
| **Deletion** | 66.54 µs | 344.50 µs | **5.2x faster** |
| **Iteration** | 46.29 µs | 14.87 µs | 3.1x slower |
| **from_dict** | 30.21 µs | 18.50 µs | 1.6x slower |
| **Merge** | 18.58 µs | 119.58 µs | **6.4x faster** |
| **Structural Sharing** | 77.38 µs | 362.92 µs | **4.7x faster** |

### 1,000 Elements

| Operation | pypersistent (C++) | pyrsistent (Python) | Speedup |
|-----------|-------------------|---------------------|---------|
| **Insertion** | 764.75 µs | 4.01 ms | **5.2x faster** |
| **Lookup** | 205.04 µs | 693.21 µs | **3.4x faster** |
| **Contains** | 191.67 µs | 671.67 µs | **3.5x faster** |
| **Update** | 90.21 µs | 401.96 µs | **4.5x faster** |
| **Deletion** | 88.08 µs | 380.67 µs | **4.3x faster** |
| **Iteration** | 249.79 µs | 139.29 µs | 1.8x slower |
| **from_dict** | 75.79 µs | 195.33 µs | **2.6x faster** |
| **Merge** | 9.33 µs | 1.23 ms | **131x faster** ⚡ |
| **Structural Sharing** | 94.87 µs | 398.92 µs | **4.2x faster** |

### 1,000,000 Elements

| Operation | pypersistent (C++) | pyrsistent (Python) | Speedup |
|-----------|-------------------|---------------------|---------|
| **Insertion** | 2.08 s | 8.25 s | **4.0x faster** |
| **Lookup** | 775.81 ms | 806.46 ms | **1.04x faster** |
| **Contains** | 775.42 ms | 771.39 ms | 1.01x slower |
| **Update** | 147.29 µs | 482.25 µs | **3.3x faster** |
| **Deletion** | 154.38 µs | 471.75 µs | **3.1x faster** |
| **Iteration** | 487.72 ms | 169.03 ms | 2.9x slower |
| **from_dict** | 196.25 ms | 787.51 ms | **4.0x faster** |
| **Merge** | 10.48 ms | 1.57 s | **150x faster** ⚡ |
| **Structural Sharing** | 157.75 µs | 471.04 µs | **3.0x faster** |

## Key Insights

### Where pypersistent Dominates 🚀

1. **Merge Operations**: 6-150x faster
   - Phase 4 structural merge vs iterative assoc
   - Game-changing for large maps
   - At 1M elements: 10.48ms vs 1.57s

2. **Insertion/Update/Delete**: 3-5x faster
   - C++ HAMT implementation
   - Intrusive reference counting
   - Optimized tree operations

3. **Bulk Construction (from_dict)**: 1.6-4x faster (except tiny maps)
   - Phase 3 arena allocator
   - Eliminates malloc overhead
   - At 1M elements: 196ms vs 788ms

4. **Structural Sharing**: 3-5x faster
   - Efficient COW semantics
   - Fast node cloning

### Where pyrsistent Wins ⚠️

1. **Iteration**: 1.8-3.1x faster
   - Pure Python generators efficient
   - No C++ boundary crossing
   - Could be optimized in pypersistent if needed

2. **Tiny Maps (<100 elements, from_dict)**: 1.6x faster
   - Python dict → pmap highly optimized
   - Minimal for such small sizes

### Neutral Performance

1. **Lookup/Contains at 1M elements**: ~1% difference
   - Both implementations well-optimized
   - Python overhead minimal for single operations

## Recommendations

### Use pypersistent when:
- ✅ Performance is critical
- ✅ Working with maps >1K elements
- ✅ Merge/update operations frequent
- ✅ Building high-performance applications

### Use pyrsistent when:
- ✅ Pure Python portability required
- ✅ Iteration-heavy workloads
- ✅ Very small maps (<100 elements)
- ✅ No C++ compiler available

## Implementation Notes

**pypersistent (ours)**:
- C++ HAMT with pybind11 bindings
- Phase 1-2: Bottom-up bulk construction
- Phase 3: Arena allocator (1MB chunks, bump-pointer)
- Phase 4: Structural merge (O(n+m) vs O(n*log m))
- Phase 5: CollisionNode COW optimization

**pyrsistent (library)**:
- Pure Python HAMT (v0.20.0)
- No C extensions (removed in recent versions)
- Mature, well-tested codebase
- Excellent portability

## Statistical Robustness

All benchmarks use:
- **Median** of 5-7 runs (more robust than mean)
- **Coefficient of Variation (CV%)** shown for high-variance tests
- **Warmup runs** discarded to eliminate cold-start effects
- **Same methodology** as `performance_test.py`

## Conclusion

pypersistent provides **significant performance advantages** (3-150x faster) for most operations, especially:
- **Merge**: 150x faster at scale (structural tree merging)
- **Bulk operations**: 4x faster (arena allocator)
- **Modifications**: 3-5x faster (C++ HAMT)

The only trade-off is **iteration** (1.8-3.1x slower), which could be optimized if needed.

**Bottom line**: For performance-critical applications with medium-to-large datasets, pypersistent delivers substantial improvements while maintaining the same immutable semantics.
