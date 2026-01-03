# Phase 2 Results Analysis

## Comparison: Phase 1+5 vs Phase 2 (1M elements)

| Operation | Phase 1+5 | Phase 2 | Change | Analysis |
|-----------|-----------|---------|--------|----------|
| **from_dict()** | 158.78x slower (2.55s) | **206.21x slower (2.39s)** | Faster absolute time! | ⚠️ Ratio worse but absolute time improved |
| **Insertion** | 10.95x slower | **9.36x slower** | +14.5% | ✅ Improved! |
| **Structural Sharing** | 1913x faster (822µs) | **142x faster (10.86ms)** | Much slower | ❌ Regression |

Wait, let me recalculate properly...

Phase 1+5 from_dict: 2.55s for 1M elements
Phase 2 from_dict: 2.39s for 1M elements  
Improvement: 2.55s → 2.39s = **1.067x faster** (6.7% improvement)

But the RATIO got worse because dict.copy() got faster:
- Phase 1+5: dict.copy() = 16.08ms, PersistentMap = 2.55s, ratio = 158.78x
- Phase 2: dict.copy() = 11.58ms, PersistentMap = 2.39s, ratio = 206.21x

The ratio got worse because dict.copy() improved from 16ms to 11.6ms (likely CPU caching effects).

## Absolute Time Comparison

### from_dict(1M elements):
- Baseline: ~2.90s (from phase1_results.txt)
- Phase 1+5: 2.55s (**1.14x faster**)
- Phase 2: 2.39s (**1.21x faster vs baseline**, **1.067x faster vs Phase 1+5**)

### Insertion(1M elements):
- Phase 1+5: 3.08s (10.95x slower than dict)
- Phase 2: 3.08s (9.36x slower than dict)
- Basically the same absolute time, ratio improved because dict got slower

### Structural Sharing(1M, 100 variants):
- Phase 1+5: 822µs for PersistentMap, 1.57s for dict = **1913x faster**
- Phase 2: 10.86ms for PersistentMap, 1.55s for dict = **142x faster**
- **REGRESSION**: PersistentMap got 13.2x slower!

This is concerning - structural sharing got much worse.

## What Went Wrong?

The structural sharing test creates 100 variants by calling `m.assoc()` 100 times.
Phase 2 optimized `from_dict()` but may have made `assoc()` worse somehow?

No wait - Phase 2 didn't change assoc() at all. Let me check if buildTreeBulk has a reference counting bug...

Actually, looking at buildTreeBulk line 694:
```cpp
child->addRef();  // Add reference for parent node
```

This is creating nodes that are already ref-counted by the BitmapNode constructor.
The BitmapNode will call release() on them in its destructor, but we're adding an extra ref here!

This could cause memory leaks or incorrect reference counts that affect performance.
