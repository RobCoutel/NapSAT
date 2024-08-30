/**
 * @file src/solver/NapSAT.cpp
 * @author Robin Coutelier
 *
 * @brief This file is part of the NapSAT solver. It implements the core functions of the
 * solver such as the CDCL loop, BCP, conflict analysis, backtracking and clause set simplification.
 */
#include "NapSAT.hpp"
#include "custom-assert.hpp"

#include <iostream>
#include <cstring>
#include <functional>

using namespace napsat;
using namespace std;

void NapSAT::imply_literal(Tlit lit, Tclause reason)
{
  /**
   * Preconditions:
   * - lit ℓ is undefined
   *    ℓ ∉ π
   * - the reason C is either undefined or a propagating clause
   *    C = ■ ∨ [ℓ ∈ C ∧ C \ {ℓ}, π ⊧ ⊥]
   * - the first literal of the clause is ℓ
   *    C ≠ ■ ⇒ C[0] = ℓ
   * - the second literal of the clause is at the highest level in C
   *    δ(ℓ) = δ(C \ {ℓ})
   */
  ASSERT_MSG(lit_undef(lit),
    "Literal: " + lit_to_string(lit) + "\nReason: " + clause_to_string(reason));
#ifndef NDEBUG
  if (reason != CLAUSE_UNDEF && reason != CLAUSE_LAZY) {
    for (unsigned i = 1; i < _clauses[reason].size; i++) {
      ASSERT(lit_false(_clauses[reason].lits[i]));
      ASSERT(lit_level(_clauses[reason].lits[i]) <= lit_level(lit));
    }
  }
#endif

  Tvar var = lit_to_var(lit);
  _trail.push_back(lit);
  TSvar& svar = _vars[var];
  svar.state = lit_pol(lit);
  svar.waiting = true;
  svar.reason = reason;
  svar.waiting = true;

  _agility *= _options.agility_decay;
  _options.agility_threshold *= _options.threshold_multiplier;

  if (reason == CLAUSE_UNDEF) {
    // Decision
    _decision_index.push_back(_trail.size() - 1);
    svar.level = solver_level();
    NOTIFY_OBSERVER(_observer, new napsat::gui::decision(lit));
  }
  else if (reason == CLAUSE_LAZY) {
    // Theory propagation
    ASSERT_MSG(false, "Lazy reason is not implemented yet");
  }
  else {
    // Implied literal
    ASSERT(lit == _clauses[reason].lits[0]);
    if (_clauses[reason].size == 1)
      svar.level = LEVEL_ROOT;
    else {
      ASSERT(lit == _clauses[reason].lits[0]);
      svar.level = lit_level(_clauses[reason].lits[1]);
    }
    NOTIFY_OBSERVER(_observer, new napsat::gui::implication(lit, reason, svar.level));
  }
  if (lit_pol(lit) != svar.phase_cache)
    _agility += 1 - _options.agility_decay;
  svar.phase_cache = lit_pol(lit);

  if (svar.level == LEVEL_ROOT) {
    _purge_counter++;
    if (_proof)
      _proof->root_assign(lit, reason);
  }
  ASSERT(svar.level != LEVEL_UNDEF);
  ASSERT(svar.level <= solver_level());
}

void napsat::NapSAT::reimply_literal(Tlit lit, Tclause reason)
{
  ASSERT(lit_true(lit));
  ASSERT(reason != CLAUSE_UNDEF);
  ASSERT(reason != CLAUSE_LAZY);
  ASSERT(lit == _clauses[reason].lits[0]);
  ASSERT(_options.lazy_strong_chronological_backtracking);

  TSclause& clause = _clauses[reason];
  Tlevel reimplication_level = clause.size == 1 ? 0 : lit_level(clause.lits[1]);
#ifndef NDEBUG
  for (unsigned i = 1; i < clause.size; i++) {
    ASSERT(lit_false(clause.lits[i]));
    ASSERT(lit_level(clause.lits[i]) <= reimplication_level);
  }
#endif

  Tlevel current_level = lit_level(lit);
  if (current_level <= reimplication_level)
    return;
  if (lit_lazy_reason(lit) != CLAUSE_UNDEF
   && lit_level(_clauses[lit_lazy_reason(lit)].lits[1]) <= reimplication_level)
    return;

  lit_set_lazy_reason(lit, reason);
}

Tlit* napsat::NapSAT::search_replacement(Tlit* lits, unsigned size)
{
  /**
   * Pre conditions:
   * - The set of literals C = {c₂, ¬ℓ, ..., cₙ} has more than two literals
   *    |C| > 2
   * - The second literal ¬ℓ of the clause is falsified but not yet propagated
   *    ℓ ∈ ω
   */
  ASSERT(size >= 2);
  ASSERT(lit_false(lits[1]));
  // This must be as efficient as possible!
  Tlit* end = lits + size;
  Tlit* k = lits + 2;
  Tlevel low_sat_lvl = lit_true(lits[0]) ? lit_level(lits[0]) : LEVEL_UNDEF;
  Tlevel high_non_sat_lvl = lit_level(lits[1]);
  Tlit* high_non_sat_lit = lits + 1;
  while (k < end) {
    if (!lit_false(*k))
      return k;
    if (lit_level(*k) > high_non_sat_lvl) {
      // in non-chronological backtracking, the watched literals are always at the highest level
      ASSERT(_options.chronological_backtracking);
      high_non_sat_lvl = lit_level(*k);
      high_non_sat_lit = k;
    }
    if (low_sat_lvl <= high_non_sat_lvl) {
      ASSERT(_options.chronological_backtracking);
      ASSERT(k == high_non_sat_lit);
      return k;
    }
    k++;
  }
  return high_non_sat_lit;
}

Tclause napsat::NapSAT::propagate_binary_clauses(Tlit lit)
{
  lit = lit_neg(lit);
  ASSERT(lit_false(lit));

  for (pair<Tlit, Tclause> bin : _binary_clauses[lit]) {
    ASSERT_MSG(_clauses[bin.second].size == 2, "Clause: " + clause_to_string(bin.second) + ",Literal: " + lit_to_string(lit));
    if (lit_true(bin.first)) {
      if (_options.lazy_strong_chronological_backtracking && lit_level(bin.first) > lit_level(lit)) {
        // missed lower implication
        Tlit* lits = _clauses[bin.second].lits;
        if (lits[0] != bin.first) {
          lits[0] = lits[0] ^ lits[1];
          lits[1] = lits[0] ^ lits[1];
          lits[0] = lits[0] ^ lits[1];
          // no need to update the watch lists because the clause is binary
        }
        reimply_literal(bin.first, bin.second);
      }
      continue;
    }
    if (lit_undef(bin.first)) {
      // ensure that the implied literal is positioned at the first position
      Tlit* lits = _clauses[bin.second].lits;
      ASSERT(lits[0] == lit || lits[1] == lit);
      ASSERT(lits[0] == bin.first || lits[1] == bin.first);
      lits[0] = bin.first;
      lits[1] = lit;
      imply_literal(bin.first, bin.second);
      continue;
    }
    // Conflict
    ASSERT(_options.chronological_backtracking || lit_level(bin.first) == lit_level(lit));
    if (_options.chronological_backtracking) {
      // make sure that the highest literal is at the first position
      Tlit* lits = _clauses[bin.second].lits;
      if (lit_level(lits[0]) < lit_level(lits[1])) {
        // in place swapping
        lits[0] ^= lits[1];
        lits[1] ^= lits[0];
        lits[0] ^= lits[1];
        // we do not need to update the next watched clause because the clause is binary
      }
    }
    ASSERT(lit_level(_clauses[bin.second].lits[0]) >= lit_level(_clauses[bin.second].lits[1]));
    return bin.second;
  }
  return CLAUSE_UNDEF;
}

