#!/usr/bin/env python3
"""
Fuzzer for NapSAT, built on two oracle-free techniques:

  func  Metamorphic testing: reorder clauses/literals (must preserve the
        SAT/UNSAT verdict exactly), and add/delete clauses (SAT/UNSAT can
        only move in one monotonic direction). Also cross-checks that every
        backtracking strategy in STRATEGIES agrees on the verdict. Uses the
        transformation logic from third_party/fuzzing-SAT-solvers/func.py.

  ub    Crash/UB fuzzing: feeds malformed and adversarial DIMACS input
        (borrowed from third_party/fuzzing-SAT-solvers/ub.py) to a
        sanitizer-instrumented build and checks stderr for ASan/UBSan
        reports.

The third_party/fuzzing-SAT-solvers submodule (an academic exercise built
around tiny single-file C solvers) is not used for its suts/*.py plumbing --
that assumes a `gcc *.c -o sat` build and a `./runsat.sh` wrapper that don't
fit NapSAT's real C++20/submodule build. Only its formula-transformation and
input-generation *functions* are reused; this script drives build/NapSAT
directly with its actual CLI.

Usage:
  python3 scripts/fuzz.py func --count 150 --max-vars 80 --timeout 5 --seed 1
  DBG_FLAGS='-O0 -g -fsanitize=address,undefined' make debug
  python3 scripts/fuzz.py ub --count 300 --exec build/NapSAT --timeout 5
"""
import argparse
import concurrent.futures
import os
import random
import subprocess
import sys
import threading

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "third_party", "fuzzing-SAT-solvers"))
import func as sat_func  # noqa: E402  (third_party/fuzzing-SAT-solvers/func.py)
import ub as sat_ub      # noqa: E402  (third_party/fuzzing-SAT-solvers/ub.py)

# Backtracking strategies that must agree on every input. Mirrors the option
# lists already used by scripts/find-bugs.py and scripts/collect_stats.py.
# Bare "-cb" is deliberately excluded: it has no matching invariant
# configuration file (invariant-configurations/chronological-backtracking.conf
# does not exist -- see NapSAT-checker.cpp:load_invariant_configuration), so
# it fails to load whenever the observer is enabled, independent of any bug
# in the solver itself. CLAUDE.md also documents "-cb" as not meant to be
# used standalone.
STRATEGIES = ["", "-rscb", "-lscb", "-gb", "-gb -lcm", "-gb -bl"]

# Passed on every run: silence warnings/info, drive the observer in
# non-interactive mode (see src/solver/SAT-options.cpp:extract_sentinel_tokens),
# and check the resolution proof whenever the solver claims UNSAT.
COMMON_OPTIONS = ["-sw", "-si", "-o", "{--check-only}", "-cp"]

CLAUSE_TO_VAR_RATIO = 4.26  # near the 3-SAT phase transition: hard instances


# --------------------------------------------------------------------------
# Formula generation (ours; the submodule's inputs/*.cnf are a fixed set of
# 20 tiny toy-solver instances, too small/few for this solver).
# --------------------------------------------------------------------------

def generate_instance(n_vars: int, n_clauses: int, rng: random.Random) -> list[list[int]]:
    """Generates a random 3-CNF instance as a list of clauses.

    Each clause ends with a trailing 0, matching the raw `line.split()` token
    layout that third_party/fuzzing-SAT-solvers/func.py's transformations
    (swap_literals in particular, which drops+re-appends the last token)
    expect a DIMACS clause line to have.
    """
    clause_set: set[tuple[int, ...]] = set()
    while len(clause_set) < n_clauses:
        clause: set[int] = set()
        while len(clause) < 3:
            var = rng.randint(1, n_vars)
            if rng.random() < 0.5:
                var = -var
            if var not in clause and -var not in clause:
                clause.add(var)
        clause_set.add(tuple(sorted(clause)))
    return [list(clause) + [0] for clause in clause_set]


