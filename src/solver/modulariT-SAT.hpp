/**
 * @file modulariT-SAT.hpp
 * @author Robin Coutelier
 *
 * @brief This file is part of the SMT Solver modulariT. It defines the interface of the SAT solver and its data structures.
 */
#pragma once

#include "SAT-config.hpp"
#include "SAT-types.hpp"
#include "SAT-options.hpp"
#include "custom-assert.hpp"
#include "heap.hpp"
#include "../observer/SAT-stats.hpp"
#include "../observer/SAT-notification.hpp"
#include "../observer/SAT-observer.hpp"

#include <vector>
#include <set>
#include <iostream>
#include <cstring>
#include <cassert>
#include <algorithm>

#define NOTIFY_OBSERVER(observer, notification) \
  if (observer) \
    observer->notify(notification); \

namespace sat
{
  class modulariT_SAT
  {
  public:
#ifndef TEST
  private:
#endif
    /*************************************************************************/
    /*                            Data structures                            */
    /*************************************************************************/
    /**
     * @brief Structure to store the state an metadata of a variable.
     * TODO add details
     */
    typedef struct TSvar
    {
      TSvar()
        : level(LEVEL_UNDEF),
        reason(CLAUSE_UNDEF),
        activity(0.0),
        seen(false),
        waiting(false),
        state(VAR_UNDEF),
        phase_cache(0),
        state_last_sync(VAR_UNDEF)
      {}
      /**
       * @brief Decision level at which the variable was assigned.
       * @details If the variable is unassigned, the level is LEVEL_UNDEF.
       */
      Tlevel level;
      /**
       * @brief Clause that propagated the variable.
       * @details If the variable is assigned by a decision, the reason is CLAUSE_UNDEF.
       */
      Tclause reason;
      /**
       * @brief Activity of the variable. Used in decision heuristics.
       */
      double activity;
      /**
       * @brief Boolean indicating if the variable was already seen. It is used in conflict analysis.
       * @details Variables must remain marked locally. That is, upon exiting the method, all variables must be unmarked.
       */
      unsigned seen : 1;
      /**
       * @brief Boolean indicating whether the variable is in the propagation queue.
       */
      unsigned waiting : 1;
      /**
       * @brief State of the variable. Can be VAR_TRUE, VAR_FALSE or VAR_UNDEF.
       */
      Tval state : 2;
      /**
       * @brief Last value assigned to the variable.
       * @note Used to compute the agility of the solver
       */
      unsigned phase_cache : 1;

      /**
       * @brief Last value assigned to the variable before the last synchronization.
       */
      Tval state_last_sync : 2;

    } TSvar;

    /**
     * @brief Structure to store a clause and its metadata.
     * TODO add details
     */
    typedef struct TSclause
    {
      TSclause(Tlit* lits, unsigned size, bool learned, bool external)
        : lits(lits),
        first_watched(CLAUSE_UNDEF),
        second_watched(CLAUSE_UNDEF),
        deleted(false),
        learned(learned),
        watched(true),
        external(external),
        size(size),
        original_size(size),
        blocker(*lits)
      {
        assert(size < (1 << 28));
      }
      /**
       * @brief Pointer to the first literal of the clause.
       */
      Tlit* lits;
      /**
       * @brief Next clause in the watch list of the first watched literal.
      */
      Tclause first_watched = CLAUSE_UNDEF;
      /**
       * @brief Next clause in the watch list of the second watched literal.
       */
      Tclause second_watched = CLAUSE_UNDEF;
      /**
       * @brief Boolean indicating whether the clause is deleted. That is, the clause is not in the clause set anymore and the memory is available for reuse.
       */
      unsigned deleted : 1;
      /**
       * @brief Boolean indicating whether the clause is a learned clause.
       */
      unsigned learned : 1;
      /**
       * @brief Boolean indicating whether the clause is watched.
       * @details Clauses satisfied at level 0 do not need to be watched
       */
      unsigned watched : 1;
      /**
       * @brief Boolean indicating whether the clause comes from an external source.
       */
      unsigned external : 1;
      /**
       * @brief Size of the clause.
       */
      unsigned size : 28; // this is not really necessary but we have some space left
      /**
       * @brief Size of the clause when it was added to the clause set
       */
      unsigned original_size;
      /**
       * @brief Blocking literal. If the clause is satisfied by the blocking literal, the watched literals are allowed to be falsified.
       * @details In chronological backtracking, the blocking literal must be at a lower level than the watched literals.
       */
      Tlit blocker = LIT_UNDEF;
      /**
       * @brief Activity of the clause. Used in clause deletion heuristics.
       */
      double activity = 0;
    } TSclause;

