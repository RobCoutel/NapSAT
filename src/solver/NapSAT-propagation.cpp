/*
 * This file is part of the source code of the software program
 * NapSAT. It is protected by applicable copyright laws.
 *
 * This source code is protected by the terms of the MIT License.
 */
/**
 * @file src/solver/NapSAT-propagation.cpp
 * @author Robin Coutelier
 *
 * @brief This file is part of the NapSAT solver. It implements the propagation
 * mechanisms, BCP and related functions.
 */
#include "NapSAT.hpp"

#include "custom-assert.hpp"

#include <iostream>
#include <cstring>

using namespace std;
using namespace napsat;

Tlit* napsat::NapSAT::quick_replacement(Tclause cl) {
  // This must be as efficient as possible!
  Tlit* lits = clause_lits(cl);
  unsigned size = clause_size(cl);
  unsigned& last_look = _clauses[cl].last_looked;
  unsigned last_look_save = last_look;

  ASSERT(size >= 2);
  ASSERT(lit_false(lits[1]));
  /**
   * Not sure where this trick comes from, but it was used in veriT and mentionned by Mathias Fleury
   *
   * We cycle from the last searched position
   * This is good on big clauses as the literals at the start of the clause are more likely to be falsified
   * Therefore, skipping them and wrapping around might be beneficial.
   * Further, this approach spreads the watches a bit more
   */
  while (last_look < size) {
    if (!lit_false(lits[last_look++])) {
      return lits + last_look - 1;
    }
  }
  last_look = 2;
  while (last_look < last_look_save) {
    if (!lit_false(lits[last_look++])) {
      return lits + last_look - 1;
    }
  }
  return lits + 1;
}

Tlit* napsat::NapSAT::advanced_level_replacement(Tlit* lits, unsigned size) const
{
  /**
   * Pre conditions:
   * - The set of literals C = {c₂, ¬ℓ, ..., cₙ} has more than two literals
   *    |C| > 2
   * - The second literal ¬ℓ of the clause is falsified but not yet propagated
   *    ℓ ∈ ω
   */
  if (!_options.chronological_backtracking && !_options.graph_backtracking) {
    return lits + 1;
  }
  ASSERT(size >= 2);
  ASSERT(lit_false(lits[1]));
  // This must be as efficient as possible!

  Tlevel high_lvl = lit_level(lits[1]);
  Tlit* high_lit = lits + 1;
  for (Tlit* k = lits + 2; k < lits + size; ++k) {
    ASSERT(lit_false(*k));
    Tlevel level = lit_level(*k);
    if (level > high_lvl) {
      high_lvl = level;
      high_lit = k;
    }
  }
  return high_lit;
}

Tlit* napsat::NapSAT::advanced_graph_replacement(Tlit* lits, unsigned size) const
{
  Tlit* top_element = lits + 1;
  /**
   * We assume that we ran the quick_replacement function before and failed to find a suitable replacement literal
   *
   * We are searching for a top element of the lattice. This is expensive.
   * Instead, we search for the biggest chunk set. Which is necessarily a top element.
   */
  unsigned chunk_size = lit_chunks(*top_element).count();
  for (Tlit* k = lits + 2; k < lits + size; ++k) {
    ASSERT(lit_false(*k));
    unsigned dep_size = lit_chunks(*k).count();
    if (dep_size > chunk_size) {
      top_element = k;
      chunk_size = dep_size;
    }
  }
  return top_element;
}

