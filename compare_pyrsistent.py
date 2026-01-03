#!/usr/bin/env python3
"""
Comparative Performance Test: pypersistent vs pyrsistent

Compares our C++-based PersistentMap implementation with the pyrsistent
library's pure Python PMap implementation.

Note: pyrsistent 0.20.0 removed C extensions and is pure Python.
"""

import sys
import time
import statistics
from typing import Callable, List

# Our implementation
sys.path.insert(0, 'src')
from pypersistent import PersistentMap

# pyrsistent library
from pyrsistent import pmap, PMap


def robust_timer(func: Callable, runs: int = 7) -> tuple:
    """
    Run function multiple times and return robust statistics.
    Returns (median, cv_percent, min, max)
    """
    times = []
    for _ in range(runs):
        start = time.perf_counter()
        func()
        elapsed = time.perf_counter() - start
        times.append(elapsed)

    median_time = statistics.median(times)
    mean_time = statistics.mean(times)
    std_dev = statistics.stdev(times) if len(times) > 1 else 0
    cv = (std_dev / mean_time * 100) if mean_time > 0 else 0

    return median_time, cv, min(times), max(times)


def format_time(seconds: float) -> str:
    """Format time in appropriate units"""
    if seconds < 1e-6:
        return f"{seconds * 1e9:.2f} ns"
    elif seconds < 1e-3:
        return f"{seconds * 1e6:.2f} µs"
    elif seconds < 1:
        return f"{seconds * 1e3:.2f} ms"
    else:
        return f"{seconds:.2f} s"


def print_section(title: str):
    """Print section header"""
    print()
    print("=" * 70)
    print(title)
    print("=" * 70)
    print()


def compare_from_dict(n: int):
    """Compare bulk construction from dict"""
    data = {i: i * 2 for i in range(n)}

    print(f"=== Bulk Construction from Dict (n={n:,}) ===")

    # Our implementation
    our_median, our_cv, our_min, our_max = robust_timer(
        lambda: PersistentMap.from_dict(data)
    )

    # pyrsistent
    pyr_median, pyr_cv, pyr_min, pyr_max = robust_timer(
        lambda: pmap(data)
    )

    ratio = pyr_median / our_median if our_median > 0 else 0

    print(f"pypersistent (C++):  {format_time(our_median)} (±{our_cv:.1f}%)")
    print(f"pyrsistent (Python): {format_time(pyr_median)} (±{pyr_cv:.1f}%)")

    if ratio > 1:
        print(f"Speedup:             {ratio:.2f}x faster (pypersistent wins!)")
    else:
        print(f"Speedup:             {1/ratio:.2f}x slower (pyrsistent wins)")

    print(f"  pypersistent range: {format_time(our_min)}-{format_time(our_max)}")
    print(f"  pyrsistent range:   {format_time(pyr_min)}-{format_time(pyr_max)}")
    print()


