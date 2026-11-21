/* Unit tests plus model-based random testing: each structure runs a long random operation
 * sequence side by side with a deliberately simple reference model, and the invariant
 * check runs as it goes. Any difference prints the seed and step to reproduce it. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ds.h"

static int checks, failures;
#define CHECK(c, ...)                                                                  \
    do {                                                                               \
        checks++;                                                                      \
        if (!(c)) { failures++; printf("FAIL %s:%d: ", __func__, __LINE__); printf(__VA_ARGS__); printf("\n"); } \
    } while (0)

static unsigned long long rng = 88172645463325252ull;
static unsigned rnd(void) { rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17; return (unsigned)(rng >> 11); }

static int cmp_int(const void *a, const void *b) {
    const int x = *(const int *)a, y = *(const int *)b;
    return (x > y) - (x < y);
}

/* ------------------------------------------------------------------ vec */
static void test_vec(void) {
    vec v;
    vec_init(&v, sizeof(int));
    for (int i = 0; i < 1000; i++) vec_push(&v, &i);
    CHECK(v.len == 1000 && v.cap == 1024 && vec_check(&v), "len %zu cap %zu", v.len, v.cap);
    CHECK(*(int *)vec_at(&v, 999) == 999 && vec_at(&v, 1000) == NULL, "bounds");
    int x;
    CHECK(vec_pop(&v, &x) && x == 999 && v.len == 999, "pop");
    CHECK(!vec_reserve(&v, SIZE_MAX / 2), "huge reserve fails cleanly instead of overflowing");
    CHECK(v.len == 999 && *(int *)vec_at(&v, 0) == 0, "vector intact after failed reserve");
    vec_free(&v);
    CHECK(!vec_pop(&v, &x) && vec_check(&v), "pop on empty");
}

/* ------------------------------------------------------------------ ring */
static void test_ring_basics(void) {
    ring r;
    CHECK(!ring_init(&r, 6, 4), "capacity must be a power of two");
    CHECK(ring_init(&r, 4, sizeof(int)), "init");
    for (int i = 0; i < 6; i++) ring_push(&r, &i);
    CHECK(ring_len(&r) == 4 && r.dropped == 2, "full ring drops: len %zu dropped %zu", ring_len(&r), r.dropped);
    int x;
    ring_pop(&r, &x);
    CHECK(x == 0, "FIFO order, oldest kept: %d", x);
    ring_free(&r);
}

static void test_ring_vs_model(void) {
    ring r;
    ring_init(&r, 64, sizeof(unsigned));
    unsigned model[64], head = 0, tail = 0;  /* model: plain array queue */
    unsigned next = 0;
    for (int step = 0; step < 200000; step++) {
        if (rnd() % 3) {
            const bool ok = ring_push(&r, &next);
            const bool model_ok = head - tail < 64;
            if (model_ok) model[head++ % 64] = next;
            if (ok != model_ok) { CHECK(0, "push disagreement at step %d", step); break; }
            next++;
        } else {
            unsigned got = 0;
            const bool ok = ring_pop(&r, &got);
            const bool model_ok = head != tail;
            const unsigned want = model_ok ? model[tail++ % 64] : 0;
            if (ok != model_ok || got != want) { CHECK(0, "pop disagreement at step %d", step); break; }
        }
        if (!ring_check(&r)) { CHECK(0, "invariant broken at step %d", step); break; }
    }
    CHECK(ring_len(&r) == head - tail, "final length");
    ring_free(&r);
}

static void test_ring_index_wraparound(void) {
    /* head/tail are free-running counters: start them near SIZE_MAX and cross the wrap */
    ring r;
    ring_init(&r, 8, sizeof(int));
    r.head = r.tail = SIZE_MAX - 3;
    for (int i = 0; i < 8; i++) ring_push(&r, &i);
    int x, ok = 1;
    for (int i = 0; i < 8; i++) ok &= ring_pop(&r, &x) && x == i;
    CHECK(ok && ring_len(&r) == 0, "counters wrapped cleanly");
    ring_free(&r);
}

