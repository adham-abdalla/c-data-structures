/* ds: small, tested C data structures. Every allocation failure is reported to the caller
 * (no abort inside the library), and every structure has a *_check() invariant function
 * the tests call after each operation. */
#ifndef DS_H
#define DS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ---- vec: growable array of fixed-size elements ---------------------------------- */
typedef struct {
    unsigned char *data;
    size_t len, cap, elem;
} vec;

void vec_init(vec *v, size_t elem_size);
void vec_free(vec *v);
bool vec_push(vec *v, const void *elem);          /* amortized O(1), capacity doubles */
bool vec_pop(vec *v, void *out);                  /* O(1) */
void *vec_at(const vec *v, size_t i);             /* NULL when out of range */
bool vec_reserve(vec *v, size_t cap);
bool vec_check(const vec *v);

/* ---- ring: fixed-capacity FIFO of bytes-sized records, capacity a power of two ----- */
typedef struct {
    unsigned char *buf;
    size_t cap, elem, head, tail;                 /* head/tail grow forever; index = x & (cap-1) */
    size_t dropped;                               /* pushes refused because the ring was full */
} ring;

bool ring_init(ring *r, size_t capacity_pow2, size_t elem_size);
void ring_free(ring *r);
bool ring_push(ring *r, const void *elem);        /* false (and dropped++) when full */
bool ring_pop(ring *r, void *out);                /* false when empty */
size_t ring_len(const ring *r);
bool ring_check(const ring *r);

/* ---- heap: binary min-heap ordered by a compare function -------------------------- */
typedef int (*ds_cmp)(const void *a, const void *b);
typedef struct {
    vec items;
    ds_cmp cmp;
} heap;

void heap_init(heap *h, size_t elem_size, ds_cmp cmp);
void heap_free(heap *h);
bool heap_push(heap *h, const void *elem);        /* O(log n) */
bool heap_pop(heap *h, void *out);                /* O(log n), smallest first */
const void *heap_peek(const heap *h);
size_t heap_len(const heap *h);
bool heap_check(const heap *h);

/* ---- map: string -> int64 hash map, open addressing with Robin Hood probing -------- */
typedef struct {
    char *key;          /* owned copy; NULL = empty slot */
    int64_t value;
    uint32_t hash;
    uint32_t dist;      /* probe distance from the home slot */
} map_slot;

typedef struct {
    map_slot *slots;
    size_t cap, len;
    size_t max_probe;   /* longest probe sequence seen, for the benchmark */
} map;

bool map_init(map *m, size_t initial_cap);
void map_free(map *m);
bool map_put(map *m, const char *key, int64_t value);   /* insert or overwrite; grows at 85% load */
bool map_get(const map *m, const char *key, int64_t *out);
bool map_del(map *m, const char *key);                  /* backward-shift delete: no tombstones */
size_t map_len(const map *m);
bool map_check(const map *m);
uint32_t ds_hash(const char *s);                        /* FNV-1a 32-bit */

/* ---- lru: fixed-capacity least-recently-used cache, map + intrusive list ---------- */
typedef struct lru_node {
    char *key;
    int64_t value;
    struct lru_node *prev, *next;
} lru_node;

typedef struct {
    map index;          /* key -> node pointer (stored as int64) */
    lru_node *head, *tail;
    size_t cap, len, hits, misses, evictions;
} lru;

bool lru_init(lru *c, size_t capacity);
void lru_free(lru *c);
bool lru_get(lru *c, const char *key, int64_t *out);   /* O(1) average, moves key to front */
bool lru_put(lru *c, const char *key, int64_t value);  /* O(1) average, evicts the oldest when full */
bool lru_check(const lru *c);

#endif
