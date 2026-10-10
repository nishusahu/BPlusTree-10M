#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 /path/to/dataset.csv"
  echo "Example: $0 /home/nishu/Downloads/bplus_dataset.csv"
  exit 1
fi

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DATASET="$1"
cd "$ROOT"

if [[ ! -f "$DATASET" ]]; then
  echo "Dataset not found: $DATASET"
  exit 1
fi
if [[ ! -x build/bptree_benchmark ]]; then
  ./scripts/build.sh
fi

mkdir -p results run_data
rm -f results/results.csv
rm -rf run_data/memory run_data/disk run_data/hybrid

echo "============================================================"
echo "FULL BENCHMARK: ALL CSV RECORDS"
echo "============================================================"
echo "Dataset: $DATASET"
echo "The three modes are separate processes so peak RAM is fair."
echo "Disk/hybrid generated files are placed under run_data/."
echo "============================================================"

COMMON=(--dataset "$DATASET" --search-count 10000 --output results/results.csv \
        --workdir run_data --progress-every 500000 --seed 42)

./build/bptree_benchmark --mode memory "${COMMON[@]}" --memory-max-keys 100
./build/bptree_benchmark --mode disk "${COMMON[@]}"
./build/bptree_benchmark --mode hybrid "${COMMON[@]}" --cache-pages 4096

echo
echo "FULL BENCHMARK COMPLETE"
python3 scripts/summarize_results.py results/results.csv

echo
echo "Raw CSV: results/results.csv"
echo "Optional plots: python3 scripts/plot_results.py results/results.csv"
echo "After saving your results, you may delete large generated files with: rm -rf run_data"