Tclause NapSAT::propagate_lit(Tlit lit)
{
  ASSERT(watch_lists_complete());
  ASSERT(watch_lists_minimal());
  /**
   * The mathematical notations and the contract of this function are defined in NapSAT.hpp
  */
  lit = lit_neg(lit);
  ASSERT(lit_false(lit));

  // level of the propagation
  Tlevel lvl = lit_level(lit);
  vector<Tclause>& watch_list = _watch_lists[lit];

  // Be careful that with this method, we do not want to push anything to the watch list.
  // Otherwise the memory might be reallocated and the pointers invalidated.
  // TODO check if this watch list shuffling is good for performance
  Tclause* i = watch_list.data();
  Tclause* end = i + watch_list.size();

  /**
   * Let F* be a set of clauses such that each clause in the set satisfies
   * - NCB:  ¬c₁ ∈ (τ ⋅ ℓ) ⇒ c₂ ∈ π ∨ b ∈ π
   *        δ(b) ≤ δ(c₂) is trivially true in NCB since the levels are monotonically increasing
   * - WCB: ¬c₁ ∈ (τ ⋅ ℓ) ⇒ ¬c₂ ∉ (τ ⋅ ℓ) ∨ [b ∈ π ∧ δ(b) ≤ δ(c₁)]
   *        We actually want to enforce ¬c₁ ∈ τ ⇒ c₂ ∈ π ∨ [b ∈ π ∧ δ(b) ≤ δ(c₁)] but cannot
   *        guarantee that this will hold after backtracking.
   * - LSCB: ¬c₁ ∈ (τ ⋅ ℓ) ⇒ [c₂ ∈ π ∧ [δ(c₂) ≤ δ(c₁) ∨ δ(λ(c₂) \ {c₂}) ≤ δ(c₁)]
   *                     ∨ [b ∈ π ∧ δ(b) ≤ δ(c₁)]
   *
   * We initialise F* with all the clauses that are not watched by ¬ℓ. If they satisfied the invariant
   * ts
   * before the propagation, they will satisfy them after ℓ is added to τ without any action.
   *
   * For the clauses watched by ¬ℓ, C = c₁ ∨ c₂ ∨ ... ∨ cₙ with b, a blocker such that b ∈ C
   * we will reason over the Haore triplets {P} loop body {Q}
   * where P is the precondition that the invariant holds before the loop
   * and Q is the postcondition that the invariant holds after the loop if ℓ = ¬c₁ is added to τ
   * - NCB:  ¬c₁ ∈ (τ ⋅ ℓ) ⇒ c₂ ∈ π ∨ b ∈ π
   *        δ(b) ≤ δ(c₂) is trivially true in NCB since the levels are monotonically increasing
   * - WCB: ¬c₁ ∈ (τ ⋅ ℓ) ⇒ ¬c₂ ∉ (τ ⋅ ℓ) ∨ [b ∈ π ∧ δ(b) ≤ δ(c₁)]
   *        We actually want to enforce ¬c₁ ∈ τ ⇒ c₂ ∈ π ∨ [b ∈ π ∧ δ(b) ≤ δ(c₁)] but cannot
   *        guarantee that this will hold after backtracking.
   * - LSCB: ¬c₁ ∈ (τ ⋅ ℓ) ⇒ [c₂ ∈ π ∧ [δ(c₂) ≤ δ(c₁) ∨ δ(λ(c₂) \ {c₂}) ≤ δ(c₁)]
   *                       ∨ [b ∈ π ∧ δ(b) ≤ δ(c₁)]
   *
   * If Q is true, then we can add C to F* and we know that the invariants are preserved.
   * If we cannot make Q true, then there is a conflict and we can return C.
   *
   * If we have not returned a conflict, at then end of the loop, we will have explored
   * all the clauses watched by ¬ℓ and F* = F. Therefore, we satisfy our contract.
   */
  while (i < end) {
    Tclause cl = *i;
    TSclause& clause = _clauses[cl];
    ASSERT(clause.watched);
    ASSERT(clause.size >= 2);
    // Skip condition before dereferencing the pointers
    if (lit_true(clause.blocker)
      && (!_options.chronological_backtracking || lit_level(clause.blocker) <= lvl)) {
      /**
       * NCB: b ∈ π
       * WCB: b ∈ π ∧ δ(b) ≤ δ(c₁)
       * SCB: b ∈ π ∧ δ(b) ≤ δ(c₁)
       * the invariants are preserved without any action
       */
      i++;
      continue;
    }

    Tlit* lits = clause.lits;
    /**
     * we call c₁ and c₂ the watched literals of the clause
     * we ensure that c₁ = ¬ℓ to make the rest of the function more efficient
     */
    ASSERT_MSG(lit == lits[0] || lit == lits[1],
      "Clause: " + clause_to_string(cl) + ",Literal: " + lit_to_string(lit));

    // ensure that c₁ = ¬ℓ and c₂ = the other watched literal
    Tlit lit2 = lits[0] ^ lits[1] ^ lit;
    ASSERT(lit2 == lits[0] || lit2 == lits[1]);
    ASSERT(lit != lit2);
    lits[0] = lit2;
    lits[1] = lit;

    /** SKIP CONDITIONS **/
    if (lit_true(lit2)
      && (!_options.lazy_strong_chronological_backtracking || lit_level(lit2) <= lvl)) {
      /**
       * NCB: c₂ ∈ π
       * WCB: c₂ ∈ π
       * SCB: c₂ ∈ π ∧ δ(c₂) ≤ δ(c₁)
       * the invariants are preserved without any action
       */
      i++;
      continue;
    }
    /** SEARCH REPLACEMENT **/
    Tlit* replacement = search_replacement(lits, clause.size);
    /**
     * Search replacement returns a literal r ∈ C \ {c₂} such that it either is a good replacement such that
     *   ¬r ∈ (τ ⋅ ¬c₁) ⇒ c₂ ∈ π ∧ δ(c₂) ≤ δ(r)
     * or C \ {c₂} is conflicting with π and r is the highest literal in C \ {c₂}
     *   C \ {c₂}, π ⊧ ⊥ ∧ δ(r) = δ(C \ {c₂})
     * NCB: If c₂ ∈ π, then we would have stopped at the skip conditions
     *      We know that [C \ {c₂}, π ⊧ ⊥] ⇒ δ(c₁) = δ(c₂) = δ(C) and c₁ will be returned
     *      if C \ {c₂} is conflicting
     *
     * We know that
     * ALL: [¬r ∈ (τ ⋅ ¬c₁) ⇒ c₂ ∈ π ∧ δ(c₂) ≤ δ(r)] ∨ [C \ {c₂}, π ⊧ ⊥ ∧ δ(r) = δ(C \ {c₂})]
     * NCB: c₂ ∉ π                   ∧ b ∉ π
     * WCB: c₂ ∉ π                   ∧ [b ∉ π ∨ δ(b) > δ(c₁)]
     * SCB: [c₂ ∉ π ∨ δ(c₂) > δ(c₁)] ∧ [b ∉ π ∨ δ(b) > δ(c₁)]
    */
    ASSERT(replacement != nullptr);

    Tlevel replacement_lvl = lit_level(*replacement);

    ASSERT_MSG(_options.chronological_backtracking || (!lit_true(*replacement) || replacement_lvl <= lvl),
      "Clause: " + clause_to_string(cl) + "\nLiteral: " + lit_to_string(lit) + "\nReplacement: " + lit_to_string(*replacement) + "\nLevel: " + to_string(lvl));
    /** TRUE literal **/
    if (lit_true(*replacement) && replacement_lvl <= lvl) {
      /**
       * r ∈ π ∧ δ(r) ≤ δ(c₁)
       * NCB: We know that r ∈ π ⇒ δ(r) ≤ δ(c₁). Therefore after this condition is satisfied in NCB,
       *      we know that r ∉ π
       * ¬c₁ ∈ τ ⇒ c₂ ∈ π ∨ [b ∈ π ∧ δ(b) ≤ δ(c₁)] is satisfied if we set b = r
      */
      clause.blocker = *replacement;
#if NOTIFY_WATCH_CHANGES
      NOTIFY_OBSERVER(_observer, new napsat::gui::block(cl, *replacement));
#endif
      i++;
      continue;
    }
    /**
     * We know that
     * ALL: [¬r ∈ (τ ⋅ ¬c₁) ⇒ c₂ ∈ π ∧ δ(c₂) ≤ δ(r)] ∨ [C \ {c₂}, π ⊧ ⊥ ∧ δ(r) = δ(C \ {c₂})]
     * NCB: r ∉ π                  ∧ c₂ ∉ π                   ∧  b ∉ π
     * WCB: [r ∉ π ∨ δ(r) > δ(c₁)] ∧ c₂ ∉ π                   ∧ [b ∉ π ∨ δ(b) > δ(c₁)]
     * SCB: [r ∉ π ∨ δ(r) > δ(c₁)] ∧ [c₂ ∉ π ∨ δ(c₂) > δ(c₁)] ∧ [b ∉ π ∨ δ(b) > δ(c₁)]
    */

    /** UNDEF or TRUE literal **/
    if (!lit_false(*replacement)) {
      /**
       * We now know that ¬r ∉ π, and a fortiori ¬r ∉ (τ ⋅ ¬c₁) since ¬c₁ ∈ ω ⊆ π
       * Therefore, we can replace c₁ by r and satisfy the invariant
       */
      // watch the replacement and stop watching lit
      lits[1] = *replacement;
      *replacement = lit;
#if NOTIFY_WATCH_CHANGES
      NOTIFY_OBSERVER(_observer, new napsat::gui::unwatch(cl, lit));
#endif
      // remove the clause from the watch list
      // bring the last watched clause to the current position
      *i = *(end - 1);
      end--;
      // watch new literal
      watch_lit(lits[1], cl);
      continue;
    }

    /** NO GOOD REPLACEMENT **/
    /**
     * We know that [¬r ∈ (τ ⋅ ¬c₁) ⇒ c₂ ∈ π ∧ δ(c₂) ≤ δ(r)] ∨ [C \ {c₂}, π ⊧ ⊥ ∧ δ(r) = δ(C \ {c₂})]
     * We also know that ¬r ∈ π and therefore δ(r) = δ(C \ {c₂}) or c₂ ∈ π ∧ δ(c₂) ≤ δ(r)
     * ALL: ¬r ∈ π ∧ [[c₂ ∈ π ∧ δ(c₂) ≤ δ(r)] ∨ [C \ {c₂}, π ⊧ ⊥ ∧ δ(r) = δ(C \ {c₂})]]
     * NCB: c₂ ∉ π                   ∧ b ∉ π
     * WCB: c₂ ∉ π                   ∧ [b ∉ π ∨ δ(b) > δ(c₁)]
     * SCB: [c₂ ∉ π ∨ δ(c₂) > δ(c₁)] ∧ [b ∉ π ∨ δ(b) > δ(c₁)]
     */
    ASSERT(lit_false(*replacement));
    if (replacement != lits + 1) {
      /**
       * If r ≠ c₁, we know that we are in WCB since in NCB we have
       *    [C \ {c₂}, π ⊧ ⊥] ⇒ δ(c₁) = δ(c₂) = δ(C) and r = c₁
       * We know that δ(r) > δ(c₁)
       * We swap the literals such that c₁ ← r
      */
      ASSERT(_options.chronological_backtracking);
      // In strong chronological backtracking, we need to swap the literals such that the highest falsified literal is at the second position. In weak chronological backtracking, it is not necessary, but it is still useful to determine the level of the conflict or the implication.
      // swap the literals
      lits[1] = *replacement;
      *replacement = lit;
#if NOTIFY_WATCH_CHANGES
      NOTIFY_OBSERVER(_observer, new napsat::gui::unwatch(cl, lit));
#endif
      // remove the clause from the watch list
      // bring the last watched clause to the current position
      *i = *(end - 1);
      end--;
      // watch new literal
      watch_lit(lits[1], cl);
    }
    else {
      // Increment for the next iteration
      // We cannot use *i to refer to the clause from this point onwards since it it ready for the next iteration
      i++;
    }

    /**
     * We no longer need r since it is now in place of c₁
     * ALL: ¬c₁ ∈ π ∧ [[c₂ ∈ π ∧ δ(c₂) ≤ δ(c₁)] ∨ [C \ {c₂}, π ⊧ ⊥ ∧ δ(c₁) = δ(C \ {c₂})]]
     * NCB: c₂ ∉ π                   ∧ b ∉ π
     * WCB: c₂ ∉ π                   ∧ [b ∉ π ∨ δ(b) > δ(c₁)]
     * SCB: [c₂ ∉ π ∨ δ(c₂) > δ(c₁)] ∧ [b ∉ π ∨ δ(b) > δ(c₁)]
     */

    // We know that all literals in clause[1:end] are false
    /** CONFLICT **/
    if (lit_false(lit2)) {
      // Conflict
      /**
       * We know that C \ {c₂}, π ⊧ ⊥, therefore, if ¬c₂ ∈ π then C, π ⊧ ⊥ and we have a conflict
       * We cannot safely add ¬c₁ to the trail.
       * ALL: ¬c₁ ∈ (τ ⋅ ¬c₁) ∧ C \ {c₂}, π ⊧ ⊥ ∧ δ(c₁) = δ(C \ {c₂})
       * NCB: b ∉ π
       * WCB: [b ∉ π ∨ δ(b) > δ(c₁)]
       * SCB: [b ∉ π ∨ δ(b) > δ(c₁)]
      */
      ASSERT(lit_level(lits[1]) == replacement_lvl);
      if (lit_level(lit2) < replacement_lvl) {
        // swap the literals
        // we want the highest literal to be at the first position
        lits[1] ^= lits[0];
        lits[0] ^= lits[1];
        lits[1] ^= lits[0];
        // also swap the next watched clause
      }
      ASSERT_MSG(lit_level(lits[0]) >= lit_level(lits[1]),
        "Conflict: " + clause_to_string(cl) + "\nLiteral: " + lit_to_string(lit));
      watch_list.resize(end - watch_list.data());
      ASSERT(watch_lists_complete());
      ASSERT(watch_lists_minimal());
      return cl;
    }

    /** UNIT CLAUSE **/
    if (lit_undef(lit2)) {
      // unit clause
      /**
       * We add the information that c₂ ∉ π
       * We know that C \ {c₂}, π ⊧ ⊥, therefore the only way to satisfy C is to set c₂ to true
       * In SCB we additionally need δ(c₂) ≤ δ(c₁), and since c₂ will be implied at level δ(C \ {c₂}),
       * we need to ensure that δ(c₁) = δ(C \ {c₂}). This is why we changed the watched literals earlier.
       * ALL: c₂ ∉ π ∧ ¬c₁ ∈ π ∧ C \ {c₂}, π ⊧ ⊥ ∧ δ(c₁) = δ(C \ {c₂})
       * NCB: b ∉ π
       * WCB: [b ∉ π ∨ δ(b) > δ(c₁)]
       * SCB: [b ∉ π ∨ δ(b) > δ(c₁)]
      */
      imply_literal(lit2, cl);
      // don't increment. We would have done so earlier if we did not change the watched literals
      continue;
    }

    /** MISSED LOWER IMPLICATION **/
    /**
     * We know that we can only be in SCB since the skip condition
     * We know that c₂ ∈ π is now satisfied
     * ALL: c₂ ∈ π ∧ ¬c₁ ∈ π ∧ [δ(c₂) ≤ δ(c₁) ∨ [C \ {c₂}, π ⊧ ⊥ ∧ δ(c₁) = δ(C \ {c₂})]]
     * NCB: c₂ ∉ π                   ∧ b ∉ π                   => not possible
     * WCB: c₂ ∉ π                   ∧ [b ∉ π ∨ δ(b) > δ(c₁)]  => not possible
     * SCB: [b ∉ π ∨ δ(b) > δ(c₁)]
     * We shall now only consider SCB
    */
    if (lit_level(lit2) <= replacement_lvl) {
      // This is not a real missed lower implication. The level of the satisfied literal is lower than or equal to the level of the replacement.
      /**
       * We have δ(c₂) ≤ δ(c₁) as well as c₂ ∈ π ∧ ¬c₁ ∈ π
       * Which satisfies the invariant:
       * ¬c₁ ∈ (τ ⋅ ℓ) ⇒ [c₂ ∈ π ∧ [δ(c₂) ≤ δ(c₁) ∨ δ(λ(c₂) \ {c₂}) ≤ δ(c₁)]
       *               ∨ [b ∈ π ∧ δ(b) ≤ δ(c₁)]
       * and we do not need to do anything more
       */
      // don't increment. We would have done so earlier if we did not change the watched literals
      continue;
    }
    // We know that lit2 is true, and it is a missed lower implication
    // lit2 is the only satisfied literal in the clause and all other literals are propagated at a level lower than the highest falsified literal
    /**
     * We now also know that δ(c₂) > δ(c₁), so we can simplify our knowledge as
     * c₂ ∈ π ∧ ¬c₁ ∈ π ∧ C \ {c₂}, π ⊧ ⊥ ∧ δ(c₁) = δ(C \ {c₂})
     */
    ASSERT(_options.lazy_strong_chronological_backtracking);
    ASSERT(lit_true(lit2));
    /**
     * If δ(λ(c₂) \ {c₂}) > δ(c₁), then reimply c₂ at the level of c₁
     * The only way to satisfy the invariant is to add C to the lazy reimplication list of c₂
     * λ(c₂) ← λ[c₂ ← C] and therefore δ(λ(c₂) \ {c₂}) =  δ(c₁)
     *
     * Otherwise do nothing
     *
     * Note that reimply literal ensures δ(λ(c₂) \ {c₂}) > δ(c₁) ∧  δ(ℓ) > δ(c₁) before reimplication
     */
    reimply_literal(lit2, cl);
    /**
     * We now have in addition that δ(λ(c₂) \ {c₂}) ≤ δ(c₁)
     * that satisfies
     * ¬c₁ ∈ (τ ⋅ ℓ) ⇒ [c₂ ∈ π ∧ [δ(c₂) ≤ δ(c₁) ∨ δ(λ(c₂) \ {c₂}) ≤ δ(c₁)]
     *               ∨ [b ∈ π ∧ δ(b) ≤ δ(c₁)]
     */
    // don't increment. We would have done so earlier if we did not change the watched literals
  }

  watch_list.resize(end - watch_list.data());
  ASSERT(watch_lists_complete());
  ASSERT(watch_lists_minimal());
  return CLAUSE_UNDEF;
}