void napsat::NapSAT::propagate_binary_clauses(Tlit c1)
{
  c1 = lit_neg(c1);
  ASSERT(lit_false(c1));

  for (TSwatch& w : _binary_watches[c1]) {
    Tlit c2 = w.block;
    Tclause cl = w.cl;
    ASSERT(clause_size(cl) == 2);

    if (lit_true(c2)) {
      Tlit* lits = clause_lits(cl);
      lits[0] = c2;
      lits[1] = c1;
      reimply_literal(c2, cl);
      continue;
    }
    if (lit_propagated(c1)) {
      print_trail();
    }
    ASSERT_MSG(!lit_propagated(c1),
               "Literal " + lit_to_string(c1) + " should not be propagated when propagating binary clause " + clause_to_string(cl));
    Tlit* lits = clause_lits(cl);
    if (lit_undef(c2)) {
      // ensure that the implied literal is positioned at the first position
      ASSERT(lits[0] == c1 || lits[1] == c1);
      ASSERT(lits[0] == c2 || lits[1] == c2);
      lits[0] = c2;
      lits[1] = c1;
      imply_literal(c2, cl);
      continue;
    }
    // Conflict
    ASSERT(_options.chronological_backtracking || _options.graph_backtracking
           || lit_level(c2) == lit_level(c1));
           // make sure that the highest literal is at the first position
    if (lit_level(c1) < lit_level(c2)) {
      ASSERT (_options.chronological_backtracking || _options.graph_backtracking);
      lits[0] = c2;
      lits[1] = c1;
      // we do not need to update the next watched clause because the clause is binary
    }
    if (lit_level(lits[0]) < lit_level(lits[1])) {
      std::swap(lits[0], lits[1]);
    }
    ASSERT_MSG(_options.graph_backtracking || lit_level(lits[0]) >= lit_level(lits[1]),
               "Clause " + clause_to_string(cl) + " is not correctly ordered after propagation of " + lit_to_string(c1));
    _conflicts.push_back(cl);
    NOTIFY_OBSERVER(conflict, cl);
    if (!_options.exhaustive_conflict_repair && !_options.partial_conflict_repair) {
      return;
    }
  }
}

