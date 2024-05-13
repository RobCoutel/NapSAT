#include "SAT-options.hpp"
#include "SAT-types.hpp"

#include <vector>

namespace napsat
{
  class NapSAT;

  /**
   * @brief Create a new SAT solver.
   * @param n_var The initial number of variables in the SAT problem.
   * @param n_clauses The initial number of clauses in the SAT problem.
   * @param opt The options for the solver.
   * @return A new SAT solver.
   * @details The solver is created with the given number of variables and
   * clauses, however, the number of variables and clauses can be increased later
   * when adding new clauses.
  */
  NapSAT* create_solver(unsigned n_var, unsigned n_clauses, options& opt);

  /**
   * @brief Delete a SAT solver.
   * @param solver The solver to delete.
  */
  void delete_solver(NapSAT* solver);

  /**
   * @brief Parse a DIMACS file and add the clauses to the clause set.
   * @details This funciton can be called multiple times to load multiple clause
   * sets and solve the conjunction of them.
   * @param solver an instance of the SAT solver
   * @param filename the name of the file containing the clauses in dimacs format
   * @pre the solver is a valid instance of NapSAT
   */
  void parse_dimacs(NapSAT* solver, const char* filename);

  /**
   * @brief Propagate literals in the queue and resolve conflicts if needed.
   * The procedure stops when all variables are assigned, or a decision is
   * needed. If the solver is in input mode, it will switch to propagation mode.
   * @param solver an instance of the SAT solver
   * @pre the solver is a valid instance of NapSAT
   * @pre The solver status must be undef
   * @return true if the solver may make a decision, false if all variables are
   * assigned or the clause set is unsatisfiable.
   */
  bool propagate(NapSAT* solver);

  /**
   * @brief Decides the value of a variable. The variable must be unassigned.
   * @param solver an instance of the SAT solver
   * @pre the solver is a valid instance of NapSAT
   * @pre The solver status must be undef
   * @pre The propagation queue must be empty
   * @return true if the solver is still undef after the decision, false if all
   * variables are assigned.
   */
  bool decide(NapSAT* solver);

  /**
   * @brief Forces the solver to decide the given literal. The literal must be
   * unassigned.
   * @param solver an instance of the SAT solver
   * @param lit literal to decide.
   * @pre the solver is a valid instance of NapSAT
   */
  bool decide(NapSAT* solver, Tlit lit);

  /**
   * @brief Solves the clause set. The procedure stops when all variables are
   * assigned, of the solver concludes that the clause set is unsatisfiable.
   * @param solver an instance of the SAT solver
   * @pre the solver is a valid instance of NapSAT
   */
  status solve(NapSAT* solver);

  /**
   * @brief Returns the status of the solver.
   * @param solver an instance of the SAT solver
   * @return status of the solver.
   * @pre the solver is a valid instance of NapSAT
   */
  status get_status(NapSAT* solver);

  /**
   * @brief Set up the solver to lits a new clause. Sets the solver in
   * clause_input mode.
   * @param solver an instance of the SAT solver
   * @pre the solver is a valid instance of NapSAT
   */
  void start_new_clause(NapSAT* solver);

  /**
   * @brief Add a literal to the current clause.
   * @param solver an instance of the SAT solver
   * @param lit literal to add to the clause.
   * @pre the solver is a valid instance of NapSAT
   * @pre The solver must be in clause_input mode.
   * @pre the literal or its negation must not already be in the clause.
   */
  void push_literal(NapSAT* solver, Tlit lit);

  /**
   * @brief Finalize the current clause and add it to the clause set. If the
   * solver is in propagation mode, it will propagate literals if needed.
   * @param solver an instance of the SAT solver
   * @pre the solver is a valid instance of NapSAT
   * @pre the solver must be in clause_input mode.
   * @return A handle to the added clause.
   */
  Tclause finalize_clause(NapSAT* solver);

  /**
   * @brief Add a complete clause to the clause set.
   * @param solver an instance of the SAT solver
   * @param lits array of literals to add to the clause set.
   * @param size size of the clause.
   * @return A handle to the added clause.
   * @pre the solver is a valid instance of NapSAT
   * @note The memory of the clause is allocated by the solver. Therefore, the
   * pointer lits is managed by the user and is not freed by the solver.
   */
  Tclause add_clause(NapSAT* solver, const Tlit* lits, unsigned n_lits);

  /**
   * @brief Returns a reference to the trail. The trail should not be modified
   * by the user.
   * @param solver an instance of the SAT solver
   * @pre the solver is a valid instance of NapSAT
   */
  const std::vector<Tlit>& get_partial_assignment(NapSAT* solver);

  /**
   * @brief Returns true if the given literal is decided.
  */
  bool is_decided(NapSAT* solver, Tlit lit);

  /**
   * @brief Prints on the standard output the statistics collected by the solver
   * if the observer was enabled. Otherwise prints a warning.
   * @param solver an instance of the SAT solver
   * @pre the solver is a valid instance of NapSAT
  */
  void print_statistics(NapSAT* solver);

  /**
   * @brief Prints the proof of the last execution of the solver.
   * @param solver an instance of the SAT solver
   * @pre the solver is a valid instance of NapSAT
   * @pre the solver was built with the option build_proof set to true.
   * @pre the status of the solver is UNSAT
  */
  void print_proof(NapSAT* solver);

  /**
   * @brief Checks the proof of the last execution of the solver.
   * @param solver an instance of the SAT solver
   * @return true if the proof is correct, false otherwise.
   * @pre the solver is a valid instance of NapSAT
   * @pre the solver was built with the option build_proof set to true.
   * @pre the status of the solver is UNSAT
  */
  bool check_proof(NapSAT* solver);
}