void napsat::NapSAT::backtrack(Tlevel level)
{
  ASSERT(level <= solver_level());
  if (level == solver_level())
    return;
  NOTIFY_OBSERVER(_observer, new napsat::gui::backtracking_started(level));
  unsigned waiting_count = 0;

  unsigned restore_point = _decision_index[level];
  unsigned j = restore_point;

  ASSERT(_backtracked_variables.empty());

  for (unsigned i = restore_point; i < _trail.size(); i++) {
    Tlit lit = _trail[i];
    Tvar var = lit_to_var(lit);
    if (lit_level(lit) > level) {
      ASSERT(_options.lazy_strong_chronological_backtracking || lit_lazy_reason(lit) == CLAUSE_UNDEF);
      if (lit_lazy_level(lit) <= level) {
        // look if the literal can be reimplied at a lower level
        ASSERT(_options.lazy_strong_chronological_backtracking);
        Tclause lazy_reason = lit_lazy_reason(lit);
        ASSERT(lazy_reason != CLAUSE_UNDEF);
        ASSERT(_clauses[lazy_reason].lits[0] == lit);
        ASSERT(lit_true(_clauses[lazy_reason].lits[0]));
        _reimplication_backtrack_buffer.push_back(lazy_reason);
      }
#if USE_OBSERVER
      // When using the observer, we cannot unassign the variables from left to right because the observer
      // assumes that if we remove a propagated literal, it is at the end of the trail.
      if (_observer)
        _backtracked_variables.push_back(var);
      else
#endif
        var_unassign(var);
    }
    else {
      _trail[j] = lit;
      j++;
      waiting_count += _vars[var].waiting;
    }
  }
#if USE_OBSERVER
  if (_observer)
    while(!_backtracked_variables.empty()) {
      Tvar var = _backtracked_variables.back();
      _backtracked_variables.pop_back();
      var_unassign(var);
    }
#endif
  _trail.resize(j);
  _decision_index.resize(level);

  ASSERT_MSG(_options.chronological_backtracking || waiting_count == 0,
             "Waiting count: " + to_string(waiting_count) + "\nLevel: " + to_string(level) + "\nRestore point: " + to_string(restore_point));
  _propagated_literals = _trail.size() - waiting_count;
  ASSERT_MSG(_options.chronological_backtracking || _propagated_literals == restore_point,
    "Propagated literals: " + to_string(_propagated_literals) + "\nRestore point: " + to_string(restore_point));
  if (_options.restoring_strong_chronological_backtracking) {
    while (_propagated_literals > restore_point) {
      Tlit lit = _trail[_propagated_literals - 1];
      Tvar var = lit_to_var(lit);
      ASSERT_MSG(!_vars[var].waiting,
                  "Literal: " + lit_to_string(lit) + "\nLevel: " + to_string(lit_level(lit)));
      _vars[var].waiting = true;
      _propagated_literals--;
      NOTIFY_OBSERVER(_observer, new napsat::gui::remove_propagation(lit));
    }
  }
  if (_reimplication_backtrack_buffer.size() > 0) {
    ASSERT(_options.lazy_strong_chronological_backtracking);
    // adds the literals on the lazy reimplication buffer to the trail by order of increasing level
    // Sort the literals by increasing level. It is not necessary, but it probably is more effective
    // TODO evaluate the performance of this
    // The topological order will automatically be respected because the reimplied literals cannot depend on each other.
    sort(_reimplication_backtrack_buffer.begin(), _reimplication_backtrack_buffer.end(), [this](Tclause a, Tclause b)
      { return lit_level(_clauses[a].lits[1]) < lit_level(_clauses[b].lits[1]); });
    for (Tclause lazy_clause : _reimplication_backtrack_buffer) {
      Tlit reimpl_lit = _clauses[lazy_clause].lits[0];
      ASSERT(lit_undef(reimpl_lit));
      imply_literal(reimpl_lit, lazy_clause);
      NOTIFY_OBSERVER(_observer, new napsat::gui::stat("Lazy reimplication used"));
    }
    _reimplication_backtrack_buffer.clear();
  }
}

