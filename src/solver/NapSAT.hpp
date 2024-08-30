/**
 * @file src/solver/NapSAT.hpp
 * @author Robin Coutelier
 *
 * @brief This file is part of the NapSAT solver. It defines the interface and
 * data structures of the SAT solver and its data structures.
 * @details
 * we call: F the set of clauses
 *          π the complete partial assignment
 *          πᵈ the set of decision literals
 *          τ the set of propagated literals
 *          ω the propagation queue
 *          δ(ℓ) the decision level of ℓ
 *          ρ(ℓ) the reason of ℓ
 *          λ(ℓ) the lazy reason of ℓ. That is, a missed lower implication of ℓ.
 *          WL(ℓ) the watch list of ℓ
 *          ■ the undefined clause
 *
 * The SAT solver complies to the following invariants:
 * (Trail construction): The partial assignment is the concatenation of the
 * propagated literals and the propagation queue
 *    π = τ ⋅ ω
 *    πᵈ ⊆ π
 * (No duplicate atoms): No atom is assigned twice in π
 *    ∀ℓ ∈ π. ¬ℓ ∉ π
 * (Implications): Each assigned literal is either a decision or is implied by
 * its reason
 *    ∀ℓ ∈ π. ℓ ∈ πᵈ ∨ [ℓ ∈ ρ(ℓ) ∧ C \ {ℓ}, π ⊧ ⊥]
 *    ∀ℓ ∈ π. ℓ ∉ πᵈ ⇔ ρ(ℓ) ∈ F
 *    ∀ℓ ∉ π. ρ(ℓ) = ■
 * (Decision level): The decision level of a literal is the highest decision
 * level of its reason if the literal was implied, and the number of decisions
 * before the literal if the literal was a decision. If p(ℓ) is the position of
 * ℓ in π, then:
 *    ∀ℓ ∈ π. ℓ ∈ πᵈ ⇒ δ(ℓ) = |{ℓ' ∈ πᵈ: p(ℓ') < p(ℓ)}| + 1
 *    ∀ℓ ∈ π. ℓ ∉ πᵈ ⇒ δ(ℓ) = δ(ρ(ℓ) \ {ℓ})
 * (Weak watched literals): If one of the watched literals is falsified, either
 * the other watched literal is satisfied, or the clause is satisfied by the
 * blocking literal. For each clause C = {c₁, c₂, ...} in F watched by c₁ and
 * c₂ and with a blocker b:
 *    c₁ ∈ C ∧ c₂ ∈ C ∧ b ∈ C ∧ c₁ ≠ c₂
 *    ¬c₁ ∈ τ ⇒ ¬c₂ ∉ τ ∨ b ∈ π
 * (Watcher lists): The watcher lists of the literals are consistent. That is,
 * if a clause is watched by a literal, the clause is in the watch list of the
 * literal. For each clause C = {c₁, c₂, ...} in F watched by c₁ and c₂:
 *    C ∈ WL(c₁) ∧ C ∈ WL(c₂)
 *    C ∉ WL(ℓ) ⇒ [c₁ ≠ ℓ ∧ c₂ ≠ ℓ]
 * (Topological order): The trail is a topological sort of the implication
 * graph. If p(ℓ) is the position of ℓ in π, then:
 *    ∀ℓ ∈ π. ∀ℓ' ∈ ρ(ℓ). p(ℓ') ≤ p(ℓ)
 *
 * In NCB, we have the additional invariants:
 * (Trail monotonicity): The level of the literals in the trail is non-
 * decreasing. If p(ℓ) is the position of ℓ in π, then:
 *     ∀ℓ ∈ π. ∀ℓ' ∈ π. p(ℓ') < p(ℓ) ⇒ δ(ℓ') ≤ δ(ℓ)
 * (Strong watched literals): If one of the watched literals is falsified,
 * either the other watched literal is satisfied, or the clause is satisfied by
 * the blocking literal. For each clause C = {c₁, c₂, ...} in F watched by c₁
 * and c₂ and with a blocker b:
 *    c₁ ∈ C ∧ c₂ ∈ C ∧ b ∈ C ∧ c₁ ≠ c₂
 *    ¬c₁ ∈ τ ⇒ [c₂ ∈ π ∨ b ∈ π]
 *
 * In WCB, the blockers have to be weakened:
 * (Weak blocked watched literals): If one of the watched literals is falsified,
 * either the other watched literal is satisfied, or the clause is satisfied by
 * the blocking literal at a level lower than the falsified literal. For each
 * clause C = {c₁, c₂, ...} in F watched by c₁ and c₂ and with a blocker b:
 *    c₁ ∈ C ∧ c₂ ∈ C ∧ b ∈ C ∧ c₁ ≠ c₂
 *    ¬c₁ ∈ τ ⇒ ¬c₂ ∉ τ ∨ [b ∈ π ∧ δ(b) ≤ δ(c₁)]
 *
 * In SCB, we have the additional invariants:
 * (Lazy reason): If a lazy reason is set for a literal, then the lazy reason
 * is a missed lower implication of the literal. That is, a clause that could
 * propagate the literal at a lower level.
 *     ∀ℓ ∉ π. λ(ℓ) = ■
 *     ∀ℓ ∈ π. λ(ℓ) ≠ ■ ⇒ ℓ ∈ π ∧ ℓ ∈ λ(ℓ)
 *                       ∧ λ(ℓ) \ {ℓ}, π ⊧ ⊥
 *                       ∧ δ(λ(ℓ) \ {ℓ}) < δ(ℓ)
 *
 * (Lazy backtrack compatible watched literals): If one of the watched literals
 * is falsified, either the other watched literal is satisfied at a lower
 * level, or it is satisfied and the lazy reason has a lower decision level, or
 * the clause is satisfied by the blocking literal. For each clause
 * C = {c₁, c₂, ...} in F watched by c₁ and c₂ and with a blocker b:
 *    c₁ ∈ C ∧ c₂ ∈ C ∧ b ∈ C ∧ c₁ ≠ c₂
 *    ¬c₁ ∈ τ ⇒ [c₂ ∈ π ∧ [δ(c₂) ≤ δ(c₁) ∨ δ(λ(c₂) \ {c₂}) ≤ δ(c₁)]
 *             ∨ [b ∈ π ∧ δ(b) ≤ δ(c₁)]
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
#include "../proof/proof.hpp"

#include <vector>
#include <set>
#include <iostream>
#include <cstring>
#include <cassert>
#include <algorithm>

#if USE_OBSERVER
#define NOTIFY_OBSERVER(observer, notification) \
  if (observer) \
    if(!observer->notify(notification)) { \
      LOG_ERROR("The notification returned an error when executed by the observer"); \
      if(observer) \
        observer->notify(new napsat::gui::marker("Notification failed")); \
      assert(false); \
    }
#else
#define NOTIFY_OBSERVER(observer, notification)
#endif
namespace napsat
{
  class NapSAT
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
       * @details If the variable is assigned by a decision, the reason is
       * CLAUSE_UNDEF.
       * @note In mathematical symbols, we write ρ(ℓ) as the reason of the
       * literal ℓ or ¬ℓ.
       */
      Tclause reason;
      /**
       * @brief Activity of the variable. Used in decision heuristics.
       */
      double activity;
      /**
       * @brief Boolean indicating if the variable was already seen. It is used
       * in conflict analysis.
       * @details Variables must remain marked locally. That is, upon exiting
       * the method, all variables must be unmarked.
       */
      unsigned seen : 1;
      /**
       * @brief Boolean indicating whether the variable is in the propagation
       * queue.
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
       * @brief Last value assigned to the variable before the last
       * synchronization.
       */
      Tval state_last_sync : 2;

      /**
       * @brief Stores the a clause that could propagate the variable at a
       * lower level.
       * @details The missed lower implication follows the following invariant
       * in strong chronological backtracking:
       * λ(ℓ) ≠ ■ ⇒ ℓ ∈ π ∧ ℓ ∈ λ(ℓ)
       *          ∧ λ(ℓ) \ {ℓ}, π' ⊧ ⊥
       *          ∧ δ(λ(ℓ) \ {ℓ}) < δ(ℓ)
       */
      Tclause missed_lower_implication = CLAUSE_UNDEF;
    } TSvar;

