#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

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

int main(int argc, char *argv[]) {
	if (argc != 2) {
		fprintf(stderr, "Usage: %s <file>\n", argv[0]);
		return 1;
	}

	// One counter per possible byte value
	unsigned long freq[256] = {0};

	// "rb" so nothing gets messed with on the way in, works for any file type
	FILE *fptr = fopen(argv[1], "rb");
	if (fptr == NULL) {
		perror(argv[1]);
		return 1;
	}

	// chr has to be an int, not a char, otherwise EOF (-1) gets mixed up with byte 255
	int chr;
	while ((chr = fgetc(fptr)) != EOF) {
		freq[chr]++;
	}

	// Done with the file once we have the counts
	fclose(fptr);

	unsigned long total = 0;
	for (int i = 0; i < 256; i++) {
		total += freq[i];
	}

	Node *root = build_tree(freq);
	
	char codes[256][256] = {0};
	char path[256];

	if (root != NULL) {
		if (root->leftChild == NULL) {
			strcpy(codes[root->byte], "0");
		} else {
			generate_codes(root, path, 0, codes);
		}
	}

	unsigned long compressedBits = 0;
	for (int i = 0; i < 256; i++) {
		compressedBits += (freq[i] * strlen(codes[i]));
	}
	// Rough preview of how much we'd save, before writing any actual compression
	unsigned long bits = encoded_bits(root, 0);
	printf("Original:   %lu bytes (%lu bits)\n", total, total * 8);
	// +7 rounds up to the next full byte
	printf("Compressed: %lu bits = %lu bytes, %lu (data only, no header)\n", bits, (bits + 7) / 8, compressedBits);

	free_tree(root);
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

// children first, then the node. Freeing the node first would lose
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

	if (node->rightChild == NULL && node->leftChild == NULL) {
		path[depth] = '\0';
		strcpy(codes[node->byte], path);
		return;
	}
	
	path[depth] = '0';
	generate_codes(node->leftChild, path, depth + 1, codes);
	path[depth] = '1';
	generate_codes(node->rightChild, path, depth + 1, codes);
}

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
