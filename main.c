#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// Huffman never has more than 256 nodes in the heap at once
#define HEAP_CAPACITY 256

// A tree node. Leaves hold an actual byte; internal nodes just hold
// the combined frequency of everything below them.
typedef struct Node {
	unsigned char byte;
	unsigned long frequency;
	struct Node* leftChild;
	struct Node* rightChild;
} Node;

// Collects bits until it has a full byte, then writes that byte out
typedef struct BitWriter {
	FILE *fp;
	unsigned char buffer;
	int count;
} BitWriter;

// The reverse: loads a byte and hands its bits out one at a time, leftmost first
typedef struct BitReader {
	FILE *fp;
	unsigned char buffer;
	int count;   // bits left in buffer
} BitReader;

void swap(Node **a, Node **b);
void sift_up(Node **arr, int index);
void sift_down(Node **arr, int size, int index);
void heap_insert(Node **arr, int *size, Node *node);
Node* heap_extract_min(Node **arr, int *size);
void print_heap(Node **arr, int size);
Node* create_node(unsigned char byte, unsigned long frequency, Node* left, Node* right);
Node* build_tree(unsigned long *freq);
void print_tree(Node *node, int depth);
unsigned long encoded_bits(Node *node, int depth);
void free_tree(Node *node);
void generate_codes(Node *node, char *path, int depth, char codes[256][256]);
void print_codes(unsigned long *freq, char codes[256][256]);
void write_bit(BitWriter *bw, int bit);
void flush_bits(BitWriter *bw);
void write_code(BitWriter *bw, const char *code);
int write_header(FILE *output, unsigned long *freq);
int read_header(FILE *input, unsigned long *freq);
int read_bit(BitReader *br);
int compress(const char *inPath, const char *outPath);
int decompress(const char *inPath, const char *outPath);

int main(int argc, char *argv[]) {
	if (argc != 4 || (strcmp(argv[1], "c") != 0 && strcmp(argv[1], "d") != 0)) {
		fprintf(stderr, "Usage: %s c|d <input file> <output file>\n", argv[0]);
		return 1;
	}

	// Opening the output with "wb" empties it, so if it's also the input we'd wipe it before reading
	if (strcmp(argv[2], argv[3]) == 0) {
		fprintf(stderr, "Error: input and output must be different files\n");
		return 1;
	}

	if (strcmp(argv[1], "c") == 0) {
		return compress(argv[2], argv[3]);
	}
	return decompress(argv[2], argv[3]);
}

// Returns 0 on success, 1 on failure, so main can pass it straight through as the exit code
int compress(const char *inPath, const char *outPath) {
	// One counter per possible byte value
	unsigned long freq[256] = {0};

	// "rb" so nothing gets messed with on the way in, works for any file type
	FILE *fptr = fopen(inPath, "rb");
	if (fptr == NULL) {
		perror(inPath);
		return 1;
	}

	// Pass 1: count every byte.
	// chr has to be an int, not a char, otherwise EOF (-1) gets mixed up with byte 255
	int chr;
	while ((chr = fgetc(fptr)) != EOF) {
		freq[chr]++;
	}

	unsigned long total = 0;
	for (int i = 0; i < 256; i++) {
		total += freq[i];
	}

	Node *root = build_tree(freq);

	char codes[256][256] = {0};
	char path[256];

	if (root != NULL) {
		// Only one distinct byte: the root is a leaf and its path is empty,
		// so give it "0" by hand or nothing would get written
		if (root->leftChild == NULL) {
			strcpy(codes[root->byte], "0");
		} else {
			generate_codes(root, path, 0, codes);
		}
	}

	FILE *optr = fopen(outPath, "wb");
	if (optr == NULL) {
		perror(outPath);
		fclose(fptr);
		free_tree(root);
		return 1;
	}

	// The decompressor needs the frequencies to rebuild the same tree
	if (!write_header(optr, freq)) {
		fprintf(stderr, "Error: failed to write header to %s\n", outPath);
		fclose(optr);
		fclose(fptr);
		free_tree(root);
		return 1;
	}

	// Pass 2: go back to the start and write each byte's code
	rewind(fptr);
	BitWriter bw = {optr, 0, 0};
	while ((chr = fgetc(fptr)) != EOF) {
		write_code(&bw, codes[chr]);
	}
	// Don't lose the last partial byte
	flush_bits(&bw);

	// ftell on the output = how many bytes we've written
	long compressedSize = ftell(optr);

	fclose(optr);
	fclose(fptr);
	free_tree(root);

	printf("Original:   %lu bytes\n", total);
	printf("Compressed: %ld bytes", compressedSize);
	if (total > 0) {
		printf(" (%.1f%% of original)", 100.0 * compressedSize / total);
	}
	printf("\n");

	return 0;
}