bool napsat::NapSAT::lit_is_required_in_learned_clause(Tlit lit)
{
  ASSERT(lit_false(lit));
  if (lit_reason(lit) == CLAUSE_UNDEF)
    return true;
  ASSERT(lit_reason(lit) < _clauses.size());
  TSclause& clause = _clauses[lit_reason(lit)];
  ASSERT_MSG(!clause.deleted,
    "Literal: " + lit_to_string(lit) + "\nClause: " + clause_to_string(lit_reason(lit)));
  for (unsigned i = 1; i < clause.size; i++)
    if (!lit_seen(clause.lits[i]))
      return true;
  return false;
}

void NapSAT::analyze_conflict(Tclause conflict)
{
  // ASSERT(watch_lists_minimal());
  ASSERT(conflict != CLAUSE_UNDEF);
  ASSERT(!_writing_clause);
  // Number of literals at level conflict_level marked in the trail
  unsigned count = 0;
  unsigned i = _trail.size() - 1;
  Tclause cl = conflict;

  if (_proof)
    _proof->start_resolution_chain();

  bump_clause_activity(conflict);
  _next_literal_index = 0;

  TSclause clause = _clauses[conflict];
  Tlevel conflict_level = lit_level(clause.lits[0]);
  Tlevel second_highest_level = LEVEL_ROOT;

  // This does nothing in non-chronological backtracking
  ASSERT(_options.chronological_backtracking || conflict_level == solver_level());
  backtrack(conflict_level);

  // Variable used to determine the first literal from the clause that should be added to the learned clause
  // This is used to avoid adding the satisfied literal of the reason to the learned clause
  unsigned not_first_round = 0;

  Tlit pivot = LIT_UNDEF;
  do {
    ASSERT(cl != CLAUSE_UNDEF);
    if (_proof)
      _proof->link_resolution(pivot, cl);

    clause = _clauses[cl];
    // Be careful that the first time, we start at index 0, then we start at index 1
    for (unsigned j = not_first_round; j < clause.size; j++) {
      Tlit lit = clause.lits[j];
      ASSERT_MSG(lit_false(lit),
        "Reason: " + clause_to_string(cl));
      bump_var_activity(lit_to_var(lit));
      if (lit_seen(lit))
        continue;
      if (lit_level(lit) == conflict_level) {
        lit_mark_seen(lit);
        count++;
      }
      else if (lit_is_required_in_learned_clause(lit)) {
        ASSERT(lit_level(lit) < conflict_level);
        lit_mark_seen(lit);
        _literal_buffer[_next_literal_index++] = lit;
        second_highest_level = max(second_highest_level, lit_level(lit));
      }
      else if (_proof)
        _proof->link_resolution(lit, lit_reason(lit));
    }

    while (!lit_seen(_trail[i]) || lit_level(_trail[i]) != conflict_level) {
      ASSERT(i > 0);
      i--;
    }
    ASSERT(count > 0);
    pivot = lit_neg(_trail[i]);
    count--;
    lit_unmark_seen(pivot);
    cl = lit_reason(pivot);
    if (lit_lazy_reason(pivot) != CLAUSE_UNDEF)
      cl = lit_lazy_reason(pivot);

    // This is no longer the first round, and we should ignore the first literal of the clause
    not_first_round = 1;

    /*************************************************************************/
    /*                         LAZY RE-IMPLICATION                           */
    /*************************************************************************/
    if (count == 0 && lit_lazy_reason(pivot) != CLAUSE_UNDEF) {
      ASSERT(_options.lazy_strong_chronological_backtracking);
      ASSERT(lit_lazy_reason(pivot) == cl);
      ASSERT(lit_neg(pivot) == _clauses[cl].lits[0]);
      NOTIFY_OBSERVER(_observer, new napsat::gui::stat("Lazy reimplication used"));

      // make sure the conflict level cannot be increased by the content of the new clause
      for (unsigned j = 1; j < _clauses[cl].size; j++)
        second_highest_level = max(second_highest_level, lit_level(_clauses[cl].lits[j]));

      conflict_level = second_highest_level;
      // This is used to reimply the literals at the right level.
      // Otherwise we need to make sure that the levels used are the lazy ones, and this is more complicated.
      backtrack(conflict_level);
      second_highest_level = LEVEL_ROOT;

      // reset the conflict at a lower level
      // update the count since we have updated the level of the conflict
      // we need to do this now to check if we need to continue the analysis. It
      // is possible that after this, there still is one literal at the highest level,
      // therefore we can stop conflict analysis.
      // Note that it is not possible to have twice a lazy reimplication on the same literal
      ASSERT(count == 0);
      for (unsigned j = 1; j < _clauses[cl].size; j++) {
        Tlit lit = _clauses[cl].lits[j];
        ASSERT_MSG(lit_false(lit),
          "Reason: " + clause_to_string(cl));
        ASSERT(lit != pivot);
        if (lit_seen(lit))
          continue;
        if (lit_level(lit) == conflict_level) {
          lit_mark_seen(lit);
          count++;
        }
      }

      // clean up the literals at conflict_level since only one should appear in the final clause
      unsigned k = 0;
      for (unsigned j = 0; j < _next_literal_index; j++) {
        Tlit lit = _literal_buffer[j];
        ASSERT(lit_false(lit));
        ASSERT(lit_level(lit) <= conflict_level);
        // we should not re-insert the pivot
        if (lit == pivot)
          continue;
        if (lit_level(lit) == conflict_level) {
          count++;
          lit_mark_seen(lit);
          continue;
        }
        _literal_buffer[k++] = lit;
        second_highest_level = max(second_highest_level, lit_level(lit));
      }
      _next_literal_index = k;

      // reset the search head since we changed the level
      i = _trail.size() - 1;

    }
    /*************************************************************************/
    /*                     END LAZY RE-IMPLICATION                           */
    /*************************************************************************/
  } while(count > 0);

  _literal_buffer[_next_literal_index++] = pivot;

  // remove the seen markers on literals in the clause
  // we need to unmark them first because of the proof
  // prove_root_literal_removal assumes that the literals are not marked
  for (unsigned j = 0; j < _next_literal_index; j++)
    lit_unmark_seen(_literal_buffer[j]);

  // purge literals falsified at level 0
  if (_proof)
    prove_root_literal_removal(_literal_buffer, _next_literal_index);

  unsigned k = 0;
  for (unsigned j = 0; j < _next_literal_index; j++) {
    Tlit lit = _literal_buffer[j];
    ASSERT(lit_false(lit));
    ASSERT_MSG(lit_level(lit) <= conflict_level,
               "Literal: " + lit_to_string(lit) + " at level: " + to_string(lit_level(lit)) + " conflict level: " + to_string(conflict_level) + " Clause: " + clause_to_string(conflict));
    if (lit_level(lit) == LEVEL_ROOT)
      continue;
    _literal_buffer[k++] = lit;
  }
  _next_literal_index = k;

  if (_next_literal_index == 0) {
    // empty clause
    _status = UNSAT;
    if (_proof)
      _proof->finalize_resolution(_clauses.size(), _literal_buffer, _next_literal_index);
    return;
  }

  // backtrack depending on the chronological backtracking strategy
  if (_options.chronological_backtracking)
    backtrack(conflict_level - 1);
  else {
    Tlevel second_highest_level = LEVEL_ROOT;
    for (unsigned j = 0; j < _next_literal_index - 1; j++) {
      Tlit lit = _literal_buffer[j];
      ASSERT(lit_false(lit));
      ASSERT(lit_level(lit) <= conflict_level);
      second_highest_level = max(second_highest_level, lit_level(lit));
    }
    backtrack(second_highest_level);
  }

  cl = internal_add_clause(_literal_buffer, _next_literal_index, true, false);
  ASSERT(cl != CLAUSE_UNDEF);
  if (_proof)
    _proof->finalize_resolution(cl, _literal_buffer, _next_literal_index);
}

