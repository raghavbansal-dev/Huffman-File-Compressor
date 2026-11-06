#ifndef HUFFMAN_H
#define HUFFMAN_H

// Both return 0 on success and 1 on failure, so main can use them as the exit code.
// With verbose set, they also print the frequency and code table.
int compress(const char *inPath, const char *outPath, int verbose);
int decompress(const char *inPath, const char *outPath, int verbose);

#endif
