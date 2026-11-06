#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tree.h"
#include "heap.h"

// Used for both leaves (children NULL) and internal nodes
Node *create_node(unsigned char byte, unsigned long frequency, Node *left, Node *right) {
	Node *node = malloc(sizeof(Node));
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

Node *build_tree(const unsigned long *freq) {
	// Local array is fine here, it's only 256 pointers
	Node *heap[HEAP_CAPACITY];
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
		Node *leftNode = heap_extract_min(heap, &size);
		Node *rightNode = heap_extract_min(heap, &size);
		heap_insert(heap, &size, create_node(0, leftNode->frequency + rightNode->frequency, leftNode, rightNode));
	}

	// Whatever's left is the root
	return heap_extract_min(heap, &size);
}

// Children first, then the node. Freeing the node first would lose
// the pointers to its children.
void free_tree(Node *node) {
	if (node == NULL) {
		return;
	}

	free_tree(node->leftChild);
	free_tree(node->rightChild);
	free(node);
}

// Walks the tree, writing '0' for left and '1' for right into path.
// At a leaf, path holds that leaf's full code.
static void generate_codes(const Node *node, char *path, int depth, char codes[256][MAX_CODE_LEN]) {
	if (node == NULL) {
		return;
	}

	// Internal nodes always have two kids, so no left child means it's a leaf.
	// The '\0' also cuts off leftover characters from deeper paths we walked earlier.
	if (node->leftChild == NULL) {
		path[depth] = '\0';
		strcpy(codes[node->byte], path);
		return;
	}

	// Going right overwrites the same spot, so there's nothing to undo
	path[depth] = '0';
	generate_codes(node->leftChild, path, depth + 1, codes);
	path[depth] = '1';
	generate_codes(node->rightChild, path, depth + 1, codes);
}

void build_codes(const Node *root, char codes[256][MAX_CODE_LEN]) {
	memset(codes, 0, 256 * MAX_CODE_LEN);

	if (root == NULL) {
		return;
	}

	// Only one distinct byte: the root is a leaf and its path is empty,
	// so give it "0" by hand or nothing would get written
	if (root->leftChild == NULL) {
		strcpy(codes[root->byte], "0");
		return;
	}

	char path[MAX_CODE_LEN];
	generate_codes(root, path, 0, codes);
}

// Each byte costs (code length) bits every time it appears,
// so the total is just frequency * depth summed over all leaves
unsigned long encoded_bits(const Node *node, int depth) {
	if (node == NULL) {
		return 0;
	}

	if (node->leftChild == NULL) {
		return node->frequency * depth;
	}

	return encoded_bits(node->leftChild, depth + 1) + encoded_bits(node->rightChild, depth + 1);
}