void napsat::NapSAT::prove_root_literal_removal(Tlit* lits, unsigned size)
{
  ASSERT(_proof);
  // we assume that a resolution chain is already started
  unsigned count = 0;

  for (unsigned i = 0; i < size; i++) {
    ASSERT(lit_false(lits[i]));
    if (lit_level(lits[i]) != LEVEL_ROOT)
      continue;
    ASSERT(!lit_seen(lits[i]));
    lit_mark_seen(lits[i]);
    count++;
  }

  unsigned i = _trail.size() - 1;
  while (count != 0) {
    while (!lit_seen(_trail[i]))
      i--;
    Tlit lit = _trail[i];
    ASSERT(lit_level(lit) == LEVEL_ROOT);
    ASSERT(lit_reason(lit) != CLAUSE_UNDEF);
    Tclause reason = lit_reason(lit);

    _proof->link_resolution(lit_neg(lit), reason);

    for (unsigned j = 1; j < _clauses[reason].size; j++) {
      Tlit lit = _clauses[reason].lits[j];
      ASSERT(lit_false(lit));
      if (lit_seen(lit))
        continue;
      lit_mark_seen(lit);
      count++;
    }
    count--;
    lit_unmark_seen(lit);
  }
}

void NapSAT::repair_conflict(Tclause conflict)
{
  /**
   * Precondition:
   * - The conflict clause C is conflicting with the current partial assignment π
   *    C, π ⊧ ⊥
   * - The conflict clause is not the empty clause
   *    |C| > 0
   * - The first literal in the conflict clause is the highest level literal
   *    δ(c₁) = δ(C)
  */
  Tlit* lits = _clauses[conflict].lits;

  /********** CHECKING PRECONDITIONS **********/
  ASSERT(_clauses[conflict].size > 0);
  ASSERT_MSG(_options.chronological_backtracking || _clauses[conflict].external
  || (lit_level(lits[0]) == solver_level()
   && lit_level(lits[1]) == solver_level()),
    "Conflict: " + clause_to_string(conflict) + "\nDecision level: " + to_string(solver_level()));
#ifndef NDEBUG
  for (unsigned i = 0; i < _clauses[conflict].size; i++) {
    ASSERT(lit_false(lits[i]));
    ASSERT(lit_level(lits[i]) <= lit_level(lits[0]));
  }
#endif

  NOTIFY_OBSERVER(_observer, new napsat::gui::conflict(conflict));
  if (_status == SAT)
    _status = UNDEF;

  if (lit_level(lits[0]) == LEVEL_ROOT) {
    _status = UNSAT;
    // NOTIFY_OBSERVER(_observer, new napsat::gui::marker("Empty clause"));
    if (_proof) {
      _proof->start_resolution_chain();
      _proof->link_resolution(LIT_UNDEF, conflict);
      prove_root_literal_removal(lits, _clauses[conflict].size);
      _proof->finalize_resolution(_clauses.size(), nullptr, 0);
    }
    return;
  }
  /********** UNIT CLAUSE **********/
  if (_clauses[conflict].size == 1) {
    Tlevel backtrack_level = LEVEL_ROOT;
    if (_options.chronological_backtracking)
      backtrack_level = lit_level(lits[0]) - 1;
    backtrack(backtrack_level);
    // In strong chronological backtracking, the literal might have been implied again during reimplication
    // Therefore, we might need to trigger another conflict analysis
    ASSERT(_options.lazy_strong_chronological_backtracking || lit_undef(lits[0]));
    if (!lit_undef(lits[0])) {
      // the problem is unsat
      // The literal could have been propagated by the reimplication
      _status = UNSAT;
      return;
    }
    imply_literal(lits[0], conflict);

    return;
  }

  // Check wether there is a unique literal at the highest level. If that is the case, there is no need to trigger conflict analysis
  bool unique = true;
  for (unsigned i = 1; unique && i < _clauses[conflict].size; i++)
    unique = lit_level(lits[i]) != lit_level(lits[0]);

  /********** CLAUSES WITH ONE LITERAL AT MAX LEVEL **********/
  if (unique && lit_lazy_reason(lits[0]) == CLAUSE_UNDEF) {
    NOTIFY_OBSERVER(_observer, new napsat::gui::stat("One literal at highest level"));
    ASSERT(_options.chronological_backtracking || _clauses[conflict].external);

    Tlevel backtrack_level = lit_level(lits[1]);
    if (_options.chronological_backtracking)
      backtrack_level = lit_level(lits[0]) - 1;
#ifndef NDEBUG
    else {
      // In NCB, we need that the second highest level is at the level of C \ {c₁}
      //    δ(c₂) = δ(C \ {c₁})
      // Such that we can backtrack to the second highest level
      for (unsigned i = 2; i < _clauses[conflict].size; i++)
        ASSERT(lit_level(lits[i]) <= lit_level(lits[1]));
    }
#endif
    backtrack(backtrack_level);
    ASSERT(lit_undef(lits[0]));
#ifndef NDEBUG
    for (unsigned i = 1; i < _clauses[conflict].size; i++)
      ASSERT_MSG(lit_false(lits[i]),
        "Conflict: " + clause_to_string(conflict) + "\nLiteral: " + lit_to_string(lits[i]));
#endif

    if (_options.chronological_backtracking) {
      // In chronological backtracking, it might be the case that the second highest literal is not at the second position.
      // We need to ensure that it becomes the second watched literal
      Tlit* end = lits + _clauses[conflict].size;
      Tlit* high_lit = lits + 1;
      Tlevel high_lvl = lit_level(*high_lit);
      for (Tlit* i = lits + 2; i < end; i++) {
        if (lit_level(*i) > high_lvl) {
          high_lvl = lit_level(*i);
          high_lit = i;
        }
      }
      if (high_lit > lits + 1) {
        stop_watch(lits[1], conflict);
        Tlit tmp = lits[1];
        lits[1] = *high_lit;
        *high_lit = tmp;
        watch_lit(lits[1], conflict);
      }
    }
    imply_literal(lits[0], conflict);
    return;
  }

  analyze_conflict(conflict);

  _var_activity_increment /= _options.var_activity_decay;
}