    /*************************************************************************/
    /*                          Fields definitions                           */
    /*************************************************************************/
    /**
     * @brief Options of the solver.
    */
    sat::options _options;
    /**
     * @brief Status of the solver.
     */
    status _status = UNDEF;
    /**
     * @brief List of variables in the clause set.
     */
    std::vector<TSvar> _vars;
    /**
     * @brief Trail of assigned literals.
     */
    std::vector<Tlit> _trail;
    /**
     * @brief Number of propagated literals
     */
    unsigned _propagated_literals = 0;

    /**  CLAUSES ALLOCATION  **/
    /**
     * @brief Set of clauses
     */
    std::vector<TSclause> _clauses;
    /**
     * @brief List of deleted clauses. The memory of these clauses is available for reuse.
     */
    std::vector<Tclause> _deleted_clauses;
    /**
     * @brief _watch_lists[i] is the first clause of the watch list of the literal i.
     */
    std::vector<Tclause> _watch_lists;
    /**
     * @brief _binary_clauses[l] is the contains the pairs <lit, cl> where lit is a literal to be propagated if l is falsified, and <cl> is the clause that propagates lit.
    */
    std::vector<std::vector<std::pair<Tlit, Tclause>>> _binary_clauses;
    /**
     * @brief _decision_index[i] is the index of the made after level i.
     * @remark _decision_index[0] is the index of the first decision.
     */
    std::vector<unsigned> _decision_index;
    /**
     * @brief Position of the last literal on the trail that was left unchanged since the last synchronization.
     */
    unsigned sync_validity_index;

    /**  ADDING CLAUSES  **/
    /**
     * @brief True if the solver is in clause input mode.
     */
    bool _writing_clause = false;
    /**
     * @brief When in clause input mode, contains a pointer to the first literal of the clause being written.
     */
    Tlit* _literal_buffer;
    /**
     * @brief When in clause input mode, contains the index of the next literal to write.
     */
    unsigned _next_literal_index;

    /**  ACTIVITY HEAP  **/
    /**
     * @brief Activity increment for variables. This value is multiplied by the _var_activity_multiplier until the activity of a variable becomes greater than 10^9. In which case, the activity of all variables is divided by 10^9 and the increment is set to 1.0.
     * TODO update the definition
     */
    double _var_activity_increment = 1.0;

    /**
     * @brief Priority queue of variables. The variables are ordered by their activity.
     */
    sat_utils::heap _variable_heap;

    /**
     * @brief Increases the activity of a variable.
     * @param var variable to bump.
     */
    void bump_var_activity(Tvar var);

    /**  CLAUSE DELETION  **/
    /**
     * @brief Number of learned clauses in the clause set.
     */
    unsigned _n_learned_clauses = 0;
    /**
     * @brief Number of learned clauses before a clause elimination procedure is called
     * @note At first, the number of learned clauses before elimination is the number of external clauses.
     */
    unsigned _next_clause_elimination = 0;
    /**
     * @brief Activity increment for clauses. Each time a clause is used to resolve a conflict, its activity is increased by _clause_activity_increment and _clause_activity_increment is multiplied by _clause_activity_multiplier.
     */
    double _clause_activity_increment = 1;
    /**
     * @brief Maximum possible activity of a clause. It is the sum of the activity increments.
     */
    double _max_clause_activity = 1;
    /**
     * @brief Threshold of the clause activity. If the activity of a clause is lower than the threshold multiplied by the maximum possible activity, the clause is deleted.
     * @details The threshold is a value between 0 and 1. The higher the threshold, the more clauses are deleted.
     */
    double _clause_activity_threshold = 1;

    /**
     * @brief Increases the activity of a clause and updates the maximum possible activity.
     * @details If the maximum possible activity is greater than 1e100, the activity of all clauses is divided by 1e100 and the maximum possible activity is divided by 1e100.
     * @details This procedure keeps a relative order identical to decaying the activity of all clauses by a factor d < 1 and increasing the activity of the clause by 1 - d. Where _clause_activity_multiplier = (1 - d) / d.
     */
    void bump_clause_activity(Tclause cl);

