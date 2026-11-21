CC       ?= gcc
CFLAGS   ?= -std=c11 -D_GNU_SOURCE -O2 -g -Wall -Wextra -Wpedantic -Wshadow -Werror
SANITIZE ?=
SRC      := src/vec_ring_heap.c src/map_lru.c
HDR      := include/ds.h

.PHONY: all test example-test mutants bench fuzz coverage clean

all: build/test_ds build/top_talkers build/bench

build:
	mkdir -p build

build/test_ds: $(SRC) $(HDR) tests/test_ds.c | build
	$(CC) $(CFLAGS) $(SANITIZE) -Iinclude $(SRC) tests/test_ds.c -o $@

build/top_talkers: $(SRC) $(HDR) examples/top_talkers.c | build
	$(CC) $(CFLAGS) $(SANITIZE) -Iinclude $(SRC) examples/top_talkers.c -o $@

build/bench: $(SRC) $(HDR) bench/bench.c | build
	$(CC) $(CFLAGS) -O3 -Iinclude $(SRC) bench/bench.c -o $@

test: build/test_ds example-test
	./build/test_ds

example-test: build/top_talkers
	./build/top_talkers 3 < tests/traffic.csv | diff -u tests/top_talkers.expected -

mutants:
	bash tests/mutants.sh

bench: build/bench
	./build/bench

fuzz: | build
	clang -std=c11 -g -O1 -fsanitize=fuzzer,address,undefined -Iinclude $(SRC) fuzz/fuzz_map.c -o build/fuzz_map
	./build/fuzz_map -max_total_time=60 -print_final_stats=1

coverage: | build
	rm -f build/*.gcda
	$(CC) -std=c11 -D_GNU_SOURCE -O0 -g --coverage -Iinclude $(SRC) tests/test_ds.c -o build/test_cov
	./build/test_cov
	gcov -n -o build build/test_cov-map_lru.gcda build/test_cov-vec_ring_heap.gcda | grep -A1 "^File 'src"

clean:
	rm -rf build
