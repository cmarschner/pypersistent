"""
Minimal test case demonstrating segfault bug.

The crash occurs when:
1. Create PersistentMap pm1 from dict d
2. Do dict merge and sorted: sorted((d | d2).items())
3. Create PersistentMap pm2 from dict d2
4. Do PM merge and sorted: sorted((pm1 | pm2).items())

The crash happens on line 4 - likely a Python object lifecycle/refcounting issue
where the dict merge affects objects stored in the PersistentMaps.

Workarounds:
- Use list(pm.items()) instead of pm.items() directly in sorted()
- Don't mix dict operations with the same dicts used to create PersistentMaps

This bug exists in the baseline code before any optimizations were added.
"""

from pypersistent import PersistentMap

d = {x: x for x in range(10000)}
d2 = {y: y for y in range(5000, 15000, 1)}
pm1 = PersistentMap.from_dict(d)
a = sorted((d | d2).items())
pm2 = PersistentMap.from_dict(d2)
b = sorted((pm1 | pm2).items())  # CRASH HERE

print(f"Equal: {a == b}")
