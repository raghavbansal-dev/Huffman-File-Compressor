#!/usr/bin/env bash
# Round-trip and error-handling tests for mzip.
# Usage: tests/run_tests.sh [path to binary]   (default: ./mzip)

BIN="${1:-./mzip}"
if [ ! -x "$BIN" ]; then
	echo "Binary not found: $BIN (run make first)"
	exit 1
fi

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

pass=0
fail=0

result() {
	if [ "$2" -eq 0 ]; then
		echo "  PASS  $1"
		pass=$((pass + 1))
	else
		echo "  FAIL  $1"
		fail=$((fail + 1))
	fi
}

# Compress, decompress, and check the result is byte-for-byte identical
roundtrip() {
	local file="$1" flags="$2"
	"$BIN" $flags c "$file" "$TMP/rt.huf" > /dev/null 2> "$TMP/err" &&
	"$BIN" $flags d "$TMP/rt.huf" "$TMP/rt.out" > /dev/null 2>> "$TMP/err" &&
	cmp -s "$file" "$TMP/rt.out" &&
	! grep -q Sanitizer "$TMP/err"
}

# The command should fail cleanly: exit code 1, an error message, no sanitizer report
expect_error() {
	"$@" > /dev/null 2> "$TMP/err"
	local code=$?
	[ "$code" -eq 1 ] && [ -s "$TMP/err" ] && ! grep -q Sanitizer "$TMP/err"
}

# ---- test inputs ----
: > "$TMP/empty.txt"
printf 'x' > "$TMP/one_byte.txt"
printf 'aaaa' > "$TMP/one_symbol.txt"
printf 'aaaabbc' > "$TMP/aaaabbc.txt"
printf 'the quick brown fox jumps over the lazy dog.\nshe sells sea shells by the sea shore.\n' > "$TMP/text.txt"
for i in $(seq 0 255); do printf "\\$(printf '%03o' "$i")"; done > "$TMP/one_each.bin"
for i in $(seq 1 40); do cat "$TMP/one_each.bin"; done > "$TMP/all_bytes.bin"
head -c 200000 /dev/urandom > "$TMP/random.bin"
for i in $(seq 1 100); do cat src/*.c 2>/dev/null || cat "$BIN"; done > "$TMP/large.txt"

echo "Round trips"
for f in empty.txt one_byte.txt one_symbol.txt aaaabbc.txt text.txt one_each.bin all_bytes.bin random.bin large.txt; do
	roundtrip "$TMP/$f" ""
	result "$f" $?
done
roundtrip "$BIN" ""
result "the binary itself" $?
roundtrip "$TMP/text.txt" "-v"
result "text.txt with -v" $?

echo "File format"
# Exact bytes for "aaaabbc": count 3, then a/4 b/2 c/1 as 8-byte counts, then data f5 00
printf '\x03\x00a\x04\x00\x00\x00\x00\x00\x00\x00b\x02\x00\x00\x00\x00\x00\x00\x00c\x01\x00\x00\x00\x00\x00\x00\x00\xf5\x00' > "$TMP/expected.huf"
"$BIN" c "$TMP/aaaabbc.txt" "$TMP/actual.huf" > /dev/null
cmp -s "$TMP/expected.huf" "$TMP/actual.huf"
result "aaaabbc produces the documented bytes" $?

echo "Error handling"
"$BIN" c "$TMP/aaaabbc.txt" "$TMP/good.huf" > /dev/null
head -c 20 "$TMP/good.huf" > "$TMP/cut_header.huf"
head -c 30 "$TMP/good.huf" > "$TMP/cut_data.huf"
printf '\x02\x00a\x01\x00\x00\x00\x00\x00\x00\x00a\x01\x00\x00\x00\x00\x00\x00\x00' > "$TMP/dup_byte.huf"

expect_error "$BIN";                                              result "no arguments" $?
expect_error "$BIN" c "$TMP/text.txt";                            result "missing output argument" $?
expect_error "$BIN" x "$TMP/text.txt" "$TMP/o";                   result "unknown mode" $?
expect_error "$BIN" -v c "$TMP/text.txt";                         result "-v with missing argument" $?
expect_error "$BIN" c "$TMP/nope.txt" "$TMP/o";                   result "missing input file" $?
expect_error "$BIN" c "$TMP/text.txt" "$TMP/text.txt";            result "same input and output" $?
expect_error "$BIN" c "$TMP/text.txt" "$TMP/no_dir/out.huf";      result "output in missing directory" $?
expect_error "$BIN" d "$TMP/text.txt" "$TMP/o";                   result "decompress a non-.huf file" $?
expect_error "$BIN" d "$TMP/empty.txt" "$TMP/o";                  result "decompress an empty file" $?
expect_error "$BIN" d "$TMP/cut_header.huf" "$TMP/o";             result "truncated header" $?
expect_error "$BIN" d "$TMP/dup_byte.huf" "$TMP/o";               result "repeated byte in header" $?
expect_error "$BIN" d "$TMP/cut_data.huf" "$TMP/partial.out";     result "truncated data" $?
[ ! -e "$TMP/partial.out" ];                                      result "truncated data leaves no partial output" $?

echo
echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
