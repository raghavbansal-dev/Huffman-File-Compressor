#ifndef TREE_H
#define TREE_H

// A tree node. Leaves hold an actual byte; internal nodes just hold
// the combined frequency of everything below them.
typedef struct Node {
	unsigned char byte;
	unsigned long frequency;
	struct Node *leftChild;
	struct Node *rightChild;
} Node;

// Longest possible code is 255 bits (256 leaves in one long chain), +1 for '\0'
#define MAX_CODE_LEN 256

Node *create_node(unsigned char byte, unsigned long frequency, Node *left, Node *right);
Node *build_tree(const unsigned long *freq);
void free_tree(Node *node);

// Fills codes[byte] with that byte's code as a string of '0'/'1'.
// Bytes that don't appear stay as "". Handles empty and single-symbol trees.
void build_codes(const Node *root, char codes[256][MAX_CODE_LEN]);

// Total size of the encoded data in bits (sum of frequency * depth)
unsigned long encoded_bits(const Node *node, int depth);

#endif
