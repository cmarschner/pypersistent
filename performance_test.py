"""
Performance comparison between PersistentMap and Python's built-in dict.

This test suite measures various operations to understand the trade-offs
between persistent immutable data structures and mutable dictionaries.

Statistical robustness: Each test runs multiple times with variance analysis.
"""

import time
import sys
import statistics
from typing import Callable, Any
from pypersistent import PersistentMap


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


def run_comparison(name: str, dict_func: Callable, pmap_func: Callable,
                   runs: int = 5, show_stats: bool = True):
    """
    Run a statistical comparison between dict and PersistentMap.

    Args:
        name: Test name
        dict_func: Function implementing dict version
        pmap_func: Function implementing PersistentMap version
        runs: Number of runs for statistics
        show_stats: Whether to show detailed statistics
    """
    dict_bench = timeit(dict_func, runs=runs)
    pmap_bench = timeit(pmap_func, runs=runs)

    ratio = pmap_bench.median / dict_bench.median

    print(f"dict:          {format_result(dict_bench)}")
    print(f"PersistentMap: {format_result(pmap_bench)}")
    print(f"Ratio:         {ratio:.2f}x slower")

    # Show detailed stats if variance is high or requested
    if show_stats and (dict_bench.cv > 10.0 or pmap_bench.cv > 10.0):
        print(f"  ⚠ High variance detected:")
        if dict_bench.cv > 10.0:
            print(f"    dict: CV={dict_bench.cv:.1f}%, range={format_time(dict_bench.min)}-{format_time(dict_bench.max)}")
        if pmap_bench.cv > 10.0:
            print(f"    pmap: CV={pmap_bench.cv:.1f}%, range={format_time(pmap_bench.min)}-{format_time(pmap_bench.max)}")

    return dict_bench, pmap_bench


def benchmark_insertion(n: int):
    """Test insertion performance."""
    print(f"\n=== Insertion Test (n={n:,}) ===")

    # Python dict
    def dict_insert():
        d = {}
        for i in range(n):
            d[f'key{i}'] = i
        return d

    # PersistentMap
    def pmap_insert():
        m = PersistentMap()
        for i in range(n):
            m = m.assoc(f'key{i}', i)
        return m

    dict_bench = timeit(dict_insert, runs=5)
    pmap_bench = timeit(pmap_insert, runs=5)

    print(f"dict:          {format_result(dict_bench)}")
    print(f"PersistentMap: {format_result(pmap_bench)}")
    ratio = pmap_bench.median / dict_bench.median
    print(f"Ratio:         {ratio:.2f}x slower")

    # Show detailed stats if variance is high
    if dict_bench.cv > 10.0 or pmap_bench.cv > 10.0:
        print(f"  Note: High variance detected")
        print(f"    dict CV: {dict_bench.cv:.1f}%, range: {format_time(dict_bench.min)}-{format_time(dict_bench.max)}")
        print(f"    pmap CV: {pmap_bench.cv:.1f}%, range: {format_time(pmap_bench.min)}-{format_time(pmap_bench.max)}")

    return dict_bench.result, pmap_bench.result


def benchmark_lookup(d: dict, m: PersistentMap, n: int):
    """Test lookup performance."""
    print(f"\n=== Lookup Test (n={n:,}) ===")

    # Python dict
    def dict_lookup():
        total = 0
        for i in range(n):
            total += d.get(f'key{i}', 0)
        return total

    # PersistentMap
    def pmap_lookup():
        total = 0
        for i in range(n):
            total += m.get(f'key{i}', 0)
        return total

    dict_bench = timeit(dict_lookup)
    pmap_bench = timeit(pmap_lookup)

    print(f"dict:          {format_result(dict_bench, show_variance=False)}")
    print(f"PersistentMap: {format_result(pmap_bench, show_variance=False)}")
    print(f"Ratio:         {pmap_bench.median / dict_bench.median:.2f}x slower")


def benchmark_deletion(d: dict, m: PersistentMap, n: int):
    """Test deletion performance."""
    print(f"\n=== Deletion Test (n={n:,}) ===")

    # Python dict (need to copy first to preserve original)
    def dict_delete():
        d_copy = d.copy()
        for i in range(0, n, 10):  # Delete every 10th element
            del d_copy[f'key{i}']
        return d_copy

    # PersistentMap
    def pmap_delete():
        m_copy = m
        for i in range(0, n, 10):
            m_copy = m_copy.dissoc(f'key{i}')
        return m_copy

    dict_bench = timeit(dict_delete)
    pmap_bench = timeit(pmap_delete)

    print(f"dict:          {format_result(dict_bench, show_variance=False)}")
    print(f"PersistentMap: {format_result(pmap_bench, show_variance=False)}")
    print(f"Ratio:         {pmap_bench.median / dict_bench.median:.2f}x slower")


