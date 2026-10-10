#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

if [[ ! -x build/bptree_benchmark ]]; then
  ./scripts/build.sh
fi

TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT

python3 tools/generate_test_data.py --rows 20000 --output "$TMP_DIR/test.csv" --seed 42

COMMON=(--dataset "$TMP_DIR/test.csv" --limit 20000 --search-count 1000 \
        --output "$TMP_DIR/results.csv" --workdir "$TMP_DIR/run_data" --progress-every 10000)

./build/bptree_benchmark --mode memory "${COMMON[@]}"
./build/bptree_benchmark --mode disk "${COMMON[@]}"
./build/bptree_benchmark --mode hybrid "${COMMON[@]}" --cache-pages 128

echo
echo "Smoke-test results:"
cat "$TMP_DIR/results.csv"
echo
echo "SUCCESS: all three implementations ran and verified sampled payloads."
