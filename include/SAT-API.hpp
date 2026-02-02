#include "SAT-options.hpp"
#include "SAT-types.hpp"

#include <vector>
#include <functional>

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
   * @brief Create a new variable in the solver.
   * @param solver The solver in which to create the variable.
   * @return The variable created.
   */
  Tvar new_variable(NapSAT* solver);

  /**
   * @brief Returns the current number of variables in the solver.
   * @param solver an instance of the SAT solver
   * @pre the solver is a valid instance of NapSAT
   * @return the current number of variables in the solver.
   */
  unsigned variables_count(const NapSAT* solver);

  /**
   * @brief Parse a DIMACS file and add the clauses to the clause set.
   * @details This funciton can be called multiple times to load multiple clause
   * sets and solve the conjunction of them.
   * @param solver an instance of the SAT solver
   * @param filename the name of the file containing the clauses in dimacs format
   * @pre the solver is a valid instance of NapSAT
   * @return true if the file was successfully parsed, false otherwise.
   */
  bool parse_dimacs(NapSAT* solver, const char* filename);

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
   * @brief Solves the clause set with a limit on the number of conflicts.
   * The procedure stops when all variables are assigned, or the solver
   * concludes that the clause set is unsatisfiable, or the number of conflicts
   * reaches the given limit.
   * @param solver an instance of the SAT solver
   * @param conflict_limit limit on the number of conflicts before stopping the
   * solve.
   * @pre the solver is a valid instance of NapSAT
   * @return status of the solver.
   */
  status solve_limited(NapSAT* solver, unsigned conflict_limit);

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
   * @brief Pushes assumptions to the solver.
   * @param solver an instance of the SAT solver
   * @param assumption a literal to assume.
   * @pre the solver is a valid instance of NapSAT
   * @return true if the assumption was added successfully, false otherwise.
   * @details Assumptions are literals assigned by the user that the solver cannot
   * remove until the user calls remove_assumption<s>.
   * @details The solver will return false if an assumption contradicts the current
   * set of assumptions
   * @details In GB, pushing assumptions can be done at any time. If the
   * assignment contradicts the assumptions, the solver will backtrack to a level where
   * the assumptions are still valid. In CB and NCB, pushing assumption will force
   * the solver to backtrack to level 0 before adding the assumptions.
   */
  bool add_assumption(NapSAT* solver, Tlit assumption);

  /**
   * @brief Pushes assumptions to the solver.
   * @param solver an instance of the SAT solver
   * @param assumptions vector of literals to assume.
   * @pre the solver is a valid instance of NapSAT
   * @return true if all assumptions were added successfully, false otherwise.
   * @details Assumptions are literals assigned by the user that the solver cannot
   * remove until the user calls remove_assumption<s>().
   * @details The solver will return false if an assumption contradicts the current
   * set of assumptions
   * @details In GB, pushing assumptions can be done at any time. If the
   * assignment contradicts the assumptions, the solver will backtrack to a level where
   * the assumptions are still valid. In CB and NCB, pushing assumption will force
   * the solver to backtrack to level 0 before adding the assumptions.
   */
  bool add_assumption(NapSAT* solver, const std::vector<Tlit>& assumptions);

  /**
   * @brief Removes an assumption from the solver.
   * @param solver an instance of the SAT solver
   * @param assumption literal to remove from the assumptions.
   * @pre the solver is a valid instance of NapSAT
   * @return true if the assumption was removed successfully, false otherwise.
   * @details In CB and NCB, removing an assumption will backtrack the solver to
   * the assumption levels. In GB, the trail is not modified, but the assumption is
   * now unlocked and can be modified by the solver.
   */
  bool forget_assumption(NapSAT* solver, Tlit assumption);

  /**
   * @brief Removes all assumptions from the solver.
   * @param solver an instance of the SAT solver
   * @pre the solver is a valid instance of NapSAT
   * @details In CB and NCB, removing all assumptions will backtrack the solver to
   * level 0. In GB, the trail is not modified, but all assumptions are now unlocked and
   * can be modified by the solver.
   */
  void forget_assumption(NapSAT* solver);

  /**
   * @brief Returns the list of failed assumptions after an UNSAT solve.
   * @param solver an instance of the SAT solver
   * @pre the solver is a valid instance of NapSAT
   * @pre the last call to solve returned UNSAT
   * @return vector of literals representing the failed assumptions.
   */
  std::vector<Tlit> unsat_core(const NapSAT* solver);

  /**
   * @brief Returns the list of clauses in the unsat core after an UNSAT solve modulo assumptions.
   * @param solver an instance of the SAT solver
   * @pre the solver is a valid instance of NapSAT
   * @pre the last call to solve returned UNSAT
   * @return vector of clause identifiers representing the clauses in the unsat core modulo assumptions.
   */
  std::vector<Tclause> clause_unsat_core(NapSAT* solver);

  /**
   * @brief Returns a reference to the trail. The trail should not be modified
   * by the user.
   * @param solver an instance of the SAT solver
   * @pre the solver is a valid instance of NapSAT
   */
  const std::vector<Tlit>& get_partial_assignment(const NapSAT* solver);

  /**
   * @brief Returns true if the given literal is decided.
   * @param solver an instance of the SAT solver
   * @param lit literal to check.
   * @return true if the literal is decided, false otherwise.
   * @pre the solver is a valid instance of NapSAT
   */
  bool is_decided(const NapSAT* solver, Tlit lit);

  /**
   * @brief Returns true if the given literal is assigned at root level.
   */
  bool is_root_level(const NapSAT* solver, Tvar var);

  /**
   * @brief Returns the value of a literal in the current assignment.
   * @param solver an instance of the SAT solver
   * @param lit literal to evaluate.
   * @return Tval representing the value of the literal.
   * @pre the solver is a valid instance of NapSAT
   */
  Tval get_variable_value(const NapSAT* solver, Tvar var);

  /**
   * @brief Suggest a polarity for a variable.
   * @param solver an instance of the SAT solver
   * @param lit literal whose variable will have a suggested polarity.
   * @param polarity suggested polarity. true for positive, false for negative.
   * @pre the solver is a valid instance of NapSAT
   * @details The first time this variable is assigned as a decision, it will be
   * assigned the suggested polarity. After that, the phase_cache will be used.
   */
  void suggest_polarity(NapSAT* solver, Tlit lit, bool polarity);

  /**
   * @brief Provide a weight function to the solver for weighted SAT solving.
   * @param solver an instance of the SAT solver
   * @param weight_function a function that takes a literal and returns its weight as an unsigned integer.
   * @pre the solver is a valid instance of NapSAT
   * @details The weight function is used to compute the weight of a set of literals or chunks during graph-based backtracking.
   */
  void set_weight_function(NapSAT* solver, std::function<double(Tlit)> weight_function);

  void synchronize(NapSAT* solver);

  /**
   * @brief Prints on the standard output the statistics collected by the solver
   * if the observer was enabled. Otherwise prints a warning.
   * @param solver an instance of the SAT solver
   * @pre the solver is a valid instance of NapSAT
  */
  void print_statistics(const NapSAT* solver);

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
