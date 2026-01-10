"""
Performance comparison between PersistentList, Python's built-in list, and pyrsistent's pvector.

This test suite measures various operations to understand the trade-offs
between persistent immutable lists and mutable lists.

Statistical robustness: Each test runs multiple times with variance analysis.
"""

import time
import sys
import statistics
from typing import Callable, Any
from pypersistent import PersistentList

# Try to import pyrsistent for comparison
try:
    from pyrsistent import pvector
    HAS_PYRSISTENT = True
except ImportError:
    HAS_PYRSISTENT = False
    print("Note: pyrsistent not installed, skipping pyrsistent comparisons")
    print("Install with: pip install pyrsistent\n")


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
        self.cv = (self.stdev / self.mean * 100) if self.mean > 0 else 0.0  # Coefficient of variation


def timeit(func: Callable, runs: int = 5, warmup: int = 1) -> BenchmarkResult:
    """
    Time a function execution with statistical analysis.

    Args:
        func: Function to benchmark
        runs: Number of measurement runs (default: 5)
        warmup: Number of warmup runs to discard (default: 1)

    Returns:
        BenchmarkResult with statistical metrics
    """
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
    if show_variance and bench.cv > 5.0:  # Show variance if CV > 5%
        return f"{format_time(bench.median)} (±{bench.cv:.1f}%)"
    else:
        return format_time(bench.median)


def benchmark_append(n: int):
    """Test append performance."""
    print(f"\n=== Append Test (n={n:,}) ===")

    # Python list
    def list_append():
        l = []
        for i in range(n):
            l.append(i)
        return l

    # PersistentList
    def pvec_append():
        v = PersistentList()
        for i in range(n):
            v = v.conj(i)
        return v

    list_bench = timeit(list_append, runs=5)
    pvec_bench = timeit(pvec_append, runs=5)

    print(f"list:            {format_result(list_bench)}")
    print(f"PersistentList: {format_result(pvec_bench)}")
    ratio = pvec_bench.median / list_bench.median
    print(f"Ratio:           {ratio:.2f}x slower")

    # pyrsistent pvector if available
    if HAS_PYRSISTENT:
        def pyr_append():
            v = pvector()
            for i in range(n):
                v = v.append(i)
            return v

        pyr_bench = timeit(pyr_append, runs=5)
        print(f"pyrsistent:      {format_result(pyr_bench)}")
        print(f"Ratio vs list:   {pyr_bench.median / list_bench.median:.2f}x slower")
        print(f"vs pyrsistent:   {pvec_bench.median / pyr_bench.median:.2f}x")

    return list_bench.result, pvec_bench.result


def benchmark_index_access(l: list, v: PersistentList, n: int):
    """Test random access performance."""
    print(f"\n=== Random Access Test (n={n:,}) ===")

    # Python list
    def list_access():
        total = 0
        for i in range(n):
            total += l[i]
        return total

    # PersistentList
    def pvec_access():
        total = 0
        for i in range(n):
            total += v.nth(i)
        return total

    list_bench = timeit(list_access, runs=5)
    pvec_bench = timeit(pvec_access, runs=5)

    print(f"list:            {format_result(list_bench, show_variance=False)}")
    print(f"PersistentList: {format_result(pvec_bench, show_variance=False)}")
    print(f"Ratio:           {pvec_bench.median / list_bench.median:.2f}x slower")

    # pyrsistent if available
    if HAS_PYRSISTENT:
        pv = pvector(l)
        def pyr_access():
            total = 0
            for i in range(n):
                total += pv[i]
            return total

        pyr_bench = timeit(pyr_access, runs=5)
        print(f"pyrsistent:      {format_result(pyr_bench, show_variance=False)}")
        print(f"vs pyrsistent:   {pvec_bench.median / pyr_bench.median:.2f}x")