def dedupe_clause(clause: list[int]) -> list[int]:
    """Drops repeated literals (keeping the trailing 0 terminator last).

    sat_func.transform()'s "add clauses" transformations draw literals
    independently and can repeat one within a clause. A clause with a
    duplicate literal of size >= 2 currently crashes NapSAT (see
    KNOWN-ISSUES.md); that's a real, already-documented solver bug, not
    something this fuzzer needs to keep rediscovering, so new clauses are
    de-duplicated before being written out.
    """
    seen: list[int] = []
    for lit in clause:
        if lit != 0 and lit not in seen:
            seen.append(lit)
    seen.append(0)
    return seen


def write_dimacs(clauses: list[list[int]], n_vars: int, path: str) -> None:
    """Writes clauses that already end with a trailing 0 (see generate_instance)."""
    with open(path, "w") as f:
        f.write(f"p cnf {n_vars} {len(clauses)}\n")
        for clause in clauses:
            f.write(" ".join(map(str, clause)) + "\n")


def parse_status(stdout: str) -> str:
    if "s SATISFIABLE" in stdout:
        return "SAT"
    if "s UNSATISFIABLE" in stdout:
        return "UNSAT"
    if "s TIMEOUT" in stdout:
        return "TIMEOUT"
    return "UNKNOWN"


def run_strategy(exec_path: str, cnf_path: str, strategy: str, timeout_s: float) -> dict:
    args = [exec_path, cnf_path] + [t for t in strategy.split(" ") if t] + COMMON_OPTIONS + ["-t", str(timeout_s * 1000)]
    try:
        proc = subprocess.run(args, capture_output=True, text=True, timeout=timeout_s + 5)
    except subprocess.TimeoutExpired:
        return {"strategy": strategy, "status": "HANG", "returncode": None,
                "stderr": f"process did not exit within {timeout_s + 5}s (options: {strategy!r})", "args": args}
    return {
        "strategy": strategy,
        "status": parse_status(proc.stdout),
        "returncode": proc.returncode,
        "stderr": proc.stderr.strip(),
        "args": args,
    }


def run_all_strategies(exec_path: str, cnf_path: str, timeout_s: float) -> list[dict]:
    return [run_strategy(exec_path, cnf_path, s, timeout_s) for s in STRATEGIES]


def find_crash_or_disagreement(results: list[dict], expected: str | None = None) -> list[str]:
    """expected: "SAT", "UNSAT", or None (unconstrained -- e.g. the tool's "UNKNOWN" relation)."""
    failures = []
    statuses = set()
    for r in results:
        if r["returncode"] not in (0, None) or r["status"] == "HANG":
            failures.append(f"strategy {r['strategy']!r} crashed/hung: returncode={r['returncode']} stderr={r['stderr'][:300]!r}")
        elif r["stderr"]:
            failures.append(f"strategy {r['strategy']!r} printed to stderr: {r['stderr'][:300]!r}")
        if r["status"] in ("SAT", "UNSAT"):
            statuses.add(r["status"])
    if len(statuses) > 1:
        detail = ", ".join(f"{r['strategy']!r}={r['status']}" for r in results)
        failures.append(f"strategies disagree on satisfiability: {detail}")
    if expected is not None and statuses and statuses != {expected}:
        detail = ", ".join(f"{r['strategy']!r}={r['status']}" for r in results)
        failures.append(f"expected {expected} (by metamorphic relation) but got: {detail}")
    return failures


# --------------------------------------------------------------------------
# "func" mode: metamorphic testing, using third_party/fuzzing-SAT-solvers/func.py
# --------------------------------------------------------------------------

