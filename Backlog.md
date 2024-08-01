# Backlog

Here are the list of tasks identified in the solver.

## Software structure

- Remove datarace in the proof manager
- Create a benchmarking framework

## SAT technology

- Change the blocker data structure
- Change the clause representation
- Improve clause minimization

## Solver features

- The display is currently only in the form of terminal output. The goal is to create a nice user interface for the solver, and to allow naming of variables and clauses, and to display the implication graph in a more user-friendly way. This is left for future work.
- The solver currently only supports the DIMACS CNF format. We would like to add support for any arbitrary propositional formula in the future.
- The solver does not support theory propagations yet.
- The solver does not support user propagation yet.
- The solver does not support additional theories. We would like to add AMO constraints, and be flexible enough to be easily hackable and add other simple theories.
- The solver does not support assumptions yet.
- The solver does not provide unsat cores yet.