    /**
     * @brief Delete a clause from the clause set.
     */
    void delete_clause(Tclause cl);

    /**
     * @brief Deletes clauses with a low activity.
     * @details Deletes clauses with an activity lower than the threshold multiplied by the maximum possible activity.
     * @details Does not delete external and propagating clauses.
     */
    void simplify_clause_set();

    /**  RESTART AGILITY  **/
    /**
     * @brief Progress metric of the solver
     * @details The agility measures a moving average of the number of flips of polarity for literals.
     * If the agility is high, the solver is making a lot of progress and changes the polarity of literals often.
     * If the agility is low, the solver is not making much progress and changes the polarity of literals rarely.
     * This metric is used to decide when to restart the solver.
     * @details The agility is updated upon implying a literal with the following formula:
     * agility = agility * _agility_decay + (1 - _agility_decay) * change_of_polarity
     * where change_of_polarity is 1 if the polarity of the literal is changed, 0 otherwise.
     * @details The idea is inspired by [2008 - Biere, Armin. "Adaptive restart strategies for conflict driven SAT solvers."]
     * TODO Check if there is not a limit to the decay factor such that it is not possible to finish propagating all literals. It probably depends on the threshold decay factor too.
     */
    double _agility = 1;

    /**  PURGE  **/
    /**
     * @brief Current progress before next purge.
     * @details Count the number of not yet purged level 0 literals on the trail.
     */
    unsigned _purge_counter = 0;
    /**
     * @brief Limit of the purge counter before the next purge.
     */
    unsigned _purge_threshold = 5;
    /**
     * @brief Increment of the purge threshold upon each purge.
     */
    unsigned _purge_inc = 1;

    /**  CHRONOLOGICAL BACKTRACKING  **/
    /**
     * @brief Buffer used in chronological backtracking to store literals that were removed from the trail and must be put back in the trail.
     */
    std::vector<Tlit> _backtrack_buffer;

    /**
     * @brief Reorder the trail by decision level.
     * @details The sorting algorithm should be stable. That is, the relative order of literals with the same decision level should not be changed.
     * @todo This function is not yet implemented.
     */
    void order_trail();

    /**  MISSED LOWER IMPLICATIONS  **/
    /**
     * @brief For each variable, contains the lowest clause that can propagate the variable.
     * The clause must be ordered such that the first literal is the one that could have been propagated earlier. And the second literal is the highest literal in the clause.
     */
    std::vector<Tclause> _lazy_reimplication_buffer;

    /**
     * @brief Buffer used in strong chronological backtracking to store literals that were removed from the trail and should be reimplied after backtracking.
     */
    std::vector<Tclause> _reimplication_backtrack_buffer;

    /**  PROOFS  **/
    /**
     * @brief True if the solver is keeping track of the proof of unsatisfiability.
     */
    bool _proofs = false;

    /**
     * @brief Callback function to print the proof of unsatisfiability.
     */
    void (*_proof_callback)(void) = nullptr;

    /**  SMT SYNCHRONIZATION  **/
    /**
     * @brief Number of literals that were valid since the last synchronization.
     */
    unsigned _number_of_valid_literals = 0;
    /**
     * @brief Set of variables that were touched by the SAT solver since the last synchronization.
     */
    std::set<Tvar> _touched_variables;

    /**  STATISTICS  **/
    /**
     * @brief Statistics of the solver.
     */
    sat::statistics _stats;

    /**  INTERACTIVE SOLVER  **/
    /**
     * @brief Observer of the solver. If _observer is not nullptr, the solver notifies the observer of its progress.
     */
    sat::gui::observer* _observer = nullptr;
    /**
     * @brief True if the solver is interactive.
     * @details The solver is interactive if it stops between decisions to let the user make a decision, hint or learn a clause.
     */
    bool _interactive = false;

    /*************************************************************************/
    /*                       Quality of life functions                       */
    /*************************************************************************/
    /**
     * @brief Returns the level of the given literal. If the literal is not assigned, returns LEVEL_UNDEF.
     * @param lit literal to evaluate.
     * @return level of the literal.
     */
    inline Tlevel lit_level(Tlit lit) const
    {
      return _vars[lit_to_var(lit)].level;
    }

    /**
     * @brief Returns true if a literal is satisfied.
     * @param lit literal to evaluate.
     * @return true if the literal is satisfied, false otherwise.
     */
    inline bool lit_true(Tlit lit) const
    {
      return _vars[lit_to_var(lit)].state == lit_pol(lit);
    }

