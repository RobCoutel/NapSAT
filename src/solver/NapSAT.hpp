/*
 * This file is part of the source code of the software program
 * NapSAT. It is protected by applicable copyright laws.
 *
 * This source code is protected by the terms of the MIT License.
 */
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
 *          α the set of assumptions
 *          δ(ℓ) the decision level of ℓ
 *          ρ(ℓ) the reason of ℓ
 *          λ(ℓ) the lazy reason of ℓ. That is, a missed lower implication of ℓ.
 *          γ(ℓ) is the set of chunks of ℓ
 *          η(ℓ) is the set of cross chunks of ℓ
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
#include "../proof/proof.hpp"
#include "../proof/dependency_tracker.hpp"
#include "../utils/printer.hpp"
#include "../utils/heap.hpp"
#include "../utils/bitset.hpp"
#include "../utils/luby-counter.hpp"
#include "../observer/SAT-notification.hpp"
#include "../observer/SAT-observer.hpp"

#include <vector>
#include <set>
#include <iostream>
#include <cstring>
#include <cassert>
#include <algorithm>

#include "../observer/SAT-stat.hpp"

#if USE_STATISTICS
#define NOTIFY_STAT(type)  NOTIFY_STAT_N(type, 1)
#define NOTIFY_STAT_N(type, n)                \
  do {                                        \
    if (_statistics) {                        \
      stat.type->inc(n);                      \
    }                                         \
  } while(0)
#else
#define NOTIFY_STAT(type)       ((void)0)
#define NOTIFY_STAT_N(type, n)  ((void)0)
#endif

#if USE_OBSERVER
#define NOTIFY_OBSERVER(NAME,...)             \
  do {                                        \
    if (_observer) {                          \
      if(!_observer->notify(new napsat::gui::NAME(__VA_ARGS__))) { \
        LOG_ERROR("The notification returned an error when executed by the observer"); \
        if(_observer)                         \
          _observer->notify(new napsat::gui::marker("Notification failed")); \
        assert(false);                        \
      }                                       \
    }                                         \
    NOTIFY_STAT(NAME);                        \
  } while(0)
#else
#define NOTIFY_OBSERVER(NAME,...)  NOTIFY_STAT(NAME)
#endif

namespace napsat
{
  class NapSAT
  {
  public:
    /*************************************************************************/
    /*                          Public interface                             */
    /*************************************************************************/
    /**
     * @brief Construct a new napsat::NapSAT object
     * @param n_var initial number of variables. Can be increased later.
     * @param n_clauses initial number of clauses. Can be increased later by
     * adding clauses.
     */
    NapSAT(unsigned n_var, unsigned n_clauses, options& options);

    /**
     * @brief Create a new variable in the solver.
     * @return The variable created.
     */
    Tvar new_variable();

    /**
     * @brief Returns the number of variables in the solver.
     * @return the number of variables in the solver.
     */
    inline unsigned var_count() const noexcept { return _vars.size(); }

    /**
     * @brief Parse a DIMACS file and add the clauses to the clause set.
     * @details supported formats  .cnf / .cnf.bz2 / .cnf.xz
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
     * @brief Returns true if the solver has statistics.
     */
    bool has_statistics() const;

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
    gui::observer* get_observer() const;

    /**
     * @brief Returns a pointer to the statistics of the solver;
     * @return pointer to the statistics of the solver.
     * @note The pointer is nullptr if the solver does not do statistics.
     */
    statistics* get_statistics() const;

    /**
     * @brief Adds an assumption to the solver. Assumptions cannot be removed
     * until the user calls forget_assumption().
     * @param assumption a literal to assume.
     * @return true if the assumption was added successfully, false otherwise.
     * @details The solver will return false if the assumption contradicts the current set of assumptions
     */
    bool assume(Tlit assumption);

    /**
     * @brief Adds assumptions to the solver. Assumptions cannot be removed until
     * the user calls forget_assumption().
     * @param assumptions vector of literals to assume.
     * @return true if all assumptions were added successfully, false otherwise.
     */
    bool add_assumption(const std::vector<Tlit>& assumptions);

    /**
     * @brief Removes an assumption from the solver.
     * @param assumption literal to remove from the assumptions.
     * @return true if the assumption was removed successfully, false otherwise.
     * @details In CB and NCB, removing an assumption will backtrack the solver to
     * the level before the assumption was added. In GB, only the assumption and
     * dependant literals are removed.
     */
    bool forget_assumption(Tlit assumption);

    /**
     * @brief Removes all assumptions from the solver.
     */
    void forget_assumption();

    /**
     * @brief Returns a list of assumptions that caused the last UNSAT result.
     * @pre the last call to solve returned UNSAT
     * @return vector of literals representing the failed assumptions.
     */
    std::vector<Tlit> unsat_core() const;

    /**
     * @brief Returns the list of clauses in the unsat core after an UNSAT solve
     * modulo assumptions.
     * @pre the last call to solve returned UNSAT
     * @return vector of clause identifiers representing the clauses in the unsat core modulo assumptions.
     */
    std::vector<Tclause> clause_unsat_core();

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
     * @brief Solves the clause set with a limit on the number of conflicts.
     * The procedure stops when all variables are assigned, or the solver
     * concludes that the clause set is unsatisfiable, or the number of conflicts
     * reaches the given limit.
     */
    status solve(unsigned conflict_limit);

    /**
     * @brief Returns the status of the solver.
     * @return status of the solver.
     */
    status get_status() const;

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
     * @param lit literal to check.
     * @return true if the literal is a decision, false otherwise.
    */
    inline bool is_decided(Tlit lit) const {
      ASSERT(!lit_undef(lit));
      return lit_decision(lit);
    }

    /**
     * @brief Returns true if the given literal is assigned at root level.
     * @pre The literal must be assigned.
     * @param lit literal to check.
     * @return true if the literal is assigned at root level, false otherwise.
     */
    inline bool is_root_level(Tlit lit) const {
      ASSERT(!lit_undef(lit));
      return lit_level(lit) == LEVEL_ROOT;
    }

    /**
     * @brief Suggest a polarity for a variable.
     * @details When the variable is decided, it will be assigned the suggested
     * polarity, unless it has already been assigned, in which case the phase_cache
     * will be used.
     * @param lit literal whose variable will have a suggested polarity.
     * @param polarity suggested polarity. true for positive, false for negative.
     */
    inline void suggest_polarity(Tlit lit, bool polarity)
    {
      unsigned var = lit_to_var(lit);
      ASSERT(var < _vars.size());
      _vars[var].phase_cache = polarity ? 1 : 0;
    }

    void set_weight_function(std::function<double(Tlit)> func) {
      _backtrack_cost_estimator = func;
    }

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

    /**
     * @brief Prints the current assignment of the solver on the standard
     * output in a human-readable format.
     */
    void print_trail() const;

    /**
     * @brief Destroy the napsat::NapSAT object
     */
    ~NapSAT();


private:
#ifdef TEST
public:
#endif

    typedef unsigned Tchunk;
    static const Tchunk CHUNK_UNDEF = 0xFFFFFFFF;

    size_t _current_order = 0;
    static const size_t ORDER_RESET = 0xFFFFFFFFFFFFFFFF;

