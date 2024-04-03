/**
 * @file src/solver/NapSAT.cpp
 * @author Robin Coutelier
 *
 * @brief This file is part of the NapSAT solver. It implements the core functions of the
 * solver such as the CDCL loop, BCP, conflict analysis, backtacking and clause set simplification.
 */
#include "NapSAT.hpp"
#include "custom-assert.hpp"

#include <iostream>
#include <cstring>
#include <functional>

using namespace sat;
using namespace std;

void NapSAT::stack_lit(Tlit lit, Tclause reason)
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
   *    δ(ℓ) = δ(C ∖ {ℓ})
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
    svar.level = _decision_index.size();
    NOTIFY_OBSERVER(_observer, new sat::gui::decision(lit));
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
    NOTIFY_OBSERVER(_observer, new sat::gui::implication(lit, reason, svar.level));
  }
  if (lit_pol(lit) != svar.phase_cache)
    _agility += 1 - _options.agility_decay;
  svar.phase_cache = lit_pol(lit);

  if (svar.level == LEVEL_ROOT)
    _purge_counter++;
  ASSERT(svar.level != LEVEL_UNDEF);
  ASSERT(svar.level <= _decision_index.size());
}

Tlit* sat::NapSAT::search_replacement(Tlit* lits, unsigned size)
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

