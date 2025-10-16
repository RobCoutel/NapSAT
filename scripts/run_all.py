#!/usr/bin/env python3
import argparse
import subprocess
from pathlib import Path
import sys
import re


def run_cmd(cmd, desc):
    print(f"[INFO] {desc}: {' '.join(cmd)}")
    try:
        subprocess.run(cmd, check=True)
    except subprocess.CalledProcessError as e:
        print(f"[ERROR] Command failed: {e}", file=sys.stderr)
        sys.exit(1)


def derive_base(path: Path) -> str:
    name = path.name
    if name.endswith('.xz'):
        name = name[:-3]
    return Path(name).stem


def get_unassignment(output: str) -> int:
    match = re.search(r"^c  - Unassignment: (\d+)", output, re.MULTILINE)
    if match:
        return int(match.group(1))
    else:
        print("[ERROR] 'Unassignment' line not found in NapSAT output.", file=sys.stderr)
        sys.exit(1)


def main():
    parser = argparse.ArgumentParser(description="Run runner.py and NapSAT twice, then clean up temp files.")
    parser.add_argument("file", help="Input file for runner.py")
    parser.add_argument("--napsat-cmd", default="../build/NapSAT", help="NapSAT binary or command (default: ./build/NapSAT)")
    args = parser.parse_args()

    input_path = Path(args.file)
    if not input_path.is_file():
        print(f"[ERROR] Input file not found: {input_path}", file=sys.stderr)
        sys.exit(1)

    base = derive_base(input_path)
    cnf_file = f"{base}.cnf"
    commands_file = f"{base}-commands.txt"

    # Run runner.py
    run_cmd([
        sys.executable, "runner.py", "-i", str(input_path), "-o", base
    ], "Running runner.py")

    # Run NapSAT twice and capture output
    napsat_cmd = args.napsat_cmd.split()
    args = f"{cnf_file} -sw --restarts off -stat -o -commands {commands_file}".split()
    options = ["-ncb", "-cb", "-gb"]
    unassignment_values = []
    for opt in options:
        result = subprocess.run(napsat_cmd + args + [opt], capture_output=True, text=True)
        if result.returncode != 0:
            print(f"[ERROR] NapSAT run failed: {result.stderr}", file=sys.stderr)
            sys.exit(1)
        unassignment = get_unassignment(result.stdout)
        unassignment_values.append(unassignment)

    # Print the difference
    print(f"{base}," + (",".join(map(str, unassignment_values))))

    # Delete temp files
    for f in [cnf_file, commands_file]:
        try:
            Path(f).unlink()
        except FileNotFoundError:
            pass

if __name__ == "__main__":
    main()
