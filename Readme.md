# mzip

mzip is a command-line file compressor I wrote in C. It uses Huffman coding, works on any kind of file (text, images, executables, whatever), and decompressing gives you back the exact original, byte for byte.

```
$ ./mzip c pride_and_prejudice.txt book.huf
Original:   717602 bytes
Compressed: 409736 bytes (57.1% of original)

$ ./mzip d book.huf book.txt
Compressed:   409736 bytes
Decompressed: 717602 bytes
```

## Getting started

You need `gcc`, `make` and a Linux or macOS terminal. On Windows, use WSL.

```
git clone https://github.com/raghavbansal-dev/huffmanFileCompressor.git
cd huffmanFileCompressor
make
```

`make` compiles everything into a single program called `mzip` in the project folder. If you change any source file, just run `make` again and it rebuilds.

## How to use it

The general form is:

```
./mzip [-v] c|d <input> <output>
```

`c` compresses, `d` decompresses. Input and output have to be different files.

**Compress a file**

```
./mzip c notes.txt notes.huf
```

**Decompress it**

```
./mzip d notes.huf notes_restored.txt
```

**Check you got the same file back**

```
cmp notes.txt notes_restored.txt
```

No output from `cmp` means the two files are identical.

**See what Huffman did (verbose mode)**

Put `-v` before `c` or `d` and mzip prints every byte that appears in the file, how many times it appears, and the code it was given:

```
$ ./mzip -v c demo.txt demo.huf
29 distinct bytes

 byte  char       count  code
   32  ' '           15  111
  101  'e'           10  100
  115  's'            9  010
  ...
  106  'j'            1  0110110

Average code length: 4.25 bits per byte (uncompressed: 8)

Original:   84 bytes
Compressed: 308 bytes (366.7% of original)
  header:   263 bytes
  data:     45 bytes
```

Space shows up 15 times and gets a 3-bit code, `j` shows up once and gets 7 bits. That's the whole trick. (This file is tiny, which is why it got bigger overall. See Limitations.)

**Help**

```
./mzip -h
```

**A note on `.huf`**

It's just the extension I use for compressed files. mzip doesn't look at file names at all. When decompressing it checks the file's header, so if you give it something that isn't an mzip file, you get an error instead of garbage.

**When something goes wrong**

Bad arguments, missing files, a file that isn't an mzip file, or a compressed file that's been cut short all give an error message and exit code 1. If decompression fails halfway, the partly written output file is deleted so you don't end up with a broken file lying around.

## Make commands

| command | what it does |
|---|---|
| `make` | builds `mzip` |
| `make test` | builds a debug copy with memory checking (`mzip-debug`) and runs the test suite |
| `make bench FILES="a.txt b.bin"` | compresses the files with both mzip and `gzip -9` and prints a comparison |
| `make clean` | deletes `mzip` and `mzip-debug` |

## How it works

Normally every byte takes 8 bits. Huffman coding gives the bytes that show up a lot shorter codes, and the rare ones longer codes, so the total comes out smaller.

**Compressing**

1. Read the file and count how many times each of the 256 possible byte values appears.
2. Make a leaf node for every byte that appears and put them all in a min-heap, smallest count on top.
3. Take the two smallest nodes out, join them under a new node whose count is their sum, and put that node back. Repeat until one node is left. That's the root of the Huffman tree.
4. Each byte's code is the path from the root to its leaf: 0 for going left, 1 for going right. Common bytes end up close to the root, so their codes are short.
5. Write a header with the counts, then go back to the start of the input and write every byte's code. Codes aren't a whole number of bytes, so a small bit writer packs them 8 at a time and pads the last byte with zeros.

Bytes only ever sit at leaves, so no code is the start of another code. That's what lets the decoder read the bits back without anything separating them.

**Decompressing**

1. Read the counts from the header and build the tree again. Same counts plus same algorithm means the exact same tree.
2. Read the data one bit at a time, going left on 0 and right on 1, until you land on a leaf. Write that byte and jump back to the root.
3. The counts add up to the original file size, so mzip stops after exactly that many bytes and never decodes the padding at the end.

## File format

| field | size |
|---|---|
| number of distinct bytes (0 to 256) | 2 bytes |
| then for each one: the byte | 1 byte |
| how many times it appears | 8 bytes |
| packed codes | rest of the file |

Numbers are stored in the machine's own byte order, which is little-endian on x86 and ARM. For example, `aaaabbc` compresses to these 31 bytes:

```
03 00                         3 distinct bytes
61 04 00 00 00 00 00 00 00    'a', 4 times
62 02 00 00 00 00 00 00 00    'b', 2 times
63 01 00 00 00 00 00 00 00    'c', once
f5 00                         codes 1 1 1 1 01 01 00, then padding
```

## Results

Compressed size compared with `gzip -9` (smaller is better):

| file | original size | mzip | gzip -9 |
|---|---|---|---|
| Alice in Wonderland | 167,546 | 59.0% | 36.1% |
| Pride and Prejudice | 717,602 | 57.1% | 36.0% |
| GPL-3 license | 35,149 | 59.6% | 34.5% |
| mzip's own source | 18,340 | 69.1% | 32.1% |
| mzip binary | 25,968 | 64.0% | 33.7% |
| 1 MB of random bytes | 1,000,000 | 100.2% | 100.0% |

Pride and Prejudice copied 20 times (about 14 MB) takes around half a second to compress and the same to decompress.

gzip does a lot better, and that's expected. Huffman coding only knows how often each byte appears. It doesn't notice that words like "the" or "return" keep repeating. gzip first replaces repeated strings with short references to where they appeared before, and only then uses Huffman coding on what's left. Random data can't be compressed by anything, and mzip's output is a little bigger than the input because of the header.

## Tests

`make test` builds a copy of mzip with AddressSanitizer and UndefinedBehaviorSanitizer switched on, so any memory bug makes a test fail, and then runs `tests/run_tests.sh`.

The script compresses and decompresses a set of files and checks with `cmp` that each one comes back identical: an empty file, a single byte, one byte repeated, normal text, every byte value from 0 to 255, random data, a large file, and the mzip binary itself. It also checks that `aaaabbc` produces exactly the bytes shown in the file format section, and that every kind of bad input gets a clean error instead of a crash.

## Code layout

```
src/main.c       reads the command-line arguments
src/huffman.c    compress, decompress, reading and writing the header
src/tree.c       building the Huffman tree and the code table
src/heap.c       the min-heap used to build the tree
src/bitio.c      writing and reading one bit at a time
tests/           test script and benchmark script
Makefile
```

## Limitations

- Small files get bigger. Each distinct byte costs 9 bytes of header, so a short text file pays a few hundred bytes before it saves anything. Files need to be a few KB before compression wins.
- The header uses the machine's byte order, so a file compressed on a big-endian machine won't decompress on a little-endian one. Not an issue on normal computers, but worth knowing.
- The input is read twice, once to count and once to encode, so mzip can't compress data piped into it.
- Codes are stored as strings of `'0'` and `'1'` characters and written one bit at a time. Simple and easy to debug, but not the fastest way.

## Things I want to add

- Canonical Huffman codes. Store only each byte's code length (1 byte) instead of its count (8 bytes) and rebuild the codes from the lengths using a fixed rule. This is what zip, gzip and PNG do, and it would make the header much smaller.
- A fixed byte order for the header, so files work on any machine.
- Storing codes as integers and writing several bits at once, for speed.
- Reading from stdin and writing to stdout.
- An LZ77 step before the Huffman step, which is what would get it close to gzip.
