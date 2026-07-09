#!/usr/bin/env python3
"""
Lightweight (stdlib-only) propagation-throughput benchmark for NapSAT, meant
for PR perf-regression CI. Two subcommands:

  run       Runs a fixed CNF corpus through NapSAT under a few backtracking
            strategies, and writes a JSON file of propagations/sec etc.

  compare   Compares two JSON files produced by "run" and fails (nonzero
            exit) if any file/strategy pair regressed propagations/sec by
            more than --threshold.

Example:
  python3 scripts/bench.py run tests/cnf/bench --out current.json
  python3 scripts/bench.py compare baseline.json current.json --threshold 0.10
"""
import argparse
import glob
import json
import os
import subprocess
import sys
import time

# Kept small and fixed so PR benchmark runs stay fast and reproducible.
# Bare "-cb" is deliberately excluded: it has no matching invariant
# configuration file (see the same note in scripts/fuzz.py), so it errors
# out whenever the observer is enabled (which -stat requires here).
STRATEGIES = ["", "-gb", "-lscb", "-rscb"]

# See src/solver/SAT-options.cpp:extract_sentinel_tokens for the "-o {...}" syntax.
COMMON_OPTIONS = ["-stat", "-o", "{--check-only}", "-sw", "-si"]


def parse_stats(output: str) -> dict[str, int]:
    """Parses the "c  - Name: value" lines emitted by -stat.

    Mirrors scripts/collect_stats.py::parse_output.
    """
    stats: dict[str, int] = {}
    for line in output.split("\n"):
        if not line.startswith("c  - "):
            continue
        name, _, rest = line[len("c  - "):].partition(":")
        value = rest.strip().replace(",", "")
        if value.isdigit():
            stats[name.strip()] = int(value)
    return stats


def run_once(exec_path: str, cnf_path: str, strategy: str, timeout_s: float) -> tuple[dict, float]:
    args = [exec_path, cnf_path] + [t for t in strategy.split(" ") if t] + COMMON_OPTIONS
    start = time.perf_counter()
    proc = subprocess.run(args, capture_output=True, text=True, timeout=timeout_s)
    elapsed = time.perf_counter() - start
    if proc.returncode != 0 or proc.stderr.strip():
        raise RuntimeError(f"{' '.join(args)} failed (returncode={proc.returncode}): {proc.stderr.strip()[:300]}")
    return parse_stats(proc.stdout), elapsed


def bench_file(exec_path: str, cnf_path: str, strategy: str, repeat: int, timeout_s: float) -> dict:
    elapsed_samples: list[float] = []
    stats: dict[str, int] = {}
    for _ in range(repeat):
        stats, elapsed = run_once(exec_path, cnf_path, strategy, timeout_s)
        elapsed_samples.append(elapsed)
    # Best-case wall-clock time is the least affected by scheduling jitter;
    # propagation/decision counts are deterministic so any repeat's stats work.
    elapsed_s = min(elapsed_samples)
    propagations = stats.get("Propagation", 0)
    decisions = stats.get("Decisions", 0)
    conflicts = stats.get("Conflicts", 0)
    return {
        "elapsed_s": elapsed_s,
        "propagation_count": propagations,
        "decision_count": decisions,
        "conflict_count": conflicts,
        "propagations_per_sec": propagations / elapsed_s if elapsed_s > 0 else 0.0,
        "decisions_per_sec": decisions / elapsed_s if elapsed_s > 0 else 0.0,
    }


def cmd_run(args: argparse.Namespace) -> int:
    if not os.path.isfile(args.exec):
        print(f"error: NapSAT executable not found at {args.exec!r}", file=sys.stderr)
        return 1

    cnf_files = sorted(glob.glob(os.path.join(args.corpus_dir, "*.cnf")))
    if not cnf_files:
        print(f"error: no .cnf files found in {args.corpus_dir!r}", file=sys.stderr)
        return 1

    results: dict[str, dict] = {}
    any_propagations = False
    for cnf_path in cnf_files:
        name = os.path.basename(cnf_path)
        results[name] = {}
        for strategy in STRATEGIES:
            print(f"running {name} {strategy!r}...", file=sys.stderr)
            entry = bench_file(args.exec, cnf_path, strategy, args.repeat, args.timeout)
            results[name][strategy or "(default)"] = entry
            any_propagations = any_propagations or entry["propagation_count"] > 0

    if not any_propagations:
        print(
            "error: every run reported a Propagation count of 0. The 'Propagation'/'Decisions'/"
            "'Conflicts' stats are not wired up (NOTIFY_STAT calls missing at the propagate/decide/"
            "conflict sites) -- see NapSAT.cpp/NapSAT-imply.cpp. Fix the instrumentation before "
            "trusting this benchmark.",
            file=sys.stderr,
        )
        return 1

    with open(args.out, "w") as f:
        json.dump(results, f, indent=2, sort_keys=True)
    print(f"wrote {args.out}", file=sys.stderr)
    return 0


def cmd_compare(args: argparse.Namespace) -> int:
    with open(args.baseline) as f:
        baseline = json.load(f)
    with open(args.current) as f:
        current = json.load(f)

    rows: list[tuple[str, str, float | None, float, float | None, str]] = []
    regressed = False
    for file_name, strategies in current.items():
        for strategy, cur in strategies.items():
            base = baseline.get(file_name, {}).get(strategy)
            if base is None:
                rows.append((file_name, strategy, None, cur["propagations_per_sec"], None, "NEW"))
                continue
            base_pps = base["propagations_per_sec"]
            cur_pps = cur["propagations_per_sec"]
            delta = (cur_pps - base_pps) / base_pps if base_pps > 0 else 0.0
            status = "OK"
            if delta < -args.threshold:
                status = "REGRESSION"
                regressed = True
            rows.append((file_name, strategy, base_pps, cur_pps, delta, status))

    header = f"{'file':<28}{'strategy':<12}{'baseline p/s':>14}{'current p/s':>14}{'delta':>10}  status"
    print(header)
    print("-" * len(header))
    for file_name, strategy, base_pps, cur_pps, delta, status in rows:
        base_str = f"{base_pps:,.0f}" if base_pps is not None else "-"
        delta_str = f"{delta:+.1%}" if delta is not None else "-"
        print(f"{file_name:<28}{strategy:<12}{base_str:>14}{cur_pps:>14,.0f}{delta_str:>10}  {status}")

    if regressed:
        print(f"\nRegression: propagations/sec dropped by more than {args.threshold:.0%} on at least one file/strategy.")
        return 1
    print("\nNo regression beyond threshold.")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)

    p_run = sub.add_parser("run", help="benchmark a corpus and write a JSON report")
    p_run.add_argument("corpus_dir")
    p_run.add_argument("--out", default="bench.json")
    p_run.add_argument("--exec", default="build/NapSAT")
    p_run.add_argument("--repeat", type=int, default=3, help="repeats per file/strategy; best wall-clock time is kept")
    p_run.add_argument("--timeout", type=float, default=30.0)
    p_run.set_defaults(func=cmd_run)

    p_cmp = sub.add_parser("compare", help="compare two JSON reports")
    p_cmp.add_argument("baseline")
    p_cmp.add_argument("current")
    p_cmp.add_argument("--threshold", type=float, default=0.10, help="fractional regression to tolerate, e.g. 0.10 = 10%")
    p_cmp.set_defaults(func=cmd_compare)

    args = parser.parse_args()
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
