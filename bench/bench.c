#define _POSIX_C_SOURCE 199309L
/* Measured cost per operation, to sit next to the big-O table in docs/COMPLEXITY.md. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "ds.h"

static double now(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec * 1e-9;
}

static int cmp_int(const void *a, const void *b) {
    const int x = *(const int *)a, y = *(const int *)b;
    return (x > y) - (x < y);
}

static volatile int64_t sink;

int main(void) {
    printf("## Lookup: hash map vs linear search (ns per lookup)\n\n| N keys | map_get | linear scan | ratio |\n|---|---|---|---|\n");
    for (int n = 10; n <= 100000; n *= 10) {
        char (*keys)[16] = malloc((size_t)n * 16);
        map m;
        map_init(&m, 8);
        for (int i = 0; i < n; i++) { snprintf(keys[i], 16, "key%d", i); map_put(&m, keys[i], i); }
        const int lookups = 200000;
        double t0 = now();
        for (int i = 0; i < lookups; i++) { int64_t v; map_get(&m, keys[(i * 7919) % n], &v); sink += v; }
        const double t_map = (now() - t0) / lookups * 1e9;
        const int scans = n >= 10000 ? 2000 : 50000;
        t0 = now();
        for (int i = 0; i < scans; i++) {
            const char *want = keys[(i * 7919) % n];
            for (int j = 0; j < n; j++) if (!strcmp(keys[j], want)) { sink += j; break; }
        }
        const double t_lin = (now() - t0) / scans * 1e9;
        printf("| %d | %.0f | %.0f | %.0fx |\n", n, t_map, t_lin, t_lin / t_map);
        map_free(&m);
        free(keys);
    }

    printf("\n## Priority queue: binary heap vs sorted array insert (ns per push+pop)\n\n| N | heap | sorted array | ratio |\n|---|---|---|---|\n");
    for (int n = 100; n <= 100000; n *= 10) {
        int *vals = malloc(sizeof(int) * n), *sorted = malloc(sizeof(int) * n);
        for (int i = 0; i < n; i++) vals[i] = (int)((unsigned)i * 2654435761u % 1000003u);
        heap h;
        heap_init(&h, sizeof(int), cmp_int);
        double t0 = now();
        for (int i = 0; i < n; i++) heap_push(&h, &vals[i]);
        for (int i = 0, x; i < n; i++) { heap_pop(&h, &x); sink += x; }
        const double t_heap = (now() - t0) / n * 1e9;
        heap_free(&h);
        int len = 0;
        t0 = now();
        for (int i = 0; i < n; i++) {               /* insertion keeps the array sorted descending */
            int j = len++;
            while (j > 0 && sorted[j - 1] < vals[i]) { sorted[j] = sorted[j - 1]; j--; }
            sorted[j] = vals[i];
        }
        while (len) sink += sorted[--len];          /* pop smallest from the end */
        const double t_arr = (now() - t0) / n * 1e9;
        printf("| %d | %.0f | %.0f | %.1fx |\n", n, t_heap, t_arr, t_arr / t_heap);
        free(vals);
        free(sorted);
    }

    printf("\n## Robin Hood probing: longest probe sequence just before each resize (85%% full)\n\n"
           "| keys | capacity | load | max probe |\n|---|---|---|---|\n");
    map m;
    map_init(&m, 8);
    char key[16];
    for (int i = 1; i <= 1 << 20; i++) {
        if ((m.len + 1) * 100 > m.cap * 85 && m.cap >= 1024)   /* the next put will grow the table */
            printf("| %zu | %zu | %.0f%% | %zu |\n", m.len, m.cap, 100.0 * m.len / m.cap, m.max_probe);
        snprintf(key, sizeof key, "x%d", i);
        map_put(&m, key, i);
    }
    map_free(&m);

    vec v;
    vec_init(&v, sizeof(int));
    size_t reallocs = 0, last_cap = 0;
    double t0 = now();
    for (int i = 0; i < 10000000; i++) {
        vec_push(&v, &i);
        if (v.cap != last_cap) { reallocs++; last_cap = v.cap; }
    }
    printf("\n## vec_push: %.1f ns per push over 10M pushes (amortized O(1): capacity doubling, %zu reallocations)\n",
           (now() - t0) / 1e7 * 1e9, reallocs);
    vec_free(&v);
    return 0;
}
