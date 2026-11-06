#include <stdio.h>
#include <string.h>

#include "huffman.h"

// Help goes to stdout when asked for, stderr when it's shown because of a mistake
static void print_usage(FILE *out, const char *prog) {
	fprintf(out, "Usage: %s [-v] c|d <input> <output>\n", prog);
	fprintf(out, "  c    compress <input> into <output>\n");
	fprintf(out, "  d    decompress <input> into <output>\n");
	fprintf(out, "  -v   verbose: also print each byte's count and code\n");
}

int main(int argc, char *argv[]) {
	if (argc == 2 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
		print_usage(stdout, argv[0]);
		return 0;
	}

	// -v is optional and has to come first, so the mode is either argv[1] or argv[2]
	int verbose = 0;
	int first = 1;
	if (argc > 1 && strcmp(argv[1], "-v") == 0) {
		verbose = 1;
		first = 2;
	}

	if (argc - first != 3) {
		print_usage(stderr, argv[0]);
		return 1;
	}

	const char *mode = argv[first];
	const char *inPath = argv[first + 1];
	const char *outPath = argv[first + 2];

	if (strcmp(mode, "c") != 0 && strcmp(mode, "d") != 0) {
		print_usage(stderr, argv[0]);
		return 1;
	}

	// Opening the output with "wb" empties it, so if it's also the input we'd wipe it before reading
	if (strcmp(inPath, outPath) == 0) {
		fprintf(stderr, "Error: input and output must be different files\n");
		return 1;
	}

	if (strcmp(mode, "c") == 0) {
		return compress(inPath, outPath, verbose);
	}
	return decompress(inPath, outPath, verbose);
}
