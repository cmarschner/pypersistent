"""
Performance comparison between PersistentMap and Python's built-in dict.

This test suite measures various operations to understand the trade-offs
between persistent immutable data structures and mutable dictionaries.
"""

import time
import sys
from typing import Callable, Any
from persistent_map_cpp import PersistentMap


def timeit(func: Callable, iterations: int = 1) -> tuple[Any, float]:
    """Time a function execution."""
    start = time.perf_counter()
    result = None
    for _ in range(iterations):
        result = func()
    end = time.perf_counter()
    return result, (end - start) / iterations


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

    dict_result, dict_time = timeit(dict_insert)
    pmap_result, pmap_time = timeit(pmap_insert)

    print(f"dict:          {format_time(dict_time)}")
    print(f"PersistentMap: {format_time(pmap_time)}")
    print(f"Ratio:         {pmap_time / dict_time:.2f}x slower")

    return dict_result, pmap_result


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

    _, dict_time = timeit(dict_lookup)
    _, pmap_time = timeit(pmap_lookup)

    print(f"dict:          {format_time(dict_time)}")
    print(f"PersistentMap: {format_time(pmap_time)}")
    print(f"Ratio:         {pmap_time / dict_time:.2f}x slower")


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

    _, dict_time = timeit(dict_delete)
    _, pmap_time = timeit(pmap_delete)

    print(f"dict:          {format_time(dict_time)}")
    print(f"PersistentMap: {format_time(pmap_time)}")
    print(f"Ratio:         {pmap_time / dict_time:.2f}x slower")


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

    _, dict_time = timeit(dict_iter)
    _, pmap_time = timeit(pmap_iter)

    print(f"dict:          {format_time(dict_time)}")
    print(f"PersistentMap: {format_time(pmap_time)}")
    print(f"Ratio:         {pmap_time / dict_time:.2f}x slower")


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

    _, dict_time = timeit(dict_update)
    _, pmap_time = timeit(pmap_update)

    print(f"dict:          {format_time(dict_time)}")
    print(f"PersistentMap: {format_time(pmap_time)}")
    print(f"Ratio:         {pmap_time / dict_time:.2f}x slower")


def benchmark_structural_sharing(m: PersistentMap, n: int):
    """
    Test the key advantage of PersistentMap: creating multiple variants efficiently.
    This is where persistent data structures shine - you can cheaply create
    many variations that share most of their structure.
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

    _, pmap_time = timeit(pmap_variants)
    _, dict_time = timeit(dict_variants)

    print(f"dict (copy):   {format_time(dict_time)}")
    print(f"PersistentMap: {format_time(pmap_time)}")
    print(f"Ratio:         {dict_time / pmap_time:.2f}x faster (PersistentMap wins!)")


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

    _, dict_time = timeit(dict_contains)
    _, pmap_time = timeit(pmap_contains)

    print(f"dict:          {format_time(dict_time)}")
    print(f"PersistentMap: {format_time(pmap_time)}")
    print(f"Ratio:         {pmap_time / dict_time:.2f}x slower")


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
