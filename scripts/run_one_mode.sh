#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 2 ]]; then
  echo "Usage: $0 <memory|disk|hybrid> /path/to/dataset.csv [record_limit]"
  exit 1
fi

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MODE="$1"
DATASET="$2"
LIMIT="${3:-0}"
cd "$ROOT"

if [[ ! -x build/bptree_benchmark ]]; then ./scripts/build.sh; fi
mkdir -p results run_data

ARGS=(--mode "$MODE" --dataset "$DATASET" --search-count 10000 \
      --output results/results.csv --workdir run_data --progress-every 500000 --seed 42)
if [[ "$LIMIT" != "0" ]]; then ARGS+=(--limit "$LIMIT"); fi
if [[ "$MODE" == "hybrid" ]]; then ARGS+=(--cache-pages 4096); fi
if [[ "$MODE" == "memory" ]]; then ARGS+=(--memory-max-keys 100); fi

./build/bptree_benchmark "${ARGS[@]}"
