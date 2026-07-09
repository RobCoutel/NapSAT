# Known issues

Bugs discovered while building the fuzzing/benchmark CI infrastructure
(`scripts/fuzz.py`, `scripts/bench.py`). Not fixed yet -- documented here so
they aren't lost and so the fuzzer/CI can be taught to treat them as known
failures instead of reporting the same thing on every run.

## `-gb -lcm` + `-cp`: proof checker fails to find the empty clause on an UNSAT instance

**Found by**: `python3 scripts/fuzz.py func --count 20 --min-vars 20 --max-vars 40 --timeout 5 --seed 7 --jobs 4` (deterministic; regenerates as instance seed `884585951`). Repro CNF also saved at `tests/cnf/fuzz-regressions/gb-lcm-proof-check-crash.cnf` (98 clauses, 23 vars; not wired into any test).

**Trigger**: graph backtracking with lazy chunk merging (`-gb -lcm`),
combined with proof checking (`-cp`), on an UNSAT instance. Reproduces
consistently (3/3 runs), with or without a `-t` timeout value set. Every
other strategy tested against the same file (default, `-rscb`, `-lscb`,
`-gb`, `-gb -bl`) completes cleanly with `-cp` -- this looks specific to the
`-gb -lcm` combination.

```
$ build/NapSAT tests/cnf/fuzz-regressions/gb-lcm-proof-check-crash.cnf -gb -lcm -sw -si -o "{--check-only}" -cp
[...]
s UNSATISFIABLE
NapSAT: src/proof/proof.cpp:284: bool napsat::proof::resolution_proof::check_proof(): Assertion `empty_clause_id != 0xFFFFFFFF' failed.
```

The resolution proof built under `-gb -lcm` apparently never derives the
empty clause, even though the solver itself correctly reports UNSAT --
suggests the proof (or its construction under lazy chunk merging
specifically) is incomplete/incorrect, rather than the SAT/UNSAT verdict
itself being wrong.

**Fuzzer tuning**: `create_input()` generates ~4MB/100k-clause formulas by
construction (hardcoded inside the function, not parametrized), which made
`scripts/fuzz.py ub` slow and produced no visible output before being killed
by an external timeout in one investigation session (stdout buffering lost
the progress prints). `run_ub_mode()` now uses it only 1 in 8 iterations
(down from 1 in 2) and gives it a larger per-run timeout, so a single CI run
doesn't spend most of its budget regenerating variants of this same crash.

**Fix description**: The problem is triggered when detecting UNSAT because of a lazy chunk merge.
To fix it, we need to trigger a special conflict analysis when the conflict is detected at an indirect root level.
This requires to indentify the conflict clause that triggers such a conflict, by building a chunk merging chain leading to the empty set of chunks.
Then, using the conflict analysis mechanism, but continuing until the empty clause is derived, we can prove that the formula is UNSAT and add the empty clause to the proof.