    /**
     * @brief Returns true if a literal is falsified.
     * @param lit literal to evaluate.
     * @return true if the literal is falsified, false otherwise.
     */
    inline bool lit_false(Tlit lit) const
    {
      return _vars[lit_to_var(lit)].state == (lit_pol(lit) ^ 1);
    }

    /**
     * @brief Returns true if the literal is undefined.
     * @param lit literal to evaluate.
     * @return true if the literal is undefined, false otherwise.
     */
    inline bool lit_undef(Tlit lit) const
    {
      return _vars[lit_to_var(lit)].state >> 1;
    }

    /**
     * @brief Returns the reason of the literal. If the literal is not assigned, returns CLAUSE_UNDEF.
     * @param lit literal to evaluate.
     * @return reason of the literal.
     */
    inline Tclause lit_reason(Tlit lit) const
    {
      return _vars[lit_to_var(lit)].reason;
    }

    /**
     * @brief Adds a clause to the watch list of a literal
     * @param lit literal to watch.
     * @param clause clause to add to the watch list.
     * @pre The literal must be the first or second literal of the clause.
     */
    inline void watch_lit(Tlit lit, Tclause cl)
    {
#if NOTIFY_WATCH_CHANGES
      NOTIFY_OBSERVER(_observer, new sat::gui::watch(cl, lit));
#endif
      ASSERT(cl != CLAUSE_UNDEF);
      ASSERT(cl < _clauses.size());
      ASSERT(_clauses[cl].size > 2);
      ASSERT(lit == _clauses[cl].lits[0] || lit == _clauses[cl].lits[1]);
      // std::cout << "watching " << lit_to_string(lit) << " in clause " << clause_to_string(cl) << std::endl;
      Tclause tmp = _watch_lists[lit];
      _watch_lists[lit] = cl;
      if (lit == _clauses[cl].lits[0])
        _clauses[cl].first_watched = tmp;
      else
        _clauses[cl].second_watched = tmp;
      ASSERT(_clauses[cl].first_watched == CLAUSE_UNDEF
        || _clauses[_clauses[cl].first_watched].lits[0] == _clauses[cl].lits[0]
        || _clauses[_clauses[cl].first_watched].lits[1] == _clauses[cl].lits[0]);
      ASSERT(_clauses[cl].second_watched == CLAUSE_UNDEF
        || _clauses[_clauses[cl].second_watched].lits[0] == _clauses[cl].lits[1]
        || _clauses[_clauses[cl].second_watched].lits[1] == _clauses[cl].lits[1]);
    }

    /**
     * @brief Find the clause cl in the watch list of lit and remove it.
     * @details Complexity: O(n), where n is the length of the watch list.
     */
    inline void stop_watch(Tlit lit, Tclause cl)
    {
#if NOTIFY_WATCH_CHANGES
      NOTIFY_OBSERVER(_observer, new sat::gui::unwatch(cl, lit));
#endif
      ASSERT(cl != CLAUSE_UNDEF);
      ASSERT(_clauses[cl].lits[0] == lit || _clauses[cl].lits[1] == lit);
      Tclause current = _watch_lists[lit];
      Tclause previous = CLAUSE_UNDEF;
      while (current != cl) {
        previous = current;
        if (_clauses[current].lits[0] == lit)
          current = _clauses[current].first_watched;
        else
          current = _clauses[current].second_watched;
        ASSERT(current != CLAUSE_UNDEF);
      }
      ASSERT(current == cl);
      if (previous == CLAUSE_UNDEF) {
        ASSERT(_watch_lists[lit] == cl);
        if (_clauses[current].lits[0] == lit)
          _watch_lists[lit] = _clauses[current].first_watched;
        else
          _watch_lists[lit] = _clauses[current].second_watched;
      }
      else {
        Tclause replacement = _clauses[current].lits[0] == lit ? _clauses[current].first_watched : _clauses[current].second_watched;
        if (_clauses[previous].lits[0] == lit) {
          ASSERT(_clauses[previous].first_watched == cl);
          _clauses[previous].first_watched = replacement;
        }
        else {
          ASSERT_MSG(_clauses[previous].second_watched == cl,
            "lit = " + lit_to_string(lit) + ", previous = " + clause_to_string(previous) + ", current = " + clause_to_string(current));
          _clauses[previous].second_watched = replacement;
        }
      }

    }

