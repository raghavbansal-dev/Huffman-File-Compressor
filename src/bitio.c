#include "bitio.h"

// Shift everything left to make room, then drop the new bit in on the right.
// Once we have 8, the byte is done.
void write_bit(BitWriter *bw, int bit) {
	bw->buffer = (bw->buffer << 1) | bit;
	bw->count += 1;
	if (bw->count == 8) {
		fputc(bw->buffer, bw->fp);
		bw->buffer = 0;
		bw->count = 0;
	}
}

// Turns a code string like "01" into actual bits
void write_code(BitWriter *bw, const char *code) {
	for (int i = 0; code[i] != '\0'; i++) {
		write_bit(bw, code[i] == '1' ? 1 : 0);
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