    /*************************************************************************/
    /*                            Data structures                            */
    /*************************************************************************/
    /**
     * @brief Structure to store the state an metadata of a propositional variable.
     */
    typedef struct TSvar
    {
      TSvar() :
        marked(false),
        propagated(false),
        state(VAR_UNDEF),
        phase_cache(0),
        synced(0),
        constrained(0),
        locked(0)
      {}
      /**
       * @brief Decision level at which the variable was assigned.
       * @details If the variable is unassigned, the level is LEVEL_UNDEF.
       */
      Tlevel level = LEVEL_UNDEF;
      /**
       * @brief Clause that propagated the variable.
       * @details If the variable is assigned by a decision, the reason is
       * CLAUSE_UNDEF.
       * @note In mathematical symbols, we write ρ(ℓ) as the reason of the
       * literal ℓ or ¬ℓ.
       */
      Tclause reason = CLAUSE_UNDEF;
      /**
       * @brief Order in which the variable as been last assigned.
       * @details Used to find the position of a literal in the trail with binary
       * search.
       * @details This number is only increased when the variable is assigned.
       * Backtracking does not change the order of variables.
       * @details The order is defragmented when the solver backtracks to level 0
       * or reaches arithmetic overflow.
       */
      size_t order = ORDER_RESET;

      /**
       * @brief Activity of the variable. Used in decision heuristics.
       */
      double activity = 0.0;
      /**
       * @brief Boolean indicating if the variable was already marked. It is used
       * in conflict analysis.
       * @details Variables must remain marked locally. That is, upon exiting
       * the method, all variables must be unmarked.
       */
      unsigned marked : 1;
      /**
       * @brief Boolean indicating whether the variable is in the propagation
       * queue.
       */
      unsigned propagated : 1;
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
       * @brief Boolean indicating the polarity of the last synchronization.
       */
      unsigned synced : 1;

      /**
       * @brief True if at least one clause constraints this variable.
       */
      unsigned constrained : 1;

      /**
       * @brief Boolean indicating whether the variable is locked as an
       * assumption.
       */
      unsigned locked : 1;

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

      /** GRAPH BACKTRACKING **/
      /**
       * @brief Contains the set of of chunk in which this variable is
       * @details The chunk set, denoted by γ(ℓ) is either
       * - the union of the chunks of the variables in the reason
       *   clause of the variable, or
       *   ρ(ℓ) ≠ ■ ⇒ γ(ℓ) = U {γ(ℓ') : ℓ' ∈ ρ(ℓ) \ {ℓ}}
       * - a new chunk if the variable is a decision
       *   ρ(ℓ) = ■ ⇒ γ(ℓ) = { new_chunk(ℓ) }
       */
      bitset chunks;

      /**
       * @brief Contains the set of variables on which this variable depends
       * in cross-chunk implications
       * @details The dependencies, denoted by η(ℓ) is the set of decisions that
       * make this variable true in cross-chunk implications.
       * A cross-chunk implication is a clause C that implies a ℓ such that there
       * is no literal ℓ' in C \ {ℓ} such that γ(ℓ) = γ(ℓ').
       * @invariant For each clause C in F watched by c₁ and c₂:
       *   ¬c₁ ∈ τ ⇒ c₂ ∈ π ∧ [γ(c₁) ⊆ γ(c₂) ∪ η(c₂)]
       */
      bitset cross_chunks;
    } TSvar;

    /**
     * @brief Structure to store a clause and its metadata.
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
        deleted(true),
        learned(learned),
        watched(false),
        external(external),
        size(size)
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
       * @brief Last literal checked during replacement search
       * @details Used to cycle through the literals not starting from the start every time.
       */
      unsigned last_looked = 1;
    } TSclause;

    typedef struct TSwatch {
      /**
       * @brief Number of the clause watched
       */
      Tclause cl = CLAUSE_UNDEF;
      /**
       * @brief Blocking literal. If the clause is satisfied by the blocking
       * literal, the watched literals are allowed to be falsified.
       * @details In chronological backtracking, the blocking literal must be
       * at a lower level than the watched literals.
       */
      Tlit block = LIT_UNDEF;

      // constructor
      TSwatch(Tclause cl, Tlit block) :
        cl(cl),
        block(block) { /* do nothing */}

      TSwatch() :
        cl(CLAUSE_UNDEF),
        block(LIT_UNDEF) { /* do nothing */ }
    } TSwatch;

    /*************************************************************************/
    /*                      General fields definitions                       */
    /*************************************************************************/
    /**
     * @brief Options of the solver.
    */
    options _options;
    /**
     * @brief Status of the solver.
     */
    status _status = UNKNOWN;
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
    unsigned _n_propagated_lits = 0;

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
     * @brief _watches[i] is the first clause of the watch list of the
     * literal i.
     */
    std::vector<std::vector<TSwatch>> _watches;
    /**
     * @brief _binary_watches[l] is the contains the pairs <lit, cl> where lit
     * is a literal to be propagated if l is falsified, and <cl> is the clause
     * that propagates lit.
    */
    std::vector<std::vector<TSwatch>> _binary_watches;
    /**
     * @brief _decision_index[i] is the index of the decision made after level i.
     * @remark _decision_index[0] is the index of the first decision.
     */
    std::vector<unsigned> _decision_index;

    /**
     * @brief List of conflicts detected during propagation
     */
    std::vector<Tclause> _conflicts;

    /**  ADDING CLAUSES  **/
    /**
     * @brief True if the solver is in clause input mode.
     */
    bool _writing_clause = false;
    /**
     * @brief When in clause input mode, contains a pointer to the first
     * literal of the clause being written.
     */
    Tlit* _lit_buffer = nullptr;
    /**
     * @brief When in clause input mode, contains the index of the next literal
     * to write.
     */
    unsigned _lit_buffer_size = 0;

    /**
     * @brief Flag recording whether the most recent conflict is due to the user
     * @details In that case, the termination of CDCL-GB is not in danger and we
     * can backtrack any chunk we want.
     */
    bool _just_learned_from_user = false;

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
    utils::heap _variable_heap;

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

    /**  RESTART  **/
    /**
     * @brief Counter to determine when is the next restart
     * @details The counter is incremented at each conflict. When the counter
     * reaches the next element in the luby sequence, the solver restarts.
     */
    luby_counter _luby_counter;

    /**  PURGE  **/
    /**
     * @brief Current progress before next purge.
     * @details Count the number of not yet purged level 0 literals on the
     * trail.
     */
    unsigned _n_root_lvl_lits = 0;
    /**
     * @brief Limit of the purge counter before the next purge.
     */
    unsigned _purge_threshold = 5;
    /**
     * @brief Increment of the purge threshold upon each purge.
     */
    unsigned _purge_inc = 2;

    /**
     * @brief Conflict counter to determine when to purge level 0 literals
     * @details The counter is incremented at each conflict. If the conflict_limit
     * option is set, the solver will stop upon reaching the conflict limit.
     */
    unsigned _conflict_count = 0;

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

    /**  MISSED LOWER IMPLICATIONS  **/
    /**
     * @brief Buffer used in strong chronological backtracking to store
     * literals that were removed from the trail and should be reimplied after
     * backtracking.
     */
    std::vector<Tclause> _reimplication_backtrack_buffer;

    /**  GRAPH BACKTRACKING  **/

    struct TSchunk
    {
      Tvar decision;
      bitset missed_implication;
    };

    /**
     * @brief Contains the set of chunks in which the variables are stored.
     * @details A chunk is a set of variables reachable from a decision in the
     * implication graph.
     */
    std::vector<TSchunk> _chunks;

    /**
     * @brief Contains the free chunks that can be reused.
     * @details Free chunks happen after backtracking, when a chunk is removed.
     */
    std::vector<Tchunk> _free_chunks;

    /**
     * @brief Callback function called upon conflict analysis to heuristically determine
     * the cost of backtracking a set of literals.
     * @param lits pointer to a list of literals
     * @param size number of backtracked literals
     * @returns the estimated cost of backtracking the given literals
     */
    std::function<double(Tlit)> _backtrack_cost_estimator;

    /**
     * @brief Default cost function
     */
    double literal_cost(Tlit lit);

    /**
     * @brief Number of allocated chunks.
     * @details This is the number of chunks that variables are able to use.
     * i.e., the size of the bitset of each variable.
     */
    unsigned _n_allocated_chunks = 0;

    /**  ASSUMPTIONS  **/
    /**
     * @brief Number of current assumptions.
     * @details In CB and NCB, the assumptions are added as decisions at the
     * bottom of the trail. Therefore, the number of assumption gives us the effective root level.
     */
    unsigned _n_assumptions = 0;

    /**
     * @brief Set of chunks that contain assumptions.
     * @details In graph backtracking, assumptions are also added as decisions.
     * However, they can be located at arbitrary levels and positions in the
     * trail. They are however locked and cannot be backtracked.
     */
    bitset _locked_chunks;

    /**  PROOFS  **/
    /**
     * @brief Proof builder of the solver. If _proof is not nullptr, the solver
     * builds a resolution proof for unsatisfiability.
    */
    proof::resolution_proof* _proof = nullptr;