    /**
     * @brief Removes deleted and non-watched clauses from the watch lists. Also removes clauses which are not watched by the literal of the list.
     */
    void repair_watch_lists();

    /**
     * @brief Returns true if the literal is currently in the propagation queue
     */
    inline bool lit_waiting(Tlit lit) const
    {
      return _vars[lit_to_var(lit)].waiting;
    }

    /**
     * @brief Mark the literal as seen
     */
    inline void lit_mark_seen(Tlit lit)
    {
      _vars[lit_to_var(lit)].seen = true;
    }

    /**
     * @brief Unmark the literal as seen
     */
    inline void lit_unmark_seen(Tlit lit)
    {
      _vars[lit_to_var(lit)].seen = false;
    }

    /**
     * @brief Returns true if the literal is marked as seen
     */
    inline bool lit_seen(Tlit lit) const
    {
      return _vars[lit_to_var(lit)].seen;
    }

    /**
     * @brief Returns the clause blocker
     */
    inline Tlit lit_blocker(Tclause cl) const
    {
      return _clauses[cl].blocker;
    }

    /**
     * @brief Returns true if the variable is not assigned.
     */
    inline bool var_undef(Tvar var) const
    {
      return _vars[var].state == VAR_UNDEF;
    }

    /**
     * @brief Returns true if the variable is assigned true.
     */
    inline bool var_true(Tvar var) const
    {
      return _vars[var].state == VAR_TRUE;
    }

    /**
     * @brief Returns true if the variable is assigned false.
     */
    inline bool var_false(Tvar var) const
    {
      return _vars[var].state == VAR_FALSE;
    }

    /**
     * @brief Unassign a variable.
     */
    inline void var_unassign(Tvar var)
    {
      TSvar& v = _vars[var];
      NOTIFY_OBSERVER(_observer, new sat::gui::unassignment(literal(var, v.state)));
      v.state = VAR_UNDEF;
      v.reason = CLAUSE_UNDEF;
      v.level = LEVEL_UNDEF;
      if (!_variable_heap.contains(var))
        _variable_heap.insert(var, v.activity);
    }

    /**
     * @brief If var is an unknown variable, reallocate the data structures to take into account the new variable.
     * @param var variable to allocate.
     * @note Every variable with an index lower than var will be allocated.
     */
    inline void var_allocate(Tvar var)
    {
      for (Tvar i = _vars.size(); i <= var; i++) {
        _variable_heap.insert(i, 0.0);
        NOTIFY_OBSERVER(_observer, new sat::gui::new_variable(i));
      }
      if (var >= _vars.size() - 1) {
        _vars.resize(var + 1);
        _lazy_reimplication_buffer.resize(var + 1, CLAUSE_UNDEF);
        _watch_lists.resize(2 * var + 2, CLAUSE_UNDEF);
        _binary_clauses.resize(2 * var + 2);
        // reallocate the literal buffer to make sure it is big enough
        Tlit* new_literal_buffer = new Tlit[_vars.size()];
        std::memcpy(new_literal_buffer, _literal_buffer, _next_literal_index * sizeof(Tlit));
        delete[] _literal_buffer;
        _literal_buffer = new_literal_buffer;
      }
    }

    /**
     * @brief Returns a literal utility metric to choose the literals to watch.
     * @param lit literal to evaluate.
     * @return utility metric of the literal.
     * @details The utility of a falsified literal is its decision level. The utility of an undefined literal is higher than the utility of a falsified literal. The utility of a satisfied literal is higher than the utility of an undefined literal and decreases with the decision level.
     */
    unsigned utility_heuristic(Tlit lit);

    /**
     * @brief Prints a literal on the standard output.
     * @param lit literal to print.
     */
    void print_lit(Tlit lit);

    /**
     * @brief Parses a command and executes it.
     * @details A valid command is a command of the type
     * DECIDE [literal]   (to decide a literal, if literal is not provided, the solver decides)
     * HINT <literal>     (to hint a literal)
     * LEARN [literal]+   (to learn a clause from the given literals)
     * DELETE_CLAUSE <clause> (to delete a clause)
     * HELP               (to print the list of commands)
     * QUIT               (to quit the solver)
     * @param command command to parse.
     * @return true if the command was parsed successfully, false otherwise.
     */
    bool parse_command(std::string command);