def benchmark_update(l: list, v: PersistentList, n: int):
    """Test update performance."""
    print(f"\n=== Update Test (n={n:,}) ===")
    print(f"Updating every 10th element...")

    # Python list - mutate in place
    def list_update():
        l_copy = l.copy()
        for i in range(0, n, 10):
            l_copy[i] = i * 2
        return l_copy

    # PersistentList - create new version
    def pvec_update():
        v_copy = v
        for i in range(0, n, 10):
            v_copy = v_copy.assoc(i, i * 2)
        return v_copy

    list_bench = timeit(list_update, runs=5)
    pvec_bench = timeit(pvec_update, runs=5)

    print(f"list (copy):     {format_result(list_bench, show_variance=False)}")
    print(f"PersistentList: {format_result(pvec_bench, show_variance=False)}")
    print(f"Ratio:           {pvec_bench.median / list_bench.median:.2f}x slower")

    # pyrsistent if available
    if HAS_PYRSISTENT:
        pv = pvector(l)
        def pyr_update():
            v_copy = pv
            for i in range(0, n, 10):
                v_copy = v_copy.set(i, i * 2)
            return v_copy

        pyr_bench = timeit(pyr_update, runs=5)
        print(f"pyrsistent:      {format_result(pyr_bench, show_variance=False)}")
        print(f"vs pyrsistent:   {pvec_bench.median / pyr_bench.median:.2f}x")


def benchmark_iteration(l: list, v: PersistentList, n: int):
    """Test iteration performance."""
    print(f"\n=== Iteration Test (n={n:,}) ===")

    # Python list
    def list_iter():
        total = 0
        for val in l:
            total += val
        return total

    # PersistentList
    def pvec_iter():
        total = 0
        for val in v:
            total += val
        return total

    list_bench = timeit(list_iter, runs=5)
    pvec_bench = timeit(pvec_iter, runs=5)

    print(f"list:            {format_result(list_bench, show_variance=False)}")
    print(f"PersistentList: {format_result(pvec_bench, show_variance=False)}")
    print(f"Ratio:           {pvec_bench.median / list_bench.median:.2f}x slower")

    # pyrsistent if available
    if HAS_PYRSISTENT:
        pv = pvector(l)
        def pyr_iter():
            total = 0
            for val in pv:
                total += val
            return total

        pyr_bench = timeit(pyr_iter, runs=5)
        print(f"pyrsistent:      {format_result(pyr_bench, show_variance=False)}")
        print(f"vs pyrsistent:   {pvec_bench.median / pyr_bench.median:.2f}x")


def benchmark_slice(l: list, v: PersistentList, n: int):
    """Test slicing performance."""
    print(f"\n=== Slice Test (n={n:,}) ===")
    print(f"Slicing middle third of vector...")

    start_idx = n // 3
    end_idx = 2 * n // 3

    # Python list
    def list_slice():
        return l[start_idx:end_idx]

    # PersistentList
    def pvec_slice():
        return v[start_idx:end_idx]

    list_bench = timeit(list_slice, runs=5)
    pvec_bench = timeit(pvec_slice, runs=5)

    print(f"list:            {format_result(list_bench, show_variance=False)}")
    print(f"PersistentList: {format_result(pvec_bench, show_variance=False)}")
    print(f"Ratio:           {pvec_bench.median / list_bench.median:.2f}x slower")

    # pyrsistent if available
    if HAS_PYRSISTENT:
        pv = pvector(l)
        def pyr_slice():
            return pv[start_idx:end_idx]

        pyr_bench = timeit(pyr_slice, runs=5)
        print(f"pyrsistent:      {format_result(pyr_bench, show_variance=False)}")
        print(f"vs pyrsistent:   {pvec_bench.median / pyr_bench.median:.2f}x")


