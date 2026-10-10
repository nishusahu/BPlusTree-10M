#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 /path/to/dataset.csv [record_limit]"
  exit 1
fi

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DATASET="$1"
LIMIT="${2:-100000}"
cd "$ROOT"

if [[ ! -f "$DATASET" ]]; then
  echo "Dataset not found: $DATASET"
  exit 1
fi
if [[ ! -x build/bptree_benchmark ]]; then
  ./scripts/build.sh
fi

rm -f results/quick_results.csv
rm -rf run_data/quick
mkdir -p run_data/quick

COMMON=(--dataset "$DATASET" --limit "$LIMIT" --search-count 2000 \
        --output results/quick_results.csv --workdir run_data/quick --progress-every 50000)

./build/bptree_benchmark --mode memory "${COMMON[@]}"
./build/bptree_benchmark --mode disk "${COMMON[@]}"
./build/bptree_benchmark --mode hybrid "${COMMON[@]}" --cache-pages 4096

echo
echo "Quick benchmark finished: results/quick_results.csv"
python3 scripts/summarize_results.py results/quick_results.csv