    /*************************************************************************/
    /*                          Internal functions                           */
    /*************************************************************************/
    /**
     * @brief Add one literal to the propagation queue.
     * @pre The literal must be unassigned.
     * @pre The literal must not be in the propagation queue.
     */
    void stack_lit(Tlit lit, Tclause reason);

    /**
     * @brief Searches for a replacement literal for the second watched literal of a clause.
     * @details In Non-Chronological Backtracking, a suitable replacement is any literal that is not falsified. In Strong Chronological Backtracking, a suitable replacement is a literal ...
     * TODO finish the description
    */
    Tlit* seach_replacement(Tlit* lits, unsigned size);

    /**
     * @brief Propagate a literal at level 0
     * @param lit literal to propagate.
     */
    Tclause propagate_fact(Tlit lit);

    /**
     * @brief Propagate one literal.
     * @param lit literal to propagate.
     */
    Tclause propagate_lit(Tlit lit);

    /**
     * @brief Undo literals above the given level.
     * @param level level to backtrack to.
     */
    void backtrack(Tlevel level);

    /**
     * @brief Backtrack literals in the chronological setting.
     */
    void CB_backtrack(Tlevel level);

    /**
     * @brief Repairs the conflict by analyzing it if needed and backtracking to the appropriate level.
     * @param conflict clause that caused the conflict.
     */
    void repair_conflict(Tclause conflict);

    /**
     * @brief Returns true if the literal is required in the learned clause. Returns false if the literal is already implied by the other literals of the clause.
     *
     * @param lit literal to evaluate.
     * @return false if the literal is redundant with the current learned clause
     */
    bool lit_is_required_in_learned_clause(Tlit lit);

    /**
     * Analyze a conflict and learn a new clause.
     * @param conflict clause that caused the conflict.
     */
    void analyze_conflict_reimply(Tclause conflict);

    /**
     * Analyze a conflict and learn a new clause.
     * @param conflict clause that caused the conflict.
     */
    void analyze_conflict(Tclause conflict);

    /**
     * @brief Restarts the solver by resetting the trail
     */
    void restart();

    /**
     * @brief Remove clauses satisfied at level 0 and remove literals falsified at level 0 from the clauses.
     */
    void purge_clauses();

    /**
     * @brief Brings forward the two most relevant literals of the clause. A literal is more relevant if it has a higher utility (see utility_heuristic()). The literals are moved to the first two positions of the clause.
     * @param lits array of literals to reorder.
     * @param size size of the clause.
     */
    void select_watched_literals(Tlit* lits, unsigned size);

    /**
     * @brief allocates a new chunk of memory for a clause, and adds to the clause set. The clause is added to the watch lists if needed (size >= 2).
     * TODO add binary clause set?
     * @param lits array of literals to add to the clause set.
     * @param size size of the clause.
     * @param learned true if the clause is a learned clause, false otherwise.
     * @param external true if the clause is an external clause, false otherwise.
     */
    Tclause internal_add_clause(Tlit* lits, unsigned size, bool learned, bool external);

    /*************************************************************************/
    /*                          Public interface                             */
    /*************************************************************************/
  public:
    /**
     * @brief Construct a new modulariT_SAT::modulariT_SAT object
     * @param n_var initial number of variables. Can be increased later.
     * @param n_clauses initial number of clauses. Can be increased later by adding clauses.
     */
    modulariT_SAT(unsigned n_var, unsigned n_clauses, sat::options& options);

    /**
     * @brief Parse a DIMACS file and add the clauses to the clause set.
     */
    void parse_dimacs(const char* filename);

    /**
     * @brief Set the callback function to print the proof of unsatisfiability.
     */
    void set_proof_callback(void (*proof_callback)(void));

    /**
     * @brief Returns true if the solver is in interactive mode.
     * @return true if the solver is in interactive mode, false otherwise.
     */
    bool is_interactive() const;

    /**
     * @brief Returns true if the solver is in observing mode.
     * @return true if the solver is in observing mode, false otherwise.
     */
    bool is_observing() const;

    /**
     * @brief Returns a pointer to the observer of the solver.
     * @return pointer to the observer of the solver.
     * @note The pointer is nullptr if the solver is not observing.
     * @note It is the responsibility of the user querying the observer to not compromise the integrity of the observer. That is, the user must not delete the observer or modify its state.
     * @warning This method is just a convenience for the main. It is not meant to be used in a library.
     */
    sat::gui::observer* get_observer() const;