void NapSAT::restart()
{
  _agility = 1;
  _options.agility_threshold *= _options.agility_threshold_decay;
  backtrack(LEVEL_ROOT);
  NOTIFY_OBSERVER(_observer, new napsat::gui::stat("Restart"));
}

void napsat::NapSAT::order_trail()
{
  ASSERT_MSG(false, "Not implemented");
}

void napsat::NapSAT::select_watched_literals(Tlit* lits, unsigned size)
{
  unsigned high_index = 0;
  unsigned second_index = 1;
  unsigned hight_utility = utility_heuristic(lits[0]);
  unsigned second_utility = utility_heuristic(lits[1]);
  if (hight_utility < second_utility) {
    high_index = 1;
    second_index = 0;
    unsigned tmp = hight_utility;
    hight_utility = second_utility;
    second_utility = tmp;
  }

  for (unsigned i = 2; i < size; i++) {
    if (lit_undef(lits[second_index]))
      break;
    unsigned lit_utility = utility_heuristic(lits[i]);
    if (lit_utility > hight_utility) {
      second_index = high_index;
      second_utility = hight_utility;
      high_index = i;
      hight_utility = lit_utility;
    }
    else if (lit_utility > second_utility) {
      second_index = i;
      second_utility = lit_utility;
    }
  }
  Tlit tmp = lits[0];
  lits[0] = lits[high_index];
  lits[high_index] = tmp;
  if (second_index == 0) {
    tmp = lits[1];
    lits[1] = lits[high_index];
    lits[high_index] = tmp;
    return;
  }
  tmp = lits[1];
  lits[1] = lits[second_index];
  lits[second_index] = tmp;
}

