#ifndef BITIO_H
#define BITIO_H

#include <stdio.h>

// Collects bits until it has a full byte, then writes that byte out
typedef struct BitWriter {
	FILE *fp;
	unsigned char buffer;
	int count;   // bits currently in buffer
} BitWriter;

// The reverse: loads a byte and hands its bits out one at a time, leftmost first
typedef struct BitReader {
	FILE *fp;
	unsigned char buffer;
	int count;   // bits left in buffer
} BitReader;

void write_bit(BitWriter *bw, int bit);
void write_code(BitWriter *bw, const char *code);
void flush_bits(BitWriter *bw);

// Returns the next bit (0 or 1), or -1 if the file has run out
int read_bit(BitReader *br);

#endif