void NapSAT::propagate_lit(Tlit lit)
{
  cout << "Propagating " << lit_to_string(lit);
  if (lit_propagated(lit)) {
    cout << " (already propagated)" << endl;
  } else {
    cout << endl;
  }
  /**
   * The mathematical notations and the contract of this function are defined in NapSAT.hpp
   */
  Tlit c1 = lit_neg(lit);
  ASSERT(lit_false(c1));

  // level of the propagation
  Tlevel c1_lvl = lit_level(c1);
  vector<TSwatch>& watch_list = _watches[c1];

  // Be careful that with this method, we do not want to push anything to the watch list.
  // Otherwise the memory might be reallocated and the pointers invalidated.
  // TODO check if this watch list shuffling is good for performance
  TSwatch* i = watch_list.data();
  TSwatch* end = i + watch_list.size();

  /**
   * Let F* be a set of clauses such that each clause in the set satisfies
   * For each clause C watched by c₁ and c₂ and such that c₁ is blocked by b, we have
   * - NCB:  ¬c₁ ∈ τ ⇒ c₂ ∈ π ∨ b ∈ π
   *        δ(b) ≤ δ(c₂) is trivially true in NCB since the levels are monotonically increasing
   * - LSCB: ¬c₁ ∈ τ ⇒ [c₂ ∈ π ∧ [δ(c₂) ≤ δ(c₁) ∨ δ(λ(c₂) \ {c₂}) ≤ δ(c₁)]
   *                 ∨ [b  ∈ π ∧  δ(b)  ≤ δ(c₁)]
   * - GB:   ¬c₁ ∈ τ ⇒ [c₂ ∈ π ∧ γ(c₂) ⊆ γ(c₁) ∪ η(c₁)]
   *                 ∨ [b  ∈ π ∧ γ(b)  ⊆ γ(c₁) ∪ η(c₁)]
   *
   * We initialise F* with all the clauses that are not watched by ¬ℓ. If they satisfied the invariant
   * before the propagation, they will satisfy them after ℓ is added to τ without any action.
   *
   * For the clauses watched by ¬ℓ, C = c₁ ∨ c₂ ∨ ... ∨ cₙ with b, a blocker such that b ∈ C
   * we will reason over the Haore triplets {P} loop body {Q}
   * where P is the precondition that the invariant holds before the loop
   * and Q is the postcondition that the invariant holds after the loop if ℓ = ¬c₁ is added to τ
   * * For each clause C watched by c₁ and c₂ and such that c₁ is blocked by b, we have
   * - NCB:  ¬c₁ ∈ (τ ⋅ ℓ) ⇒ c₂ ∈ π ∨ b ∈ π
   *        δ(b) ≤ δ(c₂) is trivially true in NCB since the levels are monotonically increasing
   * - LSCB: ¬c₁ ∈ (τ ⋅ ℓ) ⇒ [c₂ ∈ π ∧ [δ(c₂) ≤ δ(c₁) ∨ δ(λ(c₂) \ {c₂}) ≤ δ(c₁)]
   *                       ∨ [b  ∈ π ∧  δ(b)  ≤ δ(c₁)]
   * - GB:   ¬c₁ ∈ (τ ⋅ ℓ) ⇒ [c₂ ∈ π ∧ γ(c₂) ⊆ γ(c₁) ∪ η(c₁)]
   *                       ∨ [b  ∈ π ∧ γ(b)  ⊆ γ(c₁) ∪ η(c₁)]
   *
   * If Q is true, then we can add C to F* and we know that the invariants are preserved.
   * If we cannot make Q true, then there is a conflict and we can return C.
   *
   * If we have not returned a conflict, at then end of the loop, we will have explored
   * all the clauses watched by ¬ℓ and F* = F. Therefore, we satisfy our contract.
   *
   *
   * We have ensured previously (in propagate()) that  η(ℓ) = γ(ℓ) ∪ η(ℓ) such that we can simplify the test for GB
   * The expressions then become
   * P: GB:   ¬c₁ ∈ τ ⇒ [c₂ ∈ π ∧ γ(c₂) ⊆ η(c₁)]
   *                  ∨ [b  ∈ π ∧ γ(b)  ⊆ η(c₁)]
   * Q: GB:   ¬c₁ ∈ (τ ⋅ ℓ) ⇒ [c₂ ∈ π ∧ γ(c₂) ⊆ η(c₁)]
   *                        ∨ [b  ∈ π ∧ γ(b)  ⊆ η(c₁)]
   */
  ASSERT(lit_chunks(c1) <= lit_cross_chunks(c1));

  while (i < end) {
    c1 = lit_neg(lit);
    TSwatch& w = *i;
    Tclause cl = w.cl;
    TSclause& clause = _clauses[cl];
    ASSERT(clause.watched);
    ASSERT(clause.size >= 2);

    // Checking the validity of the blocker
    ASSERT(lit_cross_chunks(c1) >= lit_chunks(c1));
    Tlit b = w.block;
    if (lit_true(b)
      && (((!_options.chronological_backtracking || lit_level(b) <= c1_lvl)
        && (!_options.graph_backtracking         || lit_chunks(b) <= lit_cross_chunks(c1))))) {
      /**
       * NCB:  b ∈ π
       * LSCB: b ∈ π ∧ δ(b) ≤ δ(c₁)
       * GB:   b ∈ π ∧ γ(b) ⊆ η(c₁)
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
    Tlit c2 = lits[0] ^ lits[1] ^ c1;

    ASSERT(c1 == lits[0] || c1 == lits[1]);
    ASSERT(c2 == lits[0] || c2 == lits[1]);
    ASSERT(c1 != c2);

    lits[0] = c2;
    lits[1] = c1;
    Tlevel c2_lvl = lit_level(c2);


    /** SKIP CONDITIONS **/
    if (lit_true(c2)
    && (((!_options.lazy_strong_chronological_backtracking || c2_lvl <= c1_lvl)
      && (!_options.graph_backtracking                     || lit_chunks(c2) <= lit_cross_chunks(c1))))) {
      /**
       * NCB:  c₂ ∈ π
       * WCB:  c₂ ∈ π
       * LSCB: c₂ ∈ π ∧ δ(c₂) ≤ δ(c₁)
       * GB:   c₂ ∈ π ∧ γ(c₂) ⊆ η(c₁)
       * the invariants are preserved without any action
       */
      w.block = c2;
#if NOTIFY_WATCH_CHANGES
      NOTIFY_OBSERVER(block, cl, c2, c1);
#endif
      i++;
      continue;
    }

    /** SEARCH REPLACEMENT **/
    // Quick replacement returns a non-falsified literal r ∈ C \ {c₂} if such a literal exists.
    Tlit* r = quick_replacement(cl);
    ASSERT(*r != LIT_UNDEF);

    /**
     * Search replacement returns a literal r ∈ C \ {c₂} such that it either is a good replacement
     * such that
     *   ¬r ∈ (τ ⋅ ℓ) ⇒ C \ {c₂}, π ⊧ ⊥
     * NCB: If c₂ ∈ π, then we would have stopped at the skip conditions
     *      We know that [C \ {c₂}, π ⊧ ⊥] ⇒ δ(c₁) = δ(c₂) = δ(C) and c₁ will be returned
     *      if C \ {c₂} is conflicting
     *
     * We know that
     * ALL:  [¬r ∈ (τ ⋅ ¬c₁) ⇒ C \ {c₂}, π ⊧ ⊥]
     * NCB:   c₂ ∉ π                  ∧  b ∉ π
     * LSCB: [c₂ ∉ π ∨ δ(c₂) > δ(c₁)] ∧ [b ∉ π ∨ δ(b) > δ(c₁)]
     * GB:   [c₂ ∉ π ∨ γ(c₂) ⊈ η(c₁)] ∧ [b ∉ π ∨ γ(b) ⊈ η(c₁)]
    */
    ASSERT(r != nullptr);
    if (lit_propagated(c1)) {
      print_trail();
    }
    // in debug mode, we still run the propagation, but it should not change anything
    ASSERT_MSG(!lit_propagated(c1) || find(_conflicts.begin(), _conflicts.end(), cl) != _conflicts.end(), lit_to_string(c1) + " requires changes on clause " + clause_to_string(cl));

    Tlevel r_lvl = lit_level(*r);

    /** TRUE literal **/
    if (lit_true(*r)
    && (((_options.graph_backtracking || r_lvl <= c1_lvl)
      && (!_options.graph_backtracking || lit_chunks(*r) <= lit_cross_chunks(c1))))) {
      /**
       * r ∈ π ∧ δ(r) ≤ δ(c₁)
       * NCB: We know that r ∈ π ⇒ δ(r) ≤ δ(c₁). Therefore after this condition is satisfied in NCB,
       *      we know that r ∉ π
       * GB:   [r ∉ π ∨ γ(r) ⊆ η(c₁)]
       *    The invariant is satisfied if we set b = r
       * ¬c₁ ∈ τ ⇒ c₂ ∈ π ∨ [b ∈ π ∧ δ(b) ≤ δ(c₁)] is satisfied if we set b = r
      */
      w.block = *r;
#if NOTIFY_WATCH_CHANGES
      NOTIFY_OBSERVER(block, cl, *r, c1);
#endif
      i++;
      continue;
    }

    /**
     * We know that (droping blocker information)
     * ALL:  ¬r ∈ π ⇒ C \ {c₂}, π ⊧ ⊥
     * NCB:   r ∉ π                 ∧  c₂ ∉ π
     * LSCB: [r ∉ π ∨ δ(r) > δ(c₁)] ∧ [c₂ ∉ π ∨ δ(c₂) > δ(c₁)]
     * GB:    r ∉ π                 ∧ [c₂ ∉ π ∨ γ(c₂) ⊈ η(c₁)]
    */

    /** UNDEF or TRUE literal **/
    if (!lit_false(*r)) {
      /**
       * We now know that ¬r ∉ π, and a fortiori ¬r ∉ (τ ⋅ ℓ) since ℓ ∈ ω ⊆ π
       * Therefore, we can replace c₁ by r and satisfy the invariant
       */
      // watch the replacement and stop watching c1
      lits[1] = *r;
      *r = c1;
#if NOTIFY_WATCH_CHANGES
      NOTIFY_OBSERVER(unwatch, cl, c1);
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
     * We know that
     * ALL:  C \ {c₂}, π ⊧ ⊥
     * NCB:   c₂ ∉ π
     * LSCB: [c₂ ∉ π ∨ δ(c₂) > δ(c₁)]
     * GB:   [c₂ ∉ π ∨ γ(c₂) ⊈ η(c₁)]
     *
     * Which means that this clause is either
     * 1. Conflicting if ¬c₂ ∈ π
     * 2. Unit if c₂ ∉ π ∧ ¬c₂ ∉ π
     * 3. Missed implication if c₂ ∈ π (and was not skipped earlier)
     *   3.1 Missed lower implication if δ(c₂) > δ(C \ {c₂})
     *   3.2 Missed chunk implication if γ(c₂) ⊈ η(c₁)
     *
     * We now perform a more advanced search for a replacement.
     * In case of NCB, this is not necessary because we are certain that c₂ is at the highest level
     * We want to find a top element of the clause such that we can replace c₁ with it
     * The goal is
     * 1. In CB, we want to find the highest literal in the clause to
     *   - Determine the level of the clause
     *   - Ensure that this is the first backtracked literal in the clause
     * 2. In GB, we want to find a literal r such that for all literals ℓ' in the clause γ(r) ⊈ η(ℓ') to
     *   - reduce the number of repropagated literals
    */
    if (_options.graph_backtracking) {
      r = advanced_graph_replacement(lits, clause.size);
    } else if (_options.chronological_backtracking) {
      r = advanced_level_replacement(lits, clause.size);
    }
    ASSERT(lit_false(*r));
    ASSERT(lit_is_max_literal(*r, lits + 1, clause.size - 1));
    r_lvl = lit_level(*r);
    /**
     * ALL: ¬r ∈ π ∧ C \ {c₂}, π ⊧ ⊥
     * NCB:  c₂ ∉ π
     * LSCB: δ(r) = δ(C \ {c₂}) ∧ [c₂ ∉ π ∨ δ(c₂) > δ(c₁)]
     * GB:   ∀ℓ' ∈ C ∖ {c₂} γ(c₂) ⊈ γ(ℓ') ∧ [c₂ ∉ π ∨ γ(c₂) ⊈ η(c₁)]
     */
    if (r != lits + 1) {
      /**
       * If r ≠ c₁, we know that we are in CB or GB since in NCB we have
       *    [C \ {c₂}, π ⊧ ⊥] ⇒ δ(c₁) = δ(c₂) = δ(C) and r = c₁
       * We know that δ(r) > δ(c₁)
       * We swap the literals such that c₁ ← r
      */
      ASSERT(_options.chronological_backtracking || _options.graph_backtracking);
      // In strong chronological backtracking, we need to swap the literals such that the highest falsified literal is at the second position. In weak chronological backtracking, it is not necessary, but it is still useful to determine the level of the conflict or the implication.
      // swap the literals
      lits[1] = *r;
      *r = c1;
#if NOTIFY_WATCH_CHANGES
      NOTIFY_OBSERVER(unwatch, cl, c1);
#endif
      c1 = lits[1]; // update c1 with the replacement literal
      // remove the clause from the watch list
      // bring the last watched clause to the current position
      *i = *(end - 1);
      end--;
      // watch new literal
      watch_lit(c1, cl);
    }
    else {
      // Increment for the next iteration
      // We cannot use *i to refer to the clause from this point onwards since it it ready for the next iteration
      i++;
    }
    ASSERT(c1 == lits[1]);
    ASSERT(lit_is_max_literal(c1, lits + 2, clause.size - 2));

    /**
     * We no longer need r since we replaced c₁ ← r (which might be equal)
     * However, since we changed c₁ to r, we lost the information related to it.
     * We write (?) to denote some literal in the clause that used to be c₁
     *
     * ALL:  ¬c₁ ∈ π ∧ C \ {c₂}, π ⊧ ⊥
     * NCB:  c₂ ∉ π
     * LSCB: δ(c₁) = δ(C \ {c₂}) ∧ [c₂ ∉ π ∨ δ(c₂) > δ(?)]
     * GB:   ∀ℓ' ∈ C ∖ {c₂} γ(c₁) ⊈ γ(ℓ') ∧ [c₂ ∉ π ∨ γ(c₂) ⊈ η(?)]
     */

    // We know that all literals in clause[1:end] are false
    /** CONFLICT **/
    if (lit_false(c2)) {
      /**
       * We know that C \ {c₂}, π ⊧ ⊥, therefore, if ¬c₂ ∈ π then C, π ⊧ ⊥ and we have a conflict
       * We cannot safely add ℓ to the propagated set τ.
       */
      ASSERT(lit_level(lits[1]) == r_lvl);
      if (_options.graph_backtracking) {
        // we want to make sure that if we do not backtrack the watched literals, they will still be repropagated and fixed
        for (size_t j = 2; j < clause.size; j++) {
          lit_cross_chunks(c1) |= lit_chunks(lits[j]);
          lit_cross_chunks(c2) |= lit_chunks(lits[j]);
        }
      }
      if (c2_lvl < r_lvl) {
        ASSERT(c2 == lits[0]);
        ASSERT(c1 == lits[1]);
        // swap the literals
        // we want the highest literal to be at the first position
        lits[0] = c1;
        lits[1] = c2;
      }
      watch_list.resize(end - watch_list.data());
      _conflicts.push_back(cl);
      NOTIFY_OBSERVER(conflict, cl);
      if (!_options.exhaustive_conflict_repair && !_options.partial_conflict_repair) {
        return;
      } else {
        continue;
      }
    }

    /** UNIT CLAUSE **/
    if (lit_undef(c2)) {
      // unit clause
      /**
       * We add the information that c₂ ∉ π
       * We know that C \ {c₂}, π ⊧ ⊥, therefore the only way to satisfy C is to set c₂ to true
       * In LSCB we additionally need δ(c₂) ≤ δ(c₁), and since c₂ will be implied at level δ(C \ {c₂}),
       * we need to ensure that δ(c₁) = δ(C \ {c₂}). This is why we changed the watched literals earlier.
       *
       * ALL:  c₂ ∉ π ∧ ¬c₂ ∉ π ∧ C \ {c₂}, π ⊧ ⊥
       * NCB:  δ(c₁) = δ(C \ {c₂})
       * LSCB: δ(c₁) = δ(C \ {c₂})         ∧ [c₂ ∉ π ∨ δ(c₂) ≤ δ(?)]
       * GB:   ∀ℓ' ∈ C ∖ {c₂} γ(c₂) ⊈ γ(ℓ') ∧ [c₂ ∉ π ∨ γ(c₂) ⊈ η(?)]
       *
       * By implying c₂ with C, we ensure that the invariants hold. By construction, we have
       * - NCB:  c₂ ∈ π
       * - LSCB: c₂ ∈ π ∧ δ(c₂) ≤ δ(c₁)
       * - GB:   c₂ ∈ π ∧ γ(c₂) ⊆ γ(c₁)
       * satisfying the invariants for τ ← (τ ⋅ ℓ)
       */
      imply_literal(c2, cl);
      // don't increment. We would have done so earlier if we did not change the watched literals
      continue;
    }

    /** MISSED LOWER IMPLICATION **/
    /**
     * We know that we can only be in SCB since the skip condition
     * We know that c₂ ∈ π is now satisfied
     * ALL:   c₂ ∈ π ∧ ¬c₁ ∈ π ∧ [δ(c₂) ≤ δ(c₁) ∨ [C \ {c₂}, π ⊧ ⊥ ∧ δ(c₁) = δ(C \ {c₂})]]
     * NCB:   ⊥    => not possible
     * LSCB:  δ(c₂) > δ(?)
     * GB:    γ(c₂) ⊈ η(?)
     * We shall now only consider SCB
    */
    ASSERT(_options.graph_backtracking || _options.chronological_backtracking);
    ASSERT(check_clause_implying(cl));
    ASSERT(lit_is_max_literal(c1, lits + 2, clause.size - 2));

    if (( _options.graph_backtracking || lit_level(c2) <= r_lvl)
     && (!_options.graph_backtracking || lit_chunks(c2) <= lit_chunks(c1))) {
      // This is not a real missed lower implication. The level of the satisfied literal is lower than or equal to the level of the replacement.
      /**
       * Since we changed c₁, we cannot be sure that this is actually a MLI
       *
       * Now, we know
       * ALL:  c₂ ∈ π
       * LSCB: δ(c₂) ≤ δ(c₁)
       * GB:   γ(c₂) ⊆ η(c₁)
       *
       * And the invariants are satisfied when τ ← (τ ⋅ ℓ)
       */
      // don't increment. We would have done so earlier if we did not change the watched literals
      continue;
    }
    // We know that c2 is true, and it is a missed lower implication
    // c2 is the only satisfied literal in the clause and all other literals are propagated at a level lower than the highest falsified literal
    /**
     * We now also know that δ(c₂) > δ(c₁), so we can simplify our knowledge as
     * c₂ ∈ π ∧ ¬c₁ ∈ π ∧ C \ {c₂}, π ⊧ ⊥ ∧ δ(c₁) = δ(C \ {c₂})
     */
    /**
     * If δ(λ(c₂) \ {c₂}) > δ(c₁), then reimply c₂ at the level of c₁
     * The only way to satisfy the invariant is to add C to the lazy reimplication list of c₂
     * λ(c₂) ← λ[c₂ ← C] and therefore δ(λ(c₂) \ {c₂}) =  δ(c₁)
     *
     * Otherwise do nothing
     *
     * Note that reimply literal ensures δ(λ(c₂) \ {c₂}) > δ(c₁) ∧  δ(ℓ) > δ(c₁) before reimplication
     */
    reimply_literal(c2, cl);

    /**
     * We now have in addition that δ(λ(c₂) \ {c₂}) ≤ δ(c₁)
     * that satisfies
     * ¬c₁ ∈ (τ ⋅ ℓ) ⇒ [c₂ ∈ π ∧ [δ(c₂) ≤ δ(c₁) ∨ δ(λ(c₂) \ {c₂}) ≤ δ(c₁)]
     *               ∨ [b ∈ π ∧ δ(b) ≤ δ(c₁)]
     */
    // don't increment. We would have done so earlier if we did not change the watched literals
  }

  watch_list.resize(end - watch_list.data());
}
