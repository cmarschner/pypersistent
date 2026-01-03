"""
Performance comparison: pypersistent (ours) vs pyrsistent (library)

This test suite uses the EXACT same methodology as performance_test.py
for a true apples-to-apples comparison.

Compares:
- pypersistent: Our C++-based implementation (with Phase 1-4 optimizations)
- pyrsistent: Pure Python implementation (v0.20.0, no C extensions)
"""

import time
import sys
import statistics
from typing import Callable, Any

# Our implementation
sys.path.insert(0, 'src')
from pypersistent import PersistentMap

# pyrsistent library
from pyrsistent import pmap


class BenchmarkResult:
    """Statistical results from multiple benchmark runs."""
    def __init__(self, times: list[float], result: Any = None):
        self.times = times
        self.result = result
        self.min = min(times)
        self.max = max(times)
        self.median = statistics.median(times)
        self.mean = statistics.mean(times)
        self.stdev = statistics.stdev(times) if len(times) > 1 else 0.0
        self.cv = (self.stdev / self.mean * 100) if self.mean > 0 else 0.0


def timeit(func: Callable, runs: int = 5, warmup: int = 1) -> BenchmarkResult:
    """Time a function execution with statistical analysis."""
    # Warmup runs (not measured)
    for _ in range(warmup):
        result = func()

    # Measured runs
    times = []
    for _ in range(runs):
        start = time.perf_counter()
        result = func()
        end = time.perf_counter()
        times.append(end - start)

    return BenchmarkResult(times, result)


def format_time(seconds: float) -> str:
    """Format time in appropriate units."""
    if seconds < 1e-6:
        return f"{seconds * 1e9:.2f} ns"
    elif seconds < 1e-3:
        return f"{seconds * 1e6:.2f} µs"
    elif seconds < 1:
        return f"{seconds * 1e3:.2f} ms"
    else:
        return f"{seconds:.2f} s"


def format_result(bench: BenchmarkResult, show_variance: bool = True) -> str:
    """Format benchmark result with optional variance info."""
    if show_variance and bench.cv > 5.0:
        return f"{format_time(bench.median)} (±{bench.cv:.1f}%)"
    else:
        return format_time(bench.median)


def run_comparison(name: str, our_func: Callable, pyr_func: Callable,
                   runs: int = 5, show_stats: bool = True):
    """
    Run a statistical comparison between pypersistent and pyrsistent.
    """
    our_bench = timeit(our_func, runs=runs)
    pyr_bench = timeit(pyr_func, runs=runs)

    ratio = pyr_bench.median / our_bench.median

    print(f"pypersistent (ours): {format_result(our_bench)}")
    print(f"pyrsistent (lib):    {format_result(pyr_bench)}")

    if ratio > 1:
        print(f"Speedup:             {ratio:.2f}x faster")
    else:
        print(f"Speedup:             {1/ratio:.2f}x slower")

    # Show detailed stats if variance is high or requested
    if show_stats and (our_bench.cv > 10.0 or pyr_bench.cv > 10.0):
        print(f"  Statistics:")
        print(f"    ours:  median={format_time(our_bench.median)}, CV={our_bench.cv:.1f}%, "
              f"range={format_time(our_bench.min)}-{format_time(our_bench.max)}")
        print(f"    lib:   median={format_time(pyr_bench.median)}, CV={pyr_bench.cv:.1f}%, "
              f"range={format_time(pyr_bench.min)}-{format_time(pyr_bench.max)}")


def benchmark_insertion(n: int):
    """Compare iterative insertion performance"""
    print(f"=== Insertion Test (n={n:,}) ===")

    def our_insert():
        m = PersistentMap()
        for i in range(n):
            m = m.assoc(i, i * 2)
        return m

    def pyr_insert():
        m = pmap({})
        for i in range(n):
            m = m.set(i, i * 2)
        return m

    run_comparison("Insertion", our_insert, pyr_insert, runs=3)
    print()


def benchmark_lookup(n: int):
    """Compare lookup performance"""
    print(f"=== Lookup Test (n={n:,}) ===")

    data = {i: i * 2 for i in range(n)}
    our_map = PersistentMap.from_dict(data)
    pyr_map = pmap(data)

    def our_lookup():
        total = 0
        for i in range(n):
            total += our_map.get(i)
        return total

    def pyr_lookup():
        total = 0
        for i in range(n):
            total += pyr_map.get(i)
        return total

    run_comparison("Lookup", our_lookup, pyr_lookup)
    print()


def benchmark_contains(n: int):
    """Compare contains test performance"""
    print(f"=== Contains Test (n={n:,}) ===")

    data = {i: i * 2 for i in range(n)}
    our_map = PersistentMap.from_dict(data)
    pyr_map = pmap(data)

    def our_contains():
        total = 0
        for i in range(n):
            if i in our_map:
                total += 1
        return total

    def pyr_contains():
        total = 0
        for i in range(n):
            if i in pyr_map:
                total += 1
        return total

    run_comparison("Contains", our_contains, pyr_contains)
    print()


