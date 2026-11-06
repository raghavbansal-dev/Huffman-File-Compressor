#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>

#include "huffman.h"
#include "tree.h"
#include "bitio.h"

// File format:
//   [symbol count: uint16]  then for each byte that appears:
//   [byte: 1 byte][frequency: uint64]  then the packed data bits.
// Multi-byte numbers are stored in the machine's byte order (little-endian on x86/ARM).
#define HEADER_ENTRY_SIZE (1 + sizeof(uint64_t))

// Closes whatever is open, frees the tree and deletes a half-written output.
// Pass NULL for anything that isn't open or allocated yet. Always returns 1,
// so error paths can just do: return fail(...);
static int fail(FILE *in, FILE *out, const char *outPath, Node *root) {
	if (in != NULL) {
		fclose(in);
	}
	if (out != NULL) {
		fclose(out);
		remove(outPath);
	}
	free_tree(root);
	return 1;
}

static int count_symbols(const unsigned long *freq) {
	int count = 0;
	for (int i = 0; i < 256; i++) {
		if (freq[i] > 0) {
			count++;
		}
	}
	return count;
}

// Header: [symbol count, 2 bytes] then [byte, 1 byte][frequency, 8 bytes] for each
// byte that appears. Returns 1 on success, 0 if a write failed.
static int write_header(FILE *output, const unsigned long *freq) {
	// Can be 0 to 256, so one byte isn't enough
	uint16_t count = count_symbols(freq);
	if (fwrite(&count, sizeof(uint16_t), 1, output) != 1) {
		return 0;
	}

	for (int i = 0; i < 256; i++) {
		if (freq[i] > 0) {
			unsigned char byte = i;
			// Fixed 8 bytes no matter how big unsigned long is on this system
			uint64_t frequency = freq[i];
			if (fwrite(&byte, sizeof(unsigned char), 1, output) != 1 ||
			    fwrite(&frequency, sizeof(uint64_t), 1, output) != 1) {
				return 0;
			}
		}
	}
	return 1;
}

// Mirror of write_header. Fills freq (caller zeroes it) and returns 1 on success,
// 0 if the file ends early or contains something the compressor would never write.
static int read_header(FILE *input, unsigned long *freq) {
	uint16_t count;
	if (fread(&count, sizeof(uint16_t), 1, input) != 1) {
		return 0;
	}

	// There are only 256 possible bytes, anything more means this isn't our file
	if (count > 256) {
		return 0;
	}

	for (int i = 0; i < count; i++) {
		unsigned char byte;
		uint64_t frequency;
		if (fread(&byte, sizeof(unsigned char), 1, input) != 1 ||
		    fread(&frequency, sizeof(uint64_t), 1, input) != 1) {
			return 0;
		}

		// The compressor never writes a zero count or the same byte twice
		if (frequency == 0 || freq[byte] != 0) {
			return 0;
		}

		freq[byte] = frequency;
	}
	return 1;
}

// A short readable label for a byte: 'a', \n, \t... or nothing for other control bytes
static void byte_label(int byte, char *label, size_t size) {
	if (byte == '\n') {
		snprintf(label, size, "\\n");
	} else if (byte == '\t') {
		snprintf(label, size, "\\t");
	} else if (byte == '\r') {
		snprintf(label, size, "\\r");
	} else if (isprint(byte)) {
		snprintf(label, size, "'%c'", byte);
	} else {
		label[0] = '\0';
	}
}

// Verbose output: every byte that appears, how often, and the code it got
static void print_table(const unsigned long *freq, char codes[256][MAX_CODE_LEN]) {
	unsigned long total = 0;
	unsigned long bits = 0;

	printf("%d distinct bytes\n\n", count_symbols(freq));
	printf(" byte  char       count  code\n");

	for (int i = 0; i < 256; i++) {
		if (freq[i] > 0) {
			char label[8];
			byte_label(i, label, sizeof(label));
			printf("%5d  %-5s %10lu  %s\n", i, label, freq[i], codes[i]);

			total += freq[i];
			bits += freq[i] * strlen(codes[i]);
		}
	}

	// The number Huffman is actually trying to bring down
	if (total > 0) {
		printf("\nAverage code length: %.2f bits per byte (uncompressed: 8)\n", (double)bits / total);
	}
	printf("\n");
}