// Same return convention as compress
int decompress(const char *inPath, const char *outPath) {
	FILE *fptr = fopen(inPath, "rb");
	if (fptr == NULL) {
		perror(inPath);
		return 1;
	}

	// Get the frequencies back so we can build the exact same tree
	unsigned long freq[256] = {0};
	if (!read_header(fptr, freq)) {
		fprintf(stderr, "Error: %s is not a valid .huf file\n", inPath);
		fclose(fptr);
		return 1;
	}

	// The original size is just all the counts added up.
	// This is how we know when to stop and ignore the padding bits.
	unsigned long total = 0;
	for (int i = 0; i < 256; i++) {
		total += freq[i];
	}

	Node *root = build_tree(freq);

	FILE *optr = fopen(outPath, "wb");
	if (optr == NULL) {
		perror(outPath);
		fclose(fptr);
		free_tree(root);
		return 1;
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
				fclose(optr);
				remove(outPath);   // don't leave a half-written file lying around
				fclose(fptr);
				free_tree(root);
				return 1;
			}
			node = (bit == 0) ? node->leftChild : node->rightChild;
		}

		fputc(node->byte, optr);
	}

	long compressedSize = ftell(fptr);

	fclose(optr);
	fclose(fptr);
	free_tree(root);

	printf("Compressed:   %ld bytes\n", compressedSize);
	printf("Decompressed: %lu bytes\n", total);

	return 0;
}

// Swaps the pointers, not the nodes. The nodes stay put in memory,
// which matters because parents point straight at them.
void swap(Node **a, Node **b) {
	Node *temp = *a;
	*a = *b;
	*b = temp;
}

// Keep moving a node up while it's smaller than its parent
void sift_up(Node **arr, int index) {
	if (index == 0) {
		return;
	}

	int pIndex = (index - 1) / 2;
	if (arr[pIndex]->frequency <= arr[index]->frequency) {
		return;
	}

	swap(&arr[pIndex], &arr[index]);
	sift_up(arr, pIndex);
}

// Keep moving a node down, always swapping with the smaller child
// (swapping with the bigger one would break the heap)
void sift_down(Node **arr, int size, int index) {
	int lIndex = 2 * index + 1;
	int rIndex = 2 * index + 2;
	int smallest = index;

	// Anything at or past size isn't part of the heap, so don't look at it
	if (lIndex < size && arr[lIndex]->frequency < arr[smallest]->frequency) {
		smallest = lIndex;
	}
	if (rIndex < size && arr[rIndex]->frequency < arr[smallest]->frequency) {
		smallest = rIndex;
	}

	// Already smaller than both kids (or it's a leaf), so we're done
	if (smallest == index) {
		return;
	}

	swap(&arr[index], &arr[smallest]);
	sift_down(arr, size, smallest);
}

// Drop the new node in the first free slot, then let it bubble up
void heap_insert(Node **arr, int *size, Node *node) {
	if (*size == HEAP_CAPACITY) {
		fprintf(stderr, "heap_insert: heap is full\n");
		return;
	}
	arr[*size] = node;
	sift_up(arr, *size);
	*size += 1;
}

// Take the root (the smallest), put the last node in its place, and sink it
Node* heap_extract_min(Node **arr, int *size) {
	if (*size == 0) {
		fprintf(stderr, "heap_extract_min: heap is empty\n");
		return NULL;
	}

	Node *root = arr[0];
	arr[0] = arr[*size - 1];
	*size -= 1;
	sift_down(arr, *size, 0);

	return root;
}

// Just for debugging
void print_heap(Node **arr, int size) {
	for (int i = 0; i < size; i++) {
		printf("%lu ", arr[i]->frequency);
	}
	printf("\n");
}

// Used for both leaves (children NULL) and internal nodes
Node* create_node(unsigned char byte, unsigned long frequency, Node* left, Node* right) {
	Node* node = malloc(sizeof(Node));
	if (node == NULL) {
		// Nothing sensible to do if we're out of memory
		fprintf(stderr, "Out of memory\n");
		exit(1);
	}

	node->byte = byte;
	node->frequency = frequency;
	node->leftChild = left;
	node->rightChild = right;

	return node;
}

Node* build_tree(unsigned long *freq) {
	// Local array is fine here, it's only 256 pointers
	Node* heap[HEAP_CAPACITY];
	int size = 0;

	// One leaf per byte that actually shows up. Going 0-255 in order
	// means the decompressor will build the exact same tree.
	for (int i = 0; i < 256; i++) {
		if (freq[i] > 0) {
			heap_insert(heap, &size, create_node(i, freq[i], NULL, NULL));
		}
	}

	// Empty file, no tree
	if (size == 0) {
		return NULL;
	}

	// The core of Huffman: keep merging the two rarest nodes until one is left
	while (size > 1) {
		Node* leftNode = heap_extract_min(heap, &size);
		Node* rightNode = heap_extract_min(heap, &size);
		heap_insert(heap, &size, create_node(0, leftNode->frequency + rightNode->frequency, leftNode, rightNode));
	}

	// Whatever's left is the root
	Node* root = heap_extract_min(heap, &size);
	return root;
}

