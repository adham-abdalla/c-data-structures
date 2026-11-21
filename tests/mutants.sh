#!/usr/bin/env bash
# Mutation check: plant small bugs in the library one at a time and make sure the test
# suite fails for each. A mutant that survives means a test is missing.
set -uo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

mutants=(
  "map_lru.c|m->slots[i].dist--;|/* removed */|backward-shift delete forgets to fix probe distance"
  "map_lru.c|if (cur->dist < s.dist) {|if (0) {|insert never steals (plain linear probing)"
  "map_lru.c|(m->len + 1) * 100 > m->cap * 85|(m->len + 1) * 100 > m->cap * 99|map grows too late"
  "map_lru.c|if (++c->len > c->cap)|if (++c->len > c->cap + 1)|LRU evicts one entry too late"
  "map_lru.c|unlink_node(c, n);\n    push_front(c, n);\n    if (out)|if (out)|LRU get does not refresh recency"
  "vec_ring_heap.c|smallest = r;|smallest = l;|heap sift-down picks the wrong child"
  "vec_ring_heap.c|if (ring_len(r) == r->cap) {|if (ring_len(r) > r->cap) {|ring accepts a push when full"
  "vec_ring_heap.c|v->cap ? v->cap * 2 : 8|v->cap + 1|vector grows by one element (quadratic copying)"
)

survivors=0
for m in "${mutants[@]}"; do
  IFS='|' read -r file pattern replacement why <<<"$m"
  cp "$root"/src/*.c "$work/"
  python3 - "$work/$file" "$pattern" "$replacement" <<'PY'
import sys
path, pat, rep = sys.argv[1:4]
pat = pat.replace("\\n", "\n")          # literal match; "\n" in the table means a newline
src = open(path).read()
if src.count(pat) != 1:
    sys.exit(f"pattern must occur exactly once in {path}: {pat!r}")
open(path, "w").write(src.replace(pat, rep))
PY
  [[ $? -eq 0 ]] || { echo "ERROR: could not apply mutant: $why"; exit 2; }
  if ! gcc -std=c11 -O1 -I"$root/include" "$work"/*.c "$root/tests/test_ds.c" -o "$work/t" 2>/dev/null; then
    echo "SKIP (does not compile): $why"; continue
  fi
  if timeout 60 "$work/t" >/dev/null; then
    echo "SURVIVED: $why"; survivors=$((survivors + 1))
  else
    echo "killed:   $why"
  fi
done
echo "${#mutants[@]} mutants, $survivors survived"
[[ $survivors -eq 0 ]]