    /**
     * @brief Dependency tracker of the solver. If _dependency_tracker is not
     * nullptr, the solver tracks dependencies of learned clauses.
     * @details The dependency tracker is used to compute the UNSAT cores
     */
    proof::dependency_tracker* _dependency_tracker = nullptr;

    /**  SMT SYNCHRONIZATION  **/
    /**
     * @brief Position of the last literal on the trail that was left unchanged
     * since the last synchronization.
     */
    size_t _sync_validity_index = 0;

    /**  INTERACTIVE SOLVER  **/
#if USE_OBSERVER
    /**
     * @brief Observer of the solver. If _observer is not nullptr, the solver
     * notifies the observer of its progress.
     */
    gui::observer* _observer = nullptr;
#endif
    /**
     * @brief True if the solver is interactive.
     * @details The solver is interactive if it stops between decisions to let
     * the user make a decision, hint or learn a clause.
     */
    bool _interactive = false;

#if USE_STATISTICS
    statistics* _statistics = nullptr;

    /** STATISTICS **/
    struct {
      statistics::stat *solve_time = nullptr; // "Runtime of the SAT solver"
      statistics::stat *repair_time = nullptr; // "Runtime of the SAT solver"
      statistics::stat *cost_estimation_time = nullptr; // "Chunk cost estimation time"
      statistics::stat *backtrack_possibilities_time = nullptr; // "Backtrack possibilities time"
      statistics::stat *conflict_analysis_time = nullptr; // "Conflict analysis time"
      statistics::stat *conflict_fixing_time = nullptr; // "Conflict fixing time"
      statistics::stat *backtrack_time = nullptr; // "Backtrack time"
      statistics::stat *reimply_time = nullptr; // "Reimplication time"
      statistics::stat *subsumption_time = nullptr; // "Subsumption time"

      // stats from observable events
      statistics::stat *decision = nullptr; // "Decisions"
      statistics::stat *conflict = nullptr; // "Conflicts"
      statistics::stat *propagation = nullptr; // "Propagation"
      statistics::stat *implication = nullptr; // "Implication"
      statistics::stat *unassignment = nullptr; // "Unassignment"
      statistics::stat *remove_lower_implication = nullptr; // "Remove lower implication"
      statistics::stat *remove_propagation = nullptr; // "Remove propagation"
      statistics::stat *remove_literal = nullptr; // "Remove literal"
      statistics::stat *block = nullptr; // "Block"
      statistics::stat *check_invariants = nullptr; // "Check invariants"
      statistics::stat *missed_lower_implication = nullptr; // "Missed lower implication"
      statistics::stat *backtracking_started = nullptr; // "Backtracking started"
      statistics::stat *update_level = nullptr; // "Update level"
      statistics::stat *update_reason = nullptr; // "update reason"
      statistics::stat *new_clause = nullptr; // "Add clause"
      statistics::stat *new_variable = nullptr; // "Add variable"
      statistics::stat *delete_clause = nullptr; // "Delete clause"
      statistics::stat *marker = nullptr; // "Marker"
      statistics::stat *watch = nullptr; // "Unwatch"
      statistics::stat *unwatch = nullptr; // "Unwatch"
      statistics::stat *done = nullptr; // "Done"
      statistics::stat *lock_assumption = nullptr; // "Sync"
      statistics::stat *unlock_assumption = nullptr; // "Sync"

      // auxilary stats
      statistics::stat *_n_purged_clauses = nullptr; // "Purging clauses"
      statistics::stat *_n_binary_clause_simplified = nullptr; // "Binary clause simplified"
      statistics::stat *_n_binary_clause_added = nullptr; // "Binary clause added"
      statistics::stat *_n_clause_learned = nullptr; // "Learned clause"
      statistics::stat *_n_unit_clause_simplified = nullptr; // "Unit clause simplified"
      statistics::stat *_n_redundant_clause = nullptr; // "Clause deleted"
      statistics::stat *_n_clause_set_simplified = nullptr; // "Clause set simplified"
      statistics::stat *_n_allocated_chunks = nullptr; // "Allocated Chunk"
      statistics::stat *_n_cross_implication_decisions = nullptr; // "Cross implication for decision"
      statistics::stat *_n_lazy_reimplication_used = nullptr; // "Lazy reimplication used"
      statistics::stat *_n_propagation_replayed = nullptr; // "Replayed Propagation"
      statistics::stat *_n_skipped_propagation = nullptr; // "Skipped Propagation"
      statistics::stat *_n_sync = nullptr; // "Sync assign"
      statistics::stat *_n_restart = nullptr; // "Restart"
      statistics::stat *_n_fw_subsumption_in_set = nullptr; // "Forward subsumption in set"
      statistics::stat *_n_fw_subsumption = nullptr; // "Forward subsumption"
      statistics::stat *_n_bw_subsumption = nullptr; // "Backward subsumption"
      statistics::stat *_n_backtrack_limit_reached = nullptr; // "Backtrack limit reached"
      statistics::stat *_n_conflict_repair = nullptr; // "Conflict repair"
      statistics::stat *_n_failed_learning = nullptr; // "Failed learning"
      statistics::stat *_n_backtrack_forced_chunks = nullptr; // "Forced chunk backtrack"
      statistics::stat *_n_backtrack_better_chunks = nullptr; // "Cross implication back"
      statistics::stat *_a_learned_clause_size = nullptr; // "Avg learned clause size"
      statistics::stat *_a_bt_choices = nullptr; // "Avg backtrack size"
      statistics::stat *_a_prefix_size = nullptr; // "Avg learned clause size"
    } stat;
#endif
  public:
    /*************************************************************************/
    /*                       Quality of life functions                       */
    /*************************************************************************/
    /**
     * @brief Returns true if the variable is not assigned.
     */
    inline bool var_undef(Tvar var) const {
      ASSERT(var < _vars.size());
      return  _vars[var].state == VAR_UNDEF;
    }
    /**
     * @brief Returns true if the variable is assigned true.
     */
    inline bool var_true(Tvar var) const {
      ASSERT(var < _vars.size());
      return _vars[var].state == VAR_TRUE;
    }
    /**
     * @brief Returns true if the variable is assigned false.
     */
    inline bool var_false(Tvar var) const {
      ASSERT(var < _vars.size());
      return _vars[var].state == VAR_FALSE;
    }
    /**
     * @brief Returns the value of the given variable.
     * @details The value of a variable is either 0 (false), 1 (true), or 2 (undefined).
     * @param var variable to evaluate.
     * @return value of the variable.
     */
    inline unsigned var_value(Tvar var) const {
      ASSERT(var < _vars.size());
      return _vars[var].state;
    }

    /**
     * @brief Returns true if a literal is satisfied.
     * @param lit literal to evaluate.
     * @return true if the literal is satisfied, false otherwise.
    */
    inline bool lit_true(Tlit lit) const { return !(var_value(lit_to_var(lit)) ^ lit_pol(lit)); }

    /**
     * @brief Returns true if a literal is falsified.
     * @param lit literal to evaluate.
     * @return true if the literal is falsified, false otherwise.
     */
    inline bool lit_false(Tlit lit) const { return !(var_value(lit_to_var(lit)) ^ lit_pol(lit) ^ 1); }

    /**
     * @brief Returns true if the literal is undefined.
     * @param lit literal to evaluate.
     * @return true if the literal is undefined, false otherwise.
     */
    inline bool lit_undef(Tlit lit) const { return var_value(lit_to_var(lit)) >> 1; }

    /**
     * @brief Returns the level of the given variable.
     */
    inline Tlevel var_level(Tvar var) const {
      ASSERT(var < _vars.size());
      return _vars[var].level;
    }
    /**
     * @brief Returns the level of the given literal. If the literal is not
     * assigned, returns LEVEL_UNDEF.
     * @param lit literal to evaluate.
     * @return level of the literal.
     */
    inline Tlevel lit_level(Tlit lit) const { return var_level(lit_to_var(lit)); }

