#!/usr/bin/env python3
"""Optional: create simple benchmark plots from results.csv (requires matplotlib)."""
import csv
import sys
from pathlib import Path

try:
    import matplotlib.pyplot as plt
except ImportError:
    print("matplotlib is not installed. Run: python3 -m pip install matplotlib")
    raise SystemExit(1)

path = Path(sys.argv[1] if len(sys.argv) > 1 else "results/results.csv")
with path.open(newline="", encoding="utf-8") as f:
    rows = list(csv.DictReader(f))

modes = [r["mode"] for r in rows]
outdir = path.parent

charts = [
    ("insert_seconds", "Insertion time (seconds)", "insertion_time.png"),
    ("avg_search_us", "Average search time (microseconds)", "search_time.png"),
    ("peak_rss_mb", "Peak RAM (MB)", "memory_usage.png"),
    ("disk_bytes", "Disk usage (bytes)", "disk_usage.png"),
]

for field, ylabel, filename in charts:
    values = [float(r[field]) for r in rows]
    plt.figure(figsize=(7, 4))
    plt.bar(modes, values)
    plt.ylabel(ylabel)
    plt.xlabel("Implementation")
    plt.tight_layout()
    plt.savefig(outdir / filename, dpi=160)
    plt.close()
    print("Wrote", outdir / filename)
