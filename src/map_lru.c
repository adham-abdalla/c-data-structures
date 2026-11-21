/* Hash map (open addressing, Robin Hood probing, backward-shift deletion) and an LRU cache. */
#include <stdlib.h>
#include <string.h>

#include "ds.h"

uint32_t ds_hash(const char *s) {
    uint32_t h = 2166136261u;
    while (*s) { h ^= (unsigned char)*s++; h *= 16777619u; }
    return h;
}

static char *dup(const char *s) {
    const size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

static size_t round_pow2(size_t n) {
    size_t c = 8;
    while (c < n) c <<= 1;
    return c;
}

bool map_init(map *m, size_t initial_cap) {
    m->cap = round_pow2(initial_cap);
    m->len = m->max_probe = 0;
    m->slots = calloc(m->cap, sizeof(map_slot));
    return m->slots != NULL;
}

void map_free(map *m) {
    if (m->slots)
        for (size_t i = 0; i < m->cap; i++) free(m->slots[i].key);
    free(m->slots);
    memset(m, 0, sizeof *m);
}

size_t map_len(const map *m) { return m->len; }

/* Robin Hood insert of an already-owned key: the element further from its home slot keeps
 * the slot, which bounds probe lengths and keeps lookups fast at high load. */
static void insert_owned(map *m, map_slot s) {
    const size_t mask = m->cap - 1;
    size_t i = s.hash & mask;
    s.dist = 0;
    for (;;) {
        map_slot *cur = &m->slots[i];
        if (!cur->key) {
            *cur = s;
            if (s.dist > m->max_probe) m->max_probe = s.dist;
            return;
        }
        if (cur->dist < s.dist) {               /* steal from the rich */
            map_slot t = *cur;
            *cur = s;
            if (s.dist > m->max_probe) m->max_probe = s.dist;
            s = t;
        }
        i = (i + 1) & mask;
        s.dist++;
    }
}

static bool grow(map *m) {
    map old = *m;
    map_slot *fresh = calloc(old.cap * 2, sizeof(map_slot));
    if (!fresh) return false;                   /* map unchanged on failure */
    m->slots = fresh;
    m->cap = old.cap * 2;
    m->max_probe = 0;
    for (size_t i = 0; i < old.cap; i++)
        if (old.slots[i].key) insert_owned(m, old.slots[i]);
    free(old.slots);
    return true;
}

static map_slot *find(const map *m, const char *key, uint32_t h) {
    const size_t mask = m->cap - 1;
    size_t i = h & mask;
    for (uint32_t d = 0;; d++, i = (i + 1) & mask) {
        map_slot *s = &m->slots[i];
        if (!s->key || s->dist < d) return NULL; /* Robin Hood: key would have been placed by now */
        if (s->hash == h && strcmp(s->key, key) == 0) return s;
    }
}

bool map_put(map *m, const char *key, int64_t value) {
    const uint32_t h = ds_hash(key);
    map_slot *s = find(m, key, h);
    if (s) { s->value = value; return true; }
    if ((m->len + 1) * 100 > m->cap * 85 && !grow(m)) return false;
    char *k = dup(key);
    if (!k) return false;
    insert_owned(m, (map_slot){.key = k, .value = value, .hash = h});
    m->len++;
    return true;
}

bool map_get(const map *m, const char *key, int64_t *out) {
    const map_slot *s = find(m, key, ds_hash(key));
    if (s && out) *out = s->value;
    return s != NULL;
}

bool map_del(map *m, const char *key) {
    map_slot *s = find(m, key, ds_hash(key));
    if (!s) return false;
    free(s->key);
    const size_t mask = m->cap - 1;
    size_t i = (size_t)(s - m->slots);
    for (;;) {                                  /* backward shift: pull followers one slot closer */
        const size_t next = (i + 1) & mask;
        if (!m->slots[next].key || m->slots[next].dist == 0) break;
        m->slots[i] = m->slots[next];
        m->slots[i].dist--;
        i = next;
    }
    memset(&m->slots[i], 0, sizeof(map_slot));
    m->len--;
    return true;
}

bool map_check(const map *m) {
    size_t n = 0;
    const size_t mask = m->cap - 1;
    for (size_t i = 0; i < m->cap; i++) {
        const map_slot *s = &m->slots[i];
        if (!s->key) continue;
        n++;
        if (s->hash != ds_hash(s->key)) return false;
        if (((s->hash + s->dist) & mask) != i) return false;            /* dist matches position */
        if (s->dist > 0) {                                                /* no gap before it */
            const map_slot *prev = &m->slots[(i - 1) & mask];
            if (!prev->key || prev->dist + 1 < s->dist) return false;
        }
    }
    return n == m->len && m->len * 100 <= m->cap * 85;
}

/* ---------------------------------------------------------------- LRU */
bool lru_init(lru *c, size_t capacity) {
    memset(c, 0, sizeof *c);
    c->cap = capacity;
    return capacity > 0 && map_init(&c->index, capacity * 2);
}

static void unlink_node(lru *c, lru_node *n) {
    if (n->prev) n->prev->next = n->next; else c->head = n->next;
    if (n->next) n->next->prev = n->prev; else c->tail = n->prev;
    n->prev = n->next = NULL;
}

static void push_front(lru *c, lru_node *n) {
    n->next = c->head;
    n->prev = NULL;
    if (c->head) c->head->prev = n;
    c->head = n;
    if (!c->tail) c->tail = n;
}

void lru_free(lru *c) {
    for (lru_node *n = c->head, *next; n; n = next) {
        next = n->next;
        free(n->key);
        free(n);
    }
    map_free(&c->index);
    memset(c, 0, sizeof *c);
}

bool lru_get(lru *c, const char *key, int64_t *out) {
    int64_t ptr;
    if (!map_get(&c->index, key, &ptr)) { c->misses++; return false; }
    lru_node *n = (lru_node *)(intptr_t)ptr;
    unlink_node(c, n);
    push_front(c, n);
    if (out) *out = n->value;
    c->hits++;
    return true;
}

bool lru_put(lru *c, const char *key, int64_t value) {
    int64_t ptr;
    if (map_get(&c->index, key, &ptr)) {
        lru_node *n = (lru_node *)(intptr_t)ptr;
        n->value = value;
        unlink_node(c, n);
        push_front(c, n);
        return true;
    }
    lru_node *n = malloc(sizeof *n);
    char *k = dup(key);
    if (!n || !k || !map_put(&c->index, key, (int64_t)(intptr_t)n)) { free(n); free(k); return false; }
    n->key = k;
    n->value = value;
    push_front(c, n);
    if (++c->len > c->cap) {                    /* evict least recently used */
        lru_node *old = c->tail;
        unlink_node(c, old);
        map_del(&c->index, old->key);
        free(old->key);
        free(old);
        c->len--;
        c->evictions++;
    }
    return true;
}

bool lru_check(const lru *c) {
    size_t n = 0;
    const lru_node *prev = NULL;
    for (const lru_node *x = c->head; x; prev = x, x = x->next) {
        if (x->prev != prev) return false;
        int64_t ptr;
        if (!map_get(&c->index, x->key, &ptr) || (const lru_node *)(intptr_t)ptr != x) return false;
        if (++n > c->cap) return false;
    }
    return prev == c->tail && n == c->len && map_len(&c->index) == n && map_check(&c->index);
}