  private:
    /**
     * @brief Returns a reference to the level of the given variable.
     * @details Used to modify the level of a variable.
     * @return reference to the level of the variable.
     */
    inline Tlevel& var_level(Tvar var) {
      ASSERT(var < _vars.size());
      return _vars[var].level;
    }

    /**
     * @brief Returns a reference to the level of the given literal.
     * @details Used to modify the level of a literal.
     * @param lit literal to evaluate.
     * @return reference to the level of the literal.
     */
    inline Tlevel& lit_level(Tlit lit) { return var_level(lit_to_var(lit)); }

    /**
     * @brief Returns that clause that implied the variable if such a clause exists.
     * @param var variable to evaluate.
     * @return clause that implied the variable.
     */
    inline Tclause var_reason(Tvar var) const {
      ASSERT(var < _vars.size());
      return _vars[var].reason;
    }
    inline Tclause& var_reason(Tvar var) {
      ASSERT(var < _vars.size());
      return _vars[var].reason;
    }

    /**
     * @brief Returns the reason of the literal. If the literal is not
     * assigned, returns CLAUSE_UNDEF.
     * @param lit literal to evaluate.
     * @return reason of the literal.
     */
    inline Tclause lit_reason(Tlit lit) const { return var_reason(lit_to_var(lit)); }
    inline Tclause& lit_reason(Tlit lit) { return var_reason(lit_to_var(lit)); }

    inline double var_activity(Tvar var) const {
      ASSERT(var < _vars.size());
      return _vars[var].activity;
    }
    inline double& var_activity(Tvar var) {
      ASSERT(var < _vars.size());
      return _vars[var].activity;
    }

    /**
     * @brief Returns true if the variable is a decision variable.
     * @param var variable to evaluate.
     * @return true if the variable is a decision variable, false otherwise.
     */
    inline bool var_decision(Tvar var) const {
      ASSERT(var < _vars.size());
      return !var_undef(var) && var_reason(var) == CLAUSE_UNDEF;
    }

    /**
     * @brief Returns true if the literal is a decision literal.
     * @param lit literal to evaluate.
     * @return true if the literal is a decision literal, false otherwise.
     */
    inline bool lit_decision(Tlit lit) const { return var_decision(lit_to_var(lit)); }

    inline size_t var_order(Tvar var) const {
      ASSERT(var < _vars.size());
      return _vars[var].order;
    }
    inline size_t lit_order(Tlit lit) const { return var_order(lit_to_var(lit)); }
    inline size_t& var_order(Tvar var) {
      ASSERT(var < _vars.size());
      return _vars[var].order;
    }
    inline size_t& lit_order(Tlit lit) { return var_order(lit_to_var(lit)); }

    /**
     * @brief Returns the decision literal at the given level.
     * @param level decision level to evaluate.
     * @return decision literal at the given level.
     * @pre 1 <= level <= solver_level()
     */
    inline Tlit decision_lit(Tlevel level) const {
      ASSERT(level >= 1);
      ASSERT(level <= solver_level());
      ASSERT(lit_decision(_trail[_decision_index[level - 1]]));
      ASSERT(lit_level(_trail[_decision_index[level - 1]]) == level);
      return _trail[_decision_index[level - 1]];
    }
    inline const Tlit* decision_lit_ptr(Tlevel level) const {
      ASSERT(level >= 1);
      ASSERT(level <= solver_level());
      ASSERT(lit_decision(_trail[_decision_index[level - 1]]));
      ASSERT(lit_level(_trail[_decision_index[level - 1]]) == level);
      const Tlit* start = _trail.data();
      return start + _decision_index[level - 1];
    }

    /**
     * @brief Returns the current decision level of the solver.
     * @return current decision level of the solver.
     */
    inline Tlevel solver_level() const { return _decision_index.size(); }

    /** MARKERS **/
    /**
     * @brief Returns true if a variable was propagated and false otherwise.
     */
    inline bool var_propagated(Tvar var) const { return _vars[var].propagated; }
    /**
     * @brief Returns true if the literal was propagated and false otherwise.
     */
    inline bool lit_propagated(Tlit lit) const { return var_propagated(lit_to_var(lit)); }

    /**
     * @brief Returns true if the variable is marked
     */
    inline bool var_marked(Tvar var) const { return _vars[var].marked; }
    /**
     * @brief Returns true if the literal is marked as marked
     */
    inline bool lit_marked(Tlit lit) const { return var_marked(lit_to_var(lit)); }
    /**
     * @brief Marks the variable
     */
    inline void var_mark(Tvar var) { _vars[var].marked = true; }
    /**
     * @brief Marks the literal
     */
    inline void lit_mark(Tlit lit) { var_mark(lit_to_var(lit)); }
    /**
     * @brief Unmark the variable
     */
    inline void var_unmark(Tvar var) { _vars[var].marked = false; }
    /**
     * @brief Unmark the literal
     */
    inline void lit_unmark(Tlit lit) { var_unmark(lit_to_var(lit)); }

    inline bool var_synced(Tvar var) const {
      const TSvar& v = _vars[var];
      return v.state != VAR_UNDEF && v.synced == v.state % 2;
    }
    inline bool lit_synced(Tlit lit) const { return var_synced(lit_to_var(lit)); }
    inline void var_sync(Tvar var) {
      ASSERT(!var_undef(var));
      _vars[var].synced = _vars[var].state % 2;
      NOTIFY_STAT(_n_sync);
    }
    inline void lit_sync(Tlit lit) { var_sync(lit_to_var(lit)); }

    inline bool var_locked(Tvar var) const { return _vars[var].locked; }
    inline bool lit_locked(Tlit lit) const { return var_locked(lit_to_var(lit)); }
    inline void var_lock(Tvar var) {
      ASSERT(!var_undef(var));
      ASSERT(!_vars[var].locked);
      _vars[var].locked = true;
      NOTIFY_OBSERVER(lock_assumption, literal(var, var_true(var)));
      _n_assumptions++;
    }
    inline void lit_lock(Tlit lit) { var_lock(lit_to_var(lit)); }
    inline void var_unlock(Tvar var) {
      ASSERT(!var_undef(var));
      ASSERT(_vars[var].locked);
      _vars[var].locked = false;
      NOTIFY_OBSERVER(unlock_assumption, literal(var, var_true(var)));
      _n_assumptions--;
    }
    inline void lit_unlock(Tlit lit) { var_unlock(lit_to_var(lit)); }

    /**  LAZY REIMPLICATION  **/
    /**
     * @brief Returns an alternative reason for propagating the variable at a
     * lower level.
     * If no such reason exists, returns CLAUSE_UNDEF.
     * @param var variable to evaluate.
     * @return a missed lower implication of the variable.
     */
    inline Tclause var_lazy_reason(Tvar var) const { return _vars[var].missed_lower_implication; }
    inline Tclause& var_lazy_reason(Tvar var) { return _vars[var].missed_lower_implication; }

    /**
     * @brief Returns an alternative reason for propagating the literal at a
     * lower level.
     * If no such reason exists, returns CLAUSE_UNDEF.
     * @param lit literal to evaluate.
     * @return a missed lower implication of the literal.
     */
    inline Tclause lit_lazy_reason(Tlit lit) const { return _vars[lit_to_var(lit)].missed_lower_implication; }
    inline Tclause& lit_lazy_reason(Tlit lit) { return _vars[lit_to_var(lit)].missed_lower_implication; }

    /**
     * @brief Returns the level of the lazy reimplication of a literal.
     * @param lit literal to evaluate.
     * @return level of the lazy reimplication of the literal
     * That is, δ(λ(ℓ) \ {ℓ}) if ℓ is lit
     */
    inline Tlevel lit_lazy_level(Tlit lit)
    {
      Tclause l_reason = lit_lazy_reason(lit);
      if (l_reason == CLAUSE_UNDEF)
        return LEVEL_UNDEF;
      Tlit* lits = clause_lits(l_reason);
      ASSERT(lit == lits[0]);
      ASSERT(lit_level(lit) > LEVEL_ROOT);
      ASSERT(check_clause_implying(l_reason));
      ASSERT(clause_size(l_reason) > 1);
      ASSERT(lit_is_max_literal(lits[1], lits + 1, clause_size(l_reason) - 1));
      return lit_level(lits[1]);
    }

