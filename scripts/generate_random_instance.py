from pysat.formula import CNF
from pysat.solvers import Solver
from cnfgen import CNFGenerator

import random
import os
from rich.progress import Progress


N_VARS = 300
if N_VARS == 250:
    N_CLAUSES = 1065
elif N_VARS == 300:
    N_CLAUSES = 1300
else:
    N_CLAUSES = 1000
OUTPUT_DIR = "random-instances"

count_sat = 0
count_unsat = 0

# check how many files are already present in the output directory
if not os.path.exists(OUTPUT_DIR + "/sat"):
    os.makedirs(OUTPUT_DIR + "/sat")
if not os.path.exists(OUTPUT_DIR + "/unsat"):
    os.makedirs(OUTPUT_DIR + "/unsat")
count_sat = len([name for name in os.listdir(OUTPUT_DIR + "/sat") if name.endswith(".cnf") and name.startswith(f"uf{N_VARS}-")])
count_unsat = len([name for name in os.listdir(OUTPUT_DIR + "/unsat") if name.endswith(".cnf") and name.startswith(f"uuf{N_VARS}-")])
print(f"Found {count_sat} SAT instances and {count_unsat} UNSAT instances in the output directory.")

def generate_instance(n_vars: int, n_clauses: int) -> list[list[int]]:
    '''
    Generates a random SAT instance in CNF format.

    Parameters:
        n_vars (int): Number of variables
        n_clauses (int): Number of clauses

    Returns:
        list[list[int]]: A list of clauses, each clause is a list of integers
    '''
    clause_set: set[tuple[int]] = set()
    while len(clause_set) < n_clauses:
        clause_size = 3
        clause: set[int] = set()
        while len(clause) < clause_size:
            var = random.randint(1, n_vars)
            if random.random() < 0.5:
                var = -var
            if var not in clause and -var not in clause:
                clause.add(var)
        clause_set.add(tuple(sorted(clause)))
    return [list(clause) for clause in clause_set]


def test_instance(clause_list: list[list[int]]) -> bool:
    '''
    Tests if the given SAT instance is satisfiable.

    Parameters:
        clauses (list[list[int]]): A list of clauses, each clause is a list of integers
    Returns:
        bool: True if the instance is satisfiable, False otherwise
    '''
    cnf = CNF(from_clauses=clause_list)
    solver = Solver(bootstrap_with=cnf, name="Cadical195")

    is_sat = solver.solve()
    solver.delete()
    return is_sat

def save_instance(clauses: list[list[int]], filename: str):
    '''
    Saves the given SAT instance in CNF format to the given file.

    Parameters:
        clauses (list[list[int]]): A list of clauses, each clause is a list of integers
        filename (str): The name of the file to save the instance to
    '''
    with open(filename, "w") as f:
        f.write(f"p cnf {N_VARS} {len(clauses)}\n")
        for clause in clauses:
            f.write(" ".join(map(str, clause)) + " 0\n")

with Progress() as progress:
  task_sat = progress.add_task("Generating SAT instances", total=1000)
  task_unsat = progress.add_task("Generating UNSAT instances", total=1000)

  for _ in range (0, count_sat):
      progress.advance(task_sat)
  for _ in range (0, count_unsat):
      progress.advance(task_unsat)

  while count_sat < 1000 or count_unsat < 1000:
      clauses = generate_instance(N_VARS, N_CLAUSES)
      s = test_instance(clauses)

      if s and count_sat <= 1000:
          save_instance(clauses, OUTPUT_DIR + f"/sat/uf{N_VARS}-{count_sat:04d}.cnf")
          count_sat += 1
          progress.advance(task_sat)
      elif not s and count_unsat <= 1000:
          save_instance(clauses, OUTPUT_DIR + f"/unsat/uuf{N_VARS}-{count_unsat:04d}.cnf")
          count_unsat += 1
          progress.advance(task_unsat)