#define CLAUSE_HEAD_SIZE 5

    /**
     * @brief Structure to store a clause and its metadata.
     * TODO add details
     */
    typedef struct  alignas(16) TSclause
    {
      /**
       * @brief Constructor of the clause.
       * @details This constructor is destructive. It initializes uses the
       * memory of the lits pointer to store the clause.
      */
      TSclause(Tlit* lits, unsigned size, bool learned, bool external) :
        lits(lits),
        deleted(false),
        learned(learned),
        watched(true),
        external(external),
        size(size),
        blocker(LIT_UNDEF)
      {
        assert(size < (1 << 28));
      }
      /**
       * @brief Pointer to the first literal of the clause.
       * @details The two first literals (if they exist) are the watched
       * literals.
       */
      Tlit* lits;
      /**
       * @brief Boolean indicating whether the clause is deleted. That is, the
       * clause is not in the clause set anymore and the memory is available
       * for reuse.
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
       * @brief Boolean indicating whether the clause comes from an external
       * source.
       */
      unsigned external : 1;
      /**
       * @brief Size of the clause when it was added to the clause set
       * @details Used to know the size of the allocated memory.
       */
      unsigned size : 28;
      /**
       * @brief Blocking literal. If the clause is satisfied by the blocking
       * literal, the watched literals are allowed to be falsified.
       * @details In chronological backtracking, the blocking literal must be
       * at a lower level than the watched literals.
       */
      Tlit blocker = LIT_UNDEF;
    } TSclause;

    /*************************************************************************/
    /*                          Fields definitions                           */
    /*************************************************************************/
    /**
     * @brief Options of the solver.
    */
    napsat::options _options;
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
     * @details The trail is divided into two parts π = τ ⋅ ω
     * where τ is the set of propagated literals and ω is the propagation queue.
     */
    std::vector<Tlit> _trail;
    /**
     * @brief Number of propagated literals
     * @details This is the boundary between τ and ω.
     */
    unsigned _propagated_literals = 0;

    /**  CLAUSES ALLOCATION  **/
    /**
     * @brief Set of clauses
     */
    std::vector<TSclause> _clauses;
    /**
     * @brief List of deleted clauses. The memory of these clauses is available
     * for reuse.
     */
    std::vector<Tclause> _deleted_clauses;

    /**
     * @brief Sizes of allocated memory for the clauses.
     * @details Since we might remove literals from the clauses, we need to know
     * the original size of the allocated memory to not reallocate the memory
     * when it is not necessary.
     * @remark This information was previously stored in the TSclause structure
     * but it was moved here to reduce the size of the TSclause structure.
    */
    std::vector<unsigned> _clauses_sizes;

    /**
     * @brief _watch_lists[i] is the first clause of the watch list of the
     * literal i.
     */
    std::vector<std::vector<Tclause>> _watch_lists;
    /**
     * @brief _binary_clauses[l] is the contains the pairs <lit, cl> where lit
     * is a literal to be propagated if l is falsified, and <cl> is the clause
     * that propagates lit.
    */
    std::vector<std::vector<std::pair<Tlit, Tclause>>> _binary_clauses;
    /**
     * @brief _decision_index[i] is the index of the decision made after level i.
     * @remark _decision_index[0] is the index of the first decision.
     */
    std::vector<unsigned> _decision_index;
    /**
     * @brief Position of the last literal on the trail that was left unchanged
     * since the last synchronization.
     */
    unsigned sync_validity_index;

    /**  ADDING CLAUSES  **/
    /**
     * @brief True if the solver is in clause input mode.
     */
    bool _writing_clause = false;
    /**
     * @brief When in clause input mode, contains a pointer to the first
     * literal of the clause being written.
     */
    Tlit* _literal_buffer;
    /**
     * @brief When in clause input mode, contains the index of the next literal
     * to write.
     */
    unsigned _next_literal_index;

    /**  ACTIVITY HEAP  **/
    /**
     * @brief Activity increment for variables. This value is multiplied by the
     * _var_activity_multiplier until the activity of a variable becomes
     * greater than 10^9. In which case, the activity of all variables is
     * divided by 10^9 and the increment is set to 1.0.
     * TODO update the definition
     */
    double _var_activity_increment = 1.0;

    /**
     * @brief For a clause C, _activities[C] is the activity of the clause.
    */
    std::vector<double> _activities;

    /**
     * @brief Priority queue of variables. The variables are ordered by their
     * activity.
     */
    napsat::utils::heap _variable_heap;

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
     * @brief Number of learned clauses before a clause elimination procedure
     * is called
     * @note At first, the number of learned clauses before elimination is the
     * number of external clauses.
     */
    unsigned _next_clause_elimination = 0;
    /**
     * @brief Activity increment for clauses. Each time a clause is used to
     * resolve a conflict, its activity is increased by
     * _clause_activity_increment and _clause_activity_increment is multiplied
     * by _clause_activity_multiplier.
     */
    double _clause_activity_increment = 1;
    /**
     * @brief Maximum possible activity of a clause. It is the sum of the activity increments.
     */
    double _max_clause_activity = 1;
    /**
     * @brief Threshold of the clause activity. If the activity of a clause is
     * lower than the threshold multiplied by the maximum possible activity,
     * the clause is deleted.
     * @details The threshold is a value between 0 and 1. The higher the
     * threshold, the more clauses are deleted.
     */
    double _clause_activity_threshold = 1;

    /**
     * @brief Increases the activity of a clause and updates the maximum
     * possible activity.
     * @details If the maximum possible activity is greater than 1e100, the
     * activity of all clauses is divided by 1e100 and the maximum possible
     * activity is divided by 1e100.
     * @details This procedure keeps a relative order identical to decaying the
     * activity of all clauses by a factor d < 1 and increasing the activity of
     * the clause by 1 - d. Where _clause_activity_multiplier = (1 - d) / d.
     */
    void bump_clause_activity(Tclause cl);

    /**
     * @brief Delete a clause from the clause set.
     */
    void delete_clause(Tclause cl);

    /**
     * @brief Deletes clauses with a low activity.
     * @details Deletes clauses with an activity lower than the threshold
     * multiplied by the maximum possible activity.
     * @details Does not delete external and propagating clauses.
     */
    void simplify_clause_set();

    /**  RESTART AGILITY  **/
    /**
     * @brief Progress metric of the solver
     * @details The agility measures a moving average of the number of flips of
     * polarity for literals.
     * If the agility is high, the solver is making a lot of progress and
     * changes the polarity of literals often.
     * If the agility is low, the solver is not making much progress and
     * changes the polarity of literals rarely.
     * This metric is used to decide when to restart the solver.
     * @details The agility is updated upon implying a literal with the
     * following formula:
     * agility = agility * _agility_decay + (1 - _agility_decay) * change_of_polarity
     * where change_of_polarity is 1 if the polarity of the literal is changed,
     * 0 otherwise.
     * @details The idea is inspired by [2008 - Biere, Armin. "Adaptive restart
     * strategies for conflict driven SAT solvers."]
     * TODO Check if there is not a limit to the decay factor such that it is not possible to finish propagating all literals. It probably depends on the threshold decay factor too.
     */
    double _agility = 1;

    /**  PURGE  **/
    /**
     * @brief Current progress before next purge.
     * @details Count the number of not yet purged level 0 literals on the
     * trail.
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
     * @brief Buffer used to reorder the backtracked variables.
     * @details In the implementation of backtrack(level), the literals are
     * removed from left to right to avoid using a buffer to push back the
     * literals in chronological backtracking. But this yield an awkward inter-
     * mediate state in which the trail is not sound (e.g., decision removed
     * but implied literals still there). This buffer is used to store the
     * literals that were removed from the trail such that we can notify the
     * observer in the proper order (right to left)
     */
    std::vector<Tvar> _backtracked_variables;

    /**
     * @brief Reorder the trail by decision level.
     * @details The sorting algorithm should be stable. That is, the relative
     * order of literals with the same decision level should not be changed.
     * @details The goal of this function is to be able to go from chronological
     * backtracking to non-chronological backtracking during the search.
     * @todo This function is not yet implemented.
     */
    void order_trail();

    /**  MISSED LOWER IMPLICATIONS  **/
    /**
     * @brief Buffer used in strong chronological backtracking to store
     * literals that were removed from the trail and should be reimplied after
     * backtracking.
     */
    std::vector<Tclause> _reimplication_backtrack_buffer;

    /**  PROOFS  **/
    /**
     * @brief Proof builder of the solver. If _proof is not nullptr, the solver
     * builds a resolution proof for unsatisfiability.
    */
    napsat::proof::resolution_proof* _proof = nullptr;

    /**  SMT SYNCHRONIZATION  **/
    /**
     * @brief Number of literals that were valid since the last synchronization.
     * @todo Not supported yet.
     */
    unsigned _number_of_valid_literals = 0;
    /**
     * @brief Set of variables that were touched by the SAT solver since the
     * last synchronization.
     */
    std::set<Tvar> _touched_variables;

    /**  STATISTICS  **/
    /**
     * @brief Statistics of the solver.
     */
    napsat::statistics _stats;

    /**  INTERACTIVE SOLVER  **/