    /**  GRAPH BACKTRACKING  **/
    /**
     * @brief Returns the chunk of a variable.
     * @param var variable to evaluate.
     * @return chunk of the variable.
     */
    inline const bitset& var_chunks(Tvar var) const { return _vars[var].chunks; }
    inline bitset& var_chunks(Tvar var) { return _vars[var].chunks; }

    /**
     * @brief Returns the chunk of a literal.
     * @param lit literal to evaluate.
     * @return chunk of the variable of the literal.
     */
    inline const bitset& lit_chunks(Tlit lit) const { return var_chunks(lit_to_var(lit)); }
    inline bitset& lit_chunks(Tlit lit) { return var_chunks(lit_to_var(lit)); }

    /**
     * @brief Returns the decision level of the decision starting a chunk
     * @param chunk chunk to evaluate.
     * @return level of the decision starting the chunk.
     */
    Tlevel chunk_level(Tchunk chunk) { return var_level(_chunks[chunk].decision); }

    /**
     * @brief Returns the cross-chunk dependencies of a variable.
     * @param var variable to evaluate.
     * @return cross-chunk dependencies of the variable.
     */
    inline const bitset& var_cross_chunks(Tvar var) const { return _vars[var].cross_chunks; }
    inline bitset& var_cross_chunks(Tvar var) { return _vars[var].cross_chunks; }

    /**
     * @brief Returns the cross-chunk dependencies of a literal.
     * @param lit literal to evaluate.
     * @return cross-chunk dependencies of the variable of the literal.
     */
    inline const bitset& lit_cross_chunks(Tlit lit) const { return var_cross_chunks(lit_to_var(lit)); }
    inline bitset& lit_cross_chunks(Tlit lit) { return var_cross_chunks(lit_to_var(lit)); }

    /** CLAUSES **/
    /**
     * @brief Returns the literals of a clause.
     * @param cl clause to evaluate.
     * @return literals of the clause.
     */
    inline const Tlit* clause_lits(Tclause cl) const { return _clauses[cl].lits; }
    inline Tlit* clause_lits(Tclause cl) { return _clauses[cl].lits; }

    /**
     * @brief Returns the size of a clause.
     * @param cl clause to evaluate.
     * @return size of the clause.
     */
    inline size_t clause_size(Tclause cl) const { return _clauses[cl].size; }

    inline Tlevel implication_level(Tclause cl) const {
      const Tlit* lits = clause_lits(cl);
      Tlevel max_level = LEVEL_ROOT;
      for (size_t i = 1; i < clause_size(cl); i++) {
        max_level = std::max(max_level, lit_level(lits[i]));
      }
      return max_level;
    }

    inline void recompute_chunks(Tlit lit) {
      ASSERT(lit_reason(lit) != CLAUSE_UNDEF);
      const Tlit* lits = clause_lits(lit_reason(lit));
      lit_chunks(lit).clear();
      for (size_t i = 1; i < clause_size(lit_reason(lit)); i++) {
        lit_chunks(lit) |= lit_chunks(lits[i]);
      }
    }


    /** OTHERS **/
    /**
     * @brief Returns true if the clause is protected and cannot be deleted.
     * @param cl clause to evaluate.
     * @return true if the clause is protected, false otherwise.
     */
    inline bool is_protected(Tclause cl) const
    {
      ASSERT(cl < _clauses.size());
      return lit_reason     (_clauses[cl].lits[0]) == cl
          || lit_lazy_reason(_clauses[cl].lits[0]) == cl;
    }

    /**
     * @brief Marks a variable as being constrained. That is, the variable should be assigned a value by the solver.
     * @details if the options ignore_unused_variables is set to false, all variables are marked as constrained.
     */
    inline void var_mark_constrained(Tvar var) {
      if (var_constrained(var)) {
        return;
      }
      _variable_heap.insert(var, _vars[var].activity);
      _vars[var].constrained = 1;
    }

    /**
     * @brief Returns true if the variable is constrained. That is, the variable should be assigned a value by the solver.
     */
    inline bool var_constrained(Tvar var) const { return _vars[var].constrained == 1; }


    /*************************************************************************/
    /*                          Resource management                          */
    /*************************************************************************/
    /**
     * @brief If var is an unknown variable, reallocate the data structures to
     * take into account the new variable.
     * @param var variable to allocate.
     * @note Every variable with an index lower than var will be allocated.
     */
    void var_allocate(Tvar var);

    /**
     * @brief Allocates new chunks and rescale all variable chunks.
     */
    void allocate_chunks(size_t n_chunks);

