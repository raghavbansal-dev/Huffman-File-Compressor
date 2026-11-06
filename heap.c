#include <stdio.h>

void swap(int *a, int *b);

int main(void) {
	int size;
	scanf("%d", &size);

	int s[size];

}

void swap(int *a, int *b) {
	int temp = *a;
	*a = *b;
	*b = temp;
}