// Prints each leaf with its depth, which is also how many bits its code will take
void print_tree(Node *node, int depth) {
	if (node == NULL) {
		return;
	}

	// Internal nodes always have two kids, so no left child means it's a leaf
	if (node->leftChild == NULL) {
		if (isprint(node->byte)) {
			printf("%3d '%c': freq %lu, depth %d\n", node->byte, node->byte, node->frequency, depth);
		} else {
			printf("%3d    : freq %lu, depth %d\n", node->byte, node->frequency, depth);
		}
		return;
	}

	print_tree(node->leftChild, depth + 1);
	print_tree(node->rightChild, depth + 1);
}

// Each byte costs (code length) bits every time it appears,
// so the total is just frequency * depth summed over all leaves
unsigned long encoded_bits(Node *node, int depth) {
	if (node == NULL) {
		return 0;
	}

	if (node->leftChild == NULL) {
		return node->frequency * depth;
	}

	return encoded_bits(node->leftChild, depth + 1) + encoded_bits(node->rightChild, depth + 1);
}

// Kids first, then the node. Freeing the node first would lose
// the pointers to its children.
void free_tree(Node *node) {
	if (node == NULL) {
		return;
	}

	free_tree(node->leftChild);
	free_tree(node->rightChild);
	free(node);
}

void generate_codes(Node *node, char *path, int depth, char codes[256][256]) {
	if (node == NULL) {
		return;
	}

	// Reached a leaf: path holds its full code. The '\0' also cuts off
	// leftover characters from deeper paths we walked earlier.
	if (node->rightChild == NULL && node->leftChild == NULL) {
		path[depth] = '\0';
		strcpy(codes[node->byte], path);
		return;
	}

	// Left adds a 0, right adds a 1. Going right overwrites the same spot,
	// so there's nothing to undo.
	path[depth] = '0';
	generate_codes(node->leftChild, path, depth + 1, codes);
	path[depth] = '1';
	generate_codes(node->rightChild, path, depth + 1, codes);
}

// Debugging helper, not called normally
void print_codes(unsigned long *freq, char codes[256][256]) {
	for (int i = 0; i < 256; i++) {
		if (freq[i] > 0) {
			if (isprint(i)) {
				printf("%3d '%c': freq %lu, code %s\n", i, i, freq[i], codes[i]);
			} else {
				printf("%3d    : freq %lu, code %s\n", i, freq[i], codes[i]);
			}
		}
	}
}

// Shift everything left to make room, then drop the new bit in on the right.
// Once we have 8, the byte is done.
void write_bit(BitWriter *bw, int bit) {
	bw->buffer = ((bw->buffer << 1) | bit);
	bw->count += 1;
	if (bw->count == 8) {
		fputc(bw->buffer, bw->fp);
		bw->buffer = 0;
		bw->count = 0;
	}
}

// The leftover bits sit on the right of the buffer, but they belong on the left.
// Shift them over (zeros fill in as padding) and write the last byte.
void flush_bits(BitWriter *bw) {
	if (bw->count == 0) {
		return;
	}

	bw->buffer = bw->buffer << (8 - bw->count);
	fputc(bw->buffer, bw->fp);
	bw->buffer = 0;
	bw->count = 0;
}

// Turns a code string like "01" into actual bits
void write_code(BitWriter *bw, const char *code) {
	int i = 0;
	while (code[i] != '\0') {
		if (code[i] == '1') {
			write_bit(bw, 1);
		} else {
			write_bit(bw, 0);
		}
		i++;
	}
}

// Header format: [symbol count, 2 bytes] then for each byte that appears
// [byte, 1 byte][frequency, 8 bytes]. Returns 1 on success, 0 if a write failed.
int write_header(FILE *output, unsigned long *freq) {
	// Can be 0 to 256, so one byte isn't enough
	uint16_t count = 0;
	for (int i = 0; i < 256; i++) {
		if (freq[i] > 0) {
			count += 1;
		}
	}
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
int read_header(FILE *input, unsigned long *freq) {
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

// Returns the next bit (0 or 1), or -1 if the file has run out
int read_bit(BitReader *br) {
	// Used up the current byte, grab the next one
	if (br->count == 0) {
		int c = fgetc(br->fp);
		if (c == EOF) {
			return -1;
		}
		br->buffer = c;
		br->count = 8;
	}

	// Bits were written leftmost first, so read them back the same way:
	// take the top bit, then shift the next one up into its place
	int bit = (br->buffer >> 7) & 1;
	br->buffer <<= 1;
	br->count -= 1;
	return bit;
}
