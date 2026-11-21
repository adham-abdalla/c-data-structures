/* vec, ring buffer and binary heap. */
#include <stdlib.h>
#include <string.h>

#include "ds.h"

/* ---------------------------------------------------------------- vec */
void vec_init(vec *v, size_t elem_size) {
    v->data = NULL;
    v->len = v->cap = 0;
    v->elem = elem_size;
}

void vec_free(vec *v) {
    free(v->data);
    vec_init(v, v->elem);
}

bool vec_reserve(vec *v, size_t cap) {
    if (cap <= v->cap) return true;
    if (cap > SIZE_MAX / v->elem) return false;                 /* size overflow */
    unsigned char *p = realloc(v->data, cap * v->elem);
    if (!p) return false;                                       /* old buffer still valid */
    v->data = p;
    v->cap = cap;
    return true;
}

bool vec_push(vec *v, const void *elem) {
    if (v->len == v->cap && !vec_reserve(v, v->cap ? v->cap * 2 : 8)) return false;
    memcpy(v->data + v->len * v->elem, elem, v->elem);
    v->len++;
    return true;
}

bool vec_pop(vec *v, void *out) {
    if (v->len == 0) return false;
    v->len--;
    if (out) memcpy(out, v->data + v->len * v->elem, v->elem);
    return true;
}

void *vec_at(const vec *v, size_t i) { return i < v->len ? v->data + i * v->elem : NULL; }

bool vec_check(const vec *v) { return v->len <= v->cap && (v->cap == 0) == (v->data == NULL) && v->elem > 0; }

/* ---------------------------------------------------------------- ring */
bool ring_init(ring *r, size_t capacity_pow2, size_t elem_size) {
    memset(r, 0, sizeof *r);
    if (capacity_pow2 == 0 || (capacity_pow2 & (capacity_pow2 - 1)) || elem_size == 0) return false;
    r->buf = malloc(capacity_pow2 * elem_size);
    if (!r->buf) return false;
    r->cap = capacity_pow2;
    r->elem = elem_size;
    return true;
}

void ring_free(ring *r) {
    free(r->buf);
    memset(r, 0, sizeof *r);
}

size_t ring_len(const ring *r) { return r->head - r->tail; }  /* unsigned wrap keeps this right */

bool ring_push(ring *r, const void *elem) {
    if (ring_len(r) == r->cap) {
        r->dropped++;  /* drop newest: a full log buffer keeps the evidence leading up to the problem */
        return false;
    }
    memcpy(r->buf + (r->head & (r->cap - 1)) * r->elem, elem, r->elem);
    r->head++;
    return true;
}

bool ring_pop(ring *r, void *out) {
    if (ring_len(r) == 0) return false;
    if (out) memcpy(out, r->buf + (r->tail & (r->cap - 1)) * r->elem, r->elem);
    r->tail++;
    return true;
}

bool ring_check(const ring *r) { return r->buf && ring_len(r) <= r->cap && (r->cap & (r->cap - 1)) == 0; }

/* ---------------------------------------------------------------- heap */
static void swap_bytes(unsigned char *a, unsigned char *b, size_t n) {
    unsigned char t[64];
    while (n) {
        const size_t k = n < sizeof t ? n : sizeof t;
        memcpy(t, a, k);
        memcpy(a, b, k);
        memcpy(b, t, k);
        a += k; b += k; n -= k;
    }
}

void heap_init(heap *h, size_t elem_size, ds_cmp cmp) {
    vec_init(&h->items, elem_size);
    h->cmp = cmp;
}

void heap_free(heap *h) { vec_free(&h->items); }
size_t heap_len(const heap *h) { return h->items.len; }
const void *heap_peek(const heap *h) { return vec_at(&h->items, 0); }

bool heap_push(heap *h, const void *elem) {
    if (!vec_push(&h->items, elem)) return false;
    size_t i = h->items.len - 1;
    while (i > 0) {                                        /* sift up */
        const size_t parent = (i - 1) / 2;
        unsigned char *pi = vec_at(&h->items, i), *pp = vec_at(&h->items, parent);
        if (h->cmp(pi, pp) >= 0) break;
        swap_bytes(pi, pp, h->items.elem);
        i = parent;
    }
    return true;
}

bool heap_pop(heap *h, void *out) {
    if (h->items.len == 0) return false;
    if (out) memcpy(out, vec_at(&h->items, 0), h->items.elem);
    vec_pop(&h->items, vec_at(&h->items, 0));             /* move the last element to the root */
    const size_t n = h->items.len;
    size_t i = 0;
    for (;;) {                                             /* sift down */
        const size_t l = 2 * i + 1, r = l + 1;
        size_t smallest = i;
        if (l < n && h->cmp(vec_at(&h->items, l), vec_at(&h->items, smallest)) < 0) smallest = l;
        if (r < n && h->cmp(vec_at(&h->items, r), vec_at(&h->items, smallest)) < 0) smallest = r;
        if (smallest == i) break;
        swap_bytes(vec_at(&h->items, i), vec_at(&h->items, smallest), h->items.elem);
        i = smallest;
    }
    return true;
}

bool heap_check(const heap *h) {
    for (size_t i = 1; i < h->items.len; i++)
        if (h->cmp(vec_at(&h->items, (i - 1) / 2), vec_at(&h->items, i)) > 0) return false;
    return vec_check(&h->items);
}