/* ------------------------------------------------------------------ heap */
static void test_heap_sorts(void) {
    heap h;
    heap_init(&h, sizeof(int), cmp_int);
    enum { N = 5000 };
    static int in[N], out[N];
    for (int i = 0; i < N; i++) { in[i] = (int)(rnd() % 1000) - 500; heap_push(&h, &in[i]); }
    CHECK(heap_check(&h) && heap_len(&h) == N, "heap property after pushes");
    for (int i = 0; i < N; i++) heap_pop(&h, &out[i]);
    qsort(in, N, sizeof(int), cmp_int);
    CHECK(!memcmp(in, out, sizeof in), "pop order equals qsort order (duplicates included)");
    CHECK(!heap_pop(&h, NULL) && heap_peek(&h) == NULL, "empty heap");
    heap_free(&h);
}

static void test_heap_interleaved(void) {
    heap h;
    heap_init(&h, sizeof(int), cmp_int);
    int model[4096], n = 0;
    for (int step = 0; step < 50000; step++) {
        if (n < 4096 && (n == 0 || rnd() % 2)) {
            const int v = (int)(rnd() % 100);
            heap_push(&h, &v);
            model[n++] = v;
        } else {
            int mi = 0, got;
            for (int i = 1; i < n; i++) if (model[i] < model[mi]) mi = i;
            heap_pop(&h, &got);
            if (got != model[mi]) { CHECK(0, "step %d: got %d want %d", step, got, model[mi]); break; }
            model[mi] = model[--n];
        }
        if (step % 97 == 0 && !heap_check(&h)) { CHECK(0, "heap property broken at step %d", step); break; }
    }
    heap_free(&h);
}

/* ------------------------------------------------------------------ map */
static void test_map_basics(void) {
    map m;
    map_init(&m, 4);
    CHECK(map_put(&m, "fryer", 1) && map_put(&m, "arm", 2) && map_put(&m, "fryer", 3), "puts");
    int64_t v;
    CHECK(map_get(&m, "fryer", &v) && v == 3 && map_len(&m) == 2, "overwrite keeps one entry");
    CHECK(!map_get(&m, "griddle", &v), "missing key");
    CHECK(map_put(&m, "", 9) && map_get(&m, "", &v) && v == 9, "empty string is a valid key");
    CHECK(map_del(&m, "arm") && !map_get(&m, "arm", &v) && !map_del(&m, "arm"), "delete once");
    char key[32];
    for (int i = 0; i < 10000; i++) { snprintf(key, sizeof key, "k%d", i); map_put(&m, key, i); }
    CHECK(map_check(&m) && map_len(&m) == 10002, "after growth: len %zu cap %zu", map_len(&m), m.cap);
    int ok = 1;
    for (int i = 0; i < 10000; i++) { snprintf(key, sizeof key, "k%d", i); ok &= map_get(&m, key, &v) && v == i; }
    CHECK(ok, "every key found after rehashing");
    map_free(&m);
}

static void test_map_load_factor_bound(void) {
    /* the map must grow before it passes 85% full: probe lengths explode near 100% */
    map m;
    map_init(&m, 8);
    char key[16];
    int ok = 1;
    for (int i = 0; i < 3000 && ok; i++) {
        snprintf(key, sizeof key, "n%d", i);
        map_put(&m, key, i);
        ok = m.len * 100 <= m.cap * 85;
    }
    CHECK(ok, "load factor exceeded 85%%: len %zu cap %zu", m.len, m.cap);
    CHECK(m.max_probe < 32, "probe length stays short: %zu", m.max_probe);
    map_free(&m);
}