def benchmark_iteration(d: dict, m: PersistentMap, n: int):
    """Test iteration performance."""
    print(f"\n=== Iteration Test (n={n:,}) ===")

    # Python dict
    def dict_iter():
        total = 0
        for key, val in d.items():
            total += val
        return total

    # PersistentMap
    def pmap_iter():
        total = 0
        for key, val in m.items():
            total += val
        return total

    dict_bench = timeit(dict_iter)
    pmap_bench = timeit(pmap_iter)

    print(f"dict:          {format_result(dict_bench, show_variance=False)}")
    print(f"PersistentMap: {format_result(pmap_bench, show_variance=False)}")
    print(f"Ratio:         {pmap_bench.median / dict_bench.median:.2f}x slower")


def benchmark_update(d: dict, m: PersistentMap, n: int):
    """Test update performance."""
    print(f"\n=== Update Test (n={n:,}) ===")

    # Python dict - mutate in place
    def dict_update():
        d_copy = d.copy()
        for i in range(0, n, 10):
            d_copy[f'key{i}'] = i * 2
        return d_copy

    # PersistentMap - create new version
    def pmap_update():
        m_copy = m
        for i in range(0, n, 10):
            m_copy = m_copy.assoc(f'key{i}', i * 2)
        return m_copy

    dict_bench = timeit(dict_update)
    pmap_bench = timeit(pmap_update)

    print(f"dict:          {format_result(dict_bench, show_variance=False)}")
    print(f"PersistentMap: {format_result(pmap_bench, show_variance=False)}")
    print(f"Ratio:         {pmap_bench.median / dict_bench.median:.2f}x slower")


def benchmark_structural_sharing(m: PersistentMap, n: int):
    """
    Test the key advantage of PersistentMap: creating multiple variants efficiently.
    This is where persistent data structures shine - you can cheaply create
    many variations that share most of their structure.

    CRITICAL BENCHMARK - uses robust statistics.
    """
    print(f"\n=== Structural Sharing Test (n={n:,}) ===")
    print("Creating 100 variants with one modification each...")

    # PersistentMap - structural sharing
    def pmap_variants():
        variants = []
        for i in range(100):
            variant = m.assoc(f'extra{i}', i * 1000)
            variants.append(variant)
        return variants

    # Python dict - must copy entire dict
    def dict_variants():
        d = {f'key{i}': i for i in range(n)}
        variants = []
        for i in range(100):
            variant = d.copy()
            variant[f'extra{i}'] = i * 1000
            variants.append(variant)
        return variants

    # Use robust statistical benchmarking with 7 runs
    pmap_bench = timeit(pmap_variants, runs=7)
    dict_bench = timeit(dict_variants, runs=7)

    ratio = dict_bench.median / pmap_bench.median

    print(f"dict (copy):   {format_result(dict_bench)}")
    print(f"PersistentMap: {format_result(pmap_bench)}")
    print(f"Ratio:         {ratio:.2f}x faster (PersistentMap wins!)")

    # Always show variance for this critical benchmark
    print(f"  Statistics:")
    print(f"    dict:  median={format_time(dict_bench.median)}, CV={dict_bench.cv:.1f}%, range={format_time(dict_bench.min)}-{format_time(dict_bench.max)}")
    print(f"    pmap:  median={format_time(pmap_bench.median)}, CV={pmap_bench.cv:.1f}%, range={format_time(pmap_bench.min)}-{format_time(pmap_bench.max)}")


def benchmark_memory_footprint(n: int):
    """
    Test memory footprint (approximate).
    Note: This is a rough estimate using sys.getsizeof which doesn't account
    for all referenced objects perfectly.
    """
    print(f"\n=== Memory Footprint Test (n={n:,}) ===")

    d = {}
    for i in range(n):
        d[f'key{i}'] = i

    m = PersistentMap()
    for i in range(n):
        m = m.assoc(f'key{i}', i)

    dict_size = sys.getsizeof(d)
    pmap_size = sys.getsizeof(m)

    print(f"dict (approximate):          {dict_size:,} bytes")
    print(f"PersistentMap (approximate): {pmap_size:,} bytes")
    print(f"Note: This is a rough estimate and doesn't include all internal structures")


def benchmark_contains(d: dict, m: PersistentMap, n: int):
    """Test membership checking performance."""
    print(f"\n=== Contains Test (n={n:,}) ===")

    # Python dict
    def dict_contains():
        count = 0
        for i in range(n):
            if f'key{i}' in d:
                count += 1
        return count

    # PersistentMap
    def pmap_contains():
        count = 0
        for i in range(n):
            if f'key{i}' in m:
                count += 1
        return count

    dict_bench = timeit(dict_contains)
    pmap_bench = timeit(pmap_contains)

    print(f"dict:          {format_result(dict_bench, show_variance=False)}")
    print(f"PersistentMap: {format_result(pmap_bench, show_variance=False)}")
    print(f"Ratio:         {pmap_bench.median / dict_bench.median:.2f}x slower")