#if USE_OBSERVER
    /**
     * @brief Observer of the solver. If _observer is not nullptr, the solver
     * notifies the observer of its progress.
     */
    napsat::gui::observer* _observer = nullptr;
#endif
    /**
     * @brief True if the solver is interactive.
     * @details The solver is interactive if it stops between decisions to let
     * the user make a decision, hint or learn a clause.
     */
    bool _interactive = false;

    /*************************************************************************/
    /*                       Quality of life functions                       */
    /*************************************************************************/
    /**
     * @brief Returns the level of the given literal. If the literal is not
     * assigned, returns LEVEL_UNDEF.
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
      return !(_vars[lit_to_var(lit)].state ^ lit_pol(lit));
    }

    /**
     * @brief Returns true if a literal is falsified.
     * @param lit literal to evaluate.
     * @return true if the literal is falsified, false otherwise.
     */
    inline bool lit_false(Tlit lit) const
    {
      return !(_vars[lit_to_var(lit)].state ^ lit_pol(lit) ^ 1);
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
     * @brief Returns the reason of the literal. If the literal is not
     * assigned, returns CLAUSE_UNDEF.
     * @param lit literal to evaluate.
     * @return reason of the literal.
     */
    inline Tclause lit_reason(Tlit lit) const
    {
      return _vars[lit_to_var(lit)].reason;
    }

    /**
     * @brief Returns an alternative reason for propagating the variable at a
     * lower level.
     * If no such reason exists, returns CLAUSE_UNDEF.
     * @param var variable to evaluate.
     * @return a missed lower implication of the variable.
     */
    inline Tclause var_lazy_reason(Tvar var) const
    {
      return _vars[var].missed_lower_implication;
    }

    /**
     * @brief Returns an alternative reason for propagating the literal at a
     * lower level.
     * If no such reason exists, returns CLAUSE_UNDEF.
     * @param lit literal to evaluate.
     * @return a missed lower implication of the literal.
     */
    inline Tclause lit_lazy_reason(Tlit lit) const
    {
      return _vars[lit_to_var(lit)].missed_lower_implication;
    }

    /**
     * @brief Returns the level of the lazy reimplication of a literal.
     * @param lit literal to evaluate.
     * @return level of the lazy reimplication of the literal
     * That is, δ(λ(ℓ) \ {ℓ}) if ℓ is lit
     */
    inline Tlevel lit_lazy_level(Tlit lit) const
    {
      if (lit_lazy_reason(lit) == CLAUSE_UNDEF)
        return LEVEL_UNDEF;
      ASSERT(lit_level(lit) > LEVEL_ROOT);
#ifndef NDEBUG
      Tlit* lits = _clauses[lit_lazy_reason(lit)].lits;
      ASSERT(lit_level(lit) > lit_level(lits[1]));
      for (unsigned i = 1; i < _clauses[lit_lazy_reason(lit)].size; i++) {
        ASSERT(lit_false(lits[i]));
        ASSERT(lit_level(lits[i]) <= lit_level(lits[1]));
      }
#endif
      return lit_level(_clauses[lit_lazy_reason(lit)].lits[1]);
    }

    /**
     * @brief Sets a missed lower implication for a variable.
     * @param var variable to set the missed lower implication.
     */
    inline void var_set_lazy_reason(Tvar var, Tclause cl)
    {
      _vars[var].missed_lower_implication = cl;
      NOTIFY_OBSERVER(_observer,
                      new napsat::gui::missed_lower_implication(var, cl));
    }

    /**
     * @brief Sets a missed lower implication for a literal.
     * @param lit literal to set the missed lower implication.
     */
    inline void lit_set_lazy_reason(Tlit lit, Tclause cl)
    {
      _vars[lit_to_var(lit)].missed_lower_implication = cl;
      NOTIFY_OBSERVER(_observer,
                      new napsat::gui::missed_lower_implication(lit_to_var(lit),
                                                             cl));
    }

    inline Tlevel solver_level() const
    {
      return _decision_index.size();
    }

    /**
     * @brief Adds a clause to the watch list of a literal
     * @param lit literal to watch.
     * @param clause clause to add to the watch list.
     * @pre The literal must be the first or second literal of the clause.
     * @post
     */
    void watch_lit(Tlit lit, Tclause cl);

    /**
     * @brief Find the clause cl in the watch list of lit and remove it.
     * @details Complexity: O(n), where n is the length of the watch list.
     */
    void stop_watch(Tlit lit, Tclause cl);

    /**
     * @brief Removes deleted and non-watched clauses from the watch lists. Also
     * removes clauses which are not watched by the literal of the list.
     */
    void repair_watch_lists();

    /**
     * @brief Returns true if the literal is currently in the propagation queue.
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
      NOTIFY_OBSERVER(_observer,
                      new napsat::gui::unassignment(literal(var, v.state)));
      v.state = VAR_UNDEF;
      v.reason = CLAUSE_UNDEF;
      v.level = LEVEL_UNDEF;
      v.waiting = false;
      if (v.missed_lower_implication != CLAUSE_UNDEF) {
        NOTIFY_OBSERVER(_observer,
                        new napsat::gui::remove_lower_implication(var));
        v.missed_lower_implication = CLAUSE_UNDEF;
      }
      if (!_variable_heap.contains(var))
        _variable_heap.insert(var, v.activity);
    }

    /**
     * @brief If var is an unknown variable, reallocate the data structures to
     * take into account the new variable.
     * @param var variable to allocate.
     * @note Every variable with an index lower than var will be allocated.
     */
    inline void var_allocate(Tvar var)
    {
      for (Tvar i = _vars.size(); i <= var; i++) {
        _variable_heap.insert(i, 0.0);
        NOTIFY_OBSERVER(_observer, new napsat::gui::new_variable(i));
      }
      if (var >= _vars.size() - 1) {
        _vars.resize(var + 1);
        _watch_lists.resize(2 * var + 2);
        _binary_clauses.resize(2 * var + 2);
        // reallocate the literal buffer to make sure it is big enough
        Tlit* new_literal_buffer = new Tlit[_vars.size()];
        std::memcpy(new_literal_buffer, _literal_buffer,
                    _next_literal_index * sizeof(Tlit));
        delete[] _literal_buffer;
        _literal_buffer = new_literal_buffer;
      }
    }

    /**
     * @brief Returns a literal utility metric to choose the literals to watch.
     * @param lit literal to evaluate.
     * @return utility metric of the literal.
     * @details The utility of a falsified literal is its decision level. The
     * utility of an undefined literal is higher than the utility of a falsified
     * literal. The utility of a satisfied literal is higher than the utility of
     * an undefined literal and decreases with the decision level.
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
     * DECIDE [literal]       (to decide a literal, if literal is not provided,
     *                         the solver decides)
     * HINT <literal>         (to hint a literal)
     * LEARN [literal]+       (to learn a clause from the given literals)
     * DELETE_CLAUSE <clause> (to delete a clause)
     * HELP                   (to print the list of commands)
     * QUIT                   (to quit the solver)
     * @param command command to parse.
     * @return true if the command was parsed successfully, false otherwise.
     */
    bool parse_command(std::string command);

    /*************************************************************************/
    /*                          Internal functions                           */
    /*************************************************************************/
    /**
     * @brief Add one literal to the propagation queue.
     * @pre The literal ℓ must be unassigned.
     *     ℓ ∉ π
     * @pre The first literal of the clause is ℓ
     *    C ≠ ■ ⇒ C[0] = ℓ
     * @pre The second literal of the clause is at the highest level in C
     *    C ≠ ■ ⇒ δ(C[1]) = δ(C \ {ℓ})
     * @pre The reason C must be a propagating clause or CLAUSE_UNDEF.
     *    C = ■ ∨ [ℓ ∈ C ∧ C \ {ℓ}, π ⊧ ⊥]
     * @post The literal ℓ is added to the propagation queue.
     *    ℓ ∈ ω ∧ ℓ ∈ π
     *    C = ■ ⇔ ℓ ∈ πᵈ
     * @post The reason of the literal ℓ is set to C.
     *    ρ(ℓ) = C
     * @post The level of the literal ℓ is set to the highest level of the
     * reason clause if the reason is not CLAUSE_UNDEF.
     *    C ≠ ■ ⇔ δ(ℓ) = max(δ(C) \ {ℓ})
     *    C = ■ ⇔ δ(ℓ) = |πᵈ| + 1
     */
    void imply_literal(Tlit lit, Tclause reason);

    /**
     * @brief Attempts to reimply a literal to a lower level. If the current
     * level of the literal, or the level of the lazy reimplication is lower
     * than the level of the reason, nothing is done. Otherwise, the lazy
     * reimplication is set to the reason.
     * @pre The literal ℓ must be satisfied.
     *    ℓ ∈ π
     * @pre The first literal of the clause is ℓ
     *    C[0] = ℓ
     * @pre The second literal of the clause is at the highest level in C
     *    |C| > 1 ⇒ δ(C[1]) = δ(C \ {ℓ})
     * @pre The reason C must be a propagating clause.
     *    C ≠ ■ ∧ C \ {ℓ}, π ⊧ ⊥
     * @pre The level of the literal or the lazy reimplication is lower than
     * the level of the reason.
     *   δ(ℓ) ≤ δ(C \ {ℓ}) ∨ δ(λ(ℓ) \ {ℓ}) ≤ δ(C \ {ℓ})
     */
    void reimply_literal(Tlit lit, Tclause reason);

    /**
     * @brief Searches for a replacement literal for the second watched literal
     * of a clause.
     * @details Provided a clause C = {c₂, c₁, ...} and a partial assignment
     * π = τ ⋅ ω search_replacement(C) returns a literal r ∈ C \ {c₂} such that
     * it either is a good replacement with
     *   ¬r ∈ (τ ⋅ ¬c₁) ⇒ c₂ ∈ π ∧ δ(c₂) ≤ δ(r)
     * or C \ {c₂} is conflicting with π and r is the highest literal in
     * C \ {c₂}
     *   C \ {c₂}, π ⊧ ⊥ ∧ δ(r) = δ(C \ {c₂})
     * @pre ¬c₁ ∈ ω
    */
    Tlit* search_replacement(Tlit* lits, unsigned size);

    /**
     * @brief Propagate the literal lit on the binary clauses.
     * @pre The literal ℓ being propagated is in the propagation queue
     *     ℓ ∈ ω ∧ ℓ ∈ π
     * @post After the execution, if no clause is returned, the following
     * property hold:
     * For each binary clause C = {c₁, c₂} ∈ F:
     * NCB: ¬c₁ ∈ (τ ⋅ ℓ) ⇒ c₂ ∈ π
     * WCB: ¬c₁ ∈ (τ ⋅ ℓ) ⇒ c₂ ∉ (τ ⋅ ℓ)
     * SCB: ¬c₁ ∈ (τ ⋅ ℓ) ⇒ [c₂ ∈ π
     *                    ∧ [δ(c₂) ≤ δ(c₁) ∨ δ(λ(c₂) \ {c₂}) ≤ δ(c₁)]
     */
    Tclause propagate_binary_clauses(Tlit lit);

    /**
     * @brief Propagate one literal.
     * @param lit literal to propagate.
     * @pre We assume that the following invariants hold for the different
     * backtracking strategies:
     *    ∀C ∈ F watched by c₁ and c₂ and with a blocker b:
     *    - NCB: ¬c₁ ∈ τ ⇒ c₂ ∈ π ∨ b ∈ π
     *    - WCB: ¬c₁ ∈ τ ⇒ ¬c₂ ∉ τ ∨ [b ∈ π ∧ δ(b) ≤ δ(c₁)]
     *    - SCB: ¬c₁ ∈ τ ⇒ [c₂ ∈ π
     *                      ∧ [δ(c₂) ≤ δ(c₁) ∨ δ(λ(c₂) \ {c₂}) ≤ δ(c₁)]
     *                    ∨ [b ∈ π ∧ δ(b) ≤ δ(c₁)]
     * @post After the execution, the following properties hold:
     *    ∀C ∈ F : |C| > 2 watched by c₁ and c₂ and with a blocker b:
     *    - NCB: ¬c₁ ∈ (τ ⋅ ℓ) ⇒ c₂ ∈ π ∨ b ∈ π
     *    - WCB: ¬c₁ ∈ (τ ⋅ ℓ) ⇒ ¬c₂ ∉ τ ∨ [b ∈ π ∧ δ(b) ≤ δ(c₁)]
     *    - SCB: ¬c₁ ∈ (τ ⋅ ℓ) ⇒ [c₂ ∈ π
     *                           ∧ [δ(c₂) ≤ δ(c₁) ∨ δ(λ(c₂) \ {c₂}) ≤ δ(c₁)]
     *                         ∨ [b ∈ π ∧ δ(b) ≤ δ(c₁)]
     */
    Tclause propagate_lit(Tlit lit);

    /**
     * @brief Backtrack literals in the chronological setting.
     * @param level level to backtrack to.
     * @pre The solver runs in chronological backtracking mode. Let π be the
     * state of the trail before backtracking
     * @post Let π' be the state of the trail after backtracking at level d,
     *           λ' be the state of the lazy reimplication buffer after
     *           backtracking at level d,
     *  the following properties must hold:
     * WCB: ∀ℓ ∈ π. [ℓ ∈ π ∧ δ(ℓ) ≤ d] ⇔ ℓ ∈ π'
     * WCB: ∀ℓ ∈ π. [[ℓ ∈ π ∧ δ(ℓ) ≤ d] ∨ δ(λ(ℓ) \ {ℓ}) ≤ d] ⇔ ℓ ∈ π'
     *      ∀ℓ. λ'(ℓ) ≠ ■ ⇒ ℓ ∈ π ∧ ℓ ∈ λ'(ℓ)
     *                    ∧ λ'(ℓ) \ {ℓ}, π' ⊧ ⊥
     *                    ∧ δ(λ'(ℓ) \ {ℓ}) < δ(ℓ)
     *      ∃C ∈ F ∃ℓ ∈ C. [¬c₁ ∈ τ ∧ C \ {ℓ}, π ⊧ ⊥ ∧ δ(C \ {ℓ})] < δ(ℓ)
     *                   ⇒ δ(λ(ℓ) \ {ℓ}) ≤ δ(C \ {ℓ})
     */
    void backtrack(Tlevel level);

    /**
     * @brief Repairs the conflict by analyzing it if needed and backtracking
     * to the appropriate level.
     * @param conflict clause that caused the conflict.
     * @pre The conflict clause C is conflicting with the current partial
     * assignment
     *    C, π ⊧ ⊥
     * @pre The level of the first literal of the conflict clause is at the
     * highest decision level in C
     *    δ(C[0]) = δ(C)
     */
    void repair_conflict(Tclause conflict);

    /**
     * @brief Returns true if the literal is required in the learned clause.
     * Returns false if the literal is already implied by the other literals of
     * the clause.
     *
     * @param lit literal to evaluate.
     * @return false if the literal is redundant with the current learned clause
     */
    bool lit_is_required_in_learned_clause(Tlit lit);

    /**
     * Analyze a conflict and learn a new clause.
     * @param conflict clause that caused the conflict.
     * @pre The solver is not in Strong Chronological Backtracking mode
     * @pre The conflict clause C is conflicting with the current partial assig-
     * nment
     *    C, π ⊧ ⊥
     * @pre The first literal of the conflicting clause C should be at the
     * highest decision level in the clause.
     *    δ(C[0]) = δ(C)
     * @pre The conflicting clause should have more than one literal at the
     * highest decision level or have the highest literal be a missed lower
     * implication
     *    |{ℓ ∈ C : δ(ℓ) = δ(C)}| > 1 ∨ λ(C[0]) ≠ ■
     * @post A new clause C' is added to the clause set such that
     * The clause C' is distinct from the conflict clause C
     *    C' ≠ C
     * The clause C' is implied by the formula
     *    F ⊧ C'
     * The clause C' is conflicting with the current partial assignment
     *    C', π ⊧ ⊥
     * The clause has one unique literal at the highest decision level
     *    |{ℓ ∈ C' : δ(ℓ) = δ(C')}| = 1
     */
    void analyze_conflict(Tclause conflict);

    /**
     * @brief Link resolutions in the proof system to get rid of the literals at
     * level 0 in the clause.
     * @param lits a list of literals whose level 0 literals should be removed
     * @param size size of the list
     * @pre the proof system is enabled
     * @pre all the literals in the trail must be unmarked
     * @pre a resolution chain is already started
     * @post the proof system has linked resolutions to remove the literal from
     * the clause
     * @post all the literals in the trail are unmarked
     */
    void prove_root_literal_removal(Tlit* lits, unsigned size);

    /**
     * @brief Restarts the solver by resetting the trail
     * @post The trail is empty
     *    π = ∅
     */
    void restart();

    void purge_root_watch_lists();

    /**
     * @brief Remove clauses satisfied at level 0 and remove literals falsified
     * at level 0 from the clauses.
     * @pre Before the purge, the formula is called F
     * @post After the purge, the formula is called F'
     * @post For each clause C in F:
     * If the clause is satisfied at level 0, it is removed from the clause set
     *    [∃ℓ ∈ C [ℓ ∈ π ∧ δ(ℓ) = 0]] ⇒ C ∉ F'
     * Falsified literals at level 0 are removed from the clause
     */
    void purge_clauses();

    /**
     * @brief Brings forward the two most relevant literals of the clause. A
     * literal is more relevant if it has a higher utility (see utility
     * _heuristic). The literals are moved to the first two positions of the
     * clause.
     * @param lits array of literals to reorder.
     * @param size size of the clause.
     */
    void select_watched_literals(Tlit* lits, unsigned size);

    /**
     * @brief allocates a new chunk of memory for a clause, and adds to the
     * clause set. The clause is added to the watch lists if needed (size >= 2).
     * If the clause is satisfied at level 0, the clause is not added.
     * @param lits array of literals to add to the clause set.
     * @param size size of the clause.
     * @param learned true if the clause is a learned clause, false otherwise.
     * @param external true if the clause is an external clause, false
     * otherwise.
     * @details This function does not alter the memory space of lits. Either
     * new memory is allocated for the clause, or the clause is added in place
     * of a deleted clause.
     * @details This function removes literals falsified at level 0 from the
     * clause.
     * @return a handle to the added clause. If the clause is not added, returns
     * CLAUSE_UNDEF.
     */
    Tclause internal_add_clause(const Tlit* lits, unsigned size,
                                bool learned, bool external);

    /*************************************************************************/
    /*                          Public interface                             */
    /*************************************************************************/
  public:
    /**
     * @brief Construct a new NapSAT::NapSAT object
     * @param n_var initial number of variables. Can be increased later.
     * @param n_clauses initial number of clauses. Can be increased later by
     * adding clauses.
     */
    NapSAT(unsigned n_var, unsigned n_clauses, napsat::options& options);

    /**
     * @brief Parse a DIMACS file and add the clauses to the clause set.
     * @param filename name of the DIMACS file.
     * @return true if the file was parsed successfully, false otherwise.
     */
    bool parse_dimacs(const char* filename);

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
     * @note It is the responsibility of the user querying the observer to not
     * compromise the integrity of the observer. That is, the user must not
     * delete the observer or modify its state.
     * @warning This method is just a convenience for the main. It is not meant
     * to be used in a library.
     */
    napsat::gui::observer* get_observer() const;

    /**
     * @brief Propagate literals in the queue and resolve conflicts if needed.
     * The procedure stops when all variables are assigned, or a decision is
     * needed. If the solver is in input mode, it will switch to propagation
     * mode.
     * @pre The solver status must be undef
     * @return true if the solver may make a decision, false if all variables
     * are assigned or the clause set is unsatisfiable.
     */
    bool propagate();

    /**
     * @brief Solves the clause set. The procedure stops when all variables are
     * assigned, of the solver concludes that the clause set is unsatisfiable.
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
     * @return true if the solver is still undef after the decision, false if
     * all variables are assigned.
     */
    bool decide();

    /**
     * @brief Forces the solver to decide the given literal. The literal must be
     * unassigned.
     * @param lit literal to decide.
     */
    bool decide(Tlit lit);

    /**
     * @brief Set up the solver to lits a new clause. Sets the solver in
     * clause_input mode.
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
     * @brief Finalize the current clause and add it to the clause set. If the
     * solver is in propagation mode, it will propagate literals if needed.
     * @pre The solver must be in clause_input mode.
     * @return A handle to the added clause.
     */
    Tclause finalize_clause();

    /**
     * @brief Add a complete clause to the clause set.
     * @param lits array of literals to add to the clause set.
     * @param size size of the clause.
     * @return A handle to the added clause.
     * @note The memory of the clause is allocated by the solver. Therefore, the
     * pointer lits is managed by the user and is not freed by the solver.
     */
    Tclause add_clause(const Tlit* lits, unsigned size);

    /**
     * @brief Returns the literals of a clause.
     * @param clause clause id of the clause.
     * @return pointer to the first literal of the clause.
     * @note should be called in conjunction with get_clause_size() to get the
     * size of the clause.
     */
    const Tlit* get_clause(Tclause cl) const;

    /**
     * @brief Returns the size of a clause.
     * @param clause clause id of the clause.
     * @return size of the clause.
     */
    unsigned get_clause_size(Tclause cl) const;

    /**
     * @brief Provide a hint to the SAT solver. The hint will be considered as a
     * decision.
     * @param lit literal to assign
     * @todo Not supported yet.
     */
    void hint(Tlit lit);

    /**
     * @brief Provide a hint to the SAT solver. The hint will be assigned at the
     * given decision level.
     * @param lit literal to assign
     * @param level decision level of the assignment
     * @pre The level must be lower than or equal to the current decision level.
     * @todo Not supported yet.
     */
    void hint(Tlit lit, unsigned int level);

    /**
     * @brief Notify the solver that the trail was synchronized by the user.
     * This function will reset the colors of the variables.
     * @todo Not supported yet.
     */
    void synchronize();

    /**
     * @brief Returns the index of the last literal that remains valid since the
     * last synchronization. That is, every literal with an index lower than the
     * returned value has been unchanged since the last synchronization.
     * @todo Not supported yet.
     */
    unsigned sync_validity_limit();

    /**
     * @brief Returns the modification of state of a variable since the last
     * synchronization.
     * @return 0 if no change, 1 if the variable was assigned, 2 if the variable
     * was unassigned, 3 if the variable changed polarity.
     * @todo Not supported yet.
     */
    unsigned sync_color(Tvar var);

    /**
     * @brief Set a markup to the last literal on the trail. The markup function
     * will be called when the literal is backtracked.
     * @todo Not supported yet.
     */
    void set_markup(void (*markup_function)(void));

    /**
     * @brief Returns a reference to the trail. The trail should not be modified
     * by the user.
     */
    const std::vector<Tlit>& trail() const;

    /**
     * @brief Returns true if the literal in the trail is a decision.
     * @pre The literal must be assigned.
    */
    inline bool is_decided(Tlit lit) const
    {
      ASSERT(!lit_undef(lit));
      return _vars[lit_to_var(lit)].reason == CLAUSE_UNDEF;
    }

    /**
     * @brief Returns the current decision level.
     */
    Tlevel decision_level() const;

    /**
     * @brief Prints a proof of unsatisfiability on the standard output.
     * @pre The clause set must be unsatisfiable.
     * @pre The proof must be enabled.
    */
    void print_proof();

    /**
     * @brief Checks the proof of unsatisfiability.
     * @return true if the proof is correct, false otherwise.
     * @pre The clause set must be unsatisfiable.
     * @pre The proof must be enabled.
    */
    bool check_proof();

    /*************************************************************************/
    /*                        Printing the state                             */
    /*************************************************************************/
    /**
     * @brief Returns a colored string of a literal. The literal is printed
     * green if satisfied, red if falsified, and yellow if unassigned
     * @param lit literal to print.
     * @return colored string of the literal.
     */
    std::string lit_to_string(Tlit lit);

    /**
     * @brief Returns a string of a clause. The clause is printed in the form
     * "cl : lit1 lit2 ... litm | litm+1 ... litn" where lit1, ..., litn are the
     * literals of the clause, litm+1, ..., litn are disabled literals (false at
     * level 0) and cl is the clause id.
     * @param clause clause to print.
     * @return string of the clause.
     */
    std::string clause_to_string(Tclause cl);

    /**
     * @brief Prints the current assignment of the solver on the standard
     * output in a human-readable format.
     */
    void print_trail();

    /**
     * @brief Prints the current assignment of the solver on the standard
     * output in a human-readable format.
     */
    void print_trail_simple();

    /**
     * @brief Prints a clause on the standard output in a human-readable format.
     * @param clause clause to print.
     */
    void print_clause(Tclause cl);

    /**
     * @brief Prints the clause set on the standard output in a human-readable
     * format.
     */
    void print_clause_set();

    /**
     * @brief Prints the watch lists on the standard output in a human-readable
     * format.
     */
    void print_watch_lists(Tlit lit = LIT_UNDEF);

    /**
     * @brief Destroy the NapSAT::NapSAT object
     */
    ~NapSAT();

    /*************************************************************************/
    /*                     Functions for the checker                         */
    /*************************************************************************/
    // Note that some invariants are checked by the observer. Therefore, not
    // all relevant invariants are checked here.
  private:
    /**
     * @brief Returns trues if every variable in the trail is assigned and that
     * every assigned variable is in the trail.
     */
    bool trail_variable_consistency();

    /**
     * @brief returns true if the clause cl is in the watch list of the literal
     * lit.
    */
    bool is_watched(Tlit lit, Tclause cl);

    /**
     * @brief returns true if the watch lists of watched literals of a clause
     * contain the clause.
     */
    bool watch_lists_complete();

    /**
     * @brief returns true if each clause in a watch list is watched by the
     * corresponding literal.
     */
    bool watch_lists_minimal();
  };
}