def benchmark_structural_sharing(v: PersistentList, n: int):
    """
    Test the key advantage of PersistentList: creating multiple variants efficiently.
    This is where persistent data structures shine - you can cheaply create
    many variations that share most of their structure.

    CRITICAL BENCHMARK - uses robust statistics.
    """
    print(f"\n=== Structural Sharing Test (n={n:,}) ===")
    print("Creating 100 variants with one modification each...")

    # PersistentList - structural sharing
    def pvec_variants():
        variants = []
        for i in range(100):
            variant = v.assoc(i, i * 1000)
            variants.append(variant)
        return variants

    # Python list - must copy entire list
    def list_variants():
        l = list(range(n))
        variants = []
        for i in range(100):
            variant = l.copy()
            variant[i] = i * 1000
            variants.append(variant)
        return variants

    # Use robust statistical benchmarking with 7 runs
    pvec_bench = timeit(pvec_variants, runs=7)
    list_bench = timeit(list_variants, runs=7)

    ratio = list_bench.median / pvec_bench.median

    print(f"list (copy):     {format_result(list_bench)}")
    print(f"PersistentList: {format_result(pvec_bench)}")
    print(f"Ratio:           {ratio:.2f}x faster (PersistentList wins!)")

    # Always show variance for this critical benchmark
    print(f"  Statistics:")
    print(f"    list:  median={format_time(list_bench.median)}, CV={list_bench.cv:.1f}%, range={format_time(list_bench.min)}-{format_time(list_bench.max)}")
    print(f"    pvec:  median={format_time(pvec_bench.median)}, CV={pvec_bench.cv:.1f}%, range={format_time(pvec_bench.min)}-{format_time(pvec_bench.max)}")

    # pyrsistent if available
    if HAS_PYRSISTENT:
        pv = pvector(range(n))
        def pyr_variants():
            variants = []
            for i in range(100):
                variant = pv.set(i, i * 1000)
                variants.append(variant)
            return variants

        pyr_bench = timeit(pyr_variants, runs=7)
        pyr_ratio = list_bench.median / pyr_bench.median
        print(f"\npyrsistent:      {format_result(pyr_bench)}")
        print(f"Ratio vs list:   {pyr_ratio:.2f}x faster")
        print(f"vs pyrsistent:   {pvec_bench.median / pyr_bench.median:.2f}x")


def benchmark_from_list(n: int):
    """Test creating a vector from an existing list (CRITICAL BENCHMARK)."""
    print(f"\n=== Create from List Test (n={n:,}) ===")

    # Create source list
    source_list = list(range(n))

    # Python list - just copy
    def list_from_list():
        return list(source_list)

    # PersistentList - build from list
    def pvec_from_list():
        return PersistentList.from_list(source_list)

    # Use robust statistical benchmarking with 7 runs
    list_bench = timeit(list_from_list, runs=7)
    pvec_bench = timeit(pvec_from_list, runs=7)

    ratio = pvec_bench.median / list_bench.median

    print(f"list.copy():                {format_result(list_bench)}")
    print(f"PersistentList.from_list: {format_result(pvec_bench)}")
    print(f"Ratio:                      {ratio:.2f}x slower")

    # Always show variance for this critical benchmark
    print(f"  Statistics:")
    print(f"    list:  median={format_time(list_bench.median)}, CV={list_bench.cv:.1f}%, range={format_time(list_bench.min)}-{format_time(list_bench.max)}")
    print(f"    pvec:  median={format_time(pvec_bench.median)}, CV={pvec_bench.cv:.1f}%, range={format_time(pvec_bench.min)}-{format_time(pvec_bench.max)}")

    # pyrsistent if available
    if HAS_PYRSISTENT:
        def pyr_from_list():
            return pvector(source_list)

        pyr_bench = timeit(pyr_from_list, runs=7)
        print(f"\npyrsistent(list):           {format_result(pyr_bench)}")
        print(f"Ratio vs list:              {pyr_bench.median / list_bench.median:.2f}x slower")
        print(f"vs pyrsistent:              {pvec_bench.median / pyr_bench.median:.2f}x")