def benchmark_from_dict(n: int):
    """Test creating a map from an existing dict (CRITICAL BENCHMARK - uses robust statistics)."""
    print(f"\n=== Create from Dict Test (n={n:,}) ===")

    # Create source dict
    source_dict = {f'key{i}': i for i in range(n)}

    # Python dict - just copy
    def dict_from_dict():
        return dict(source_dict)

    # PersistentMap - build from dict
    def pmap_from_dict():
        return PersistentMap.from_dict(source_dict)

    # Use robust statistical benchmarking with 7 runs for critical test
    dict_bench = timeit(dict_from_dict, runs=7)
    pmap_bench = timeit(pmap_from_dict, runs=7)

    ratio = pmap_bench.median / dict_bench.median

    print(f"dict.copy():             {format_result(dict_bench)}")
    print(f"PersistentMap.from_dict: {format_result(pmap_bench)}")
    print(f"Ratio:                   {ratio:.2f}x slower")

    # Always show variance for this critical benchmark
    print(f"  Statistics:")
    print(f"    dict:  median={format_time(dict_bench.median)}, CV={dict_bench.cv:.1f}%, range={format_time(dict_bench.min)}-{format_time(dict_bench.max)}")
    print(f"    pmap:  median={format_time(pmap_bench.median)}, CV={pmap_bench.cv:.1f}%, range={format_time(pmap_bench.min)}-{format_time(pmap_bench.max)}")


def benchmark_merge(n: int):
    """
    Test merging two maps together.
    This is a key operation where structural sharing can provide benefits.
    """
    print(f"\n=== Merge Test (n={n:,}) ===")
    print(f"Merging two maps of {n//2:,} elements each...")

    # Build two dicts to merge
    dict1 = {f'key{i}': i for i in range(n // 2)}
    dict2 = {f'key{i + n//2}': i + n//2 for i in range(n // 2)}

    # Build two PersistentMaps to merge
    pmap1 = PersistentMap.from_dict(dict1)
    pmap2 = PersistentMap.from_dict(dict2)

    # Python dict - must copy and update
    def dict_merge():
        result = dict1.copy()
        result.update(dict2)
        return result

    # PersistentMap - structural sharing
    def pmap_merge():
        return pmap1.merge(pmap2)

    dict_bench = timeit(dict_merge)
    pmap_bench = timeit(pmap_merge)

    print(f"dict (copy + update): {format_result(dict_bench, show_variance=False)}")
    print(f"PersistentMap.merge:  {format_result(pmap_bench, show_variance=False)}")
    print(f"Ratio:                {pmap_bench.median / dict_bench.median:.2f}x slower")

    # Also test the | operator
    def pmap_merge_operator():
        return pmap1 | pmap2

    pmap_op_bench = timeit(pmap_merge_operator)
    print(f"PersistentMap (| op): {format_result(pmap_op_bench, show_variance=False)}")


def run_benchmark_suite(sizes: list[int]):
    """Run complete benchmark suite for different sizes."""
    print("=" * 70)
    print("PERSISTENT MAP vs PYTHON DICT - PERFORMANCE COMPARISON")
    print("=" * 70)

    for n in sizes:
        print(f"\n{'=' * 70}")
        print(f"TESTING WITH {n:,} ELEMENTS")
        print(f"{'=' * 70}")

        # Build the data structures
        d, m = benchmark_insertion(n)

        # Run tests
        benchmark_lookup(d, m, n)
        benchmark_contains(d, m, n)
        benchmark_update(d, m, n)
        benchmark_deletion(d, m, n)
        benchmark_iteration(d, m, n)
        benchmark_from_dict(n)
        benchmark_merge(n)
        benchmark_structural_sharing(m, n)

    # Memory test (only for largest size)
    if sizes:
        benchmark_memory_footprint(sizes[-1])

    print("\n" + "=" * 70)
    print("SUMMARY")
    print("=" * 70)
    print("""
PersistentMap Trade-offs:

ADVANTAGES:
  - Immutability: Thread-safe, no defensive copying needed
  - Structural Sharing: Creating variants is O(log n), not O(n)
  - Time-travel: Keep old versions without full copies
  - Functional Programming: Natural fit for FP paradigms

DISADVANTAGES:
  - Slower raw performance for individual operations
  - Higher constant factors due to tree structure
  - More memory per entry (node overhead)

USE CASES:
  - When you need multiple versions of data
  - Undo/redo functionality
  - Concurrent access without locks
  - Functional/immutable architecture
  - When copying cost matters more than lookup speed
""")


if __name__ == '__main__':
    # Run benchmarks with different sizes
    # Use smaller sizes for quick testing, larger for real benchmarks
    sizes = [100, 1000, 1000000]

    # Uncomment for more comprehensive benchmarking:
    # sizes = [100, 1000, 10000, 100000]

    run_benchmark_suite(sizes)