def run_func_mode(args: argparse.Namespace) -> int:
    rng = random.Random(args.seed)
    os.makedirs(args.out_dir, exist_ok=True)
    print(f"[func] fuzzing {args.exec} with {args.count} seed instances (seed={args.seed}, "
          f"{args.min_vars}-{args.max_vars} vars)")

    print_lock = threading.Lock()
    failure_count = 0

    def check_one(idx: int) -> bool:
        n_vars = rng.randint(args.min_vars, args.max_vars)
        n_clauses = round(n_vars * CLAUSE_TO_VAR_RATIO)
        instance_seed = rng.randint(0, 2**32 - 1)
        seed_clauses = generate_instance(n_vars, n_clauses, random.Random(instance_seed))

        tmp_path = os.path.join(args.out_dir, f".tmp-{idx}.cnf")
        write_dimacs(seed_clauses, n_vars, tmp_path)
        try:
            base_results = run_all_strategies(args.exec, tmp_path, args.timeout)
            failures = find_crash_or_disagreement(base_results)
            base_statuses = {r["status"] for r in base_results if r["status"] in ("SAT", "UNSAT")}

            # sat_func.transform() returns [(formula, n_clauses, "SAT->X\nUNSAT->Y\n"), ...]
            # -- see third_party/fuzzing-SAT-solvers/func.py for the exact relations
            # (reorderings preserve status exactly; add/delete clauses are monotonic).
            if not failures and len(base_statuses) == 1:
                base_status = next(iter(base_statuses))
                transformations = sat_func.transform(seed_clauses, n_clauses, n_vars)
                for t_idx, (t_clauses, t_n_clauses, relation) in enumerate(transformations):
                    expected = None
                    for line in relation.strip().split("\n"):
                        if line.startswith(base_status + "->"):
                            rel = line.split("->")[1].strip()
                            expected = rel if rel != "UNKNOWN" else None
                    t_clauses = [dedupe_clause(c) for c in t_clauses]
                    t_path = os.path.join(args.out_dir, f".tmp-{idx}-{t_idx}.cnf")
                    write_dimacs(t_clauses, n_vars, t_path)
                    try:
                        t_results = run_all_strategies(args.exec, t_path, args.timeout)
                        t_failures = find_crash_or_disagreement(t_results, expected)
                        if t_failures:
                            failures.append(f"[transform {t_idx}, base={base_status}] " + "; ".join(t_failures))
                            os.replace(t_path, os.path.join(args.out_dir, f"func-seed{instance_seed}-t{t_idx}.cnf"))
                    finally:
                        if os.path.exists(t_path):
                            os.remove(t_path)

            if not failures:
                return True

            saved_path = os.path.join(args.out_dir, f"func-seed{instance_seed}.cnf")
            os.replace(tmp_path, saved_path)
            with print_lock:
                print(f"\nFAILURE on instance seed={instance_seed} (saved to {saved_path}):")
                for f in failures:
                    print(f"  - {f}")
                print(f"  repro: {args.exec} {saved_path} <strategy options> {' '.join(COMMON_OPTIONS)}")
            return False
        finally:
            if os.path.exists(tmp_path):
                os.remove(tmp_path)

    ok_count = 0
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
        for i, ok in enumerate(pool.map(check_one, range(args.count))):
            ok_count += ok
            failure_count += not ok
            if (i + 1) % 20 == 0:
                print(f"[func] progress: {i + 1}/{args.count} ({failure_count} failures)")

    print(f"\n[func] done: {ok_count}/{args.count} instances passed, {failure_count} failures.")
    return 1 if failure_count else 0


# --------------------------------------------------------------------------
# "ub" mode: crash fuzzing, using third_party/fuzzing-SAT-solvers/ub.py
# --------------------------------------------------------------------------

