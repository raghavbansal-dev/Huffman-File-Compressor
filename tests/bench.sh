#!/usr/bin/env bash
# Compares mzip against gzip -9 on the given files.
# Usage: tests/bench.sh <path to mzip> <file>...

BIN="$1"
shift
if [ ! -x "$BIN" ] || [ "$#" -eq 0 ]; then
	echo "Usage: $0 <path to mzip> <file>..."
	exit 1
fi

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

# Milliseconds since epoch, for rough timing
now_ms() { date +%s%3N; }

printf '%-28s %12s %12s %8s %12s %8s %10s %10s\n' \
	"file" "original" "mzip" "ratio" "gzip -9" "ratio" "c (ms)" "d (ms)"

for f in "$@"; do
	[ -f "$f" ] || { echo "skipping $f (not a file)"; continue; }

	orig=$(wc -c < "$f")

	t0=$(now_ms)
	"$BIN" c "$f" "$TMP/out.huf" > /dev/null || { echo "mzip failed on $f"; continue; }
	t1=$(now_ms)
	"$BIN" d "$TMP/out.huf" "$TMP/out.raw" > /dev/null || { echo "mzip -d failed on $f"; continue; }
	t2=$(now_ms)
	cmp -s "$f" "$TMP/out.raw" || echo "WARNING: round trip mismatch on $f"

	mz=$(wc -c < "$TMP/out.huf")
	gz=$(gzip -9 -c "$f" | wc -c)

	if [ "$orig" -gt 0 ]; then
		mr=$(awk "BEGIN { printf \"%.1f%%\", 100 * $mz / $orig }")
		gr=$(awk "BEGIN { printf \"%.1f%%\", 100 * $gz / $orig }")
	else
		mr="-"; gr="-"
	fi

	printf '%-28s %12s %12s %8s %12s %8s %10s %10s\n' \
		"$(basename "$f")" "$orig" "$mz" "$mr" "$gz" "$gr" "$((t1 - t0))" "$((t2 - t1))"
done
