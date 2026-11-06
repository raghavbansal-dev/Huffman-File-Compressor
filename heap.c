#include <stdio.h>

void swap(int *a, int *b);
void sift_up(int arr[], int index);
void print_heap(int *arr, int size);

int main(void) {
	int size;
	scanf("%d", &size);

	int s[size];

	int t[3] = {9,8,6};
	sift_up(t, 2);
	print_heap(t, 3);

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

void print_heap(int *arr, int size) {
	for (int i = 0; i < size; i++) {
		printf("%d", arr[i]);
	}
}