def benchmark_contains(l: list, v: PersistentList, n: int):
    """Test membership checking performance."""
    print(f"\n=== Contains Test (n={n:,}) ===")
    print("Checking first 100 elements...")

    # Python list
    def list_contains():
        count = 0
        for i in range(min(100, n)):
            if i in l:
                count += 1
        return count

    # PersistentList
    def pvec_contains():
        count = 0
        for i in range(min(100, n)):
            if i in v:
                count += 1
        return count

    list_bench = timeit(list_contains, runs=5)
    pvec_bench = timeit(pvec_contains, runs=5)

    print(f"list:            {format_result(list_bench, show_variance=False)}")
    print(f"PersistentList: {format_result(pvec_bench, show_variance=False)}")
    print(f"Ratio:           {pvec_bench.median / list_bench.median:.2f}x slower")
    print("Note: Contains is O(n) for both - use sets for O(1) membership")


def benchmark_pop(v: PersistentList, n: int):
    """Test pop (remove last) performance."""
    print(f"\n=== Pop Test (n={n:,}) ===")
    print("Removing last 100 elements...")

    # Python list
    def list_pop():
        l = list(range(n))
        for _ in range(100):
            l = l[:-1]  # Slice to create new list
        return l

    # PersistentList
    def pvec_pop():
        v_copy = v
        for _ in range(100):
            v_copy = v_copy.pop()
        return v_copy

    list_bench = timeit(list_pop, runs=5)
    pvec_bench = timeit(pvec_pop, runs=5)

    print(f"list (slice):    {format_result(list_bench, show_variance=False)}")
    print(f"PersistentList: {format_result(pvec_bench, show_variance=False)}")
    print(f"Ratio:           {pvec_bench.median / list_bench.median:.2f}x slower")


def run_benchmark_suite(sizes: list[int]):
    """Run complete benchmark suite for different sizes."""
    print("=" * 70)
    print("PERSISTENT LIST vs PYTHON LIST - PERFORMANCE COMPARISON")
    print("=" * 70)

    if HAS_PYRSISTENT:
        print("Including comparisons with pyrsistent.pvector")
    else:
        print("pyrsistent not available - install with: pip install pyrsistent")

    for n in sizes:
        print(f"\n{'=' * 70}")
        print(f"TESTING WITH {n:,} ELEMENTS")
        print(f"{'=' * 70}")

        # Build the data structures
        l, v = benchmark_append(n)

        # Run tests
        benchmark_index_access(l, v, n)
        benchmark_update(l, v, n)
        benchmark_iteration(l, v, n)
        benchmark_slice(l, v, n)
        benchmark_from_list(n)
        benchmark_structural_sharing(v, n)
        if n <= 10000:  # Contains is O(n), so skip for large n
            benchmark_contains(l, v, n)
        benchmark_pop(v, n)

    print("\n" + "=" * 70)
    print("SUMMARY")
    print("=" * 70)
    print("""
PersistentList Trade-offs:

ADVANTAGES:
  - Immutability: Thread-safe, no defensive copying needed
  - Structural Sharing: Creating variants is O(log₃₂ n), not O(n)
  - Time-travel: Keep old versions without full copies
  - Functional Programming: Natural fit for FP paradigms
  - O(1) amortized append with tail optimization
  - O(log₃₂ n) ≈ O(1) random access for practical sizes

DISADVANTAGES:
  - Slower raw performance for individual operations
  - Higher constant factors due to tree structure
  - More memory per entry (node overhead + Python objects)
  - Contains is O(n) (use PersistentSet for O(log n))

USE CASES:
  - When you need multiple versions of sequential data
  - Undo/redo functionality for lists
  - Concurrent access without locks
  - Functional/immutable architecture
  - When copying cost matters more than access speed
  - Building up collections incrementally

COMPLEXITY:
  - Append (conj):     O(1) amortized (tail optimization)
  - Access (nth):      O(log₃₂ n) ≈ O(1) practical
  - Update (assoc):    O(log₃₂ n) with structural sharing
  - Pop:               O(log₃₂ n)
  - Slice:             O(m) where m is slice size
  - Contains:          O(n) - use sets for membership
""")


if __name__ == '__main__':
    # Run benchmarks with different sizes
    sizes = [100, 1000, 10000]

    # Uncomment for more comprehensive benchmarking:
    # sizes = [100, 1000, 10000, 100000]

    run_benchmark_suite(sizes)
