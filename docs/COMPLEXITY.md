# Complexity and measured cost

| Structure | Operation | Time | Space | Notes |
|---|---|---|---|---|
| `vec` | push | amortized O(1), worst O(n) | O(n) | capacity doubles, so n pushes copy fewer than 2n elements in total |
| | pop, index | O(1) | | `vec_at` returns NULL out of range instead of reading past the end |
| `ring` | push, pop | O(1) | O(capacity) fixed | power-of-two capacity: index is `counter & (cap - 1)`, no division; free-running counters handle wraparound; when full it **drops the newest** and counts it |
| `heap` | push, pop | O(log n) | O(n) | array-backed binary heap; peek O(1) |
| `map` | put, get, del | O(1) average, O(n) worst | O(n) | open addressing with Robin Hood probing; grows at 85% load; backward-shift delete (no tombstones, so lookups don't slow down after many deletes) |
| `lru` | get, put | O(1) average | O(capacity) | map from key to node + doubly linked recency list; eviction is O(1) from the tail |

## Measured (bench/bench.c, gcc -O3, Intel Xeon @ 2.10 GHz)

**Lookup**, hash map vs scanning an array of strings (ns per lookup):

| N keys | map_get | linear scan | ratio |
|---|---|---|---|
| 10 | 9 | 20 | 2x |
| 100 | 12 | 203 | 16x |
| 1,000 | 15 | 1,913 | 124x |
| 10,000 | 39 | 19,391 | 500x |
| 100,000 | 76 | 184,812 | 2,446x |

map_get grows slowly with N only because the table stops fitting in cache; the linear scan grows exactly with N.

**Priority queue**, binary heap vs keeping a sorted array (ns per push + pop):

| N | heap | sorted array | ratio |
|---|---|---|---|
| 100 | 106 | 32 | **0.3x** |
| 1,000 | 107 | 188 | 1.7x |
| 10,000 | 136 | 2,184 | 16x |
| 100,000 | 224 | 19,813 | 88x |

For 100 items the sorted array wins: shifting a few contiguous ints is cheaper than the heap's compare-function calls. Big-O tells you what happens as N grows, not which is faster at small N.

**Robin Hood probing**, longest probe sequence just before each resize (table 85% full):

| keys | capacity | max probe |
|---|---|---|
| 870 | 1,024 | 7 |
| 13,926 | 16,384 | 14 |
| 222,822 | 262,144 | 24 |
| 891,289 | 1,048,576 | 30 |

The worst case grows roughly with log n even at 85% load; that is the point of Robin Hood (plain linear probing has much longer tails at the same load).

**vec_push**: 5.0 ns per push over 10 million pushes, 22 reallocations in total.
