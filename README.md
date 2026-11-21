# c-data-structures

[![CI](https://github.com/adham-abdalla/c-data-structures/actions/workflows/ci.yml/badge.svg)](https://github.com/adham-abdalla/c-data-structures/actions/workflows/ci.yml)
![C11](https://img.shields.io/badge/C-C11-00599C)
![Coverage](https://img.shields.io/badge/line%20coverage-100%25-2e7d4f)
![Platforms](https://img.shields.io/badge/Linux%20%7C%20Windows-tested-555)

Five core data structures in C11, written the way production C has to be: no crash on allocation failure, invariant checks for every structure, and a test suite that is itself tested.

| Structure | What it is | Big-O |
|---|---|---|
| `vec` | growable array | push amortized O(1) |
| `ring` | fixed-size FIFO (power-of-two, free-running counters, drop-newest when full, drop counter) | O(1) |
| `heap` | binary min-heap with a compare function | O(log n) |
| `map` | string → int64 hash map, **Robin Hood** open addressing, backward-shift delete (no tombstones) | O(1) average |
| `lru` | least-recently-used cache: map + intrusive doubly linked list | O(1) average |

Full complexity table with measured numbers: [docs/COMPLEXITY.md](docs/COMPLEXITY.md).

## How it is tested

| Technique | What it does |
|---|---|
| Unit tests | edges: empty structures, full ring, overwrite, empty-string key, 10,000-key rehash, failed huge allocation leaves the vector intact, ring counters crossing `SIZE_MAX` |
| **Model-based random testing** | each structure runs 50k–300k random operations side by side with a deliberately simple reference model (array queue, linear-scan map, timestamp LRU) and must agree at every step; invariant checks (`*_check()`) run along the way |
| **Mutation check** (`tests/mutants.sh`) | plants 8 realistic bugs one at a time (forgotten probe-distance fix, heap picks wrong child, LRU doesn't refresh on get, ring accepts a push when full, ...). CI fails if any mutant survives. It found one gap during development: nothing checked the load-factor limit, so a test was added |
| **Fuzzing** | libFuzzer target drives map + LRU with arbitrary operation scripts for 60 s in CI under ASan + UBSan |
| Sanitizers + Valgrind | gcc and clang with AddressSanitizer + UBSan; a separate Valgrind job with full leak checking |
| Coverage | 100% line coverage of `src/` (gcov), printed in every CI run |
| Windows | builds and runs the suite with MinGW gcc on `windows-latest` |

## Example: top talkers

`examples/top_talkers.c` reads `radio_id,bytes` lines and prints the K radios that sent the most, using `map` to aggregate and a size-K `heap` to keep the leaders (O(n log K)). Malformed lines are counted, not fatal.

```
$ ./build/top_talkers 3 < tests/traffic.csv
6 lines, 4 radios, 3 malformed
 1. r3           500 bytes
 2. r2           140 bytes
 3. r5           140 bytes
```

## Build

```bash
make test        # unit + model-based tests and the example, add SANITIZE="-fsanitize=address,undefined"
make mutants     # mutation check
make coverage    # gcov line coverage
make fuzz        # libFuzzer (clang), 60 s
make bench       # numbers in docs/COMPLEXITY.md
```

## Design notes

- **Errors are returned, never aborted on.** `vec_reserve` checks `cap * elem` for overflow and leaves the old buffer valid when `realloc` fails; `map` is unchanged if growing fails.
- **Robin Hood + backward shift.** On insert, an element that is further from its home slot takes the slot from one that is closer, which keeps the longest probe short (30 at 85% load with a million keys). On delete, following elements shift back one slot, so there are no tombstones to slow later lookups.
- **The ring drops the newest record when full** and counts it. For a diagnostic log on a device, the records leading up to a problem are the ones worth keeping.
- **Invariant functions** (`map_check`: every key is at `home + dist`, no gaps in a probe run, load ≤ 85%; `heap_check`; `lru_check`: list and index agree) make the random tests catch logic errors, not just crashes.

## License

MIT