static void test_map_vs_model(void) {
    enum { KEYS = 600 };
    map m;
    map_init(&m, 8);
    int64_t model[KEYS];
    bool present[KEYS] = {false};
    char key[24];
    for (int step = 0; step < 300000; step++) {
        const int k = (int)(rnd() % KEYS), op = (int)(rnd() % 10);
        snprintf(key, sizeof key, "key-%d", k);
        if (op < 5) {
            const int64_t val = (int64_t)rnd();
            map_put(&m, key, val);
            model[k] = val;
            present[k] = true;
        } else if (op < 8) {
            const bool ok = map_del(&m, key);
            if (ok != present[k]) { CHECK(0, "del disagreement step %d key %d", step, k); break; }
            present[k] = false;
        } else {
            int64_t v = 0;
            const bool ok = map_get(&m, key, &v);
            if (ok != present[k] || (ok && v != model[k])) { CHECK(0, "get disagreement step %d key %d", step, k); break; }
        }
        if (step % 1000 == 0 && !map_check(&m)) { CHECK(0, "map invariant broken at step %d", step); break; }
    }
    size_t n = 0;
    for (int k = 0; k < KEYS; k++) n += present[k];
    CHECK(map_len(&m) == n && map_check(&m), "final size %zu vs model %zu", map_len(&m), n);
    map_free(&m);
}

/* ------------------------------------------------------------------ lru */
static void test_lru_order(void) {
    lru c;
    lru_init(&c, 3);
    lru_put(&c, "a", 1); lru_put(&c, "b", 2); lru_put(&c, "c", 3);
    int64_t v;
    lru_get(&c, "a", &v);                /* a is now most recent: b is the oldest */
    lru_put(&c, "d", 4);                 /* evicts b */
    CHECK(!lru_get(&c, "b", &v) && lru_get(&c, "a", &v) && lru_get(&c, "c", &v) && lru_get(&c, "d", &v), "evicted b");
    CHECK(c.evictions == 1 && c.len == 3 && lru_check(&c), "bookkeeping");
    lru_put(&c, "c", 30);                /* update does not evict */
    CHECK(lru_get(&c, "c", &v) && v == 30 && c.evictions == 1, "update in place");
    lru_free(&c);
}

static void test_lru_vs_model(void) {
    enum { CAP = 16, KEYS = 40 };
    lru c;
    lru_init(&c, CAP);
    long stamp[KEYS];                    /* model: last-use time per key, -1 = absent */
    int64_t val[KEYS];
    for (int k = 0; k < KEYS; k++) stamp[k] = -1;
    char key[16];
    for (long t = 0; t < 100000; t++) {
        const int k = (int)(rnd() % KEYS);
        snprintf(key, sizeof key, "%d", k);
        if (rnd() % 2) {
            int64_t v;
            const bool hit = lru_get(&c, key, &v);
            if (hit != (stamp[k] >= 0) || (hit && v != val[k])) { CHECK(0, "get mismatch t=%ld key %d", t, k); break; }
            if (hit) stamp[k] = t;
        } else {
            lru_put(&c, key, t);
            if (stamp[k] < 0) {
                int live = 0, oldest = -1;
                for (int i = 0; i < KEYS; i++)
                    if (stamp[i] >= 0) { live++; if (oldest < 0 || stamp[i] < stamp[oldest]) oldest = i; }
                if (live == CAP) stamp[oldest] = -1;          /* model evicts least recently used */
            }
            stamp[k] = t;
            val[k] = t;
        }
        if (t % 500 == 0 && !lru_check(&c)) { CHECK(0, "lru invariant broken t=%ld", t); break; }
    }
    lru_free(&c);
}

int main(void) {
    test_vec();
    test_ring_basics();
    test_ring_vs_model();
    test_ring_index_wraparound();
    test_heap_sorts();
    test_heap_interleaved();
    test_map_basics();
    test_map_load_factor_bound();
    test_map_vs_model();
    test_lru_order();
    test_lru_vs_model();
    printf("%d checks, %d failures\n", checks, failures);
    return failures != 0;
}
