# NapSAT solver

## Description

NapSAT is a SAT solver written in C++ specialized for academic study of backtracking strategies. As such, it is not intended (yet) to be a production-ready SAT solver, but rather a research tool to study the behavior of different backtracking strategies in (incremental) SAT solving.

NapSAT is the object of two research papers [1, 2].
You are invited to read these papers to understand the backtracking algorithms employed in NapSAT.

Known limitations and open bugs are tracked in [KNOWN-ISSUES.md](KNOWN-ISSUES.md).

## Quick start

```bash
# 1. Clone the repository together with the SATSentinel submodule
git clone --recursive git@github.com:RobCoutel/NapSAT.git
cd NapSAT

# 2. Install the dependencies (liblzma, libbz2) and initialize the submodules
sudo make install

# 3. Build the solver
make

# 4. Solve a formula
build/NapSAT tests/cnf/unsat-01.cnf
```

The solver prints `s SATISFIABLE`, `s UNSATISFIABLE`. The exit code is `0` whenever the solver ran (whatever the answer) and `1` if the command line or the input file could not be parsed.

### Dependencies

- `g++` with C++20 support
- `liblzma` and `libbz2` (decompression of `.xz` and `.bz2` inputs)
- [Catch2](https://github.com/catchorg/Catch2) — for the unit tests only (`sudo make install-test`)
- Python 3 — for the fuzzing and benchmarking scripts only

## Features

### Chronological Backtracking

NapSAT implements different Chronological Backtracking (CB) strategies [1, 3, 4] that can be selected at runtime. CB allows the solver to backtrack to the highest decision level in a conflict clause, minus one. CB contrasts with Non-Chronological Backtracking (NCB), which backtracks until the second-highest decision level in the conflict clause. CB therefore saves the solver from undoing decisions that are unrelated to the conflict.

### Graph Backtracking

NapSAT implements a unique Graph Backtracking (GB) strategy [2]. GB enables the user to provide a cost function to the solver, which is used to determine the minimal set of decisions to undo in order to resolve a conflict as cheaply as possible.

### Sentinel

For debugging and pedagogical purposes, NapSAT implements a sentinel mechanism that allows the user to observe the behavior of the solver with different levels of granularity. The sentinel was later severed from NapSAT and is now available in a separate project called [SATSentinel](https://github.com/RobCoutel/SATSentinel).

### Proof Generation and Checking

NapSAT can generate primitive resolution proofs for UNSAT formulas. Future work will use standard formats such as DRAT and LRAT to generate proofs that can be checked by external proof checkers.

### Incremental Solving

NapSAT can be used as an incremental SAT solver. The user can add clauses at runtime. As opposed to other incremental SAT solvers, NapSAT does not always require to backtrack when adding new clauses. For example, a missed lower implication [5] would break an NCB solver. But NapSAT can continue without backtracking, provided that it uses a CB or GB strategy.

### Solving with Assumptions

This work is still ongoing. In principle NapSAT can solve formulas with assumptions when using Graph Backtracking. But the current implementation is not yet thoroughly tested and may not work in all cases.

## Building

The project uses Make to generate the build files.

| Command | Result |
|---------|--------|
| `make` | Builds the solver executable `build/NapSAT` |
| `make lib` | Builds the static library `build/NapSAT.a` |
| `make all` | Builds both |
| `make debug` | Builds both with debug flags (`-O0 -g3 -ftrapv`) and aggressive invariant checks |
| `make tests` | Builds the unit test executable `build/NapSAT-tests` (requires Catch2) |
| `make clean` | Removes the build directories of NapSAT and SATSentinel |

The debug build writes to the same paths as the release build, so building one overwrites the other. A debug binary announces itself at startup with `INFO: Running NapSAT (debug)`. It aggressively checks for invariant violations and is much slower than the release version.

## Usage

### Running the SAT solver

```bash
build/NapSAT <input-file> [options]
```

The input file must be a valid DIMACS CNF file, optionally compressed as `.cnf.xz` or `.cnf.bz2`. The solver will then try to solve the given formula and print the result on the standard output.

The complete list of options is available with `build/NapSAT -h`. The most important ones select the backtracking strategy:

| Option | Strategy |
|--------|----------|
| *(none)* | Non-chronological backtracking (default) |
| `-cb` | Chronological backtracking [3, 4] |
| `-lscb` | Lazy strong chronological backtracking [1]; subsumes `-cb` |
| `-gb` | Graph backtracking [2] |

Graph backtracking accepts several modifiers (all of them require `-gb`): `-lcm` / `-ecm` for lazy or eager chunk merging, `-bsc` / `-bfc` to choose the backtracked chunk, `-bl` to backtrack the chunks that were analyzed to learn the clause, and `-max-approx-cost` / `-sum-approx-cost` / `-vsids-approx-cost` to select the cost estimation.

`-stat` prints statistics about the solving process, and `-o` enables the sentinel observer. The sentinel can be configured with its own options, which are passed in braces right after `-o`. For example, `-o "{-dl 10 -commands commands.txt}"` sets the display level to 10 and loads a command file for interactive solving.

### NapSAT as a library

NapSAT can be used as a library. The API is defined in [include/SAT-API.hpp](include/SAT-API.hpp). This allows a more abstract use of the solver. All the symbols live in the `napsat` namespace.

```cpp
#include "SAT-API.hpp"
#include <iostream>

int main()
{
  napsat::options opt;   // defaults
  opt.lazy_strong_chronological_backtracking = true;
  napsat::NapSAT* solver = napsat::create_solver(0, 0, opt);

  napsat::Tvar v1(1);
  napsat::Tvar v2(2);

  // (1 v ~2) ^ (~1 v 2)
  napsat::Tlit c1[] = { napsat::Tlit(v1, 1), napsat::Tlit(v2, 0) };
  napsat::Tlit c2[] = { napsat::Tlit(v1, 0), napsat::Tlit(v2, 1) };
  napsat::add_clause(solver, c1, 2);
  napsat::add_clause(solver, c2, 2);

  if (napsat::solve(solver) == napsat::status::SAT) {
    for (napsat::Tlit lit : napsat::get_partial_assignment(solver))
      std::cout << lit.to_string() << " ";
    std::cout << std::endl;
  }

  napsat::delete_solver(solver);
  return 0;
}
```

The solver library depends on the SATSentinel library, so both archives must be linked:

```bash
make lib
g++ -std=c++20 -I include -I SATSentinel/include example.cpp \
    build/NapSAT.a SATSentinel/build/SATSentinel.a -llzma -lbz2 -o example
```

Clauses can also be built literal by literal with `start_new_clause` / `push_literal` / `finalize_clause`, or loaded from a file with `parse_dimacs`.

## Functionalities

### Observing

An observer can be attached to the solver to check and debug the solver. The observer can be used to generate tikz figures of the trail, the clause set and the implication graph. This is useful for creating slides for presentations.

It also allows the user to check the behavior of the solver with a configurable level of detail. This can be useful for debugging large formulas.

The observer is enabled with `-o`, and its own options are passed as a group in braces right after that flag (quote the group, since an unquoted `{` is a syntax error in zsh). `-c` additionally enables the solver invariant checks. The sentinel and navigation commands are listed by `build/NapSAT -hs` and `build/NapSAT -hn`.

Furthermore, the interactive mode of the solver (`-i`) allows the user to interact with the solver during runtime. Choosing specific decisions or learning clauses can be done interactively. For bug reproduction, this functionality can be accompanied by a command file that will be executed by the solver.

For example, the following command replays a recorded session on a specific test case, in lazy strong chronological backtracking mode. The display level `-dl 10` makes the sentinel stop and print its state on every notification; lower it to show fewer details.

```bash
build/NapSAT tests/cnf/test-trigger-mli.cnf -lscb -o "{-commands tests/cnf/test-trigger-mli-commands.txt -dl 10}"
```

### Proof generation

The solver generates proofs for UNSAT formulas. The proof can be printed in a human-readable format or simply checked using the options `-pp` and `-cp` respectively.

```bash
build/NapSAT tests/cnf/unsat-01.cnf -pp
```

## Development

- [HACKME.md](HACKME.md) describes the folder structure and the internals of the solver.
- [KNOWN-ISSUES.md](KNOWN-ISSUES.md) lists the bugs that are known but not fixed yet.
- [Backlog.md](Backlog.md) lists the planned improvements.
- `make tests` builds the Catch2 unit tests in `build/NapSAT-tests`. The unit tests are not very elaborate at the moment, and will be improved in the future.
- `make fuzz` runs the metamorphic and cross-strategy fuzzer ([scripts/fuzz.py](scripts/fuzz.py)) on a debug build; `make fuzz-ub` does the same with the address and undefined behavior sanitizers.
- `make perf-bench` measures propagations per second on `tests/cnf/bench` ([scripts/bench.py](scripts/bench.py)).
- Both the fuzzer and the benchmark also run in CI, see [.github/workflows/](.github/workflows/).

## Citation

If you use NapSAT in your research, please cite the relevant paper:

```bibtex
@inproceedings{coutelier2024lazy,
  author    = {Robin Coutelier and Mathias Fleury and Laura Kov{\'a}cs},
  title     = {Lazy Reimplication in Chronological Backtracking},
  booktitle = {27th International Conference on Theory and Applications of
               Satisfiability Testing (SAT 2024)},
  series    = {LIPIcs},
  year      = {2024}
}

@inproceedings{coutelier2026generalizing,
  author    = {Robin Coutelier and Thomas Hader and Laura Kov{\'a}cs},
  title     = {Generalizing {CDCL} with Graph Backtracking},
  booktitle = {29th International Conference on Theory and Applications of
               Satisfiability Testing (SAT 2026)},
  year      = {2026}
}
```

## Acknowledgements

NapSAT is part of the PhD research of Robin Coutelier, supervised by Prof. Laura Kovács at the TU Wien, Austria. The author would like to thank Prof. Laura Kovács for her guidance and support during the development of this project.
This project started supervised by Prof. Pascal Fontaine from the University of Liège, Belgium. We thank him for his guidance and support during the early stages of this project.

Other contributors to this project include Thomas Hader, who contributed to the development of the Graph Backtracking algorithm and the implementation of relevant data structures.

## Funding

This project has received funding from the ERC Consolidator Grant ARTIST 101002685;
the TU Wien Doctoral Colleges TrustACPS and SecInt; the FWF SpyCoDe SFB projects F8504;
and the WWTF Grant ForSmart 10.47379/ICT22007 and the University of Liège.

## License

This project is protected under the MIT license. See the LICENSE file for more information.

## Contact

If you have any questions or suggestions, feel free to contact me at robin.coutelier@tuwien.ac.at

## Bibliography

[1] Robin Coutelier, Mathias Fleury, Laura Kovács. Lazy Reimplication in Chronological Backtracking. In Proceedings of the 27th International Conference on Theory and Applications of Satisfiability Testing (SAT 2024), 2024.

[2] Robin Coutelier, Thomas Hader, Laura Kovács. Generalizing CDCL with Graph Backtracking. In Proceedings of the 29th International Conference on Theory and Applications of Satisfiability Testing (SAT 2026), 2026.

[3] Alexander Nadel, Vadim Ryvchin. Chronological Backtracking. In Proceedings of the 21st International Conference on Theory and Applications of Satisfiability Testing (SAT 2018), 2018.

[4] Sibylle Möhle, Armin Biere. Backing Backtracking. In Proceedings of the 22nd International Conference on Theory and Applications of Satisfiability Testing (SAT 2019), 2019.

[5] Alexander Nadel. Introducing Intel(R) SAT Solver. In Proceedings of the 25th International Conference on Theory and Applications of Satisfiability Testing (SAT 2022), 2022.