def run_ub_mode(args: argparse.Namespace) -> int:
    # sat_ub.create_input()/create_garbage() use the `random` module globally
    # (see third_party/fuzzing-SAT-solvers/ub.py), so seed it directly rather
    # than through a local Random instance.
    random.seed(args.seed)
    os.makedirs(args.out_dir, exist_ok=True)
    print(f"[ub] fuzzing {args.exec} with {args.count} malformed/adversarial inputs", flush=True)

    failure_count = 0
    for i in range(args.count):
        if i < len(sat_ub.INTERSTING_CNFS):
            cnf_text = sat_ub.INTERSTING_CNFS[i]
            kind = "interesting"
        # create_input() generates a ~4MB/100k-clause formula every call
        # (hardcoded inside the function, not parametrized) -- see
        # KNOWN-ISSUES.md's "Fuzzer tuning" note. Used sparingly so a run
        # isn't dominated by regenerating variants of the same crash.
        elif i % 8 == 0:
            cnf_text = sat_ub.create_input()
            kind = "input"
        else:
            cnf_text = sat_ub.create_garbage()
            kind = "garbage"

        run_timeout = args.timeout * 4 if kind == "input" else args.timeout

        tmp_path = os.path.join(args.out_dir, f".tmp-ub-{i}.cnf")
        with open(tmp_path, "w") as f:
            f.write(cnf_text)

        try:
            proc = subprocess.run(
                [args.exec, tmp_path] + COMMON_OPTIONS + ["-t", str(run_timeout * 1000)],
                capture_output=True, timeout=run_timeout + 5,
            )
        except subprocess.TimeoutExpired:
            print(f"[ub] #{i} ({kind}): timed out (possible hang)", flush=True)
            failure_count += 1
            os.replace(tmp_path, os.path.join(args.out_dir, f"ub-hang-{i}.cnf"))
            continue

        ub_hits = sat_ub.check_ub(proc.stderr)
        crashed = proc.returncode not in (0, 1)  # 1 = NapSAT's own clean parse-error exit
        if ub_hits > 0 or crashed:
            failure_count += 1
            saved_path = os.path.join(args.out_dir, f"ub-{kind}-{i}.cnf")
            os.replace(tmp_path, saved_path)
            print(f"[ub] #{i} ({kind}): returncode={proc.returncode} ub_hits={ub_hits} -> saved to {saved_path}", flush=True)
            print(f"     stderr: {proc.stderr.decode('utf-8', 'replace')[:300]!r}", flush=True)
        else:
            os.remove(tmp_path)

        if (i + 1) % 50 == 0:
            print(f"[ub] progress: {i + 1}/{args.count} ({failure_count} failures)", flush=True)

    print(f"\n[ub] done: {args.count - failure_count}/{args.count} inputs passed, {failure_count} failures.")
    return 1 if failure_count else 0


def main() -> int:
    # Shared options are on a `parents=` parser so they can appear on either
    # side of the "func"/"ub" subcommand (argparse does not let a subparser's
    # own args come before options defined only on the top-level parser).
    common = argparse.ArgumentParser(add_help=False)
    common.add_argument("--exec", default="build/NapSAT", help="path to the NapSAT executable")
    common.add_argument("--count", type=int, default=150, help="number of instances/inputs to try")
    common.add_argument("--seed", type=int, default=0)
    common.add_argument("--timeout", type=float, default=5.0, help="per-run timeout in seconds")
    common.add_argument("--out-dir", default="fuzz-failures", help="where to save reproducer CNFs")
    common.add_argument("--jobs", type=int, default=max(1, (os.cpu_count() or 2) - 1))

    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter,
                                      parents=[common])
    sub = parser.add_subparsers(dest="mode", required=True)
    p_func = sub.add_parser("func", help="metamorphic testing + cross-strategy agreement", parents=[common])
    p_func.add_argument("--min-vars", type=int, default=20)
    p_func.add_argument("--max-vars", type=int, default=80)

    sub.add_parser("ub", help="crash/UB fuzzing against a sanitizer build", parents=[common])

    args = parser.parse_args()
    if not os.path.isfile(args.exec):
        print(f"error: NapSAT executable not found at {args.exec!r}", file=sys.stderr)
        return 1

    if args.mode == "func":
        return run_func_mode(args)
    return run_ub_mode(args)


if __name__ == "__main__":
    sys.exit(main())
