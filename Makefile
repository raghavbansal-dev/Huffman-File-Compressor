CC          = gcc
CFLAGS      = -std=c11 -Wall -Wextra -O2
DEBUG_FLAGS = -std=c11 -Wall -Wextra -g -fsanitize=address,undefined

SRC = src/main.c src/huffman.c src/tree.c src/heap.c src/bitio.c
HDR = src/huffman.h src/tree.h src/heap.h src/bitio.h

# Optimized build
mzip: $(SRC) $(HDR)
	$(CC) $(CFLAGS) $(SRC) -o $@

# Same code with AddressSanitizer + UndefinedBehaviorSanitizer, for testing
mzip-debug: $(SRC) $(HDR)
	$(CC) $(DEBUG_FLAGS) $(SRC) -o $@

debug: mzip-debug

# Tests run against the sanitizer build so memory bugs show up as failures
test: mzip-debug
	bash tests/run_tests.sh ./mzip-debug

# Compares against gzip. Pass your own files with: make bench FILES="a.txt b.bin"
FILES ?= $(SRC) mzip
bench: mzip
	bash tests/bench.sh ./mzip $(FILES)

clean:
	rm -f mzip mzip-debug

.PHONY: debug test bench clean