    /**
     * @brief Propagate literals in the queue and resolve conflicts if needed. The procedure stops when all variables are assigned, or a decision is needed. If the solver is in input mode, it will switch to propagation mode.
     * @pre The solver status must be undef
     * @return true if the solver may make a decision, false if all variables are assigned or the clause set is unsatisfiable.
     */
    bool propagate();

    /**
     * @brief Solves the clause set. The procedure stops when all variables are assigned, of the solver concludes that the clause set is unsatisfiable.
     */
    status solve();

    /**
     * @brief Returns the status of the solver.
     * @return status of the solver.
     */
    status get_status();

    /**
     * @brief Decides the value of a variable. The variable must be unassigned.
     * @pre The solver status must be undef
     * @pre The propagation queue must be empty
     * @return true if the solver is still undef after the decision, false if all variables are assigned.
     */
    bool decide();

    /**
     * @brief Forces the solver to decide the given literal. The literal must be unassigned.
     * @param lit literal to decide.
     */
    bool decide(Tlit lit);

    /**
     * @brief Set up the solver to lits a new clause. Sets the solver in clause_input mode.
     */
    void start_clause();

    /**
     * @brief Add a literal to the current clause.
     * @param lit literal to add to the clause.
     * @pre The solver must be in clause_input mode.
     * @pre The literal or its negation must not already be in the clause.
     */
    void add_literal(Tlit lit);

    /**
     * @brief Finalize the current clause and add it to the clause set. If the solver is in propagation mode, it will propagate literals if needed.
     * @pre The solver must be in clause_input mode.
     * @return status of the solver after adding the clause.
     */
    status finalize_clause();

    /**
     * @brief Add a complete clause to the clause set.
     * @param lits array of literals to add to the clause set.
     * @param size size of the clause.
     * @return status of the solver after adding the clause.
     * @note The memory of the clause is allocated by the solver. Therefore, the pointer lits is managed by the user and is not freed by the solver.
     */
    status add_clause(Tlit* lits, unsigned size);

    /**
     * @brief Returns the literals of a clause.
     * @param clause clause id of the clause.
     * @return pointer to the first literal of the clause.
     * @note should be called in conjunction with get_clause_size() to get the size of the clause.
     */
    const Tlit* get_clause(Tclause cl) const;

    /**
     * @brief Returns the size of a clause.
     * @param clause clause id of the clause.
     * @return size of the clause.
     */
    unsigned get_clause_size(Tclause cl) const;

    /**
     * @brief Provide a hint to the SAT solver. The hint will be considered as a decision.
     * @param lit literal to assign
     */
    void hint(Tlit lit);

    /**
     * @brief Provide a hint to the SAT solver. The hint will be assigned at the given decision level.
     * @param lit literal to assign
     * @param level decision level of the assignment
     * @pre The level must be lower than or equal to the current decision level.
     */
    void hint(Tlit lit, unsigned int level);

    /**
     * @brief Notify the solver that the trail was synchronized by the user. This function will reset the colors of the variables.
     */
    void synchronize();

    /**
     * @brief Returns the index of the last literal that remains valid since the last synchronization. That is, every literal with an index lower than the returned value has been unchanged since the last synchronization.
     */
    unsigned sync_validity_limit();

    /**
     * @brief Returns the modification of state of a variable since the last synchronization.
     * @return 0 if no change, 1 if the variable was assigned, 2 if the variable was unassigned, 3 if the variable changed polarity.
     */
    unsigned sync_color(Tvar var);

    /**
     * @brief Set a markup to the last literal on the trail. The markup function will be called when the literal is backtracked.
     */
    void set_markup(void (*markup_function)(void));

    /**
     * @brief Returns a reference to the trail. The trail should not be modified by the user.
     */
    const std::vector<Tlit>& trail() const;

    /**
     * @brief Returns the current decision level.
     */
    Tlevel decision_level() const;

    /*************************************************************************/
    /*                        Printing the state                             */
    /*************************************************************************/
    /**
     * @brief Returns a colored string of a literal. The literal is printed green if satisfied, red if falsified, and yellow if unassigned
     * @param lit literal to print.
     * @return colored string of the literal.
     */
    std::string lit_to_string(Tlit lit);

    /**
     * @brief Returns a string of a clause. The clause is printed in the form "cl : lit1 lit2 ... litm | litm+1 ... litn" where lit1, ..., litn are the literals of the clause, litm+1, ..., litn are disabled literals (false at level 0) and cl is the clause id.
     * @param clause clause to print.
     * @return string of the clause.
     */
    std::string clause_to_string(Tclause cl);

