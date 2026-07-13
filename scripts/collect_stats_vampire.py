#!/usr/bin/env python3
"""
Collect Vampire (NapSAT) statistics from a directory of BenchExec log files
into a single CSV, one row per log file and one column per statistic.

Only lines of the form "c  - STAT_NAME: value" are collected (the nested
statistics printed under a "c N. Section:" header). Numeric values are
converted to int/float; "Xs Yms" style durations are converted to
milliseconds. Everything else is kept as a string.

The benchmark's return status and cpu time are read from the companion
BenchExec results file: for a log directory "<name>.logfiles", the results
file is expected at "<name>.results.stable.txt" in the same parent
directory.
"""

import argparse
import os
import re
import sys

import pandas as pd

STAT_LINE_RE = re.compile(r"^c  - (.+?):(.+)$")
RESULT_LINE_RE = re.compile(
    r"^(\S+)\s+(.*?)\s+(\d+(?:\.\d+)?)\s+(\d+(?:\.\d+)?)\s+None(.*)$"
)
INT_RE = re.compile(r"-?\d+")
FLOAT_RE = re.compile(r"-?\d+\.\d+")
TIME_RE = re.compile(r"(?:(\d+)s)?\s*(?:(\d+)ms)?$")


def parse_value(raw: str):
    """Convert a raw statistic value to an int/float when possible."""
    value = raw.strip().replace(",", "")
    if INT_RE.fullmatch(value):
        return int(value)
    if FLOAT_RE.fullmatch(value):
        return float(value)
    time_match = TIME_RE.fullmatch(value)
    if time_match and (time_match.group(1) or time_match.group(2)):
        seconds = int(time_match.group(1) or 0)
        millis = int(time_match.group(2) or 0)
        return seconds * 1000 + millis
    return value


def parse_log_file(path: str) -> dict:
    """Extract every 'c  - NAME: value' statistic from a solver log."""
    stats = {}
    with open(path, "r", errors="replace") as f:
        for line in f:
            match = STAT_LINE_RE.match(line.rstrip())
            if not match:
                continue
            name = match.group(1).strip()
            stats[name] = parse_value(match.group(2))
    return stats


def parse_results_file(path: str) -> dict:
    """Map input-file basename -> {status, cpu_time}, from a BenchExec results.txt."""
    results = {}
    with open(path, "r", errors="replace") as f:
        lines = f.readlines()

    header_idx = next(
        (i for i, line in enumerate(lines) if line.startswith("inputfile")), None
    )
    if header_idx is None:
        print(f"Warning: could not find results table header in {path}", file=sys.stderr)
        return results

    for line in lines[header_idx + 2:]:
        line = line.rstrip()
        if not line.strip() or set(line.strip()) == {"-"}:
            break
        match = RESULT_LINE_RE.match(line)
        if not match:
            continue
        inputfile, status, cpu_time = match.group(1), match.group(2).strip(), match.group(3)
        results[os.path.basename(inputfile)] = {
            "status": status,
            "cpu_time": float(cpu_time),
        }
    return results


def results_file_for(log_dir: str) -> str:
    """Compute the path of the BenchExec results.txt matching a '*.logfiles' directory."""
    log_dir = log_dir.rstrip("/")
    if log_dir.endswith(".logfiles"):
        base = log_dir[: -len(".logfiles")]
    else:
        base = log_dir
    return base + ".results.stable.txt"


def basename_for_log(filename: str) -> str:
    """Recover the TPTP problem basename from a '<runset>.<basename>.log' filename."""
    name = filename[:-len(".log")] if filename.endswith(".log") else filename
    _, _, basename = name.partition(".")
    return basename or name


def main():
    parser = argparse.ArgumentParser(
        description="Collect Vampire statistics from a directory of log files into a CSV."
    )
    parser.add_argument(
        "directory",
        help="Directory containing the *.log files (a BenchExec '*.logfiles' directory)",
    )
    parser.add_argument(
        "-o", "--output", default=None,
        help="Output CSV path (default: <directory-name>.csv)",
    )
    parser.add_argument(
        "-r", "--results-file", default=None,
        help="Path to the BenchExec results .txt file (default: derived from the directory name)",
    )
    args = parser.parse_args()

    directory = args.directory
    if not os.path.isdir(directory):
        print(f"Error: {directory} is not a directory", file=sys.stderr)
        sys.exit(1)

    results_path = args.results_file or results_file_for(directory)
    results = {}
    if os.path.isfile(results_path):
        results = parse_results_file(results_path)
    else:
        print(
            f"Warning: results file {results_path} not found, "
            "'status'/'cpu_time' columns will be empty",
            file=sys.stderr,
        )

    rows = []
    column_order = ["file", "status", "cpu_time"]
    seen_columns = set(column_order)

    log_files = sorted(
        f for f in os.listdir(directory) if os.path.isfile(os.path.join(directory, f))
    )
    for filename in log_files:
        stats = parse_log_file(os.path.join(directory, filename))
        basename = basename_for_log(filename)
        result = results.get(basename, {})
        row = {
            "file": basename,
            "status": result.get("status"),
            "cpu_time": result.get("cpu_time"),
        }
        row.update(stats)
        rows.append(row)
        for key in stats:
            if key not in seen_columns:
                seen_columns.add(key)
                column_order.append(key)

    if not rows:
        print(f"No log files found in {directory}", file=sys.stderr)
        sys.exit(1)

    df = pd.DataFrame(rows, columns=column_order)

    output_path = args.output or (os.path.basename(directory.rstrip("/")) + ".csv")
    df.to_csv(output_path, index=False)
    print(f"Wrote {len(df)} rows and {len(df.columns)} columns to {output_path}")


if __name__ == "__main__":
    main()
