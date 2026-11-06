#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
	if (argc != 2) {
		printf("Incorrct Usage!");
		return 1;
	}

	int *freq = (int *)calloc(256, sizeof(int));
	if (freq == NULL) {
		printf("Memory Allocation Failed!");
		return 1;
	}

	FILE *fptr = fopen(argv[1], "rb");
	if (fptr == NULL) {
		printf("Error Opening File!");
		return 1;
	}

	int chr;

	while ((chr = fgetc(fptr)) != EOF) {
		putchar(chr);
	}

	fclose(fptr);
	free(freq);

	return 0;
}