    /**
     * @brief Prints the current assignment of the solver on the standard output in a human-readable format.
     */
    void print_trail();

    /**
     * @brief Prints the current assignment of the solver on the standard output in a human-readable format.
     */
    void print_trail_simple();

    /**
     * @brief Prints a clause on the standard output in a human-readable format.
     * @param clause clause to print.
     */
    void print_clause(Tclause cl);

    /**
     * @brief Prints the clause set on the standard output in a human-readable format.
     */
    void print_clause_set();

    /**
     * @brief Prints the watch lists on the standard output in a human-readable format.
     */
    void print_watch_lists(Tlit lit = LIT_UNDEF);

    /**
     * @brief Destroy the modulariT_SAT::modulari T_SAT object
     */
    ~modulariT_SAT();

    /*************************************************************************/
    /*                     Functions for the checker                         */
    /*************************************************************************/
  private:
    /**
     * @brief returns true if no clause is falsified by the propagated literals.
     */
    bool trail_consistency();

    /**
     * @brief Returns trues if every variable in the trail is assigned and that every assigned variable is in the trail.
     */
    bool trail_variable_consistency();

    /**
     * @brief returns true if the levels of the literals in the trail are monotonically increasing.
     */
    bool trail_monotonicity();

    /**
     * @brief returns true if for each decision, the literals in the trail before that decision are at a level lower than the decision level.
     */
    bool trail_decision_levels();

    /**
     * @brief returns true if the trail is a topological sort of the implication graph.
     */
    bool trail_topological_sort();

    /**
     * @brief returns true if no clause is made unit by propagated literals
     */
    bool trail_unit_propagation();

    /**
     * @brief returns true if no clause is unisat by propagated literals and the decision level of the satisfied literal is higher than the decision level of the falsified literals.
     */
    bool bcp_safety();

    /**
     * @brief returns true if the clause cl is in the watch list of the literal lit.
    */
    bool is_watched(Tlit lit, Tclause cl);

    /**
     * @brief returns true if the watch lists of watched literals of a clause contain the clause.
     */
    bool watch_lists_complete();

    /**
     * @brief returns true if each clause in a watch list is watched by the corresponding literal.
     */
    bool watch_lists_minimal();

    /**
     * @brief Returns true if, for each clause, at least one watched literal is not falsified by propagated literals. Otherwise, prints an error message on cout and cerr and returns false.
     */
    bool strong_watched_literals();

    /**
     * @brief Returns true if, for each clause, at least one watched literal is not falsified by propagated literals or the clause is satisfied at a lower level than the watched literals. Otherwise, prints an error message on cout and cerr and returns false.
     */
    bool weak_watched_literals();

    /**
     * @brief Returns true if the clause is watched by at least one literal at the highest decision level, or one non-falsified literal. Otherwise, prints an error message on cout and cerr and returns false.
     * @param clause clause to check.
     */
    bool watched_levels(Tclause cl);

    /**
     * @brief returns true if the clause is not falsified by the trail. Otherwise, prints an error message on cout and cerr and returns false.
     * @param clause clause to check.
     */
    bool non_falsified_clause(Tclause cl);

    /**
     * @brief Returns true if the clause is not falsified by propagated literals. Otherwise, prints an error message on cout and cerr and returns false.
     * @param clause clause to check.
     * @note This method is distinct from non_falsified_clause() because it allows the clause to be falsified if one of the falsified literals is in the propagation queue.
     */
    bool weak_non_falsified_clause(Tclause cl);

    /**
     * @brief returns true if no clause is made unit by propagated literals. Otherwise, prints an error message on cout and cerr and returns false.
     */
    bool no_unit_clauses();

    /**
     * @brief Returns true is the clause does not contain the same literal multiple times and the clause is not trivially satisfied (contains both a literal and its negation). Otherwise, prints an error message on cout and cerr and returns false.
     * @param clause clause to check.
     */
    bool clause_soundness(Tclause cl);

    /**
     * @brief Returns true if the watched literals of the clause are the two most relevant literals of the clause following the utility heuristic. Otherwise, prints an error message on cout and cerr and returns false.
     * @param clause clause to check.
     */
    bool most_relevant_watched_literals(Tclause cl);
  };
}
