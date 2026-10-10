#!/usr/bin/env python3
import csv
import sys

path = sys.argv[1] if len(sys.argv) > 1 else "results/results.csv"
with open(path, newline="", encoding="utf-8") as f:
    rows = list(csv.DictReader(f))

if not rows:
    print("No result rows found.")
    raise SystemExit(0)

headers = ["mode", "records", "insert_seconds", "avg_search_us", "peak_rss_mb", "disk_bytes", "tree_height", "page_reads_insert", "page_reads_search", "cache_hit_rate_total"]
widths = {h: max(len(h), *(len(r.get(h, "")) for r in rows)) for h in headers}

print()
print(" | ".join(h.ljust(widths[h]) for h in headers))
print("-+-".join("-" * widths[h] for h in headers))
for r in rows:
    print(" | ".join(r.get(h, "").ljust(widths[h]) for h in headers))