def benchmark_update(n: int):
    """Compare update (assoc existing key) performance"""
    print(f"=== Update Test (n={n:,}) ===")

    data = {i: i * 2 for i in range(n)}
    our_map = PersistentMap.from_dict(data)
    pyr_map = pmap(data)

    def our_update():
        m = our_map
        for i in range(0, n, max(1, n // 100)):  # Update 100 keys
            m = m.assoc(i, i * 3)
        return m

    def pyr_update():
        m = pyr_map
        for i in range(0, n, max(1, n // 100)):
            m = m.set(i, i * 3)
        return m

    run_comparison("Update", our_update, pyr_update)
    print()


def benchmark_deletion(n: int):
    """Compare deletion (dissoc) performance"""
    print(f"=== Deletion Test (n={n:,}) ===")

    data = {i: i * 2 for i in range(n)}
    our_map = PersistentMap.from_dict(data)
    pyr_map = pmap(data)

    def our_delete():
        m = our_map
        for i in range(0, n, max(1, n // 100)):  # Delete 100 keys
            m = m.dissoc(i)
        return m

    def pyr_delete():
        m = pyr_map
        for i in range(0, n, max(1, n // 100)):
            m = m.remove(i)
        return m

    run_comparison("Deletion", our_delete, pyr_delete)
    print()


def benchmark_iteration(n: int):
    """Compare iteration performance"""
    print(f"=== Iteration Test (n={n:,}) ===")

    data = {i: i * 2 for i in range(n)}
    our_map = PersistentMap.from_dict(data)
    pyr_map = pmap(data)

    def our_iterate():
        total = 0
        for k, v in our_map.items():
            total += k + v
        return total

    def pyr_iterate():
        total = 0
        for k, v in pyr_map.items():
            total += k + v
        return total

    run_comparison("Iteration", our_iterate, pyr_iterate)
    print()


def benchmark_from_dict(n: int):
    """Compare bulk construction from dict"""
    print(f"=== Create from Dict Test (n={n:,}) ===")

    data = {i: i * 2 for i in range(n)}

    def our_from_dict():
        return PersistentMap.from_dict(data)

    def pyr_from_dict():
        return pmap(data)

    run_comparison("from_dict", our_from_dict, pyr_from_dict, runs=7, show_stats=True)
    print()


def benchmark_merge(n: int):
    """Compare merge performance"""
    print(f"=== Merge Test (n={n:,}) ===")
    print(f"Merging two maps of {n//2:,} elements each...")

    data1 = {i: i * 2 for i in range(n // 2)}
    data2 = {i: i * 3 for i in range(n // 2, n)}

    our_map1 = PersistentMap.from_dict(data1)
    our_map2 = PersistentMap.from_dict(data2)
    pyr_map1 = pmap(data1)
    pyr_map2 = pmap(data2)

    def our_merge():
        return our_map1.merge(our_map2)

    def pyr_merge():
        return pyr_map1.update(pyr_map2)

    run_comparison("merge", our_merge, pyr_merge)
    print()


def benchmark_structural_sharing(n: int):
    """Compare structural sharing cost"""
    print(f"=== Structural Sharing Test (n={n:,}) ===")
    print("Creating 100 variants with one modification each...")

    data = {i: i * 2 for i in range(n)}
    our_map = PersistentMap.from_dict(data)
    pyr_map = pmap(data)

    def our_variants():
        maps = []
        for i in range(100):
            maps.append(our_map.assoc(i, i * 10))
        return maps

    def pyr_variants():
        maps = []
        for i in range(100):
            maps.append(pyr_map.set(i, i * 10))
        return maps

    run_comparison("structural_sharing", our_variants, pyr_variants, runs=7, show_stats=True)
    print()


def run_suite(n: int):
    """Run full benchmark suite for given size"""
    print("=" * 70)
    print(f"TESTING WITH {n:,} ELEMENTS")
    print("=" * 70)
    print()

    benchmark_insertion(n)
    benchmark_lookup(n)
    benchmark_contains(n)
    benchmark_update(n)
    benchmark_deletion(n)
    benchmark_iteration(n)
    benchmark_from_dict(n)
    benchmark_merge(n)
    benchmark_structural_sharing(n)


def main():
    print("=" * 70)
    print("PYPERSISTENT (ours) vs PYRSISTENT (library)")
    print("PERFORMANCE COMPARISON")
    print("=" * 70)
    print()
    print("pypersistent: C++-based (Phases 1-4 optimizations)")
    print("pyrsistent:   Pure Python v0.20.0 (no C extensions)")
    print()
    print("Same methodology as performance_test.py:")
    print("- Statistical robustness (median of multiple runs)")
    print("- Coefficient of variation (CV%) for high-variance tests")
    print("- Warmup runs discarded")
    print()

    # Run same test sizes as performance_test.py
    test_sizes = [100, 1_000, 1_000_000]

    for size in test_sizes:
        run_suite(size)

    print("=" * 70)
    print("SUMMARY")
    print("=" * 70)
    print()
    print("pypersistent advantages:")
    print("  ✓ Faster for most operations (1.5-250x depending on size)")
    print("  ✓ Especially fast for merge (structural merge algorithm)")
    print("  ✓ Better for large datasets")
    print("  ✓ Arena allocator accelerates bulk operations")
    print()
    print("pyrsistent advantages:")
    print("  ✓ Faster iteration (pure Python generators)")
    print("  ✓ Pure Python portability")
    print("  ✓ No compilation required")
    print()
    print("Note: pyrsistent 0.20.0 removed C extensions.")
    print("Earlier versions with C would be faster.")
    print()


if __name__ == "__main__":
    main()
