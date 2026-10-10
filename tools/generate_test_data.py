#!/usr/bin/env python3
"""Generate a shuffled CSV with the same key,payload shape as the assignment."""

import argparse
import csv
import random
import string


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--rows", type=int, default=10000)
    ap.add_argument("--output", default="data/test_dataset.csv")
    ap.add_argument("--seed", type=int, default=42)
    args = ap.parse_args()

    rng = random.Random(args.seed)
    indices = list(range(args.rows))
    rng.shuffle(indices)
    alphabet = string.ascii_letters + string.digits

    with open(args.output, "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["key", "payload"])
        for i in indices:
            key = f"K{i:08X}"
            payload = "".join(rng.choices(alphabet, k=64))
            w.writerow([key, payload])

    print(f"Wrote {args.rows:,} rows to {args.output}")


if __name__ == "__main__":
    main()
