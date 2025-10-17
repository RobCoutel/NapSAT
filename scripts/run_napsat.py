#!/usr/bin/env python3
import argparse
import subprocess
from pathlib import Path
import sys
import re


# find out -type f -print0 | xargs -0 -n 1 -P 8 python3 run_napsat.py --napsat-cmd ~/Work/NapSAT/build/NapSAT | tee ~/out.csv

def run_cmd(cmd, desc):
    try:
        subprocess.run(cmd, check=True)
    except subprocess.CalledProcessError as e:
        print(f"[ERROR] Command failed: {e}", file=sys.stderr)
        sys.exit(1)


def get_value(output: str, key: str = "Unassignment") -> int:
    match = re.search(fr"^c  - {key}: (\d+)", output, re.MULTILINE)
    if match:
        return int(match.group(1))
    else:
        print(f"[ERROR] '{key}' line not found in NapSAT output.", file=sys.stderr)
        sys.exit(1)


def get_value_float(output: str, key: str = "Unassignment") -> float:
    match = re.search(fr"^c  - {key}: (\d+\.?\d*)", output, re.MULTILINE)
    if match:
        return float(match.group(1))
    else:
        print(f"[ERROR] '{key}' line not found in NapSAT output.", file=sys.stderr)
        sys.exit(1)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("file", default="", help="Input file for runner.py", nargs="?")
    parser.add_argument("--napsat-cmd", default="", help="NapSAT binary or command (default: ../build/NapSAT)")
    args = parser.parse_args()

    if not args.file:
        print('name, unassign-rscb, unassign-ncb, unassign-gb, conflict-rscb, conflict-ncb, conflict-gb, avg-size-rscb, avg-size-ncb, avg-size-gb, actual-rscb, actual-ncb, actual-gb')
        sys.exit(0)

    input_path = Path(args.file)
    if not input_path.is_file():
        print(f"[ERROR] Input file not found: {input_path}", file=sys.stderr)
        sys.exit(1)

    base = input_path.name
    cnf_file = f"{base}.cnf"
    commands_file = f"{base}-commands.txt"

    script_dir = Path(__file__).parent.resolve()
    runner_py = script_dir / "runner.py"
    napsat_cmd = args.napsat_cmd if args.napsat_cmd else str(script_dir.parent / "build" / "NapSAT")

    # Run runner.py
    run_cmd([
        sys.executable, runner_py, "-i", str(input_path), "-o", base
    ], "Running runner.py")

    napsat_cmd = napsat_cmd.split()
    args = f"{cnf_file} -sw --restarts off --delete-clauses off -stat -o -commands {commands_file}".split()
    options = ["-rscb", "-ncb", "-gb"]
    unassignment_values = []
    unassignment_actual = []
    conflict_values = []
    clause_size_avg = []
    for opt in options:
        result = subprocess.run(napsat_cmd + args + [opt], capture_output=True, text=True)
        if result.returncode != 0:
            print(f"[ERROR] NapSAT run failed: {result.stderr}", file=sys.stderr)
            sys.exit(1)
        conflicts = get_value(result.stdout, "Conflicts")
        if conflicts == 0:
            sys.exit(0)
        conflict_values.append(conflicts)
        unassignment_values.append(get_value(result.stdout, "Unassignment"))
        unassignment_actual.append(get_value(result.stdout, "Actual unassigned"))
        clause_size_avg.append(get_value_float(result.stdout, "Avg learned clause size"))

    # Print the difference
    print(f"{base},"
          + (",".join(map(str, unassignment_values))) + ","
          + (",".join(map(str, conflict_values))) + ","
          + (",".join(map(str, clause_size_avg))) + ","
          + (",".join(map(str, unassignment_actual))))

    # Delete temp files
    for f in [cnf_file, commands_file]:
        try:
            Path(f).unlink()
        except FileNotFoundError:
            pass

if __name__ == "__main__":
    main()
