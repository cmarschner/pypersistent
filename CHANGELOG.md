# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased] - feature/bulk-optimizations branch

### Added
- Fast iteration methods: `items_list()`, `keys_list()`, `values_list()` (1.7-3x faster for maps < 100K elements)
- Arena allocator for bulk operations (reduces allocation overhead)
- Bottom-up tree construction for `from_dict()` operations
- Structural tree merging for efficient `merge()` operations
- COW (Copy-on-Write) semantics for CollisionNode (5-17% improvement on hash collisions)
- Comprehensive benchmarking suite with statistical analysis (median + variance)
- Detailed documentation in `docs/` directory:
  - Optimization phase summaries
  - Performance comparison with pyrsistent
  - Benchmarking methodology
  - Proposals for future optimizations (PersistentArrayMap)

### Performance Improvements
- **Overall**: 6-8% faster for bulk operations compared to baseline
- **vs pyrsistent (pure Python)**:
  - Merge operations: **150x faster**
  - Construction (from_dict): **4x faster**
  - Iteration: **1.7-3x faster** with `items_list()` for maps < 100K elements
- **Structural sharing**: 1.7x faster for creating variants
- **Hash collisions**: 5-17% improvement with CollisionNode COW optimization

### Changed
- Reorganized documentation: moved detailed docs to `docs/` directory
- Updated README.md with comprehensive performance section
- Improved benchmark methodology with median-based analysis and Coefficient of Variation (CV%)

### Documentation
- Created `docs/optimizations/` with detailed optimization phase summaries
- Created `docs/comparisons/` with pyrsistent performance analysis
- Created `docs/proposals/` with future optimization proposals
- Created `docs/features/` with fast iteration feature documentation
- Added benchmarking methodology documentation
- Cleaned up root directory by removing 25+ benchmark result files

### Testing
- 34 tests passing (11 new collision node tests)
- Statistical benchmarking with 5-run median analysis
- Comprehensive test coverage for all optimization phases

### Technical Details

#### Phase 1: Threshold Detection + Pre-allocation
- Detect bulk operation thresholds (>100 elements)
- Pre-allocate arena and vectors for known sizes
- **Result**: 1.14x faster (2.90s → 2.55s)

#### Phase 2: Bottom-Up Tree Construction
- Build HAMT tree level-by-level for bulk operations
- Enables structural sharing during construction
- **Result**: Neutral on from_dict, 1.7x improvement on structural sharing

#### Phase 3: Arena Allocator
- Bump-pointer allocator for fast bulk node allocation
- 1MB chunk allocation with O(1) bump-pointer allocation
- **Result**: 6.8% improvement (2.74s final time)

#### Phase 4: Structural Merge
- O(n + m) tree-to-tree merging instead of O(n * log m) iterative
- Correct algorithm for future optimizations
- **Result**: Neutral (+1%), foundation for future work

#### Phase 5: CollisionNode COW
- Share CollisionNode entries with std::shared_ptr
- Copy-on-write semantics for collision lists
- **Result**: 5-17% improvement on hash collision workloads

#### Fast Iteration Feature
- Pre-allocated list methods bypass pybind11 iterator overhead
- Single boundary crossing vs. one per item
- **Result**: 1.7-3x faster for complete iteration on maps < 100K elements
