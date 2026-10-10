# BPlusTree-10M

C++17 implementation and performance comparison of three B+ Tree storage strategies on a shuffled dataset of 10,000,000 unique key-payload records:

- Memory-only B+ Tree
- Disk-only paged B+ Tree
- Hybrid memory + disk B+ Tree with LRU page cache

## Final 10M Results

| Metric | Memory | Disk | Hybrid |
|---|---:|---:|---:|
| Insertion time | 18.254 s | 154.679 s | 143.018 s |
| Avg. search | 1.690 us | 13.088 us | 11.004 us |
| Peak RSS | 1685.76 MB | 5.61 MB | 20.29 MB |
| Disk usage | 0 | 1212.17 MB | 1212.17 MB |
| Tree height | 4 | 4 | 4 |
| Insert page reads | 0 | 39,670,624 | 9,670,360 |
| Search page reads | 0 | 40,000 | 12,559 |
| Cache hit rate | N/A | N/A | 75.62% |
| Verification failures | 0 | 0 | 0 |

See `docs/FINAL_REPORT.md` for methodology, assumptions, results, and discussion.

## Dataset

Expected CSV format:

```text
key,payload
K026A13E,...
K00B8035,...
```

The assignment dataset itself is intentionally not committed to Git because of its size.

Observed dataset properties:

- 10,000,000 records
- unique 8-character keys
- 64-character payloads
- shuffled insertion order

## Build

```bash
./scripts/build.sh
```

or manually:

```bash
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
```

## Smoke Test

```bash
./scripts/smoke_test.sh
```

## Quick Benchmark

```bash
./scripts/run_quick_benchmark.sh "/path/to/dataset.csv" 100000
```

## Full 10M Benchmark

```bash
./scripts/run_full_benchmark.sh "/path/to/dataset.csv"
```

## Results

The repository includes compact benchmark outputs for:

- 100K records
- 1M records
- 10M records

Generated B+ Tree data files and the original dataset are excluded from version control.
