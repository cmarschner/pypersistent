"""
Quick benchmark: Test fast iteration optimization (items_list)

Compares:
1. pypersistent items() iterator (SLOW - 2.9x slower than pyrsistent)
2. pypersistent items_list() (NEW - expected 3-4x faster than iterator)
3. pyrsistent items() iterator (pure Python baseline)
"""

import time
import sys
import statistics

sys.path.insert(0, 'src')
from pypersistent import PersistentDict
from pyrsistent import pmap


def timeit(func, runs=5, warmup=1):
    """Time a function with multiple runs."""
    # Warmup
    for _ in range(warmup):
        func()

    # Measured runs
    times = []
    for _ in range(runs):
        start = time.perf_counter()
        func()
        times.append(time.perf_counter() - start)

    return statistics.median(times)


def format_time(seconds):
    """Format time in appropriate units."""
    if seconds < 1e-3:
        return f"{seconds * 1e6:.2f} µs"
    elif seconds < 1:
        return f"{seconds * 1e3:.2f} ms"
    else:
        return f"{seconds:.2f} s"


def benchmark_iteration(n):
    """Compare iteration methods at given size."""
    print(f"\n{'=' * 70}")
    print(f"ITERATION BENCHMARK - {n:,} elements")
    print('=' * 70)

    # Create test data
    data = {i: i * 2 for i in range(n)}
    our_map = PersistentDict.from_dict(data)
    pyr_map = pmap(data)

    # Test 1: pypersistent items() iterator (current, slow)
    def our_iterate_slow():
        total = 0
        for k, v in our_map.items():
            total += k + v
        return total

    # Test 2: pypersistent items_list() (NEW - fast)
    def our_iterate_fast():
        total = 0
        for k, v in our_map.items_list():
            total += k + v
        return total

    # Test 3: pyrsistent items() (pure Python baseline)
    def pyr_iterate():
        total = 0
        for k, v in pyr_map.items():
            total += k + v
        return total

    # Run benchmarks
    print("\nRunning benchmarks (5 runs each, median reported)...\n")

    time_slow = timeit(our_iterate_slow, runs=5)
    time_fast = timeit(our_iterate_fast, runs=5)
    time_pyr = timeit(pyr_iterate, runs=5)

    # Results
    print(f"pypersistent items() iterator:  {format_time(time_slow)}")
    print(f"pypersistent items_list() [NEW]: {format_time(time_fast)}")
    print(f"pyrsistent items() iterator:    {format_time(time_pyr)}")

    print("\n" + "=" * 70)
    print("SPEEDUP ANALYSIS")
    print("=" * 70)

    speedup_vs_iter = time_slow / time_fast
    speedup_vs_pyr = time_pyr / time_fast

    print(f"\n✅ items_list() vs items() iterator:  {speedup_vs_iter:.2f}x FASTER")

    if speedup_vs_pyr > 1:
        print(f"✅ items_list() vs pyrsistent:        {speedup_vs_pyr:.2f}x FASTER")
    else:
        print(f"⚠️  items_list() vs pyrsistent:        {1/speedup_vs_pyr:.2f}x slower")

    print(f"\n📊 Improvement from iterator optimization: {speedup_vs_iter:.1f}x speedup")

    return {
        'n': n,
        'iter_slow': time_slow,
        'iter_fast': time_fast,
        'pyrsistent': time_pyr,
        'speedup_vs_iter': speedup_vs_iter,
        'speedup_vs_pyr': speedup_vs_pyr
    }


def main():
    print("=" * 70)
    print("FAST ITERATION OPTIMIZATION TEST")
    print("=" * 70)
    print("\nTesting new items_list() method that eliminates pybind11 overhead")
    print("by materializing all items in C++ and returning a list.\n")

    # Test at multiple sizes
    sizes = [1_000, 10_000, 100_000, 1_000_000]
    results = []

    for size in sizes:
        result = benchmark_iteration(size)
        results.append(result)

    # Summary
    print("\n" + "=" * 70)
    print("SUMMARY TABLE")
    print("=" * 70)
    print(f"\n{'Size':<12} {'Iterator':<12} {'items_list()':<12} {'pyrsistent':<12} {'Speedup vs iter':<18} {'Speedup vs pyr':<18}")
    print("-" * 100)

    for r in results:
        print(f"{r['n']:<12,} {format_time(r['iter_slow']):<12} {format_time(r['iter_fast']):<12} "
              f"{format_time(r['pyrsistent']):<12} {r['speedup_vs_iter']:<18.2f}x {r['speedup_vs_pyr']:<18.2f}x")

    print("\n" + "=" * 70)
    print("KEY FINDINGS")
    print("=" * 70)

    avg_speedup_iter = sum(r['speedup_vs_iter'] for r in results) / len(results)
    avg_speedup_pyr = sum(r['speedup_vs_pyr'] for r in results) / len(results)

    print(f"\n✅ Average speedup vs iterator: {avg_speedup_iter:.2f}x")

    if avg_speedup_pyr > 1:
        print(f"✅ Average speedup vs pyrsistent: {avg_speedup_pyr:.2f}x")
        print(f"\n🎯 items_list() is now FASTER than pyrsistent's pure Python iterator!")
    else:
        print(f"⚠️  Average slowdown vs pyrsistent: {1/avg_speedup_pyr:.2f}x")
        print(f"\n🎯 items_list() is {avg_speedup_iter:.1f}x faster than our iterator, but still slower than pyrsistent.")

    print("\n" + "=" * 70)
    print("RECOMMENDATION")
    print("=" * 70)
    print("\nUse items_list() when:")
    print("  - You need to iterate over all items")
    print("  - You're iterating multiple times")
    print("  - Performance is critical")
    print("\nUse items() iterator when:")
    print("  - You need lazy evaluation (early exit possible)")
    print("  - Memory is constrained (streaming iteration)")
    print()


if __name__ == "__main__":
    main()
