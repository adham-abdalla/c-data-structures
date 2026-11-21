/* libFuzzer target: the input bytes are a script of map and LRU operations on a small key
 * space. ASan/UBSan catch memory errors; the invariant checks catch logic errors.
 *   make fuzz   (clang -fsanitize=fuzzer,address,undefined, runs 60 s) */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "ds.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    map m;
    lru c;
    if (!map_init(&m, 4) || !lru_init(&c, 5)) abort();
    char key[8];
    for (size_t i = 0; i + 1 < size; i += 2) {
        const uint8_t op = data[i] % 5, k = data[i + 1];
        snprintf(key, sizeof key, "k%u", (unsigned)(k % 64));
        int64_t v;
        switch (op) {
        case 0: map_put(&m, key, k); break;
        case 1: map_del(&m, key); break;
        case 2: if (map_get(&m, key, &v) && (v < 0 || v > 255)) abort(); break;   /* only bytes are stored */
        case 3: lru_put(&c, key, k); break;
        case 4: lru_get(&c, key, &v); break;
        }
        if (!map_check(&m) || !lru_check(&c)) abort();
    }
    map_free(&m);
    lru_free(&c);
    return 0;
}
