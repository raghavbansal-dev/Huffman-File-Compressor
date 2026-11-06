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

	unsigned long total = 0;

	for (int i = 0; i < 256; i++) {
		if (freq[i] > 0) {
			if (isprint(i)) {
				printf("%3d '%c': %lu\n", i, i, freq[i]);
			} else {
				printf("%3d    : %lu\n", i, freq[i]);
			}
			total += freq[i];
		}
	}

	printf("Total: %lu\n", total);

	fclose(fptr);
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
	arr[0] = arr[*size - 1];   // last element is at size - 1
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
	Node** heap = malloc(HEAP_CAPACITY * sizeof(Node*));
	int size = 0;
	for (int i = 0; i < 256; i++) {
		if (freq[i] > 0) {
			heap_insert(heap, &size, create_node(i, freq[i], NULL, NULL));
			size += 1;
		}
	}
	
	if (size == 0) {
		free(heap);
		return NULL;
	}
	
	while (size > 1) {
		Node* leftNode = heap_extract_min(heap, &size);
		Node* rightNode = heap_extract_min(heap, &size);
		heap_insert(heap, &size, create_node(0, leftNode->frequency + rightNode->frequency, leftNode, rightNode));
		size -= 2;
	}

	Node* root = heap_extract_min(heap, &size);
	free(heap);
	return root;
}
