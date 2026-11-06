#include <stdio.h>
#include <ctype.h>

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
