#!/usr/bin/python3
"""
Parse a NapSAT runner.txt log and extract:
 1. Original CNF in DIMACS format
 2. A command file (DECIDE sequence) derived from decision literals marked with @<number>

Heuristics / Assumptions (based on existing runner.txt structure):
 - File sections are separated by lines made only of '----' (a header starts with '====').
 - Header contains lines: nvar: <num>, nclauses: <num>
 - Clause section: lines shaped like "<idx>:  lit lit lit" where lit is either '~<num>' or '<num>'.
 - Trail section: lines with literals, some annotated with '@<decision_order>' meaning a decision literal.
   Other annotations like '(123)' are treated as propagation reason clause ids and ignored for command generation.
 - After trail another '----' precedes conflict / learnt clause information which is ignored here.

Outputs:
 - DIMACS CNF file named <base>.cnf (default base derived from input filename stem, e.g. runner.cnf)
 - Commands file named <base>-commands.txt containing DECIDE lines in increasing decision order.

Usage:
  python scripts/generate_from_runner.py [-i INPUT] [-o OUT_BASE]

If OUT_BASE isn't given, it uses the input filename stem without extension.

The script is robust to extra spaces and skips empty / comment-looking lines.
"""
import argparse
import re
from pathlib import Path
import sys

def parse_lit(tok: str) -> int | None:
    # minisat does have variables in range 0..n-1, but in DIMACS format variables are 1..n
    return -(int(tok[1:]) + 1) if tok.startswith('~') else int(tok) + 1

def fail(msg: str, line: str | None = None):
    """Print an error message to stderr and exit with status 1."""
    if line is not None:
        print(f"ERROR: {msg} | line='{line}'", file=sys.stderr)
    else:
        print(f"ERROR: {msg}", file=sys.stderr)
    sys.exit(1)


def parse_runner(path: Path):
    text = path.read_text(encoding='utf-8', errors='ignore').splitlines()

    # Find separators
    # Expect structure: '====' (optional) header ... '----' clauses ... '----' trail ... '----' rest
    nvar = None
    nclauses = None
    clauses: dict[int, list[int]] = {} # clause_index -> list of literals
    assumptions: list[int] = []  # list of asserted literals in trail
    actions: list[tuple[int, int]] = []  # decision_index -> (literal, clause_id); clause_id may be -1 if decision

    separator_regex = re.compile(r'^----$')
    header_nv = re.compile(r'^nvar:\s*(\d+)')
    header_nc = re.compile(r'^nclauses:\s*(\d+)')
    lit_token = re.compile(r'^~?\d+$')
    assumption_token = re.compile(r'^\s*(?P<lit>~?\d+)\s*$')
    action_token = re.compile(r'^\s*(?P<lit>~?\d+)\s*((@\d*)|(\((?P<cid>\d+)\)))\s*$')

    it = iter(text)
    phase = 'header'
    for raw in it:
        line = raw.strip() if raw is not None else ''
        if line == '====':
            continue
        if separator_regex.match(line):
            if phase == 'header':
                # ensure header basics collected
                if nvar is None or nclauses is None:
                    fail("Missing nvar/nclauses in header before clause section")
                phase = 'clauses'
                continue
            elif phase == 'clauses':
                phase = 'trail'
                continue
            elif phase == 'trail':
                # Done reading what we need
                break
        if phase == 'header':
            m = header_nv.match(line)
            if m:
                nvar = int(m.group(1)); continue
            m = header_nc.match(line)
            if m:
                nclauses = int(m.group(1)); continue
        elif phase == 'clauses':
            if not line:
                continue
            if ':' not in line:
                fail("Malformed clause line (missing colon)", line)
            try:
                idx_part, clause_part = line.split(':', 1)
            except ValueError:
                fail("Unable to split clause line", line)
            # Extract literal tokens
            lits: list[int] = []
            for tok in clause_part.strip().split():
                if lit_token.match(tok):
                    lits.append(parse_lit(tok))
            clauses[int(idx_part)] = lits
        elif phase == 'trail':
            if not line:
                continue
            if assumption_token.match(line):
                if len(actions) > 0:
                    # Error if we see simple assumptions after actions
                    fail("Simple assertion after decisions in trail", line)
                lit = parse_lit(line)
                assumptions.append(lit)
            else:
                m = action_token.match(line)
                if not m:
                    fail("Malformed trail line", line)
                lit_raw = m.group('lit')
                lit = parse_lit(lit_raw)
                cid = int(m.group('cid')) if m.group('cid') else -1
                actions.append((lit, cid))

    return nvar, nclauses, clauses, actions, assumptions


def write_dimacs(path: Path, nvar: int, clauses: dict[int, list[int]], assumptions: list[int]):
    nclauses = len(clauses) + len(assumptions)
    lines = [f"p cnf {nvar} {nclauses}"]
    # Sort by clause index to produce deterministic output
    for idx in sorted(clauses.keys()):
        cl = clauses[idx]
        lits = [str(l) for l in cl if l != 0]
        lines.append(' '.join(lits) + ' 0')
    for a in assumptions:
        lines.append(f"{a} 0")
    path.write_text('\n'.join(lines) + '\n', encoding='utf-8')


def write_commands(path: Path, actions: list[tuple[int, int]]):
    if not actions:
        path.write_text('', encoding='utf-8')
        return
    lines = []
    for lit, cid in actions:
        if cid == -1:
            lines.append(f"DECIDE {lit}")
    path.write_text('\n'.join(lines) + '\n', encoding='utf-8')


def main():
    ap = argparse.ArgumentParser(description='Generate DIMACS + command file from a runner.txt log.')
    ap.add_argument('-i', '--input', default='scripts/runner.txt', help='Path to runner.txt log (default: scripts/runner.txt)')
    ap.add_argument('-o', '--out-base', default=None, help='Base name (without extension) for outputs (default: input stem)')
    ap.add_argument('-d', '--out-dir', default='.', help='Directory to place outputs (default: current dir)')
    args = ap.parse_args()

    in_path = Path(args.input)
    if not in_path.is_file():
        fail("Input file not found", str(in_path))

    base = args.out_base if args.out_base else in_path.stem
    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    nvar, nclauses, clauses, actions, assumptions = parse_runner(in_path)

    dimacs_path = out_dir / f"{base}.cnf"
    commands_path = out_dir / f"{base}-commands.txt"

    write_dimacs(dimacs_path, nvar, clauses, assumptions)
    write_commands(commands_path, actions)

    print(f"Generated DIMACS:   {dimacs_path}")
    print(f"Generated commands: {commands_path}")

if __name__ == '__main__':
    main()
