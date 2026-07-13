#!/usr/bin/env python3
"""
Merge several per-run CSV files (as produced by collect_stats_vampire.py)
into a single CSV, tagging each row with the "option set" it came from
(derived from the source CSV's filename).
"""

import argparse
import glob
import os
import sys

import pandas as pd


def collect_input_files(paths: list[str]) -> list[str]:
    """Expand a list of files/directories/globs into a flat list of CSV paths."""
    files = []
    for path in paths:
        if os.path.isdir(path):
            files.extend(sorted(glob.glob(os.path.join(path, "*.csv"))))
        elif any(ch in path for ch in "*?["):
            files.extend(sorted(glob.glob(path)))
        else:
            files.append(path)
    return files


def option_set_name(path: str) -> str:
    """Derive the option-set label from a CSV filename (basename without extension)."""
    return os.path.splitext(os.path.basename(path))[0]


def main():
    parser = argparse.ArgumentParser(
        description="Merge per-run stats CSVs into one file, tagging rows with the source option set."
    )
    parser.add_argument(
        "inputs", nargs="+",
        help="CSV files, directories (all *.csv inside), or glob patterns to merge",
    )
    parser.add_argument(
        "-o", "--output", default="merged_stats.csv",
        help="Output CSV path (default: merged_stats.csv)",
    )
    parser.add_argument(
        "-c", "--column", default="option_set",
        help="Name of the column to store the option-set label in (default: option_set)",
    )
    args = parser.parse_args()

    csv_files = collect_input_files(args.inputs)
    if not csv_files:
        print("Error: no CSV files found", file=sys.stderr)
        sys.exit(1)

    frames = []
    for path in csv_files:
        if not os.path.isfile(path):
            print(f"Warning: {path} not found, skipping", file=sys.stderr)
            continue
        df = pd.read_csv(path)
        df.insert(0, args.column, option_set_name(path))
        frames.append(df)

    if not frames:
        print("Error: no CSV files could be read", file=sys.stderr)
        sys.exit(1)

    merged = pd.concat(frames, ignore_index=True, sort=False)
    # sort the columns such that the problem column is first, then the option_set column, then the rest
    cols = list(merged.columns)
    if "file" in cols:
        cols.remove("file")
        cols.insert(0, "file")
    if args.column in cols:
        cols.remove(args.column)
        cols.insert(1, args.column)
    merged = merged[cols]

    # sort the rows by the problem column, then by the option_set column
    if "file" in merged.columns and args.column in merged.columns:
        merged.sort_values(by=["file", args.column], inplace=True, ignore_index=True)
    merged.to_csv(args.output, index=False)
    print(
        f"Merged {len(frames)} files into {len(merged)} rows, "
        f"{len(merged.columns)} columns -> {args.output}"
    )


if __name__ == "__main__":
    main()