def compare_lookup(n: int):
    """Compare lookup performance"""
    data = {i: i * 2 for i in range(n)}
    our_map = PersistentMap.from_dict(data)
    pyr_map = pmap(data)

    lookup_keys = list(range(0, n, max(1, n // 1000)))  # Sample 1000 keys

    print(f"=== Lookup Test (n={n:,}, {len(lookup_keys)} lookups) ===")

    def our_lookup():
        for k in lookup_keys:
            _ = our_map.get(k)

    def pyr_lookup():
        for k in lookup_keys:
            _ = pyr_map.get(k)

    our_median, our_cv, _, _ = robust_timer(our_lookup)
    pyr_median, pyr_cv, _, _ = robust_timer(pyr_lookup)

    ratio = pyr_median / our_median if our_median > 0 else 0

    print(f"pypersistent (C++):  {format_time(our_median)} (±{our_cv:.1f}%)")
    print(f"pyrsistent (Python): {format_time(pyr_median)} (±{pyr_cv:.1f}%)")

    if ratio > 1:
        print(f"Speedup:             {ratio:.2f}x faster")
    else:
        print(f"Speedup:             {1/ratio:.2f}x slower")
    print()


def compare_assoc(n: int):
    """Compare assoc (set) performance"""
    data = {i: i * 2 for i in range(n)}
    our_map = PersistentMap.from_dict(data)
    pyr_map = pmap(data)

    updates = list(range(0, n, max(1, n // 100)))  # 100 updates

    print(f"=== Assoc/Update Test (n={n:,}, {len(updates)} updates) ===")

    def our_assoc():
        m = our_map
        for k in updates:
            m = m.assoc(k, k * 3)

    def pyr_assoc():
        m = pyr_map
        for k in updates:
            m = m.set(k, k * 3)

    our_median, our_cv, _, _ = robust_timer(our_assoc)
    pyr_median, pyr_cv, _, _ = robust_timer(pyr_assoc)

    ratio = pyr_median / our_median if our_median > 0 else 0

    print(f"pypersistent (C++):  {format_time(our_median)} (±{our_cv:.1f}%)")
    print(f"pyrsistent (Python): {format_time(pyr_median)} (±{pyr_cv:.1f}%)")

    if ratio > 1:
        print(f"Speedup:             {ratio:.2f}x faster")
    else:
        print(f"Speedup:             {1/ratio:.2f}x slower")
    print()


def compare_iteration(n: int):
    """Compare iteration performance"""
    data = {i: i * 2 for i in range(n)}
    our_map = PersistentMap.from_dict(data)
    pyr_map = pmap(data)

    print(f"=== Iteration Test (n={n:,}) ===")

    def our_iter():
        for k, v in our_map.items():
            pass

    def pyr_iter():
        for k, v in pyr_map.items():
            pass

    our_median, our_cv, _, _ = robust_timer(our_iter)
    pyr_median, pyr_cv, _, _ = robust_timer(pyr_iter)

    ratio = pyr_median / our_median if our_median > 0 else 0

    print(f"pypersistent (C++):  {format_time(our_median)} (±{our_cv:.1f}%)")
    print(f"pyrsistent (Python): {format_time(pyr_median)} (±{pyr_cv:.1f}%)")

    if ratio > 1:
        print(f"Speedup:             {ratio:.2f}x faster")
    else:
        print(f"Speedup:             {1/ratio:.2f}x slower")
    print()


def compare_merge(n: int):
    """Compare merge performance"""
    data1 = {i: i * 2 for i in range(n // 2)}
    data2 = {i: i * 3 for i in range(n // 2, n)}

    our_map1 = PersistentMap.from_dict(data1)
    our_map2 = PersistentMap.from_dict(data2)
    pyr_map1 = pmap(data1)
    pyr_map2 = pmap(data2)

    print(f"=== Merge Test (n={n:,}, merging {n//2:,} + {n//2:,}) ===")

    def our_merge():
        return our_map1.merge(our_map2)

    def pyr_merge():
        return pyr_map1.update(pyr_map2)

    our_median, our_cv, _, _ = robust_timer(our_merge)
    pyr_median, pyr_cv, _, _ = robust_timer(pyr_merge)

    ratio = pyr_median / our_median if our_median > 0 else 0

    print(f"pypersistent (C++):  {format_time(our_median)} (±{our_cv:.1f}%)")
    print(f"pyrsistent (Python): {format_time(pyr_median)} (±{pyr_cv:.1f}%)")

    if ratio > 1:
        print(f"Speedup:             {ratio:.2f}x faster")
    else:
        print(f"Speedup:             {1/ratio:.2f}x slower")
    print()


def compare_structural_sharing(n: int):
    """Compare structural sharing cost (creating variants)"""
    data = {i: i * 2 for i in range(n)}
    our_map = PersistentMap.from_dict(data)
    pyr_map = pmap(data)

    variants = 100

    print(f"=== Structural Sharing Test (n={n:,}, {variants} variants) ===")

    def our_variants():
        maps = []
        for i in range(variants):
            maps.append(our_map.assoc(i, i * 10))
        return maps

    def pyr_variants():
        maps = []
        for i in range(variants):
            maps.append(pyr_map.set(i, i * 10))
        return maps

    our_median, our_cv, _, _ = robust_timer(our_variants)
    pyr_median, pyr_cv, _, _ = robust_timer(pyr_variants)

    ratio = pyr_median / our_median if our_median > 0 else 0

    print(f"pypersistent (C++):  {format_time(our_median)} (±{our_cv:.1f}%)")
    print(f"pyrsistent (Python): {format_time(pyr_median)} (±{pyr_cv:.1f}%)")

    if ratio > 1:
        print(f"Speedup:             {ratio:.2f}x faster")
    else:
        print(f"Speedup:             {1/ratio:.2f}x slower")
    print()


def main():
    print_section("PYPERSISTENT vs PYRSISTENT - PERFORMANCE COMPARISON")
    print("pypersistent: Our C++-based implementation (with Phase 1-4 optimizations)")
    print("pyrsistent:   Pure Python implementation (v0.20.0, no C extensions)")
    print()

    test_sizes = [100, 1_000, 10_000, 100_000]

    for n in test_sizes:
        print_section(f"TESTING WITH {n:,} ELEMENTS")
        compare_from_dict(n)
        compare_lookup(n)
        compare_assoc(n)
        compare_iteration(n)
        compare_merge(n)
        compare_structural_sharing(n)

    print_section("SUMMARY")
    print("pypersistent leverages C++ for:")
    print("  - Efficient HAMT node operations")
    print("  - Optimized hash calculations")
    print("  - Fast memory management (intrusive refcounting)")
    print("  - Arena allocator for bulk operations (Phase 3)")
    print()
    print("pyrsistent (pure Python) is:")
    print("  - Portable and easy to install")
    print("  - Well-tested and mature")
    print("  - But slower due to Python overhead")
    print()
    print("Use pypersistent when:")
    print("  - Performance is critical")
    print("  - Working with large datasets")
    print("  - Building performance-sensitive applications")
    print()
    print("Use pyrsistent when:")
    print("  - Pure Python is required")
    print("  - Compatibility across platforms is paramount")
    print("  - Performance is not the primary concern")


if __name__ == "__main__":
    main()
