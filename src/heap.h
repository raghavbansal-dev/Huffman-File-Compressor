#ifndef HEAP_H
#define HEAP_H

#include "tree.h"

// Huffman never has more than 256 nodes in the heap at once:
// at most 256 leaves go in, and every merge takes out two and puts back one
#define HEAP_CAPACITY 256

// A min-heap of node pointers, ordered by frequency.
// The caller owns the array and the size counter.
void heap_insert(Node **arr, int *size, Node *node);
Node *heap_extract_min(Node **arr, int *size);

#endif