Tclause napsat::NapSAT::internal_add_clause(const Tlit* lits_input, unsigned input_size, bool learned, bool external)
{
  ASSERT(lits_input != nullptr);
  for (unsigned i = 0; i < input_size; i++)
    bump_var_activity(lit_to_var(lits_input[i]));
  Tlit* lits;
  Tclause cl;
  TSclause* clause;
  if (learned)
    _n_learned_clauses++;
  if (external)
    _next_clause_elimination++;

  // remove the literals falsified at level 0
  unsigned n_removed = 0;
  bool satisfied_at_root = false;
  for (unsigned i = 0; !satisfied_at_root && i < input_size; i++) {
    if (lit_level(lits_input[i]) == LEVEL_ROOT) {
      // The solver should not generate redundant literals and clauses
      ASSERT(external);
      satisfied_at_root = lit_true(lits_input[i]);
      n_removed++;
    }
  }
  // If the clause is satisfied at level 0, we do not need to add it
  // No need to justify it in the proof since clauses satisfied at level 0 and not propagating are not necessary for the proof
  // If it is satisfied already here, it means another clauses propagates the literal at level 0
  if (satisfied_at_root)
    return CLAUSE_UNDEF;

  unsigned clause_size = input_size - n_removed;

  if (_deleted_clauses.empty()) {
    lits = new Tlit[clause_size];
    TSclause added(lits, clause_size, learned, external);
    _clauses.push_back(added);
    _clauses_sizes.push_back(clause_size);
    _activities.push_back(_max_clause_activity);
    clause = &_clauses.back();
    cl = _clauses.size() - 1;
  }
  else {
    cl = _deleted_clauses.back();
    ASSERT(cl < _clauses.size());
    _deleted_clauses.pop_back();
    clause = &_clauses[cl];
    ASSERT(clause->deleted);
    ASSERT(!clause->watched);
    if (_clauses_sizes[cl] < clause_size) {
      delete[] clause->lits;
      clause->lits = new Tlit[clause_size];
      _clauses_sizes[cl] = clause_size;
    }
    // fill the end of the clause with LIT_UNDEF for printing purposes
    // Note that this is not necessary for the solver
    for (unsigned i = clause_size; i < _clauses_sizes[cl]; i++)
      clause->lits[i] = LIT_UNDEF;

    lits = clause->lits;
    *clause = TSclause(lits, clause_size, learned, external);
  }


  // copy the literals to the clause
  if (n_removed == 0)
    memcpy(lits, lits_input, input_size * sizeof(Tlit));
  else {
    // cannot use memcpy because we skip the literals falsified at level 0
    for (unsigned i = 0, j = 0; i < input_size; i++) {
      if (lit_level(lits_input[i]) == LEVEL_ROOT)
        continue;
      lits[j++] = lits_input[i];
    }
  }

  // Remove duplicate literals
  if (input_size > 1) {
    sort(lits, lits + clause_size);
    unsigned j = 1;
    for (unsigned i = 1; i < clause_size; i++) {
      if (lits[i] == lits[i - 1]) {
        ASSERT(external);
        continue;
      }
      lits[j++] = lits[i];
    }
    clause_size = min(j, clause_size);
    clause->size = clause_size;
  }

  if (_proof && external) {
    _proof->input_clause(cl, lits_input, input_size);
    // Remove the literals falsified at level 0 in the proof
    if (n_removed > 0)
      _proof->remove_root_literals(cl);
  }

  _activities[cl] = _max_clause_activity;
  #if USE_OBSERVER
  if (_observer) {
    vector<Tlit> lits_vector;
    for (unsigned i = 0; i < clause_size; i++)
      lits_vector.push_back(lits[i]);
    _observer->notify(new napsat::gui::new_clause(cl, lits_vector, learned, external));
  }
  #endif

  if (clause_size == 0) {
    clause->watched = false;
    _status = UNSAT;
    return cl;
  }

  if (clause_size == 1) {
    clause->watched = false;
    if (lit_undef(lits[0]))
      imply_literal(lits[0], cl);
    if (lit_true(lits[0])) {
      if (_options.lazy_strong_chronological_backtracking)
        reimply_literal(lits[0], cl);
      return cl;
    }
    if (lit_false(lits[0])) {
      if (_status == SAT)
        _status = UNDEF;
      repair_conflict(cl);
    }
    return cl;
  }
  else if (clause_size == 2) {
    NOTIFY_OBSERVER(_observer, new napsat::gui::stat("Binary clause added"));
    // clause->watched = false;
    _binary_clauses[lits[0]].push_back(make_pair(lits[1], cl));
    _binary_clauses[lits[1]].push_back(make_pair(lits[0], cl));
    NOTIFY_OBSERVER(_observer, new napsat::gui::watch(cl, lits[0]));
    NOTIFY_OBSERVER(_observer, new napsat::gui::watch(cl, lits[1]));
    if (lit_false(lits[0]) && !lit_false(lits[1])) {
      // swap the literals so that the false literal is at the second position
      lits[1] = lits[0] ^ lits[1];
      lits[0] = lits[0] ^ lits[1];
      lits[1] = lits[0] ^ lits[1];
      // no need to update the watch list
    }
    if (lit_false(lits[1])) {
      if (lit_undef(lits[0]))
        imply_literal(lits[0], cl);
      else if (lit_false(lits[0]))
        repair_conflict(cl);
      else if (_options.lazy_strong_chronological_backtracking) {
        ASSERT(lit_true(lits[0]));
        if (lit_lazy_reason(lits[0]) == CLAUSE_UNDEF || lit_level(_clauses[lit_lazy_reason(lits[0])].lits[1]) > lit_level(lits[0]))
          lit_set_lazy_reason(lits[1], cl);
      }
    }
  }
  else {
    select_watched_literals(lits, clause_size);
    watch_lit(lits[0], cl);
    watch_lit(lits[1], cl);
    if (lit_false(lits[0]))
      repair_conflict(cl);
    else if (lit_false(lits[1]) && lit_undef(lits[0]))
      imply_literal(lits[0], cl);
    else if (lit_false(lits[1]) && lit_true(lits[0]) && _options.lazy_strong_chronological_backtracking)
      reimply_literal(lits[0], cl);
  }
  if (_options.delete_clauses && _n_learned_clauses >= _next_clause_elimination)
    simplify_clause_set();
  return cl;
}

/*****************************************************************************/
/*                            Public interface                               */
/*****************************************************************************/

napsat::NapSAT::NapSAT(unsigned n_var, unsigned n_clauses, napsat::options& options) :
  _options(options)
{
  _vars = vector<TSvar>(n_var + 1);
  _trail = vector<Tlit>();
  _trail.reserve(n_var);
  _watch_lists.resize(2 * n_var + 2);

  for (Tvar var = 1; var <= n_var; var++) {
    NOTIFY_OBSERVER(_observer, new napsat::gui::new_variable(var));
    _variable_heap.insert(var, 0);
  }

  _clauses = vector<TSclause>();
  _clauses.reserve(n_clauses);
  _activities.reserve(n_clauses);

  _literal_buffer = new Tlit[n_var];
  _next_literal_index = 0;

#if USE_OBSERVER
  if (options.interactive || options.observing || options.check_invariants || options.print_stats) {
    _observer = new napsat::gui::observer(options);
    // make a functional object that will parse the command
    if (options.interactive) {
      std::function<bool(const std::string&)> command_parser = [this](const std::string& command) {
        return this->parse_command(command);
        };
      _observer->set_command_parser(command_parser);
    }
  }
  else
    _observer = nullptr;
#else
  if (options.interactive || options.observing || options.check_invariants || options.print_stats) {
    LOG_WARNING("Observer not available in this build");
    if (options.interactive)
      LOG_WARNING("The option --interactive is not available in this build");
    if (options.observing)
      LOG_WARNING("The option --observing is not available in this build");
    if (options.check_invariants)
      LOG_WARNING("The option --check-invariants is not available in this build");
    if (options.print_stats)
      LOG_WARNING("The option --print-stats is not available in this build");
  }
#endif

  if (options.build_proof)
    _proof = new napsat::proof::resolution_proof();
  else
    _proof = nullptr;
}