Tclause sat::NapSAT::propagate_binary_clauses(Tlit lit)
{
  lit = lit_neg(lit);
  ASSERT(lit_false(lit));

  for (pair<Tlit, Tclause> bin : _binary_clauses[lit]) {
    ASSERT_MSG(_clauses[bin.second].size == 2, "Clause: " + clause_to_string(bin.second) + ",Literal: " + lit_to_string(lit));
    if (lit_true(bin.first)) {
      if (_options.strong_chronological_backtracking && lit_level(bin.first) > lit_level(lit)) {
        // missed lower implication
        Tclause lazy_reason = lit_lazy_reason(bin.first);
        if (lazy_reason == CLAUSE_UNDEF || lit_level(_clauses[lazy_reason].lits[1]) > lit_level(bin.first)) {
          // reorder the literals such that the satisfied literal is at the first position
          Tlit* lits = _clauses[bin.second].lits;
          if (lits[0] != bin.first) {
            lits[0] = lits[0] ^ lits[1];
            lits[1] = lits[0] ^ lits[1];
            lits[0] = lits[0] ^ lits[1];
            // no need to update the watch lists because the clause is binary
          }
          lit_set_lazy_reason(bin.first, bin.second);
        }
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
      stack_lit(bin.first, bin.second);
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
  /**
   * The mathematical notations and the contract of this function are defined in NapSAT.hpp
  */
  lit = lit_neg(lit);
  ASSERT(lit_false(lit));

  // level of the propagation
  Tlevel lvl = lit_level(lit);
  Tclause cl = _watch_lists[lit];
  Tclause prev = CLAUSE_UNDEF;
  Tclause next = CLAUSE_UNDEF;

  /**
   * Let F* be a set of clauses such that each clause in the set satisfies
   * - NCB:  ¬c₁ ∈ (τ ⋅ ℓ) ⇒ c₂ ∈ π ∨ b ∈ π
   *        δ(b) ≤ δ(c₂) is trivially true in NCB since the levels are monotonicaly increasing
   * - WCB: ¬c₁ ∈ (τ ⋅ ℓ) ⇒ ¬c₂ ∉ (τ ⋅ ℓ) ∨ [b ∈ π ∧ δ(b) ≤ δ(c₁)]
   *        We actually want to enforce ¬c₁ ∈ τ ⇒ c₂ ∈ π ∨ [b ∈ π ∧ δ(b) ≤ δ(c₁)] but cannot
   *        guarantee that this will hold after backtracking.
   * - LSCB: ¬c₁ ∈ (τ ⋅ ℓ) ⇒ [c₂ ∈ π ∧ [δ(c₂) ≤ δ(c₁) ∨ δ(λ(c₂) ∖ {c₂}) ≤ δ(c₁)]
   *                     ∨ [b ∈ π ∧ δ(b) ≤ δ(c₁)]
   *
   * We initialise F* with all the clauses that are not watched by ¬ℓ. If they satisfied the invarian
   * ts
   * before the propagation, they will satisfy them after ℓ is added to τ without any action.
   *
   * For the clauses watched by ¬ℓ, C = c₁ ∨ c₂ ∨ ... ∨ cₙ with b, a blocker such that b ∈ C
   * we will reason over the Haore triplets {P} loop body {Q}
   * where P is the precondition that the invariant holds before the loop
   * and Q is the postcondition that the invariant holds after the loop if ℓ = ¬c₁ is added to τ
   * - NCB:  ¬c₁ ∈ (τ ⋅ ℓ) ⇒ c₂ ∈ π ∨ b ∈ π
   *        δ(b) ≤ δ(c₂) is trivially true in NCB since the levels are monotonicaly increasing
   * - WCB: ¬c₁ ∈ (τ ⋅ ℓ) ⇒ ¬c₂ ∉ (τ ⋅ ℓ) ∨ [b ∈ π ∧ δ(b) ≤ δ(c₁)]
   *        We actually want to enforce ¬c₁ ∈ τ ⇒ c₂ ∈ π ∨ [b ∈ π ∧ δ(b) ≤ δ(c₁)] but cannot
   *        guarantee that this will hold after backtracking.
   * - LSCB: ¬c₁ ∈ (τ ⋅ ℓ) ⇒ [c₂ ∈ π ∧ [δ(c₂) ≤ δ(c₁) ∨ δ(λ(c₂) ∖ {c₂}) ≤ δ(c₁)]
   *                       ∨ [b ∈ π ∧ δ(b) ≤ δ(c₁)]
   *
   * If Q is true, then we can add C to F* and we know that the invariants are preserved.
   * If we cannot make Q true, then there is a conflict and we can return C.
   *
   * If we have not returned a conflict, at then end of the loop, we will have explored
   * all the clauses watched by ¬ℓ and F* = F. Therefore, we satisfy our contract.
   */
  while (cl != CLAUSE_UNDEF) {
    ASSERT(cl != prev);
    TSclause& clause = _clauses[cl];
    ASSERT(clause.watched);
    ASSERT(clause.size >= 2);
    Tlit* lits = clause.lits;
    /**
     * we call c₁ and c₂ the watched literals of the clause
     * we assume that c₁ = ¬ℓ
     */
    //check that c₁ = C[0] or c₁ = C[1]
    ASSERT_MSG(lit == lits[0] || lit == lits[1],
      "Clause: " + clause_to_string(cl) + ",Literal: " + lit_to_string(lit));
    // check that the next clause in the watch list contains the right literal.
    ASSERT_MSG(clause.first_watched == CLAUSE_UNDEF
      || _clauses[clause.first_watched].lits[0] == lits[0]
      || _clauses[clause.first_watched].lits[1] == lits[0],
      "Propagating lit: " + lit_to_string(lit) + " on clause " + clause_to_string(cl)
      + " clause " + clause_to_string(clause.first_watched) + " does not watch " + lit_to_string(lits[0]));
    ASSERT_MSG(clause.second_watched == CLAUSE_UNDEF
      || _clauses[clause.second_watched].lits[0] == lits[1]
      || _clauses[clause.second_watched].lits[1] == lits[1],
      "Propagating lit: " + lit_to_string(lit) + " on clause " + clause_to_string(cl)
      + " clause " + clause_to_string(clause.second_watched) + " does not watch " + lit_to_string(lits[1]));

    // ensure that c₁ = ¬ℓ and c₂ = the other watched literal
    if (lit == lits[0]) {
      lits[0] ^= lits[1];
      lits[1] ^= lits[0];
      lits[0] ^= lits[1];
      // also swap the next watched clause
      clause.first_watched ^= clause.second_watched;
      clause.second_watched ^= clause.first_watched;
      clause.first_watched ^= clause.second_watched;
    }
    ASSERT(prev == CLAUSE_UNDEF || _clauses[prev].lits[1] == lit);
    ASSERT(prev == CLAUSE_UNDEF || _clauses[prev].lits[1] == lit);

    Tlit lit2 = lits[0];
    ASSERT(lit == lits[1]);
    ASSERT(lit != lit2);

    /** SKIP CONDITIONS **/
    // TODO could put this before the clause dereferencing?
    if (lit_true(clause.blocker)
      && (!_options.chronological_backtracking || lit_level(clause.blocker) <= lvl)) {
      /**
       * NCB: b ∈ π
       * WCB: b ∈ π ∧ δ(b) ≤ δ(c₁)
       * SCB: b ∈ π ∧ δ(b) ≤ δ(c₁)
       * the invariants are preserved without any action
       * */
      prev = cl;
      cl = clause.second_watched;
      continue;
    }
    if (lit_true(lit2)
      && (!_options.strong_chronological_backtracking || lit_level(lit2) <= lvl)) {
      /**
       * NCB: c₂ ∈ π
       * WCB: c₂ ∈ π
       * SCB: c₂ ∈ π ∧ δ(c₂) ≤ δ(c₁)
       * the invariants are preserved without any action
       */
      prev = cl;
      cl = clause.second_watched;
      continue;
    }
    /** SEARCH REPLACEMENT **/
    Tlit* replacement = search_replacement(lits, clause.size);
    /**
     * Search replacement returns a literal r ∈ C \ {c₂} such that it either is a good replacement such that
     *   ¬r ∈ (τ ⋅ ¬c₁) ⇒ c₂ ∈ π ∧ δ(c₂) ≤ δ(r)
     * or C \ {c₂} is conflicting with π and r is the highest literal in C \ {c₂}
     *   C ∖ {c₂}, π ⊧ ⊥ ∧ δ(r) = δ(C ∖ {c₂})
     * NCB: If c₂ ∈ π, then we would have stopped at the skip conditions
     *      We know that [C ∖ {c₂}, π ⊧ ⊥] ⇒ δ(c₁) = δ(c₂) = δ(C) and c₁ will be returned
     *      if C \ {c₂} is conflicting
     *
     * We know that
     * ALL: [¬r ∈ (τ ⋅ ¬c₁) ⇒ c₂ ∈ π ∧ δ(c₂) ≤ δ(r)] ∨ [C ∖ {c₂}, π ⊧ ⊥ ∧ δ(r) = δ(C ∖ {c₂})]
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
       * NCB: We know that r ∈ π ⇒ δ(r) ≤ δ(c₁). Therefore after this codition is satisfied in NCB,
       *      we know that r ∉ π
       * ¬c₁ ∈ τ ⇒ c₂ ∈ π ∨ [b ∈ π ∧ δ(b) ≤ δ(c₁)] is satisfied if we set b = r
      */
      clause.blocker = *replacement;
#if NOTIFY_WATCH_CHANGES
      NOTIFY_OBSERVER(_observer, new sat::gui::block(cl, *replacement));
#endif
      prev = cl;
      cl = clause.second_watched;
      continue;
    }
    /**
     * We know that
     * ALL: [¬r ∈ (τ ⋅ ¬c₁) ⇒ c₂ ∈ π ∧ δ(c₂) ≤ δ(r)] ∨ [C ∖ {c₂}, π ⊧ ⊥ ∧ δ(r) = δ(C ∖ {c₂})]
     * NCB: r ∉ π                  ∧ c₂ ∉ π                   ∧ b ∉ π
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
      NOTIFY_OBSERVER(_observer, new sat::gui::unwatch(cl, lit));
#endif
      // remove the clause from the watch list
      // first store the next clause in the list
      next = clause.second_watched;
      if (prev == CLAUSE_UNDEF) {
        ASSERT(_watch_lists[lit] == cl);
        _watch_lists[lit] = next;
      }
      else {
        ASSERT(_clauses[prev].second_watched == cl);
        _clauses[prev].second_watched = next;
      }
      // watch new literal
      watch_lit(lits[1], cl);
      // we do not update prev because cl is removed from the watch list
      //   prev  ->  cl  ->  next
      //   prev  ->  next
      cl = next;
      continue;
    }

    /** NO GOOD REPLACEMENT **/
    /**
     * We know that [¬r ∈ (τ ⋅ ¬c₁) ⇒ c₂ ∈ π ∧ δ(c₂) ≤ δ(r)] ∨ [C ∖ {c₂}, π ⊧ ⊥ ∧ δ(r) = δ(C ∖ {c₂})]
     * We also know that ¬r ∈ π and therefore δ(r) = δ(C ∖ {c₂}) or c₂ ∈ π ∧ δ(c₂) ≤ δ(r)
     * ALL: ¬r ∈ π ∧ [[c₂ ∈ π ∧ δ(c₂) ≤ δ(r)] ∨ [C ∖ {c₂}, π ⊧ ⊥ ∧ δ(r) = δ(C ∖ {c₂})]]
     * NCB: c₂ ∉ π                   ∧ b ∉ π
     * WCB: c₂ ∉ π                   ∧ [b ∉ π ∨ δ(b) > δ(c₁)]
     * SCB: [c₂ ∉ π ∨ δ(c₂) > δ(c₁)] ∧ [b ∉ π ∨ δ(b) > δ(c₁)]
     */
    ASSERT(lit_false(*replacement));
    if (replacement != lits + 1) {
      /**
       * If r ≠ c₁, we know that we are in WCB since in NCB we have
       *    [C ∖ {c₂}, π ⊧ ⊥] ⇒ δ(c₁) = δ(c₂) = δ(C) and r = c₁
       * We know that δ(r) > δ(c₁)
       * We swap the literals such that c₁ ← r
      */
      ASSERT(_options.chronological_backtracking);
      // In strong chronological backtracking, we need to swap the literals such that the highest falsified literal is at the second position. In weak chronological backtracking, it is not necessary, but it is still useful to determine the level of the conflict or the implication.
      // swap the literals
      next = clause.second_watched;
      lits[1] = *replacement;
      *replacement = lit;
#if NOTIFY_WATCH_CHANGES
      NOTIFY_OBSERVER(_observer, new sat::gui::unwatch(cl, lit));
#endif
      // remove the clause from the watch list
      if (prev == CLAUSE_UNDEF) {
        ASSERT(_watch_lists[lit] == cl);
        _watch_lists[lit] = next;
      }
      else {
        ASSERT(_clauses[prev].second_watched == cl);
        _clauses[prev].second_watched = next;
      }
      // watch new literal
      watch_lit(lits[1], cl);
      // Since we remove the clause from the watch list, we do not update prev
      // prev -> cl -> next
      // prev -> next
    }
    else { // if (replacement != lits + 1)
      // Since the clause stayed in the watch list, we will need to update prev
      // prev -> cl -> next
      // cl -> next
      prev = cl;
      next = clause.second_watched;
    }
    /**
     * We no longer need r since it is now in place of c₁
     * ALL: ¬c₁ ∈ π ∧ [[c₂ ∈ π ∧ δ(c₂) ≤ δ(c₁)] ∨ [C ∖ {c₂}, π ⊧ ⊥ ∧ δ(c₁) = δ(C ∖ {c₂})]]
     * NCB: c₂ ∉ π                   ∧ b ∉ π
     * WCB: c₂ ∉ π                   ∧ [b ∉ π ∨ δ(b) > δ(c₁)]
     * SCB: [c₂ ∉ π ∨ δ(c₂) > δ(c₁)] ∧ [b ∉ π ∨ δ(b) > δ(c₁)]
     */

    // We know that all literals in clause[1:end] are false
    /** CONFLICT **/
    if (lit_false(lit2)) {
      // Conflict
      /**
       * We know that C ∖ {c₂}, π ⊧ ⊥, therefore, if ¬c₂ ∈ π then C, π ⊧ ⊥ and we have a conflict
       * We cannot safely add ¬c₁ to the trail.
       * ALL: ¬c₁ ∈ (τ ⋅ ¬c₁) ∧ C ∖ {c₂}, π ⊧ ⊥ ∧ δ(c₁) = δ(C ∖ {c₂})
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
        clause.first_watched ^= clause.second_watched;
        clause.second_watched ^= clause.first_watched;
        clause.first_watched ^= clause.second_watched;
      }
      ASSERT_MSG(lit_level(lits[0]) >= lit_level(lits[1]),
        "Conflict: " + clause_to_string(cl) + "\nLiteral: " + lit_to_string(lit));
      // NOTIFY_OBSERVER(_observer, new sat::gui::marker("Conflict detected " + clause_to_string(cl)));
      // ASSERT(watch_lists_complete());
      // ASSERT(watch_lists_minimal());
      return cl;
    }

    /** UNIT CLAUSE **/
    if (lit_undef(lit2)) {
      // unit clause
      /**
       * We add the information that c₂ ∉ π
       * We know that C ∖ {c₂}, π ⊧ ⊥, therefore the only way to satisfy C is to set c₂ to true
       * In SCB we additionnaly need δ(c₂) ≤ δ(c₁), and since c₂ will be implied at level δ(C ∖ {c₂}),
       * we need to ensure that δ(c₁) = δ(C ∖ {c₂}). This is why we changed the watched literals earlier.
       * ALL: c₂ ∉ π ∧ ¬c₁ ∈ π ∧ C ∖ {c₂}, π ⊧ ⊥ ∧ δ(c₁) = δ(C ∖ {c₂})
       * NCB: b ∉ π
       * WCB: [b ∉ π ∨ δ(b) > δ(c₁)]
       * SCB: [b ∉ π ∨ δ(b) > δ(c₁)]
      */
      stack_lit(lit2, cl);
      cl = next;
      continue;
    }

    /** MISSED LOWER IMPLICATION **/
    /**
     * We know that we can only be in SCB since the skip condition
     * We know that c₂ ∈ π is now satisfied
     * ALL: c₂ ∈ π ∧ ¬c₁ ∈ π ∧ [δ(c₂) ≤ δ(c₁) ∨ [C ∖ {c₂}, π ⊧ ⊥ ∧ δ(c₁) = δ(C ∖ {c₂})]]
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
       * ¬c₁ ∈ (τ ⋅ ℓ) ⇒ [c₂ ∈ π ∧ [δ(c₂) ≤ δ(c₁) ∨ δ(λ(c₂) ∖ {c₂}) ≤ δ(c₁)]
       *               ∨ [b ∈ π ∧ δ(b) ≤ δ(c₁)]
       * and we do not need to do anything more
       */
      cl = next;
      continue;
    }
    // We know that lit2 is true, and it is a missed lower implication
    // lit2 is the only satisfied literal in the clause and all other literals are propagated at a level lower than the highest falsified literal
    /**
     * We now also know that δ(c₂) > δ(c₁), so we can simplify our knowledge as
     * c₂ ∈ π ∧ ¬c₁ ∈ π ∧ C ∖ {c₂}, π ⊧ ⊥ ∧ δ(c₁) = δ(C ∖ {c₂})
     */
    ASSERT(_options.strong_chronological_backtracking);
    ASSERT(lit_true(lit2));
    Tclause lazy_reason = lit_lazy_reason(lit2);
    ASSERT(lazy_reason == CLAUSE_UNDEF || lazy_reason < _clauses.size());
    if (lazy_reason == CLAUSE_UNDEF || lit_level(_clauses[lazy_reason].lits[1]) > replacement_lvl) {
      /**
       * We have δ(λ(c₂) \ {c₂}) > δ(c₁)
       * The only way to satisfy the invariant is to add C to the lazy reimplication list of c₂
       * λ(c₂) ← λ[c₂ ← C] and therefore δ(λ(c₂) \ {c₂}) =  δ(c₁)
      */
      lit_set_lazy_reason(lit2, cl);
    }
    /**
     * We now have in addition that δ(λ(c₂) \ {c₂}) ≤ δ(c₁)
     * that safifies
     * ¬c₁ ∈ (τ ⋅ ℓ) ⇒ [c₂ ∈ π ∧ [δ(c₂) ≤ δ(c₁) ∨ δ(λ(c₂) ∖ {c₂}) ≤ δ(c₁)]
     *               ∨ [b ∈ π ∧ δ(b) ≤ δ(c₁)]
    */
    cl = next;
  }

  // ASSERT(watch_lists_complete());
  // ASSERT(watch_lists_minimal());
  return CLAUSE_UNDEF;
}

void NapSAT::NCB_backtrack(Tlevel level)
{
  ASSERT(!_options.chronological_backtracking);
  ASSERT(level <= _decision_index.size());
  if (level == _decision_index.size())
    return;
  while (_decision_index.size() > level) {
    Tlit lit = _trail.back();
    _trail.pop_back();
    if (lit_reason(lit) == CLAUSE_UNDEF) {
      ASSERT(lit_level(lit) == _decision_index.size());
      _decision_index.pop_back();
    }
    Tvar var = lit_to_var(lit);
    var_unassign(var);
  }
  _propagated_literals = _trail.size();
}

void sat::NapSAT::CB_backtrack(Tlevel level)
{
  // cout << "-----------------------------------" << endl;
  ASSERT(_options.chronological_backtracking);
  ASSERT(level <= _decision_index.size());
  ASSERT(_backtrack_buffer.empty());
  if (level == _decision_index.size())
    return;
  NOTIFY_OBSERVER(_observer, new sat::gui::backtracking_started(level));
  unsigned waiting_count = 0;

  unsigned restore_point = _decision_index[level];

  // print_trail();
  while (_decision_index.size() > level) {
    Tlit lit = _trail.back();
    _trail.pop_back();
    if (lit_reason(lit) == CLAUSE_UNDEF) {
      ASSERT(lit_level(lit) == _decision_index.size());
      _decision_index.pop_back();
    }
    if (lit_level(lit) > level) {
      ASSERT(_options.strong_chronological_backtracking || lit_lazy_reason(lit) == CLAUSE_UNDEF);
      if (_options.strong_chronological_backtracking) {
        // look if the literal can be reimplied at a lower level
        Tclause lazy_reason = lit_lazy_reason(lit);
        if (lazy_reason != CLAUSE_UNDEF && lit_level(_clauses[lazy_reason].lits[1]) <= level) {
          ASSERT(_clauses[lazy_reason].lits[0] == lit);
          ASSERT(lit_true(_clauses[lazy_reason].lits[0]));
          _reimplication_backtrack_buffer.push_back(lazy_reason);
        }
      }
      var_unassign(lit_to_var(lit));
      continue;
    }
    _backtrack_buffer.push_back(lit);
    waiting_count += _vars[lit_to_var(lit)].waiting;
  }
  // cout << _backtrack_buffer.size() << " literals to restore" << endl;
  // cout << "Waiting count: " << waiting_count << endl;
  while (!_backtrack_buffer.empty()) {
    Tlit lit = _backtrack_buffer.back();
    _backtrack_buffer.pop_back();
    _trail.push_back(lit);
  }
  _propagated_literals = _trail.size() - waiting_count;
  // print_trail();
  if (_options.restoring_chronological_backtracking
   && restore_point < _propagated_literals) {
    // cout << "_propagated_literals: " << _propagated_literals << ", restore_point: " << restore_point << endl;
    while (_propagated_literals > restore_point) {
      // print_trail();
      // cout << "Restoring " << lit_to_string(_trail[_propagated_literals-1]) << endl;
      Tlit lit = _trail[_propagated_literals - 1];
      Tvar var = lit_to_var(lit);
      ASSERT_MSG(!_vars[var].waiting,
                  "Literal: " + lit_to_string(lit) + "\nLevel: " + to_string(lit_level(lit)));
      _vars[var].waiting = true;
      _propagated_literals--;
      NOTIFY_OBSERVER(_observer, new sat::gui::remove_propagation(lit));
    }
    _propagated_literals = restore_point;
  }
  if (_options.strong_chronological_backtracking && _reimplication_backtrack_buffer.size() > 0) {
    // adds the literals on the lazy reimplication buffer to the trail by order of increasing level
    // Sort the literals by increasing level
    // The topological order will automatically be respected because the reimplied literals cannot depend on each other.
    // todo maybe we want a stable sort for level collapsing. In this case, the literals will actually depend on each other, and their relative order should be preserved.
    sort(_reimplication_backtrack_buffer.begin(), _reimplication_backtrack_buffer.end(), [this](Tclause a, Tclause b)
      { return lit_level(_clauses[a].lits[1]) < lit_level(_clauses[b].lits[1]); });
    for (Tclause lazy_clause : _reimplication_backtrack_buffer) {
      Tlit reimpl_lit = _clauses[lazy_clause].lits[0];
      ASSERT(lit_undef(reimpl_lit));
      stack_lit(reimpl_lit, lazy_clause);
      NOTIFY_OBSERVER(_observer, new sat::gui::stat("Lazy reimplication used"));
    }
    _reimplication_backtrack_buffer.clear();
  }
}

bool sat::NapSAT::lit_is_required_in_learned_clause(Tlit lit)
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

void NapSAT::analyze_conflict_reimply(Tclause conflict)
{
  // ASSERT(watch_lists_minimal());
  ASSERT(conflict != CLAUSE_UNDEF);
  ASSERT(!_writing_clause);
  unsigned count = 0;
  TSclause& clause = _clauses[conflict];
  bump_clause_activity(conflict);
  _next_literal_index = 0;
  Tlevel conflict_level = lit_level(clause.lits[0]);
  assert(conflict_level > LEVEL_ROOT);
  Tlevel second_highest_level = LEVEL_ROOT;
  // prepare the learn clause in the literal buffer
  for (unsigned j = 0; j < clause.size; j++) {
    Tlit lit = clause.lits[j];
    ASSERT(lit_false(lit));
    ASSERT(lit_level(lit) <= conflict_level);
    ASSERT(!lit_seen(lit));
    bump_var_activity(lit_to_var(lit));
    lit_mark_seen(lit);
    ASSERT(lit_false(lit));
    ASSERT(lit_level(lit) <= conflict_level);
    if (lit_level(lit) == conflict_level)
      count++;
    else {
      _literal_buffer[_next_literal_index++] = lit;
      second_highest_level = max(second_highest_level, lit_level(lit));
    }
  }

  ASSERT(count > 1 || lit_lazy_reason(clause.lits[0]) != CLAUSE_UNDEF);
  // replace the literals in the clause by their reason
  unsigned i = _trail.size() - 1;

  while (count > 0) {
    // search the next literal involved in the conflict (in topological order)
    while (!lit_seen(_trail[i]) || lit_level(_trail[i]) != conflict_level)
      i--;
    ASSERT(i < _trail.size());
    Tlit lit = _trail[i];
    /*************************************************************************/
    /*                         LAZY RE-IMPLICATION                           */
    /*************************************************************************/
    if (count == 1) {
      Tclause lazy_reason = lit_lazy_reason(lit);
      if (lazy_reason == CLAUSE_UNDEF || lit_level(_clauses[lazy_reason].lits[1]) > conflict_level) {
        _literal_buffer[_next_literal_index++] = lit_neg(_trail[i]);
        break;
      }
      NOTIFY_OBSERVER(_observer, new sat::gui::stat("Lazy reimplication used"));
      // The missed lower implication will be propagated again after backtracking.
      // So we anticipate and continue conflict analysis directly
      count = 0;
      for (unsigned j = 1; j < _clauses[lazy_reason].size; j++) {
        Tlit lazy_lit = _clauses[lazy_reason].lits[j];
        bump_var_activity(lit_to_var(lazy_lit));
        ASSERT(lit_false(lazy_lit));
        if (lit_seen(lazy_lit))
          continue;
        lit_mark_seen(lazy_lit);
        Tlevel curr_lit_level = lit_level(lazy_lit);
        ASSERT(curr_lit_level <= conflict_level);
        if (curr_lit_level == conflict_level)
          count++;
        else {
          second_highest_level = max(second_highest_level, curr_lit_level);
          _literal_buffer[_next_literal_index++] = lazy_lit;
        }
      }
      lit_unmark_seen(lit);
      // clean up the literals at level conflict_level
      conflict_level = second_highest_level;
      second_highest_level = LEVEL_ROOT;
      unsigned k = 0;
      for (unsigned j = 0; j < _next_literal_index; j++) {
        ASSERT(lit_false(_literal_buffer[j]));
        ASSERT(lit_level(_literal_buffer[j]) <= conflict_level);
        if (lit_level(_literal_buffer[j]) == conflict_level) {
          count++;
          continue;
        }
        _literal_buffer[k++] = _literal_buffer[j];
        second_highest_level = max(second_highest_level, lit_level(_literal_buffer[j]));
      }
      _next_literal_index = k;
      // reset the seach head since we changed the level
      i = _trail.size() - 1;
      continue;
    } // end if (count == 1)
    /*************************************************************************/
    /*                             STANDARD FUIP                             */
    /*************************************************************************/
    // process the literals at highest level
    Tclause reason = lit_lazy_reason(lit);
    if (reason == CLAUSE_UNDEF)
      reason = lit_reason(lit);
    else {
      NOTIFY_OBSERVER(_observer, new sat::gui::stat("Lazy reimplication used"));
    }
    ASSERT_MSG(reason != CLAUSE_UNDEF,
      "Conflict: " + clause_to_string(conflict) + "\nLiteral: " + lit_to_string(lit));
    TSclause& clause = _clauses[reason];
    ASSERT(lit_true(clause.lits[0]));
    // start at 1 because the first literal is the one that was propagated
    ASSERT_MSG(lit == clause.lits[0],
      "Conflict: " + clause_to_string(reason) + "\nLiteral: " + lit_to_string(lit));
    for (unsigned j = 1; j < clause.size; j++) {
      Tlit curr_lit = clause.lits[j];
      bump_var_activity(lit_to_var(curr_lit));
      if (lit_seen(curr_lit))
        continue;
      lit_mark_seen(curr_lit);
      Tlevel curr_lit_level = lit_level(curr_lit);
      ASSERT(curr_lit_level <= conflict_level);
      if (curr_lit_level == conflict_level)
        count++;
      else {
        second_highest_level = max(second_highest_level, curr_lit_level);
        _literal_buffer[_next_literal_index++] = curr_lit;
      }
    }
    // unmark the replaced literal
    lit_unmark_seen(lit);
    count--;
  } // end while (count > 0)

  // remove the seen markers and the literals that can be removed with subsumption resolution
  unsigned k = 0;
  for (unsigned j = 0; j < _next_literal_index; j++) {
    Tlit lit = _literal_buffer[j];
    ASSERT(lit_false(lit));
    ASSERT(lit_level(lit) <= conflict_level);
    // TODO check if removing them in the other direct is not better
    if (lit_is_required_in_learned_clause(lit))
      _literal_buffer[k++] = _literal_buffer[j];
    else
      lit_unmark_seen(_literal_buffer[j]);
  }
  _next_literal_index = k;
  for (unsigned j = 0; j < _next_literal_index; j++)
    lit_unmark_seen(_literal_buffer[j]);
  if (conflict_level != LEVEL_ROOT) {
    CB_backtrack(conflict_level - 1);
  }
  internal_add_clause(_literal_buffer, _next_literal_index, true, false);
}

void NapSAT::analyze_conflict(Tclause conflict)
{
  // ASSERT(watch_lists_minimal());
  ASSERT(conflict != CLAUSE_UNDEF);
  ASSERT(!_writing_clause);
  unsigned count = 0;
  unsigned i = _trail.size() - 1;
  TSclause& clause = _clauses[conflict];
  bump_clause_activity(conflict);
  _next_literal_index = 0;
  Tlevel conflict_level = lit_level(clause.lits[0]);
  Tlevel second_highest_level = LEVEL_ROOT;
  for (unsigned j = 0; j < clause.size; j++) {
    Tlit lit = clause.lits[j];
    ASSERT(lit_false(lit));
    bump_var_activity(lit_to_var(lit));
    if (lit_level(lit) == conflict_level) {
      lit_mark_seen(lit);
      count++;
    }
    else if (lit_is_required_in_learned_clause(lit)) {
      ASSERT(lit_level(lit) < conflict_level);
      ASSERT(lit_false(lit));
      lit_mark_seen(lit);
      _literal_buffer[_next_literal_index++] = lit;
      second_highest_level = max(second_highest_level, lit_level(lit));
    }
  }
  ASSERT(count > 1);
  while (count > 1) {
    while (!lit_seen(_trail[i]) || lit_level(_trail[i]) != conflict_level) {
      ASSERT(i > 0);
      i--;
    }
    ASSERT(count <= _trail.size());
    ASSERT(i < _trail.size());
    ASSERT(lit_seen(_trail[i]));
    ASSERT(lit_level(_trail[i]) == conflict_level);
    Tlit lit = _trail[i];
    lit_unmark_seen(lit);
    count--;
    Tclause reason = lit_reason(lit);
    bump_clause_activity(reason);
    ASSERT(reason != CLAUSE_UNDEF);
    TSclause& clause = _clauses[reason];

    ASSERT(lit_true(clause.lits[0]));
    // start at 1 because the first literal is the one that was propagated
    for (unsigned j = 1; j < clause.size; j++) {
      Tlit lit = clause.lits[j];
      ASSERT_MSG(lit_false(lit),
        "Conflict: " + clause_to_string(conflict));
      bump_var_activity(lit_to_var(lit));
      if (lit_seen(lit))
        continue;
      if (lit_level(lit) == conflict_level) {
        lit_mark_seen(lit);
        count++;
      }
      else if (lit_is_required_in_learned_clause(lit)) {
        ASSERT(lit_level(lit) < conflict_level);
        ASSERT(lit_false(lit));
        lit_mark_seen(lit);
        _literal_buffer[_next_literal_index++] = lit;
        second_highest_level = max(second_highest_level, lit_level(lit));
      }
    }
  }
  while (!lit_seen(_trail[i]) || lit_level(_trail[i]) != conflict_level) {
    ASSERT(i > 0);
    i--;
  }
  _literal_buffer[_next_literal_index++] = lit_neg(_trail[i]);
  // remove the seen markers on literals in the clause
  for (unsigned j = 0; j < _next_literal_index; j++)
    lit_unmark_seen(_literal_buffer[j]);
  // backtrack depending on the chronological backtracking strategy
  if (_options.chronological_backtracking) {
    CB_backtrack(conflict_level - 1);
  }
  else {
    Tlevel second_highest_level = LEVEL_ROOT;
    for (unsigned j = 0; j < _next_literal_index - 1; j++) {
      Tlit lit = _literal_buffer[j];
      ASSERT(lit_false(lit));
      ASSERT(lit_level(lit) <= conflict_level);
      second_highest_level = max(second_highest_level, lit_level(lit));
    }
    NCB_backtrack(second_highest_level);
  }
  internal_add_clause(_literal_buffer, _next_literal_index, true, false);
}

void NapSAT::repair_conflict(Tclause conflict)
{
  /**
   * Precondition:
   * - The conflict clause C is conflicting with the current partial assignment π
   *    C, π ⊧ ⊥
   * - The conflict clause is not a unit clause
   *    |C| > 0
   * - The first literal in the conflict clause is the highest level literal
   *    δ(c₁) = δ(C)
  */
  ASSERT(_clauses[conflict].size > 0);


  NOTIFY_OBSERVER(_observer, new sat::gui::conflict(conflict));
  if (_status == SAT)
    _status = UNDEF;
  ASSERT(lit_false(_clauses[conflict].lits[0]));
  if (lit_level(_clauses[conflict].lits[0]) == LEVEL_ROOT) {
    _status = UNSAT;
    return;
  }
  if (_clauses[conflict].size == 1) {
    if (_options.chronological_backtracking)
      CB_backtrack(lit_level(_clauses[conflict].lits[0]) - 1);
    else
      NCB_backtrack(LEVEL_ROOT);
    // In strong chronological backtracking, the literal might have been propagated again during reimplication
    // Therefore, we might need to trigger another conflict analysis
    ASSERT(_options.strong_chronological_backtracking || lit_undef(_clauses[conflict].lits[0]));
    if (!lit_undef(_clauses[conflict].lits[0])) {
      // the problem is unsat
      // The literal could have been propagated by the reimplication
      _status = UNSAT;
      return;
    }
    stack_lit(_clauses[conflict].lits[0], conflict);

    return;
  }
  ASSERT(_options.chronological_backtracking || lit_level(_clauses[conflict].lits[0]) == _decision_index.size());
  if (_options.chronological_backtracking) {
    unsigned n_literal_at_highest_level = 1;
    Tlevel high_lvl = lit_level(_clauses[conflict].lits[0]);
    for (unsigned i = 1; i < _clauses[conflict].size; i++) {
      Tlevel level = lit_level(_clauses[conflict].lits[i]);
      /**
       * The first literal in the conflict clause is the highest level literal
       *    δ(c₁) = δ(C) */
      ASSERT(level <= high_lvl);
      if (level == high_lvl) {
        n_literal_at_highest_level++;
        break;
      }
    }
    if (n_literal_at_highest_level == 1
     && lit_lazy_reason(_clauses[conflict].lits[0]) == CLAUSE_UNDEF) {
      NOTIFY_OBSERVER(_observer, new sat::gui::stat("One literal at highest level"));
      ASSERT(_options.chronological_backtracking);
      CB_backtrack(lit_level(_clauses[conflict].lits[0]) - 1);
      // In strong chronological backtracking, the literal might have been propagated again during reimplication
      // Therefore, we might need to trigger another conflict analysis
      ASSERT_MSG(_options.strong_chronological_backtracking || lit_undef(_clauses[conflict].lits[0]),
        "Conflict: " + clause_to_string(conflict) + "\nLiteral: " + lit_to_string(_clauses[conflict].lits[0]));
      Tlit* lits = _clauses[conflict].lits;
      Tlit* end = lits + _clauses[conflict].size;
      if (!lit_undef(_clauses[conflict].lits[0])) {
        Tlit* high_lit = lits;
        Tlevel high_lvl = lit_level(*high_lit);
        for (Tlit* i = lits + 1; i < end; i++) {
          if (lit_level(*i) > high_lvl) {
            ASSERT(lit_false(*i));
            high_lvl = lit_level(*i);
            high_lit = i;
          }
        }
        if (high_lit == lits + 1) {
          lits[0] ^= lits[1];
          lits[1] ^= lits[0];
          lits[0] ^= lits[1];
          // swap the first and second watch. We just swapped lits[0] and lits[1]
          _clauses[conflict].first_watched ^= _clauses[conflict].second_watched;
          _clauses[conflict].second_watched ^= _clauses[conflict].first_watched;
          _clauses[conflict].first_watched ^= _clauses[conflict].second_watched;
        }
        else if (high_lit > lits + 1) {
          stop_watch(*lits, conflict);
          Tlit tmp = *lits;
          *lits = *high_lit;
          *high_lit = tmp;
          watch_lit(*lits, conflict);
        }
        // the first literal might not be at the highest level anymore
        // therefore we want to bring the highest level literal to the front
        // and then we can restart conflict analysis
        // repair_conflict(conflict);
        return;
      }
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
      stack_lit(_clauses[conflict].lits[0], conflict);
      return;
    }
  }
  if (_options.strong_chronological_backtracking)
    analyze_conflict_reimply(conflict);
  else {
    analyze_conflict(conflict);
  }
  _var_activity_increment /= _options.var_activity_decay;
}

void NapSAT::restart()
{
  _agility = 1;
  _options.agility_threshold *= _options.agility_threshold_decay;
  if (_options.chronological_backtracking)
    CB_backtrack(LEVEL_ROOT);
  else
    NCB_backtrack(LEVEL_ROOT);
  NOTIFY_OBSERVER(_observer, new sat::gui::stat("Restart"));
}

void sat::NapSAT::purge_clauses()
{
  ASSERT(watch_lists_complete());
  ASSERT(watch_lists_minimal());
  // print_watch_lists();
  _purge_threshold = _purge_counter + _purge_inc;
  // We assume that all the literals are propagated
  ASSERT(_propagated_literals == _trail.size());

  if (_options.chronological_backtracking && !_options.strong_chronological_backtracking) {
    /** PURGE THE WATCH LISTS **/
    // in weak chronological backtracking, a missed lower implication can create a clause that has a watched literal falsified at level 0 while not being satisfied at level 0
    // Therefore we need to clean the watch lists
    for (unsigned i = 0; i < _propagated_literals; i++) {
      Tlit lit = _trail[i];
      if (lit_level(lit) != LEVEL_ROOT) {
        continue;
      }
      lit = lit_neg(lit);

      Tclause cl = _watch_lists[lit];
      // since we are purging the watch lists, we can set the watch head to CLAUSE_UNDEF
      _watch_lists[lit] = CLAUSE_UNDEF;
      while (cl != CLAUSE_UNDEF) {
        TSclause& clause = _clauses[cl];
        if (!clause.watched || clause.deleted) {
          cl = clause.second_watched;
          continue;
        }
        if (cl == lit_reason(clause.lits[0])) {
          clause.watched = false;
          cl = clause.second_watched;
          continue;
        }

        ASSERT_MSG(clause.size > 2,
          "Clause: " + clause_to_string(cl) + "\nLiteral: " + lit_to_string(lit));
        Tlit* lits = clause.lits;
        if (lit_true(clause.blocker) && lit_level(clause.blocker) == LEVEL_ROOT) {
          // delete the clause. repair_watch_lists will take care of the rest
          delete_clause(cl);
          cl = clause.second_watched;
          if (lit == lits[0])
            cl = clause.first_watched;
          continue;
        }

        if (lit == lits[0]) {
          // swap the two watched literals such that the second one is the one that is falsified at level 0
          lits[0] ^= lits[1];
          lits[1] ^= lits[0];
          lits[0] ^= lits[1];
          // also swap the next watched clause
          clause.first_watched ^= clause.second_watched;
          clause.second_watched ^= clause.first_watched;
          clause.first_watched ^= clause.second_watched;
        }
        ASSERT(lit == lits[1]);
        if (lit_true(lits[0]) && lit_level(lits[0]) == LEVEL_ROOT) {
          // delete the clause. repair_watch_lists will take care of the rest
          delete_clause(cl);
          cl = clause.second_watched;
          continue;
        }
        Tclause next = clause.second_watched;
        while (clause.size > 2) {
          if (lit_level(lits[1]) != LEVEL_ROOT || lit_true(lits[1]))
            break;
          // bring the last literal to the front and reduce the size of the clause
          lits[1] ^= lits[clause.size - 1];
          lits[clause.size - 1] ^= lits[1];
          lits[1] ^= lits[clause.size - 1];
          clause.size--;
        }

        if (clause.size > 2) {
          watch_lit(lits[1], cl);
          cl = next;
          continue;
        }
        if (clause.size == 2 && !lit_false(lits[1])) {
          clause.watched = false;
          _binary_clauses[lits[0]].push_back(make_pair(lits[1], cl));
          _binary_clauses[lits[1]].push_back(make_pair(lits[0], cl));
          cl = next;
          continue;
        }
        // The clause is propagating a literal at level 0
        // However, adding it to the stack may create a conflict, which we do not want to handle here.
        // The conflict will remain until it gets propagated later.
        // We add it to the stack and let the propagation handle it.
        if (lit_undef(lits[0])) {
          stack_lit(lits[0], cl);
          clause.watched = false;
          // we do not need to udpate the watch lists because the clause is satified at level 0
          // we just need to remove cl from the watch list of lits[1] because it is now binary
          // the repair_watch_lists will take care of the rest
        }
        else {
          // we need to push the clause in the binary watch list
          _binary_clauses[lits[0]].push_back(make_pair(lits[1], cl));
          _binary_clauses[lits[1]].push_back(make_pair(lits[0], cl));
        }
        cl = next;
      }
    }
  }

  for (Tclause cl = 0; cl < _clauses.size(); cl++) {
    // Do not remove clauses that are used as reasons
    TSclause& clause = _clauses[cl];
    if (cl == lit_reason(clause.lits[0]))
      continue;
    if (clause.deleted || !clause.watched || clause.size <= 2)
      continue;
    // Since all literals are propagated, if a clause has a watched literal falsified at level 0, then the other must be satisfied.
    // In strong chronological backtracking, the other watched literal must be satisfied at level 0 too.
    Tlit* lits = clause.lits;
    ASSERT_MSG(!(lit_false(lits[1]) && lit_level(lits[1]) == LEVEL_ROOT && !lit_waiting(lits[1]))
      || lit_true(lits[0]) || lit_true(clause.blocker),
      "Clause : " + clause_to_string(cl) + " Watched? " + to_string(clause.watched) + " Deleted? " + to_string(clause.deleted));
    if ((lit_true(lits[0]) && lit_level(lits[0]) == LEVEL_ROOT)
      || (lit_true(lits[1]) && lit_level(lits[1]) == LEVEL_ROOT)) {
      delete_clause(cl);
      continue;
    }

    Tlit* i = lits + 2;
    Tlit* end = lits + clause.size - 1;
    while (i <= end) {
      if (lit_level(*i) != LEVEL_ROOT) {
        i++;
        continue;
      }
      if (lit_false(*i)) {
        // remove the literal and push it to the back
        Tlit tmp = *i;
        *i = *end;
        *end = tmp;
        end--;
        continue;
      }
      else {
        ASSERT(lit_true(*i));
        delete_clause(cl);
        break;
      }
    }
    clause.size = end - lits + 1;
    if (clause.size == 2) {
      // clause.watched = false;
      _binary_clauses[lits[0]].push_back(make_pair(lits[1], cl));
      _binary_clauses[lits[1]].push_back(make_pair(lits[0], cl));
      NOTIFY_OBSERVER(_observer, new sat::gui::stat("Binary clause simplified"));
    }
  }
  // remove the deleted clauses
  repair_watch_lists();
  ASSERT(watch_lists_complete());
  ASSERT(watch_lists_minimal());
}

void sat::NapSAT::simplify_clause_set()
{
  _next_clause_elimination *= _options.clause_elimination_multiplier;
  _clause_activity_threshold *= _options.clause_activity_threshold_decay;
  double threshold = _max_clause_activity * _clause_activity_threshold;
  for (Tclause cl = 0; cl < _clauses.size(); cl++) {
    ASSERT(_clauses[cl].activity <= _max_clause_activity);
    ASSERT(_clauses[cl].size > 0);
    if (_clauses[cl].deleted || !_clauses[cl].watched || !_clauses[cl].learned)
      continue;
    if (_clauses[cl].size <= 2)
      continue;
    if (lit_reason(_clauses[cl].lits[0]) == cl)
      continue;
    if (_clauses[cl].activity < threshold) {
      delete_clause(cl);
      NOTIFY_OBSERVER(_observer, new sat::gui::stat("Clause deleted"));
    }
  }
  repair_watch_lists();
  ASSERT(watch_lists_complete());
  ASSERT(watch_lists_minimal());
  NOTIFY_OBSERVER(_observer, new sat::gui::stat("Clause set simplified"));
}

void sat::NapSAT::order_trail()
{}

void sat::NapSAT::select_watched_literals(Tlit* lits, unsigned size)
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

Tclause sat::NapSAT::internal_add_clause(const Tlit* lits_input, unsigned size, bool learned, bool external)
{
  for (unsigned i = 0; i < size; i++)
    bump_var_activity(lit_to_var(lits_input[i]));
  Tlit* lits;
  Tclause cl;
  TSclause* clause;
  if (learned)
    _n_learned_clauses++;
  if (external)
    _next_clause_elimination++;

  if (_deleted_clauses.empty()) {
    lits = new Tlit[size];
    memcpy(lits, lits_input, size * sizeof(Tlit));

    TSclause added(lits, size, learned, external);
    _clauses.push_back(added);
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
    if (clause->original_size < size) {
      delete[] clause->lits;
      clause->lits = new Tlit[size];
    }
    lits = clause->lits;
    memcpy(lits, lits_input, size * sizeof(Tlit));
    clause->size = size;
    clause->original_size = size;
    clause->deleted = false;
    clause->watched = true;
    clause->blocker = LIT_UNDEF;
    clause->first_watched = CLAUSE_UNDEF;
    clause->second_watched = CLAUSE_UNDEF;
  }
  // Remove duplicate literals
  if (size >= 2) {
    sort(lits, lits + size);
    unsigned j = 1;
    for (unsigned i = 1; i < size; i++) {
      if (lits[i] == lits[i - 1])
        continue;
      lits[j++] = lits[i];
    }
    size = j;
    clause->size = size;
  }

  clause->activity = _max_clause_activity;
  if (_observer) {
    vector<Tlit> lits_vector;
    for (unsigned i = 0; i < size; i++)
      lits_vector.push_back(lits[i]);
    _observer->notify(new sat::gui::new_clause(cl, lits_vector, learned, external));
  }

  if (size == 0) {
    clause->watched = false;
    _status = UNSAT;
    return cl;
  }
  if (size == 1) {
    clause->watched = false;
    if (lit_true(lits[0]))
      return cl;
    if (lit_false(lits[0]))
      repair_conflict(cl);
    else
      stack_lit(lits[0], cl);
    return cl;
  }
  else if (size == 2) {
    NOTIFY_OBSERVER(_observer, new sat::gui::stat("Binary clause added"));
    // clause->watched = false;
    _binary_clauses[lits[0]].push_back(make_pair(lits[1], cl));
    _binary_clauses[lits[1]].push_back(make_pair(lits[0], cl));
    if (lit_false(lits[0]) && !lit_false(lits[1])) {
      // swap the literals so that the false literal is at the second position
      lits[1] = lits[0] ^ lits[1];
      lits[0] = lits[0] ^ lits[1];
      lits[1] = lits[0] ^ lits[1];
      // no need to update the watch list
    }
    if (lit_false(lits[1])) {
      if (lit_undef(lits[0]))
        stack_lit(lits[0], cl);
      else if (lit_false(lits[0]))
        repair_conflict(cl);
      else if (_options.strong_chronological_backtracking) {
        ASSERT(lit_true(lits[0]));
        if (lit_lazy_reason(lits[0]) == CLAUSE_UNDEF || lit_level(_clauses[lit_lazy_reason(lits[0])].lits[1]) > lit_level(lits[0]))
          lit_set_lazy_reason(lits[1], cl);
      }
    }
  }
  else {
    select_watched_literals(lits, size);
    watch_lit(lits[0], cl);
    watch_lit(lits[1], cl);
    if (lit_false(lits[0]))
      repair_conflict(cl);
    else if (lit_false(lits[1]) && lit_undef(lits[0]))
      stack_lit(lits[0], cl);
    else if (lit_false(lits[1]) && lit_true(lits[0]) && (lit_level(lits[1]) < lit_level(lits[0]))) {
      Tclause lazy_reason = lit_lazy_reason(lits[1]);
      if (lazy_reason == CLAUSE_UNDEF || lit_level(_clauses[lazy_reason].lits[1]) > lit_level(lits[1])) {
        lit_set_lazy_reason(lits[1], cl);
      }
    }
  }
  if (_options.delete_clauses && _n_learned_clauses >= _next_clause_elimination)
    simplify_clause_set();
  return cl;
}

/*****************************************************************************/
/*                            Public interface                               */
/*****************************************************************************/

sat::NapSAT::NapSAT(unsigned n_var, unsigned n_clauses, sat::options& options) :
  _options(options)
{
  _vars = vector<TSvar>(n_var + 1);
  _trail = vector<Tlit>();
  _trail.reserve(n_var);
  _watch_lists = vector<Tclause>(2 * n_var + 2, CLAUSE_UNDEF);

  for (Tvar var = 1; var <= n_var; var++) {
    NOTIFY_OBSERVER(_observer, new sat::gui::new_variable(var));
    _variable_heap.insert(var, 0);
  }

  _clauses = vector<TSclause>();
  _clauses.reserve(n_clauses);

  _literal_buffer = new Tlit[n_var];
  _next_literal_index = 0;

  if (options.interactive || options.observing || options.check_invariants || options.print_stats) {
    _observer = new sat::gui::observer(options);
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
}

NapSAT::~NapSAT()
{
  for (unsigned i = 0; i < _clauses.size(); i++)
    delete[] _clauses[i].lits;
  if (_observer)
    delete _observer;
  delete[] _literal_buffer;
}

void NapSAT::set_proof_callback(void (*proof_callback)(void))
{
  _proof_callback = proof_callback;
}

bool sat::NapSAT::is_interactive() const
{
  return _options.interactive;
}

bool sat::NapSAT::is_observing() const
{
  return _observer != nullptr;
}

sat::gui::observer* sat::NapSAT::get_observer() const
{
  return _observer;
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
      NOTIFY_OBSERVER(_observer, new sat::gui::propagation(lit));
      continue;
    }
    NOTIFY_OBSERVER(_observer, new sat::gui::conflict(conflict));
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
    NOTIFY_OBSERVER(_observer, new sat::gui::check_invariants());
    if (!propagate()) {
      if (_status == UNSAT || !_options.interactive)
        break;
      NOTIFY_OBSERVER(_observer, new sat::gui::done(_status == SAT));
    }
    if (_purge_counter >= _purge_threshold) {
      purge_clauses();
      _purge_counter = 0;
      if (_status == UNSAT)
        return _status;
    }
    if (_observer && _options.interactive)
      _observer->notify(new sat::gui::checkpoint());
    else
      decide();
    if (_status == SAT || _status == UNSAT)
      break;
  }
  NOTIFY_OBSERVER(_observer, new sat::gui::check_invariants());
  NOTIFY_OBSERVER(_observer, new sat::gui::done(_status == SAT));
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
  stack_lit(lit, CLAUSE_UNDEF);
  return true;
}

bool sat::NapSAT::decide(Tlit lit)
{
  ASSERT(lit_undef(lit));
  stack_lit(lit, CLAUSE_UNDEF);
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

sat::Tclause NapSAT::finalize_clause()
{
  ASSERT(_writing_clause);
  _writing_clause = false;
  Tclause cl = internal_add_clause(_literal_buffer, _next_literal_index, false, true);
  propagate();
  return cl;
}

sat::Tclause sat::NapSAT::add_clause(const Tlit* lits, unsigned size)
{
  Tvar max_var = 0;
  for (unsigned i = 0; i < size; i++)
    if (lit_to_var(lits[i]) > max_var)
      max_var = lit_to_var(lits[i]);
  var_allocate(max_var);
  Tclause cl = internal_add_clause(lits, size, false, true);
  propagate();
  return cl;
}

const Tlit* sat::NapSAT::get_clause(Tclause cl) const
{
  assert(cl < _clauses.size());
  return _clauses[cl].lits;
}

unsigned sat::NapSAT::get_clause_size(Tclause cl) const
{
  assert(cl < _clauses.size());
  return _clauses[cl].size;
}

void NapSAT::hint(Tlit lit)
{
  ASSERT(lit_to_var(lit) < _vars.size());
  ASSERT(!_writing_clause);
  ASSERT(lit_undef(lit));
  stack_lit(lit, CLAUSE_LAZY);
}

void NapSAT::hint(Tlit lit, unsigned int level)
{
  ASSERT(lit_to_var(lit) < _vars.size());
  ASSERT(!_writing_clause);
  ASSERT(lit_undef(lit));
  ASSERT(level <= _decision_index.size() + 1);
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