    /**
     * @brief Returns the next clause identifier.
     * @details If there are deleted clauses, returns the identifier of a
     * deleted clause. Otherwise, returns the next identifier.
     * Ensures that the clause has enough memory allocated to store size
     * literals.
     * @return the identifier of a new clause.
     */
    Tclause next_clause_id(size_t size);

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
                                bool learned, bool external,
                                Tclause id = CLAUSE_UNDEF);

    /**
     * @brief Returns the chunks of a clause.
     * @param cl clause to evaluate.
     * @return chunk of the clause.
     */
    bitset clause_chunks(Tclause cl) const;

    /**
     * @brief Returns the level of a clause.
     * @param cl clause to evaluate.
     * @return the maximum level of the literals in the clause.
     */
    Tlevel clause_level(Tclause cl) const;

    /**
     * @brief Defragments the order of the trail. That is, the order of the variables in the trail is updated to
     * be contiguous and increasing from 0 to the number of literals in the trail.
     * This is used to ensure that no arithmetic overflow happens in the order of the variables.
     */
    void defragment_order();

    /**
     * @brief Defragments the trail by sorting the trails by levels.
     */
    void defragment_trail();

    /**
     * @brief Performs binary search to find a literal in the trail. Returns the index of the literal if it is in the
     * trail, and TRAIL_UNDEF otherwise.
     * @pre The trail is sorted by the order of the variables, so the search is done on the variable of the literal.
     */
    size_t find_literal_in_trail(Tlit lit) const;


    /*************************************************************************/
    /*                             Watched lists                            */
    /*************************************************************************/
    /**
     * @brief Adds a clause to the watch list of a literal
     * @param lit literal to watch.
     * @param clause clause to add to the watch list.
     * @pre The literal must be the first or second literal of the clause.
     * @post
     */
    void watch_lit(Tlit lit, Tclause cl);

    /**
     * @brief Add a binary clause to the watch list of its two literals.
     */
    void watch_lit_bin(Tclause cl);

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
     * @brief Returns that maximal utility heuristic value for a literal at the current state of the solver.
     * @return maximal utility heuristic value.
     */
    unsigned max_utility_heuristic();

    /**
     * @brief After conflict learning and backtracking, this function ensures that the two watched literals are correct. That is, the first literal is unassigned and the second is the highest in the clause.
     */
    void fix_watched_literals(Tclause conflict);

    /**
     * @brief Brings forward the two most relevant literals of the clause. A
     * literal is more relevant if it has a higher utility (see utility
     * _heuristic). The literals are moved to the first two positions of the
     * clause.
     * @param lits array of literals to reorder.
     * @param size size of the clause.
     */
    void select_watched_literals(Tlit* lits, unsigned size);


    /*************************************************************************/
    /*                     Boolean Constraint Propagation                    */
    /*************************************************************************/
    /**
     * @brief Add one literal to the propagation queue.
     * @pre The literal ℓ must be unassigned.
     *     ¬ℓ ∉ π ∧ ℓ ∉ π
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
     *
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
     * @brief Eagerly reimplies a literal to level ROOT.
     * @pre The literal ℓ must be satisfied.
     *   ℓ ∈ π
     * @pre The first literal of the clause is ℓ
     *   C[0] = ℓ
     * @pre The reason C must be a propagating clause.
     *   C ≠ ■ ∧ C \ {ℓ}, π ⊧ ⊥
     * @post The level of the literal ℓ is set to LEVEL_ROOT.
     *   δ(ℓ) = LEVEL_ROOT
     * @post The reason of the literal ℓ is set to C.
     *   ρ(ℓ) = C
     */
    void reimply_literal_root(Tlit lit, Tclause reason);

    /**
     * @brief Eagerly reimplies a decision literal to an implied literals with new justification.
     * @details Restores the topological order of the trail by reimplying the decision
     */
    void eager_decision_reimplication(Tlit decision, Tclause reason);

    /**
     * @brief Checks whether reimplying lit (a decision literal) would create a cycle.
     */
    bool reimplication_cycle(Tchunk decision_chunk, const bitset& reimplying_chunks);

    /**
     * @brief Searches for a replacement literal for the first watched literal
     * of a clause.
     * @details Provided a clause C = {c₁, c₂, ...} and a partial assignment
     * π = τ ⋅ ω quick_replacement(C) returns a literal r ∈ C \ {c₁} such that
     *   ¬r ∈ (τ ⋅ ¬c₁) ⇒ C \ {c₂}, π ⊧ ⊥
     * that is, this procedure simply searches for a literal that is not falsified
     * by the current assignment. If it fails, it returns the second watched
     * literal of the clause.
     */
    Tlit* quick_replacement(Tclause cl);

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
    Tlit* advanced_level_replacement(Tlit* lits, unsigned size) const;

    /**
     * @brief Searches for a replacement literal for the first watched literal
     * of a clause.
     * @details Provided a clause C = {c₁, c₂, ...} and a partial assignment
     * π = τ ⋅ ω advanced_graph_replacement(C) returns a literal r ∈ C \ {c₁} such that
     * - the chunks of r is a superset of the chunks of c₁
     *   γ(c₁) ⊆ γ(r)
     * - r is a top element of the chunk lattice of the clause
     *   ∀ ℓ' ∈ C ∖ {ℓ} . γ(r) ⊈ γ(ℓ')
     * @pre All literals in C \ {c₁} are falsified by the current assignment.
     */
    Tlit* advanced_graph_replacement(Tlit* lits, unsigned size) const;

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
    void propagate_binary_clauses(Tlit lit);

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
    void propagate_lit(Tlit lit);

    /*************************************************************************/
    /*                           Conflict analysis                           */
    /*************************************************************************/

    /**
     * @brief Removes the clauses that are no longer conflicting from the conflict buffer.
     * @details In incremental solving, it can be that adding a missed lower implication to the
     * formula forces a backtrack that invalidates some of the conflicts stored in the conflict buffer.
     * (not in graph backtracking)
     */
    void purge_conflict_buffer();

    void subsumption_filter(std::vector<bitset>& possibilities);

    void compute_lazy_merge_chunk_combination(std::vector<bitset>& combinations, bitset current, bitset processed);

    void enhance_backtrack_possibilities_with_lazy_merging(std::vector<bitset>& possibilities);

    void fix_conflicts_and_learned_in_order(const std::vector<std::pair<Tclause, std::vector<Tlit>>>& learned);

    /**
     * @brief Returns true if the learned clause is redundant with any other confliting clauses in the set of conflicts.
     * @details The clause is held in the _lit_buffer and its size in _lit_buffer_size.
     * @details A learned clause is redundant if it is subsumed by any other conflicting clause
     * @return true if the learned clause is redundant, false otherwise.
     */
    bool learned_clause_is_redundant();

    /**
     * @brief Returns true if the clause already exists in the clause set.
     * @pre The literals are marked
     */
    Tclause clause_subsumed_in_formula(const Tlit* lits, size_t size) const;

    /**
     * @brief Returns true if there is a conflict at root level, complete the proof if needed, and set the status to UNSAT.
     * @return true if there is a conflict at root level, false otherwise.
     */
    bool root_level_conflict();

    typedef struct Tweight {
      /**
       * @brief The set of chunks being considered by the weight counter
       */
      bitset chunks;
      /**
       * @brief The level of the highest decision in the chunks being considered
       */
      Tlevel highest_level = LEVEL_ROOT;
      /**
       * @brief The level of the lowest decision in the chunks being considered
       */
      Tlevel lowest_level = LEVEL_UNDEF;
      /**
       * @brief Total weight measured until the give_up_point
       */
      double total_weight = 0.0;
      /**
       * @brief When the weight counter decided to stop counting
       */
      size_t give_up_point = 0;
      /**
       * @brief Whether the weight counter has finished counting
       */
      bool finished = false;
      /**
       * @brief true if the bitset does not lead to learning a new clause or assigning a literal at root level
       */
      bool maybe_learning = true;

      bool operator<(const Tweight& other) const {
        if (finished ^ other.finished)
          return finished;

        if (finished) {
          if (total_weight == other.total_weight) {
            if (maybe_learning != other.maybe_learning)
              return maybe_learning;
            if (lowest_level != other.lowest_level)
              return lowest_level < other.lowest_level;
            if (highest_level != other.highest_level)
              return highest_level < other.highest_level;
            // if everything is identical, go thought the chunks to have a deterministic order
            auto it1 = chunks.cbegin();
            auto it2 = other.chunks.cbegin();
            while (it1 != chunks.cend() && it2 != other.chunks.cend()) {
              if (*it1 != *it2)
                return *it1 < *it2;
              ++it1;
              ++it2;
            }
            assert(false); // should never happen
          }
          return total_weight < other.total_weight;
        }

        if (lowest_level != other.lowest_level)
          return lowest_level < other.lowest_level;
        return highest_level < other.highest_level;
      }

      std::string to_string() const {
        std::string res = "chunks: " + chunks.to_string();
        res += "(" + std::to_string(lowest_level) + "-" + std::to_string(highest_level) + ")";
        res += " = " + std::to_string(total_weight);
        if (finished)
          res += " (finished)";
        if (maybe_learning)
          res += " (maybe learning)";
        return res;
      }
    } Tweight;

    /**
     * @brief Given a set of bitsets, calculate their weights according to the current state of the solver.
     * @details This functions stop as soon as it can determine which chunks are the lightest. It does not fully compute the weight of all bitsets. After removing the best bitset and calling the function again, the computation will resume from where it stopped.
     * @param weights vector of bitsets to evaluate. After the call, the weights are updated and sorted such that the lightest bitset is at the beginning of the vector.
     */
    void calculate_bitset_weights(std::vector<Tweight>& weights);

    /**
     * @brief Given a set of bitsets, calculate an approximation of their weights according to the current state of the solver.
     * @details The approximation is done by ignoring the intersections between chunks. The cost of each chunk is calculated separately and summed to obtain the total weight of the bitset.
     * This function is faster than calculate_bitset_weights but less accurate.
     * @param weights vector of bitsets to evaluate. After the call, the weights are updated.
     */
    void calculate_bitset_weights_approx(std::vector<Tweight>& weights);

    double calculate_weight(const bitset& chunks);

    /**
     * @brief Given a set of conflicting clauses, computes the set of chunk sets, that, if backtracked, will resolve all conflicts at once.
     * @details The chunk sets are stored in the vector possibilities. A possibility Γ is a set of chunks such that ∀ C ∈ κ. γ(C) ∩ Γ ≠ ∅
     * @param conflicts set of conflicting clauses.
     * @param possibilities vector to store the resulting chunk sets.
     */
    void compute_backtrack_possibilities(std::vector<bitset>& conflict_chunks,
                                         std::vector<bitset>& possibilities);

    /**
     * @brief Given a learned clause, chooses the level to backtrack to
     * according to the options and the literals in the clause.
     */
    Tlevel choose_backtracked_level(Tlit* learned_lits, unsigned size) const;

    /**
     * @brief Checks if a conflict clause has exactly one literal in the given chunks.
     * @param conflict the conflict clause to check.
     * @param chunks the chunks to check against.
     * @return true if the conflict clause has exactly one literal in the chunks, false otherwise.
     */
    bool conflict_is_UIP_cut(Tclause conflict, const bitset& chunks);

    /**
     * @brief Checks if a conflict clause has exactly one literal at the highest level.
     * @param conflict the conflict clause to check.
     * @return true if the conflict clause has exactly one literal at the highest level, false otherwise.
     */
    bool conflict_is_UIP_cut(Tclause conflict, Tlevel level);

    /**
     * @brief Based on the conflicting clauses, compute the highest level that will fix all conflicts at once
     */
    Tlevel compute_repair_level();

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
    void repair_conflicts();

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
     * @brief Checks if a conflict clause can generate a learned clause when analyzed on the chunks bt.
     * @details A conflict clause can generate a learned clause if it intersects with exactly one chunk in bt, after considering the lazy reimplications.
     * @param conflict the conflict clause to check.
     * @param bt the chunks to check against.
     * @return true if the conflict clause can generate a learned clause, false otherwise.
     */
    bool conflict_can_generate_learned_clause(Tclause conflict, const bitset& bt);

    /**
     * @brief Checks if a conflict clause can generate a learned clause when analyzed at level bt.
     * @details A conflict clause can generate a learned clause if it does not have literals above level bt.
     * @param conflict the conflict clause to check.
     * @param bt the level to check against.
     * @return true if the conflict clause can generate a learned clause, false otherwise.
     */
    bool conflict_can_generate_learned_clause(Tclause conflict, Tlevel bt);

    /**
     * @brief Checks if a learned clause will be able to produce a propagating learned clause after backtracking.
     * @param conflict the conflict clause to check.
     * @param chunks the chunks to check against.
     * @return true if the learned clause will be propagating after backtracking, false otherwise.
     */
    bool implication_active_after_backtrack(Tclause conflict, const bitset& chunks);

    /**
     * @brief Checks if a conflict clause will be able to produce a propagating learned clause after backtracking.
     * @param conflict the conflict clause to check.
     * @param level the level to check against.
     * @return true if the learned clause will be propagating after backtracking, false otherwise.
     */
    bool implication_active_after_backtrack(Tclause conflict, Tlevel level);

    /**
     * @brief Adds a learned clause to the clause set and removes it right away.
     * @details This method is employed when a clause is discovered to be backward subsumed by another clause. When the clause is generated, it is materialized in the proof before being added to the solver. However, since it is subsumed, we do not wish to add it to the solver. So, we add it and delete it right away to ensure the proof is correct.
     * @param cl the clause number that will be added and deleted.
     * @param lits the literals of the clause to add and delete.
     */
    void add_and_delete_clause(Tclause cl, const std::vector<Tlit>& lits);

    /**
     * @brief Given a set of clauses, computes which clauses are backward subsumed by other clauses in the set of learned clauses.
     * @details Let S be the set of learned clauses {Cᵢ} A clause Cᵢ ∈ S is backward subsumed by a clause Cⱼ ∈ S, j > i if Cⱼ ⊆ Cᵢ.
     */
    void compute_subsumed_clauses(const std::vector<std::pair<Tclause, std::vector<Tlit>>> &clauses, std::vector<bool> &subsumed);

    /**
     * @brief This function does nothing, but it is used by the template function @a try_and_learn_impl. The variant with levels is usefull (see below)
     */
    const bitset& update_bt_after_analysis_of_reimplication(const bitset& chunks);

    /**
     * @brief Computes the maximum level in the _lit_buffer
     * @details After using lazy reimplication on the UIP, the next conflict analysis level is lower
     * @param level this parameter is useless, it's just used in the bitset variants to return it
     */
    Tlevel update_bt_after_analysis_of_reimplication(Tlevel level);

    /**
     * @brief Given a set of conflicts in @p _conflicts, attempt to learn new clauses using the 1UIP algorithm on the level or bitset specified by bt.
     * @details This is a template function meant to be instantiated only with @c T = @c Tlevel or @c T = @c const bitset&
     * @details The clauses must have a provided clause index because the proofs need to be aware of such index before the clauses are actually inserted in the formula.
     * @param bt the level or bitset on which that conflicts should be analyzed
     * @param learned_clauses a buffer in which the literals of the learned clauses will be store. The clauses are accompanied by a clause index that will be used for the insertion in the clause set.
     */
    template<typename T>
    void try_and_learn_impl(T bt, std::vector<std::pair<Tclause, std::vector<Tlit>>>& learned_clauses);

    /**
     * @brief Given a set of conflicts in @p _conflicts, and a set of @p chunks, pushes in @p learned_clause
     * @details This function is a wrapper around the template function @a try_and_learn_impl, providing a convenient interface for the user.
     * @param chunks the chunks on which that conflicts should be analyzed
     * @param learned_clauses a buffer in which the literals of the learned clauses will be store. The clauses are accompanied by a clause index that will be used for the insertion in the clause set.
     */
    void try_and_learn(const bitset& chunks, std::vector<std::pair<Tclause, std::vector<Tlit>>>& learned_clauses);

    /**
     * @brief Given a set of conflicts in @p _conflicts, and a level, pushes in @p learned_clause
     * @details This function is a wrapper around the template function @a try_and_learn_impl, providing a convenient interface for the user.
     * @param level the level on which that conflicts should be analyzed
     * @param learned_clauses a buffer in which the literals of the learned clauses will be store. The clauses are accompanied by a clause index that will be used for the insertion in the clause set.
     */
    void try_and_learn(Tlevel level, std::vector<std::pair<Tclause, std::vector<Tlit>>>& learned_clauses);

    /**
     * @brief Perform conflict repair using the graph-based criteria.
     * @details This function repairs the conflicts in the set of conflicts. That is, it decides the best chunks to undo, analyzes the conflicts, (possibly) learns new clauses, backtracks the chosen chunks, and implies literals using the conflicting and learned clauses.
     */
    void graph_repair();

    /**
     * @brief Setup the weights for a set of chunk sets.
     * @details This function initializes the weights vector for the given possibilities.
     * It is meant to be called before calculate_bitset_weights.
     * @param possibilities the set of chunk sets to evaluate.
     * @param highest_level the highest level in the set of conflicts.
     * @param weights the vector to store the weights.
     */
    void setup_weights(std::vector<bitset>& possibilities,
                       napsat::Tlevel& highest_level,
                       std::vector<napsat::NapSAT::Tweight>& weights);

    /**
     * @brief Perform conflict repair using the level-based criteria.
     * @details This function repairs the conflicts in the set of conflicts. That is, it chooses the appropriate backtrack level (based on CB, or NCB), analyzes the conflicts, (possibly) learns new clauses, backtracks to the chosen level, and implies literals using the conflicting and learned clauses.
     */
    void level_repair();

    /**
     * @brief Returns true if a literal must be further analyzed in conflict analysis.
     * That is, depending on the options, check the level or chunk requirements.
     */
    bool lit_analyzed(Tlit lit, Tlevel level);

    /**
     * @brief Returns true if a literal must be further analyzed in conflict analysis.
     * That is, depending on the options, check the level or chunk requirements.
     */
    bool lit_analyzed(Tlit lit, const bitset& chunks);

    template<typename T>
    bool mark_relevant_literals(Tlit lit, T level, unsigned& count);

    /**
     * @brief Helper to implement both the level and chunk conflict analysis together
     */
    template <typename T>
    void analyze_conflict_impl(T x);

    /**
     * Analyze a conflict and learn a new clause.
     * @param level level to analyze the conflict on.
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
     * @details Sets the clause in the literal_buffer and _lit_buffer_size variables.
     * @post The literal_buffer is set such that the first literal is the UIP
     */
    void analyze_conflict(Tlevel level);

    /**
     * @details Sets the clause in the literal_buffer and _lit_buffer_size variables.
     * @post The literal_buffer is set such that the first literal is the UIP
     */
    void analyze_conflict(const bitset& chunks);


    /*************************************************************************/
    /*                              Backtracking                             */
    /*************************************************************************/
    /**
     * @brief Unassign a variable.
     * @param var variable to unassign.
     * @pre The variable must be assigned.
     */
    void var_unassign(Tvar var);

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
     * @brief Unassigns all the variables in the chunk.
     * @param chunks the set of chunks to undo.
     */
    void backtrack(const bitset& backtracked_chunks);

    /*************************************************************************/
    /*                              Assumptions                              */
    /*************************************************************************/
    bool add_assumption_N_CB(Tlit lit);

    /**
     * @brief Adds the literal as an assumption in graph backtracking.
     * @param lit literal to add as an assumption in α.
     * @pre The literal ℓ must be satisfied.
     *   ℓ ∈ π
     * @pre The solver is using graph backtracking.
     * @details If ℓ ∈ π, then either ℓ is a decision, or it is implied.
     * - If ℓ is a decision, we simply lock it as an assumption.
     * - If ℓ is implied by a clause C, we convert it to a decision and lock it.
     *   Then, we need to update all clauses belonging to the original chunks
     *   of ℓ. That is, for each literal ℓ' after ℓ in the trail that depends
     *   on ℓ must be updated.
     *   Let Γ = γ(ℓ) be the chunks of ℓ before being converted to a decision,
     *   and ck be the new chunk of ℓ as a decision. We set γ(ℓ) = {ck}.
     *   For each literal ℓ' after ℓ in the trail such that Γ ⊆ γ(ℓ'),
     *   we need to recompute γ(ℓ').
     */
    void add_assumption_GB_true(Tlit lit);

    /**
     * @brief Adds the literal as a false assumption in graph backtracking.
     * @param lits list of literals to add as assumptions.
     * @pre All literals ℓ in the list L must be falsified.
     *   ∀ℓ ∈ L. ¬ℓ ∈ π
     * @pre The solver is using graph backtracking.
     * @details We treat the addition of a false assumptions in L as a
     * set of conflicts.
     * That is, we find the cheapest set of chunks to backtrack to in order to
     * free all literals in L. Then, we backtrack those chunks, convert each ℓ
     * in L into decisions, and lock them as assumptions.
     * @returns true if all assumptions were added successfully, false if
     * γ(ℓ) ⊆ α and the assumption could not be added without contradicting
     * other assumptions.
     */
    bool add_assumption_GB_false(std::vector<Tlit>& lits);

    /**
     * @brief Adds the literal as a false assumption in graph backtracking.
     * @param lit literal to add as an assumption.
     * @pre The literal ℓ must be falsified.
     *  ¬ℓ ∈ π
     * @pre The solver is using graph backtracking.
     * @details We treat the addition of a false assumption ¬ℓ as a conflict.
     * That is, we find the cheapest set of chunks to backtrack to in order to
     * free ℓ. Then, we backtrack those chunks, convert ℓ to a decision, and
     * lock it as an assumption.
     * @details If several assumptions are added, it is better to add all of
     * them at the same time to compute a better set of chunks to backtrack.
     */
    bool add_assumption_GB_false(Tlit lit);

    /**
     * @brief Adds the literal as assumption in graph backtracking.
     * @param lit literal to add as an assumption.
     * @pre The literal ℓ is undefined.
     *   ℓ ∉ π ∧ ¬ℓ ∉ π
     * @pre The solver is using graph backtracking.
     * @details The literal ℓ is converted to a decision and locked as an
     * assumption.
     */
    void add_assumption_GB_undef(Tlit lit);

    void remove_assumption_N_CB(Tlit lit);

    void remove_assumption_GB(Tlit lit);

    /*************************************************************************/
    /*                             Purge Clauses                             */
    /*************************************************************************/
    /**
     * @brief After purging clauses, ensure that the watch lists do not contains
     * clauses that were deleted
     */
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

    /*************************************************************************/
    /*                                 Other                                */
    /*************************************************************************/
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
     * @post The trail only contains literals that are assigned at level 0
     *   ∀ℓ ∈ π. δ(ℓ) = 0
     */
    void restart();

    /**
     * @brief Given a list of literals, removes duplicates and returns the new size.
     * @param lits array of literals to check for duplicates.
     * @param size size of the array.
     * @return new size of the array.
     */
    unsigned cleanup_duplicate_literals(Tlit* lits, unsigned size);

    /**
     * @brief Increases the activity of a variable.
     * @param var variable to bump.
     */
    void bump_var_activity(Tvar var);

    /**
     * @brief Prints a literal on the standard output.
     * @param lit literal to print.
     */
    void print_lit(Tlit lit) const;

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
    /*                        Printing the state                             */
    /*************************************************************************/
    /**
     * @brief Returns a colored string of a literal. The literal is printed
     * green if satisfied, red if falsified, and yellow if unassigned
     * @param lit literal to print.
     * @return colored string of the literal.
     */
    std::string lit_to_string(Tlit lit) const;

    /**
     * @brief Returns a string of a clause. The clause is printed in the form
     * "cl : lit1 lit2 ... litm | litm+1 ... litn" where lit1, ..., litn are the
     * literals of the clause, litm+1, ..., litn are disabled literals (false at
     * level 0) and cl is the clause id.
     * @param clause clause to print.
     * @return string of the clause.
     */
    std::string clause_to_string(Tclause cl) const;

    /**
     * @brief Returns a string of a clause. The clause is printed in the form
     * "{lit1 lit2 ... litm}"
     * @param lits array of literals to print.
     * @param size size of the array.
     */
    std::string clause_to_string(const Tlit* lits, size_t size) const;

    /**
     * @brief Prints the current assignment of the solver on the standard
     * output in a human-readable format.
     */
    void print_trail_simple() const;

    void print_chunks() const;

    /**
     * @brief Prints a clause on the standard output in a human-readable format.
     * @param clause clause to print.
     */
    void print_clause(Tclause cl) const;

    /**
     * @brief Prints the clause set on the standard output in a human-readable
     * format.
     */
    void print_clause_set() const;

    /**
     * @brief Prints the watch lists on the standard output in a human-readable
     * format.
     */
    void print_watch_lists(Tlit lit = LIT_UNDEF) const;


    /*************************************************************************/
    /*                     Functions for the checker                         */
    /*************************************************************************/
    // Note that some invariants are checked by the observer. Therefore, not
    // all relevant invariants are checked here.
    /**
     * @brief Returns trues if every variable in the trail is assigned and that
     * every assigned variable is in the trail.
     */
    bool check_trail_variable_consistency() const;

    /**
     * @brief Returns true if the decision indices of the literals in the trail
     * are consistent with their position in the trail.
     */
    bool check_decision_index_consistency() const;

    /**
     * @brief Returns true if the order of variables is monotonic in the trail
     */
    bool check_trail_order_consistency();

    /**
     * @brief Returns true if the decision levels of the literals in the trail
     * are monotonic.
     */
    bool check_trail_monotonicity();

    /**
     * @brief returns true if the clause cl is in the watch list of the literal
     * lit.
    */
    bool check_is_watched(Tlit lit, Tclause cl) const;

    /**
     * @brief returns true if the watch lists of watched literals of a clause
     * contain the clause.
     */
    bool check_watch_lists_complete() const;

    /**
     * @brief returns true if each clause in a watch list is watched by the
     * corresponding literal.
     */
    bool check_watch_lists_minimal() const;

    /**
     * @brief Returns true if the clause is a unit clause.
     * Further, it checks that a unit clause has its unassigned literal at the front of the clause
     * @warning This function is meant to be used for debugging
     */
    bool check_clause_unit(Tclause cl) const;

    /**
     * @brief Returns true if the clause is implying its first literal.
     * A clause C is implying its first literal ℓ if C[0] = ℓ and C \ {ℓ} ⊧ ⊥
     * @warning This function is meant to be used for debugging
     */
    bool check_clause_implying(Tclause cl) const;

    /**
     * @brief Returns true if the clause is satisfied.
     * @warning This function is meant to be used for debugging
     */
    bool check_clause_satisfied(Tclause cl) const;

    /**
     * @brief Returns true if the clause is falsified.
     * @warning This function is meant to be used for debugging
     */
    bool check_clause_falsified(Tclause cl) const;

    /**
     * @brief Returns true if the literal lit is the maximum literal in cl according to
     * - either decision level if in NCB or CB
     * - or chunk lattice if in GB
     * @warning This function is meant to be used for debugging
     */
    bool lit_is_max_literal(Tlit lit, const Tlit* lits, size_t size) const;

    /**
     * @brief Returns true if there is at least one clause in the watch list of lit that
     * does not satisfy the invariants.
     * @warning This function is very expensive and should only be used for debugging
     */
    bool check_lit_needs_fixing(Tlit lit) const;
  };
}