int compress(const char *inPath, const char *outPath, int verbose) {
	// "rb" so nothing gets messed with on the way in, works for any file type
	FILE *fptr = fopen(inPath, "rb");
	if (fptr == NULL) {
		perror(inPath);
		return 1;
	}

	// Pass 1: count every byte.
	// chr has to be an int, not a char, otherwise EOF (-1) gets mixed up with byte 255
	unsigned long freq[256] = {0};
	int chr;
	while ((chr = fgetc(fptr)) != EOF) {
		freq[chr]++;
	}
	// fgetc also returns EOF on a read error, so check which one it was
	if (ferror(fptr)) {
		fprintf(stderr, "Error: failed to read %s\n", inPath);
		return fail(fptr, NULL, outPath, NULL);
	}

	unsigned long total = 0;
	for (int i = 0; i < 256; i++) {
		total += freq[i];
	}

	Node *root = build_tree(freq);
	char codes[256][MAX_CODE_LEN];
	build_codes(root, codes);

	if (verbose) {
		print_table(freq, codes);
	}

	FILE *optr = fopen(outPath, "wb");
	if (optr == NULL) {
		perror(outPath);
		return fail(fptr, NULL, outPath, root);
	}

	// The decompressor needs the frequencies to rebuild the same tree
	if (!write_header(optr, freq)) {
		fprintf(stderr, "Error: failed to write header to %s\n", outPath);
		return fail(fptr, optr, outPath, root);
	}

	// Pass 2: go back to the start and write each byte's code
	rewind(fptr);
	BitWriter bw = {optr, 0, 0};
	while ((chr = fgetc(fptr)) != EOF) {
		write_code(&bw, codes[chr]);
	}
	// Don't lose the last partial byte
	flush_bits(&bw);

	if (ferror(fptr)) {
		fprintf(stderr, "Error: failed to read %s\n", inPath);
		return fail(fptr, optr, outPath, root);
	}
	if (ferror(optr)) {
		fprintf(stderr, "Error: failed to write %s\n", outPath);
		return fail(fptr, optr, outPath, root);
	}

	// ftell on the output = how many bytes we've written
	long compressedSize = ftell(optr);

	fclose(fptr);
	free_tree(root);
	// fclose flushes whatever is still buffered, so it can fail too (e.g. disk full)
	if (fclose(optr) != 0) {
		fprintf(stderr, "Error: failed to write %s\n", outPath);
		remove(outPath);
		return 1;
	}

	printf("Original:   %lu bytes\n", total);
	printf("Compressed: %ld bytes", compressedSize);
	if (total > 0) {
		printf(" (%.1f%% of original)", 100.0 * compressedSize / total);
	}
	printf("\n");

	if (verbose) {
		long headerSize = sizeof(uint16_t) + count_symbols(freq) * HEADER_ENTRY_SIZE;
		printf("  header:   %ld bytes\n", headerSize);
		printf("  data:     %ld bytes\n", compressedSize - headerSize);
	}

	return 0;
}

int decompress(const char *inPath, const char *outPath, int verbose) {
	FILE *fptr = fopen(inPath, "rb");
	if (fptr == NULL) {
		perror(inPath);
		return 1;
	}

	// Get the frequencies back so we can build the exact same tree
	unsigned long freq[256] = {0};
	if (!read_header(fptr, freq)) {
		fprintf(stderr, "Error: %s is not a valid .huf file\n", inPath);
		return fail(fptr, NULL, outPath, NULL);
	}

	// The original size is just all the counts added up.
	// This is how we know when to stop and ignore the padding bits.
	unsigned long total = 0;
	for (int i = 0; i < 256; i++) {
		total += freq[i];
	}

	Node *root = build_tree(freq);

	// Decoding walks the tree directly, codes are only needed for the printout
	if (verbose) {
		char codes[256][MAX_CODE_LEN];
		build_codes(root, codes);
		print_table(freq, codes);
	}

	FILE *optr = fopen(outPath, "wb");
	if (optr == NULL) {
		perror(outPath);
		return fail(fptr, NULL, outPath, root);
	}

	// read_header left us right where the data starts
	BitReader br = {fptr, 0, 0};

	// One loop = one byte of the original file.
	// Empty file: total is 0, so we never touch the NULL root.
	for (unsigned long written = 0; written < total; written++) {
		Node *node = root;

		// Walk down until we hit a leaf: 0 goes left, 1 goes right.
		// With a single-symbol file the root is already a leaf, so this
		// never runs and we just write that byte total times.
		while (node->leftChild != NULL) {
			int bit = read_bit(&br);
			if (bit == -1) {
				fprintf(stderr, "Error: %s is truncated or corrupt\n", inPath);
				return fail(fptr, optr, outPath, root);
			}
			node = (bit == 0) ? node->leftChild : node->rightChild;
		}

		fputc(node->byte, optr);
	}

	if (ferror(optr)) {
		fprintf(stderr, "Error: failed to write %s\n", outPath);
		return fail(fptr, optr, outPath, root);
	}

	long compressedSize = ftell(fptr);

	fclose(fptr);
	free_tree(root);
	if (fclose(optr) != 0) {
		fprintf(stderr, "Error: failed to write %s\n", outPath);
		remove(outPath);
		return 1;
	}

	printf("Compressed:   %ld bytes\n", compressedSize);
	printf("Decompressed: %lu bytes\n", total);

	return 0;
}
