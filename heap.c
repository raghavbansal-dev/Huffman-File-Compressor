#include <stdio.h>
#define HEAP_CAPACITY 256

void swap(int *a, int *b);
void sift_up(int *arr, int index);
void sift_down(int *arr, int size, int index);
void heap_insert(int *arr, int *size, int value);
int heap_extract_min(int *arr, int *size);
void print_heap(int *arr, int size);

int main(void) {
	int size = 0;
	int heap[HEAP_CAPACITY];

	// Test 1: worked example (expect final heap: 1 3 8 5)
	int values[] = {5, 3, 8, 1};
	for (int i = 0; i < 4; i++) {
		heap_insert(heap, &size, values[i]);
		print_heap(heap, size);
	}

	// Test 2: extract all (expect 1 3 5 8)
	while (size > 0) {
		printf("%d ", heap_extract_min(heap, &size));
	}
	printf("\n");

	// Test 3: extract from empty heap (expect error message, no crash)
	heap_extract_min(heap, &size);

	return 0;
}

void swap(int *a, int *b) {
	int temp = *a;
	*a = *b;
	*b = temp;
}

void sift_up(int *arr, int index) {
	if (index == 0) {
		return;
	}

	int pIndex = (index - 1) / 2;
	if (arr[pIndex] <= arr[index]) {
		return;
	}

	swap(&arr[pIndex], &arr[index]);
	sift_up(arr, pIndex);
}

void sift_down(int *arr, int size, int index) {
	int lIndex = 2 * index + 1;
	int rIndex = 2 * index + 2;
	int smallest = index;

	// A child exists only if its index is < size
	if (lIndex < size && arr[lIndex] < arr[smallest]) {
		smallest = lIndex;
	}
	if (rIndex < size && arr[rIndex] < arr[smallest]) {
		smallest = rIndex;
	}

	// Parent is already the smallest (this also covers leaves): heap is valid
	if (smallest == index) {
		return;
	}

	swap(&arr[index], &arr[smallest]);
	sift_down(arr, size, smallest);
}

void heap_insert(int *arr, int *size, int value) {
	if (*size == HEAP_CAPACITY) {
		fprintf(stderr, "heap_insert: heap is full\n");
		return;
	}
	arr[*size] = value;
	sift_up(arr, *size);
	*size += 1;
}

int heap_extract_min(int *arr, int *size) {
	if (*size == 0) {
		fprintf(stderr, "heap_extract_min: heap is empty\n");
		return -1;
	}

	int root = arr[0];
	arr[0] = arr[*size - 1];  
	*size -= 1;
	sift_down(arr, *size, 0);

	return root;
}

void print_heap(int *arr, int size) {
	for (int i = 0; i < size; i++) {
		printf("%d ", arr[i]);
	}
	printf("\n");
}
