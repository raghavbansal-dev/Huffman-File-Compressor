#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>

#define HEAP_CAPACITY 256

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

int main(int argc, char *argv[]) {
	if (argc != 2) {
		fprintf(stderr, "Usage: %s <file>\n", argv[0]);
		return 1;
	}

	unsigned long freq[256] = {0};

	FILE *fptr = fopen(argv[1], "rb");
	if (fptr == NULL) {
		perror(argv[1]);
		return 1;
	}

	int chr;

	while ((chr = fgetc(fptr)) != EOF) {
		// fgetc reads one byte and returns its value (0-255) as an int, or EOF (-1)
		freq[chr]++;
	}

	fclose(fptr);

	unsigned long total = 0;
	for (int i = 0; i < 256; i++) {
		total += freq[i];
	}

	Node *root = build_tree(freq);
	print_tree(root, 0);

	unsigned long bits = encoded_bits(root, 0);
	printf("Original:   %lu bytes (%lu bits)\n", total, total * 8);
	printf("Compressed: %lu bits = %lu bytes (data only, no header)\n", bits, (bits + 7) / 8);

	free_tree(root);
	return 0;
}

// Swaps two node pointers (the nodes themselves never move in memory)
void swap(Node **a, Node **b) {
	Node *temp = *a;
	*a = *b;
	*b = temp;
}

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

void sift_down(Node **arr, int size, int index) {
	int lIndex = 2 * index + 1;
	int rIndex = 2 * index + 2;
	int smallest = index;

	// A child exists only if its index is < size
	if (lIndex < size && arr[lIndex]->frequency < arr[smallest]->frequency) {
		smallest = lIndex;
	}
	if (rIndex < size && arr[rIndex]->frequency < arr[smallest]->frequency) {
		smallest = rIndex;
	}

	// Parent is already the smallest (this also covers leaves): heap is valid
	if (smallest == index) {
		return;
	}

	swap(&arr[index], &arr[smallest]);
	sift_down(arr, size, smallest);
}

void heap_insert(Node **arr, int *size, Node *node) {
	if (*size == HEAP_CAPACITY) {
		fprintf(stderr, "heap_insert: heap is full\n");
		return;
	}
	arr[*size] = node;
	sift_up(arr, *size);
	*size += 1;
}

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

void print_heap(Node **arr, int size) {
	for (int i = 0; i < size; i++) {
		printf("%lu ", arr[i]->frequency);
	}
	printf("\n");
}

Node* create_node(unsigned char byte, unsigned long frequency, Node* left, Node* right) {
	Node* node = malloc(sizeof(Node));
	if (node == NULL) {
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
	Node* heap[HEAP_CAPACITY];
	int size = 0;

	for (int i = 0; i < 256; i++) {
		if (freq[i] > 0) {
			heap_insert(heap, &size, create_node(i, freq[i], NULL, NULL));
		}
	}

	if (size == 0) {
		return NULL;
	}

	while (size > 1) {
		Node* leftNode = heap_extract_min(heap, &size);
		Node* rightNode = heap_extract_min(heap, &size);
		heap_insert(heap, &size, create_node(0, leftNode->frequency + rightNode->frequency, leftNode, rightNode));
	}

	Node* root = heap_extract_min(heap, &size);
	return root;
}

// Prints every leaf with its frequency and depth (= code length in bits)
void print_tree(Node *node, int depth) {
	if (node == NULL) {
		return;
	}

	if (node->leftChild == NULL) {   // leaf
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

// Sum of frequency * depth over all leaves = size of the encoded data in bits
unsigned long encoded_bits(Node *node, int depth) {
	if (node == NULL) {
		return 0;
	}

	if (node->leftChild == NULL) {   // leaf
		return node->frequency * depth;
	}

	return encoded_bits(node->leftChild, depth + 1) + encoded_bits(node->rightChild, depth + 1);
}

// Frees children before the node itself (post-order)
void free_tree(Node *node) {
	if (node == NULL) {
		return;
	}

	free_tree(node->leftChild);
	free_tree(node->rightChild);
	free(node);
}