NapSAT::~NapSAT()
{
  for (unsigned i = 0; i < _clauses.size(); i++)
    delete[] _clauses[i].lits;
#if USE_OBSERVER
  if (_observer)
    delete _observer;
#endif
  if (_proof)
    delete _proof;
  delete[] _literal_buffer;
}


bool napsat::NapSAT::is_interactive() const
{
  return _options.interactive;
}

bool napsat::NapSAT::is_observing() const
{
#if USE_OBSERVER
  return _observer != nullptr;
#else
  return false;
#endif
}

napsat::gui::observer* napsat::NapSAT::get_observer() const
{
#if USE_OBSERVER
  return _observer;
#else
  return nullptr;
#endif
}

bool NapSAT::propagate()
{
  ASSERT(watch_lists_complete());
  ASSERT(watch_lists_minimal());
  if (_status != UNDEF)
    return false;
  while (_propagated_literals < _trail.size()) {
    Tlit lit = _trail[_propagated_literals];
    Tclause conflict = propagate_binary_clauses(lit);
    if (conflict == CLAUSE_UNDEF)
      conflict = propagate_lit(lit);
    if (conflict == CLAUSE_UNDEF) {
      _vars[lit_to_var(lit)].waiting = false;
      _propagated_literals++;
      NOTIFY_OBSERVER(_observer, new napsat::gui::propagation(lit));
      continue;
    }
    NOTIFY_OBSERVER(_observer, new napsat::gui::conflict(conflict));
    repair_conflict(conflict);
    if (_status == UNSAT)
      return false;
    if (_agility < _options.agility_threshold)
      restart();
  }
  if (_trail.size() == _vars.size() - 1) {
    _status = SAT;
    return false;
  }
  return true;
}

status NapSAT::solve()
{
  if (_status != UNDEF)
    return _status;
  while (true) {
    NOTIFY_OBSERVER(_observer, new napsat::gui::check_invariants());
    if (!propagate()) {
      if (_status == UNSAT || !_options.interactive)
        break;
      NOTIFY_OBSERVER(_observer, new napsat::gui::done(_status == SAT));
    }
    NOTIFY_OBSERVER(_observer, new napsat::gui::check_invariants());
    if (_purge_counter >= _purge_threshold
    && ((!_options.weak_chronological_backtracking && !_options.restoring_strong_chronological_backtracking)
       || solver_level() == LEVEL_ROOT)) {
      // in weak and restoring chronological backtracking, not all literals are fully propagated
      // the can be a challenge if the solver is not at level 0
      purge_clauses();
      _purge_counter = 0;
      if (_status == UNSAT)
        return _status;
      // in chronological backtracking, the purge might have implied some literals
      // therefore we cannot take a decision before we propagate
      continue;
    }
    NOTIFY_OBSERVER(_observer, new napsat::gui::check_invariants());
#if USE_OBSERVER
    if (_observer && _options.interactive)
      _observer->notify(new napsat::gui::checkpoint());
    else
#endif
      decide();
    if (_status == SAT || _status == UNSAT)
      break;
  }
  NOTIFY_OBSERVER(_observer, new napsat::gui::check_invariants());
  NOTIFY_OBSERVER(_observer, new napsat::gui::done(_status == SAT));
  return _status;
}

status NapSAT::get_status()
{
  return _status;
}

bool NapSAT::decide()
{
  while (!_variable_heap.empty() && !var_undef(_variable_heap.top()))
    _variable_heap.pop();
  if (_variable_heap.empty()) {
    _status = SAT;
    return false;
  }
  Tvar var = _variable_heap.top();
  Tlit lit = literal(var, _vars[var].phase_cache);
  imply_literal(lit, CLAUSE_UNDEF);
  return true;
}

bool napsat::NapSAT::decide(Tlit lit)
{
  ASSERT(lit_undef(lit));
  imply_literal(lit, CLAUSE_UNDEF);
  return true;
}

void NapSAT::start_clause()
{
  ASSERT(!_writing_clause);
  _writing_clause = true;
  _next_literal_index = 0;
}

void NapSAT::add_literal(Tlit lit)
{
  ASSERT(_writing_clause);
  Tvar var = lit_to_var(lit);
  var_allocate(var);
  ASSERT(_next_literal_index < _vars.size());
  _literal_buffer[_next_literal_index++] = lit;
}

napsat::Tclause NapSAT::finalize_clause()
{
  ASSERT(_writing_clause);
  _writing_clause = false;
  Tclause cl = internal_add_clause(_literal_buffer, _next_literal_index, false, true);
  return cl;
}

napsat::Tclause napsat::NapSAT::add_clause(const Tlit* lits, unsigned size)
{
  Tvar max_var = 0;
  for (unsigned i = 0; i < size; i++)
    if (lit_to_var(lits[i]) > max_var)
      max_var = lit_to_var(lits[i]);
  var_allocate(max_var);
  Tclause cl = internal_add_clause(lits, size, false, true);
  return cl;
}

const Tlit* napsat::NapSAT::get_clause(Tclause cl) const
{
  assert(cl < _clauses.size());
  return _clauses[cl].lits;
}

unsigned napsat::NapSAT::get_clause_size(Tclause cl) const
{
  assert(cl < _clauses.size());
  return _clauses[cl].size;
}

void NapSAT::hint(Tlit lit)
{
  ASSERT(lit_to_var(lit) < _vars.size());
  ASSERT(!_writing_clause);
  ASSERT(lit_undef(lit));
  imply_literal(lit, CLAUSE_LAZY);
}

void NapSAT::hint(Tlit lit, unsigned int level)
{
  ASSERT(lit_to_var(lit) < _vars.size());
  ASSERT(!_writing_clause);
  ASSERT(lit_undef(lit));
  ASSERT(level <= solver_level() + 1);
  hint(lit);
  _vars[lit_to_var(lit)].level = level;
}

void NapSAT::synchronize()
{
  _number_of_valid_literals = _trail.size();
  for (Tvar var : _touched_variables)
    _vars[var].state_last_sync = _vars[var].state;

  _touched_variables.clear();
}

unsigned NapSAT::sync_validity_limit()
{
  return _number_of_valid_literals;
}

unsigned NapSAT::sync_color(Tvar var)
{
  ASSERT(var < _vars.size() && var > 0);
  if (_vars[var].state == _vars[var].state_last_sync)
    return 0;
  if (VAR_UNDEF == _vars[var].state)
    return 1;
  if (VAR_UNDEF == _vars[var].state_last_sync)
    return 2;
  return 3;
}

void NapSAT::set_markup(void (*markup_function)(void))
{
  ASSERT(markup_function);
}

const std::vector<Tlit>& NapSAT::trail() const
{
  return _trail;
}

Tlevel NapSAT::decision_level() const
{
  return _decision_index.size();
}

void napsat::NapSAT::print_proof()
{
  ASSERT(_proof);
  ASSERT(_status == UNSAT)
  _proof->print_proof();
}

bool napsat::NapSAT::check_proof()
{
  ASSERT(_proof);
  ASSERT(_status == UNSAT)
  return _proof->check_proof();
}
