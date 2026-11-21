/* top_talkers: read "radio_id,bytes" lines from stdin, print the K radios that sent the most.
 * map aggregates per radio in O(1) average; a size-K min-heap keeps the leaders in O(n log K).
 *   top_talkers [K] < traffic.csv */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ds.h"

typedef struct { int64_t bytes; char id[32]; } entry;

static int by_bytes(const void *a, const void *b) {
    const int64_t x = ((const entry *)a)->bytes, y = ((const entry *)b)->bytes;
    if (x != y) return (x > y) - (x < y);
    return -strcmp(((const entry *)a)->id, ((const entry *)b)->id);  /* ties: alphabetical id wins */
}

int main(int argc, char **argv) {
    const int k = argc > 1 ? atoi(argv[1]) : 5;
    if (k <= 0) { fprintf(stderr, "K must be positive\n"); return 2; }
    map totals;
    vec ids;
    if (!map_init(&totals, 64)) return 1;
    vec_init(&ids, sizeof(char *));
    char line[256];
    size_t lines = 0, bad = 0;
    while (fgets(line, sizeof line, stdin)) {
        char *comma = strchr(line, ',');
        char *end;
        long long n = comma ? strtoll(comma + 1, &end, 10) : -1;
        if (!comma || comma == line || comma - line >= 32 || n < 0 || (*end && *end != '\n' && *end != '\r')) { bad++; continue; }
        *comma = '\0';
        int64_t cur = 0;
        if (!map_get(&totals, line, &cur)) {
            char *copy = strdup(line);
            if (!copy || !vec_push(&ids, &copy)) return 1;
        }
        if (!map_put(&totals, line, cur + n)) return 1;
        lines++;
    }
    heap top;
    heap_init(&top, sizeof(entry), by_bytes);
    for (size_t i = 0; i < ids.len; i++) {
        entry e = {0};
        const char *id = *(char **)vec_at(&ids, i);
        map_get(&totals, id, &e.bytes);
        snprintf(e.id, sizeof e.id, "%s", id);
        if ((int)heap_len(&top) < k) heap_push(&top, &e);
        else if (by_bytes(&e, heap_peek(&top)) > 0) { heap_pop(&top, NULL); heap_push(&top, &e); }
    }
    const int n = (int)heap_len(&top);
    entry *out = malloc(sizeof(entry) * (n ? n : 1));
    for (int i = n - 1; i >= 0; i--) heap_pop(&top, &out[i]);   /* smallest comes out first */
    printf("%zu lines, %zu radios, %zu malformed\n", lines, map_len(&totals), bad);
    for (int i = 0; i < n; i++) printf("%2d. %-12s %lld bytes\n", i + 1, out[i].id, (long long)out[i].bytes);
    for (size_t i = 0; i < ids.len; i++) free(*(char **)vec_at(&ids, i));
    free(out);
    vec_free(&ids);
    heap_free(&top);
    map_free(&totals);
    return 0;
}
