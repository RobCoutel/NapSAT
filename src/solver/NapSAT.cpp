/*
 * This file is part of the source code of the software program
 * NapSAT. It is protected by applicable copyright laws.
 *
 * This source code is protected by the terms of the MIT License.
 */
/**
 * @file src/solver/NapSAT.cpp
 * @author Robin Coutelier
 *
 * @brief This file is part of the NapSAT solver. It implements the core functions of the
 * solver such as the CDCL loop, BCP, conflict analysis, and backtracking.
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
   * STANDARD BACKTRACKING :
   * - the second literal of the clause is at the highest level in C
   *    δ(ℓ) = δ(C \ {ℓ})
   * GRAPH BACKTRACKING :
   * - the second literal of the clause is a top element of the lattice
   * while not strictly necessary, this condition is useful to reduce the
   * number of repropagations.
   */
  ASSERT(lit_undef(lit));
  ASSERT(reason == CLAUSE_UNDEF || clause_unit(reason));

  _trail.push_back(lit);

  Tvar var = lit_to_var(lit);
  TSvar& svar = _vars[var];
  svar.state = lit_pol(lit);
  svar.propagated = false;
  svar.reason = reason;
  svar.phase_cache = lit_pol(lit);

  // for the logic, look at the comment in NapSAT.hpp
  ASSERT(svar.synced != 0);
  if (svar.synced == 1)
    svar.synced = 3;

  if (reason == CLAUSE_UNDEF) {
    // Decision
    _decision_index.push_back(_trail.size() - 1);
    svar.level = solver_level();
    NOTIFY_OBSERVER(_observer, new napsat::gui::decision(lit));
    if (_options.graph_backtracking) {
      if (_free_chunks.empty()) {
        allocate_chunks(2 * _n_allocated_chunks);
      }
      Tchunk chunk_number = _free_chunks.back();
      ASSERT (_n_allocated_chunks == _chunks.size());
      ASSERT_MSG(chunk_number < _n_allocated_chunks,
        "Chunk number: " + std::to_string(chunk_number) +
        "\nNumber of allocated chunks: " + std::to_string(_n_allocated_chunks));
      _free_chunks.pop_back();
      svar.chunks.set(chunk_number, true);
      _chunks[chunk_number].decision = var;
      ASSERT(_chunks.size() == solver_level() + _free_chunks.size());
    }
  }
  else if (reason == CLAUSE_LAZY) {
    // Theory propagation
    ASSERT_MSG(false, "Lazy reason is not implemented yet");
  }
  else {
    // Implied literal
    const Tlit* lits = _clauses[reason].lits;
    const unsigned size = _clauses[reason].size;
    ASSERT(lit == lits[0]);
    ASSERT(clause_implying(reason));
    ASSERT(size < 2 || max_literal(lits[1], lits + 2, size - 2));

    if (_clauses[reason].size == 1) {
      svar.level = LEVEL_ROOT;
    } else {
      if (_options.graph_backtracking) {
        // In graph backtracking we do not have any information about the levels
        // we need to compute it by going through the clause
        svar.level = lit_level(lits[1]);
        for (unsigned i = 2; i < size; i++) {
          svar.level = std::max(svar.level, lit_level(lits[i]));
        }

        // compute the chunks and cross-chunks of the variable
        ASSERT(svar.chunks.empty());
        for (unsigned i = 1; i < size; i++) {
          svar.chunks |= lit_chunks(lits[i]);
        }
        if (size > 2) {
          lit_cross_chunks(lits[1]) |= svar.chunks;
        }
      } else {
        svar.level = lit_level(lits[1]);
      }
    }
    NOTIFY_OBSERVER(_observer, new napsat::gui::implication(lit, reason, svar.level));
  }

  if (svar.level == LEVEL_ROOT) {
    _n_root_lvl_lits++;
    NOTIFY_OBSERVER(_observer, new napsat::gui::stat("Root level literal"));
    if (_proof)
      _proof->root_assign(lit, reason);
  }
  ASSERT(svar.level != LEVEL_UNDEF);
  ASSERT(svar.level <= solver_level());
}

void NapSAT::var_unassign(Tvar var)
{
  ASSERT(!var_undef(var));

  TSvar& v = _vars[var];
  NOTIFY_OBSERVER(_observer, new napsat::gui::unassignment(literal(var, v.state)));
  if (v.missed_lower_implication != CLAUSE_UNDEF) {
    NOTIFY_OBSERVER(_observer, new napsat::gui::remove_lower_implication(var));
    v.missed_lower_implication = CLAUSE_UNDEF;
  }
  if (!_variable_heap.contains(var))
    _variable_heap.insert(var, v.activity);

  if (_options.graph_backtracking) {
    if (v.reason == CLAUSE_UNDEF) {
      ASSERT(v.chunks.count() == 1);
      for (Tchunk ck = 0; ck < _n_allocated_chunks; ck++) {
        TSchunk& chunk = _chunks[ck];
        if (chunk.decision == var) {
          _free_chunks.push_back(ck);
          chunk.decision = LIT_UNDEF;
          chunk.missed_implication.clear();
          break;
        }
      }
    }
    v.chunks.clear();
    v.cross_chunks.clear();
  }
  v.state = VAR_UNDEF;
  v.reason = CLAUSE_UNDEF;
  v.level = LEVEL_UNDEF;
  v.propagated = false;

  // for the logic, look at the comment in NapSAT.hpp
  switch (v.synced)
  {
  case 0:
  case 2:
    v.synced = 2;
    break;
  case 3:
    v.synced = 1;
    break;
  default:
    ASSERT(false);
  }
}

void napsat::NapSAT::reimply_literal(Tlit lit, Tclause reason)
{
  TSclause& clause = _clauses[reason];
  unsigned size = clause.size;

  ASSERT(lit_true(lit));
  ASSERT(_options.lazy_strong_chronological_backtracking || _options.graph_backtracking);
  ASSERT(reason != CLAUSE_UNDEF && reason != CLAUSE_LAZY);
  ASSERT(lit == clause.lits[0]);
  ASSERT(clause_implying(reason));
  ASSERT(size < 2 || max_literal(clause.lits[1], clause.lits + 2, size - 2));

  Tlevel reimplication_level = size == 1 ? 0 : lit_level(clause.lits[1]);
  if (lit_level(lit) <= reimplication_level)
    return;
  if (lit_lazy_reason(lit) != CLAUSE_UNDEF
   && lit_level(_clauses[lit_lazy_reason(lit)].lits[1]) <= reimplication_level)
    return;

  lit_set_lazy_reason(lit, reason);
}

Tlit* napsat::NapSAT::quick_replacement(Tclause cl) {
  // This must be as efficient as possible!
  Tlit* lits = _clauses[cl].lits;
  unsigned size = _clauses[cl].size;
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

Tlit* napsat::NapSAT::advanced_graph_replacement(Tlit* lits, unsigned size)
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

Tlit* napsat::NapSAT::advanced_level_replacement(Tlit* lits, unsigned size)
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

bool napsat::NapSAT::reimplication_cycle(Tchunk decision_chunk, const bitset& reimplying_chunks)
{
  bitset closure = reimplying_chunks;
  bool changed = true;

  while (changed) {
    if(closure[decision_chunk]) {
      // we found a cycle
      return true;
    }
    changed = false;
    for (auto it = closure.cbegin(); it != closure.cend(); ++it) {
      Tchunk c = *it;
      bitset& m = _chunks[c].missed_implication;
      if (!(m < closure)) {
        closure |= m;
        changed = true;
      }
    }
  }
  return false;
}

Tclause napsat::NapSAT::propagate_binary_clauses(Tlit c1)
{
  c1 = lit_neg(c1);
  ASSERT(lit_false(c1));

  for (TSwatch& w : _binary_watch[c1]) {
    Tlit c2 = w.block;
    Tclause cl = w.cl;
    ASSERT(_clauses[cl].size == 2);
    if (lit_true(c2)) {
      if (_options.lazy_strong_chronological_backtracking && lit_level(c2) > lit_level(c1)) {
        // missed lower implication
        Tlit* lits = _clauses[cl].lits;
        lits[0] = c2;
        lits[1] = c1;
        reimply_literal(c2, cl);
      }
      if (_options.graph_backtracking && !(lit_chunks(c2) <= lit_chunks(c1))) {
        lit_cross_chunks(c1) |= lit_chunks(c2);
        if (_options.lazy_chunk_merging && lit_decision(c2)) {
          // TODO check for missed implications
        }
      }
      continue;
    }
    ASSERT(!lit_propagated(c1));
    Tlit* lits = _clauses[cl].lits;
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
    ASSERT(lits[0] == c1 || lits[1] == c1);
    ASSERT(lits[0] == c2 || lits[1] == c2);
    if (lit_level(c1) < lit_level(c2)) {
      ASSERT (_options.chronological_backtracking || _options.graph_backtracking);
      lits[0] = c2;
      lits[1] = c1;
      // we do not need to update the next watched clause because the clause is binary
    } else {
      lits[0] = c1;
      lits[1] = c2;
    }
    ASSERT(_options.graph_backtracking || lit_level(lits[0]) >= lit_level(lits[1]));
    return cl;
  }
  return CLAUSE_UNDEF;
}

Tclause NapSAT::propagate_lit(Tlit lit)
{
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
      NOTIFY_OBSERVER(_observer, new napsat::gui::block(cl, c2, c1));
#endif
      i++;
      continue;
    }

    /** SEARCH REPLACEMENT **/
    // Quick replacement returns a non-falsified literal r ∈ C \ {c₂} if such a literal exists.
    Tlit* r = quick_replacement(cl);

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
    // in debug mode, we still run the propagation, but it should not change anything
    ASSERT_MSG(!lit_propagated(c1), lit_to_string(c1) + " requires changes on clause " + clause_to_string(cl));

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
      NOTIFY_OBSERVER(_observer, new napsat::gui::block(cl, *r, c1));
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
      NOTIFY_OBSERVER(_observer, new napsat::gui::unwatch(cl, c1));
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
    ASSERT(max_literal(*r, lits + 1, clause.size - 1));
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
      NOTIFY_OBSERVER(_observer, new napsat::gui::unwatch(cl, c1));
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
    ASSERT(max_literal(c1, lits + 2, clause.size - 2));

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
      return cl;
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
    ASSERT(clause_implying(cl));
    ASSERT(max_literal(c1, lits + 2, clause.size - 2));

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
    if (_options.lazy_strong_chronological_backtracking) {
      reimply_literal(c2, cl);
    } else if (_options.lazy_chunk_merging && lit_decision(c2) && lit_lazy_reason(c2) == CLAUSE_UNDEF) {
      Tlit reimp_lit = lits[0];
      // compute the chunk set of the clause, excluding the lits[1]
      bitset clause_chunks(_n_allocated_chunks);
      for (unsigned i = 1; i < _clauses[cl].size; i++) {
        clause_chunks |= lit_chunks(lits[i]);
      }

      auto it = lit_chunks(reimp_lit).cbegin();
      Tchunk decision_chunk = *it;
      ASSERT(++it == lit_chunks(reimp_lit).cend());

      if (!reimplication_cycle(decision_chunk, clause_chunks)) {
        NOTIFY_OBSERVER(_observer, new napsat::gui::stat("Cross implication for decision"));

        lit_set_lazy_reason(c2, cl);
        _chunks[decision_chunk].missed_implication = clause_chunks;
      }
    }

    lit_cross_chunks(c1) |= lit_chunks(c2);
    /**
     * We now have in addition that δ(λ(c₂) \ {c₂}) ≤ δ(c₁)
     * that satisfies
     * ¬c₁ ∈ (τ ⋅ ℓ) ⇒ [c₂ ∈ π ∧ [δ(c₂) ≤ δ(c₁) ∨ δ(λ(c₂) \ {c₂}) ≤ δ(c₁)]
     *               ∨ [b ∈ π ∧ δ(b) ≤ δ(c₁)]
     */
    // don't increment. We would have done so earlier if we did not change the watched literals
  }

  watch_list.resize(end - watch_list.data());

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
      ASSERT(_options.lazy_strong_chronological_backtracking || _options.graph_backtracking || lit_lazy_reason(lit) == CLAUSE_UNDEF);
      if (!_options.graph_backtracking && lit_lazy_level(lit) <= level) {
        // look if the literal can be reimplied at a lower level
        ASSERT(_options.lazy_strong_chronological_backtracking);
        Tclause lazy_reason = lit_lazy_reason(lit);
        ASSERT(lazy_reason != CLAUSE_UNDEF);
        ASSERT(_clauses[lazy_reason].lits[0] == lit);
        ASSERT(lit_true(_clauses[lazy_reason].lits[0]));
        _reimplication_backtrack_buffer.push_back(lazy_reason);
      }
      /* in LSCB, we cannot backtrack from front to back because it breaks the missed lower implications
        for example, if ℓ₁ ∨ ℓ₂ ∨ ℓ₃ is a missed lower implication and the trail looks like
        δ = 2          - ℓ₁ -
        δ = 1     - ¬ℓ₂      - ¬ℓ₃ -
        δ = 0 - -
        then backtracking to level 0 will first unassign ¬ℓ₂ such that we cannot determine the level of
        the MLI anymore

        This is why we store a buffer of the literals that need to be unassigned and unassign them only
        at the end of the backtracking
      */
      _backtracked_variables.push_back(var);
    }
    else { // lit_level(lit) <= level
      _trail[j++] = lit;
      waiting_count += (i >= _n_propagated_lits);
    }
  }
  // Here we unassign the literals as mentioned above
  while(!_backtracked_variables.empty()) {
    Tvar var = _backtracked_variables.back();
    _backtracked_variables.pop_back();
    var_unassign(var);
  }
  _trail.resize(j);
  _decision_index.resize(level);

  ASSERT_MSG(_options.chronological_backtracking || _options.graph_backtracking || waiting_count == 0,
             "Waiting count: " + to_string(waiting_count) + "\nLevel: " + to_string(level) + "\nRestore point: " + to_string(restore_point));
  _n_propagated_lits = _trail.size() - waiting_count;
  ASSERT_MSG(_options.chronological_backtracking || _options.graph_backtracking || _n_propagated_lits == restore_point,
    "Propagated literals: " + to_string(_n_propagated_lits) + "\nRestore point: " + to_string(restore_point));
  // in RSCB we need to move the propagation head back to the location of the first literal that moved
  // that is, the location of the first literal that was unassigned.
  if (_options.restoring_strong_chronological_backtracking) {
    while (_n_propagated_lits > restore_point) {
      Tlit lit = _trail[_n_propagated_lits - 1];
      Tvar var = lit_to_var(lit);
      ASSERT_MSG(_vars[var].propagated,
                  "Literal: " + lit_to_string(lit) + "\nLevel: " + to_string(lit_level(lit)));
      _vars[var].propagated = false;
      _n_propagated_lits--;
      NOTIFY_OBSERVER(_observer, new napsat::gui::remove_propagation(lit));
    }
  }
  if (_reimplication_backtrack_buffer.size() > 0) {
    ASSERT(_options.lazy_strong_chronological_backtracking);
    // adds the literals on the lazy reimplication buffer to the trail by order of increasing level
    // Sort the literals by increasing level. It is not necessary, but it probably is more effective
    // TODO evaluate the performance of this. Is sorting useful?
    // The topological order will automatically be respected because the reimplied literals cannot depend on each other.
    // see Theorem 17 in [Lazy Reimplication in Chronological Backtracking, Robin Coutelier and Mathias Fleury and Laura Kovács]
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

static Tvar last_backtracked_decision = 0;

void napsat::NapSAT::undo_chunks(const bitset& backtracked_chunks)
{
  ASSERT(!backtracked_chunks.empty());
  ASSERT(_backtracked_variables.empty());
  // Mapping to the new level of literals after backtracking
  vector<Tlevel> level_transformation(solver_level() + 1);
  Tlevel real_level = 1;
  Tlevel min_level = INT32_MAX;
  for (Tlevel lvl = 1; lvl <= solver_level(); lvl++) {
    Tlit decision = _trail[_decision_index[lvl - 1]];
    ASSERT(lit_decision(decision));
    ASSERT(lit_level(decision) == lvl);
    if (lit_chunks(decision).has_intersection(backtracked_chunks)) {
      min_level = min(min_level, lvl);
      level_transformation[lvl] = LEVEL_ERROR;
    } else {
      level_transformation[lvl] = real_level++;
    }
  }

  size_t start_position = _decision_index[min_level - 1];
  ASSERT(start_position < _trail.size());
  Tlit* i = _trail.data() + start_position;
  Tlit* j = i;
  Tlit* end = i + _trail.size() - start_position;
  Tlevel decision_counter = min_level - 1;
  unsigned new_propagation_head = _n_propagated_lits;

  while (i < end) {
    Tlit lit = *i;
    bitset& chunks = lit_chunks(lit);
    Tlevel lit_lvl = lit_level(lit);
    // belonging to one of the deleted levels is a sufficient condition, faster to compute than the chunk intersection
    if (level_transformation[lit_lvl] == LEVEL_ERROR
     || chunks.has_intersection(backtracked_chunks)) {
      if(lit_decision(lit)) {
        last_backtracked_decision = lit_to_var(lit);
      }
      // we need to unassign the variable
      var_unassign(lit_to_var(lit));
      unsigned loc = j - _trail.data();
      _n_propagated_lits   -= loc < _n_propagated_lits;
      new_propagation_head -= loc < new_propagation_head;
      ASSERT(_n_propagated_lits <= _trail.size());
      i++;
      continue;
    }

    if (lit_decision(lit)) {
      _decision_index[decision_counter++] = j - _trail.data();
    }

    // check if the lazy reimplication still holds
    Tclause lazy_reason = lit_lazy_reason(lit);
    if (lazy_reason != CLAUSE_UNDEF) {
      for (unsigned k = 1; k < _clauses[lazy_reason].size; k++) {
        Tlit l = _clauses[lazy_reason].lits[k];
        if (lit_undef(l) || lit_chunks(l).has_intersection(backtracked_chunks)) {
          lit_set_lazy_reason(lit, CLAUSE_UNDEF);
          break;
        }
      }
    }
    Tvar var = lit_to_var(lit);
    if (lit_lvl > min_level) {
      // We need to fix the level of the literal
      _vars[var].level = level_transformation[_vars[var].level];
      ASSERT(var_level(var) != LEVEL_ERROR);
      NOTIFY_OBSERVER(_observer, new napsat::gui::update_level(lit, var_level(var)));
    }
    if (lit_propagated(lit) && lit_cross_chunks(lit).has_intersection(backtracked_chunks)) {
      unsigned loc = j - _trail.data();
      _vars[var].propagated = false;
      NOTIFY_OBSERVER(_observer, new napsat::gui::stat("Replayed Propagation"));
      new_propagation_head = min(new_propagation_head, loc);
    }
    *(j++) = *(i++);
  }
  ASSERT(decision_counter == _decision_index.size() - backtracked_chunks.count());
  _decision_index.resize(decision_counter);
  _trail.resize(j - _trail.data());

  // we now search for literals that need to be repropagated on the left side of the decision
  // todo: we would like not to have to do that...
  for (size_t i = 0; i < start_position; i++) {
    Tlit lit = _trail[i];
    if (lit_propagated(lit) && lit_cross_chunks(lit).has_intersection(backtracked_chunks)) {
      Tvar var = lit_to_var(lit);
      _vars[var].propagated = false;
      NOTIFY_OBSERVER(_observer, new napsat::gui::stat("Replayed Propagation"));
      new_propagation_head = min(new_propagation_head, (unsigned)i);
    }
  }

  ASSERT(_n_propagated_lits <= _trail.size());
  while(new_propagation_head < _n_propagated_lits) {
    _n_propagated_lits--;
    NOTIFY_OBSERVER(_observer, new napsat::gui::remove_propagation(_trail[_n_propagated_lits]));
  }

  // We need to kill the invalidated missed_implications
  for (unsigned location : _decision_index) {
    Tlit lit  = _trail[location];
    ASSERT(lit_decision(lit));
    auto it = lit_chunks(lit).cbegin();
    Tchunk decision_chunk = *it;
    ASSERT(++it == lit_chunks(lit).cend());
    bitset& missed_implication = _chunks[decision_chunk].missed_implication;
    if (missed_implication.has_intersection(backtracked_chunks)) {
      _vars[lit_to_var(lit)].missed_lower_implication = CLAUSE_UNDEF;
      missed_implication.clear();
    }
  }

#ifndef NDEBUG
  for (Tlit lit : _trail) {
    ASSERT(!lit_chunks(lit).has_intersection(backtracked_chunks));
    ASSERT(!lit_propagated(lit) || !lit_cross_chunks(lit).has_intersection(backtracked_chunks));
  }
#endif
}

Tlevel napsat::NapSAT::choose_backtracked_level(Tlit* learned_lits, unsigned size)
{
  ASSERT(!_options.graph_backtracking);
#ifndef NDEBUG
  // The first literal of the clause is at the highest level
  for (unsigned i = 1; i < size; i++) {
    ASSERT(lit_level(learned_lits[i]) <= lit_level(learned_lits[0]));
  }
#endif
  if (_options.lazy_strong_chronological_backtracking) {
    Tlevel highest_virtual_level = LEVEL_ROOT;
    for (unsigned i = 0; i < size; i++) {
      Tlit lit = learned_lits[i];
      Tlevel lit_lvl = lit_level(lit);
      if (lit_lazy_reason(lit) != CLAUSE_UNDEF && lit_lazy_level(lit) < lit_lvl) {
        lit_lvl = lit_lazy_level(lit);
      }
      highest_virtual_level = std::max(highest_virtual_level, lit_lvl);
    }
    if (highest_virtual_level == LEVEL_ROOT) {
      return LEVEL_UNDEF;
    }
    return highest_virtual_level - 1;
  }

  if (size == 0) {
    // If the learned clause is empty, we can backtrack to the root level
    return LEVEL_UNDEF;
  }
  if (_options.chronological_backtracking) {
    return lit_level(learned_lits[0]) - 1;
  }
  if (size == 1) {
    // If the learned clause is a unit clause, we can backtrack to the level of the literal
    return LEVEL_ROOT;
  }
  return lit_level(learned_lits[1]);
}

void napsat::NapSAT::compute_chunk_combination(Tclause cl, vector<bitset>& combinations, const bitset& current)
{
  bitset clause_set(_n_allocated_chunks);
  ASSERT(cl != CLAUSE_UNDEF);
  for (unsigned i = 0; i < _clauses[cl].size; i++) {
    Tlit lit = _clauses[cl].lits[i];
    if (lit_true(lit)) {
      ASSERT_MSG (i == 0, "Expected first literal to be true. But got " + clause_to_string(cl));
      continue;
    }
    ASSERT(lit_false(lit));
    clause_set |= lit_chunks(lit);
  }

  // in practice, if the clause_set is empty, we can conclude unsatisfiable. But then the proof construction struggles.
  // for that reason, we allow this last backtrack step
  if ((clause_set - current).empty() && lit_true(_clauses[cl].lits[0])) {
    combinations.push_back(current);
  }

  for (auto i = clause_set.cbegin(); i != clause_set.cend(); ++i) {
    // break cycles
    if (current[*i]) {
      continue;
    }

    bitset to_add = current;
    to_add.set(*i, true);
    Tclause missed_implication = var_lazy_reason(_chunks[*i].decision);
    if (missed_implication == CLAUSE_UNDEF) {

      // check if to_add is subsumed by some bitvectors in combinations
      bool subsumed = false;
      for (const auto& comb : combinations) {
        if (to_add > comb) {
          subsumed = true;
          break;
        }
      }
      if (subsumed) {
        continue;
      }

      // check if to_add subsumes some bitvectors in combinations
      for (size_t i = 0; i < combinations.size(); i++) {
        if (combinations[i] > to_add) {
          combinations[i--] = combinations.back();
          combinations.pop_back();
        }
      }

      // finally add to_add
      combinations.push_back(to_add);
      continue;
    }
    // there is a missed implication, we need to consider other chunks
    compute_chunk_combination(missed_implication, combinations, to_add);

    // the chunk is a missed implication
    ASSERT_MSG(missed_implication != CLAUSE_UNDEF,
      "Missed implication is undefined for chunk " << *i);
    // NOTIFY_OBSERVER(_observer, new napsat::gui::marker("Computed Chunk Combination Recursive for chunk " + std::to_string(*i)));
  }
}

static unsigned estimate_backtrack_cost(Tlit lits) {
  return 1;
}

bitset napsat::NapSAT::choose_analyzed_chunk(Tclause conflict, const vector<bitset>& possible_set_of_chunks) {
  ASSERT(conflict != CLAUSE_UNDEF);
  ASSERT(_options.graph_backtracking);
  ASSERT(_clauses[conflict].size > 0);

  vector<bitset> possible_set_of_chunks_copy = possible_set_of_chunks;

  if (possible_set_of_chunks_copy.empty()) {
    // all literals must either be at root level, or the decision is reimplied at level 0
    ASSERT(lit_level(_clauses[conflict].lits[0]) == LEVEL_ROOT);
    return bitset(_n_allocated_chunks);
  }

  vector<double> weights(possible_set_of_chunks_copy.size());
  vector<Tlevel> chunks_level(possible_set_of_chunks_copy.size());
  vector<double> penalty(possible_set_of_chunks_copy.size());
  for (size_t i = 0; i < possible_set_of_chunks_copy.size(); i++) {
    Tlevel level = LEVEL_UNDEF;
    for (auto j = possible_set_of_chunks_copy[i].cbegin(); j != possible_set_of_chunks_copy[i].cend(); ++j) {
      level = std::min(level, var_level(_chunks[*j].decision));
    }
    chunks_level[i] = level;

    if (conflict_has_one_literal_in_chunks(conflict, possible_set_of_chunks_copy[i])) {
      penalty[i] = _options.conflict_penalty;
    } else {
      penalty[i] = 1.0;
    }
  }

  double min_weight = std::numeric_limits<double>::max();
  bitset lightest_chunk_set;

  for (size_t i = _trail.size(); i-- > 0;) {
    if (possible_set_of_chunks_copy.size() == 0) {
      break;
    }

    Tlit lit = _trail[i];
    // if it is a decision, check if we have finished calculating some sets
    if (lit_decision(lit)) {
      Tlevel level = lit_level(lit);
      for (size_t j = 0; j < possible_set_of_chunks_copy.size(); j++) {
        if (chunks_level[j] == level) {
          weights[j] += estimate_backtrack_cost(lit) * penalty[j];
          // we have finished calculating the weight of this set
          if (weights[j] < min_weight) {
            min_weight = weights[j];
            lightest_chunk_set = possible_set_of_chunks_copy[j];
          }
          if (weights[j] == min_weight) {
            NOTIFY_OBSERVER(_observer, new napsat::gui::stat("Weight tie"));
          }
          possible_set_of_chunks_copy[j] = possible_set_of_chunks_copy.back();
          chunks_level[j] = chunks_level.back();
          weights[j] = weights.back();
          possible_set_of_chunks_copy.pop_back();
          chunks_level.pop_back();
          weights.pop_back();
          j--;
        }
      }
    }

    // update the calculated weights of the right chunks
    bitset& chunks = lit_chunks(lit);
    unsigned lit_weight = _backtrack_cost_estimator(lit);
    for (size_t j = 0; j < possible_set_of_chunks_copy.size(); j++) {
      if (!possible_set_of_chunks_copy[j].has_intersection(chunks)) {
        continue;
      }
      weights[j] += lit_weight * penalty[j];
      if (weights[j] > min_weight) {
        // remove the chunks
        possible_set_of_chunks_copy[j] = possible_set_of_chunks_copy.back();
        chunks_level[j] = chunks_level.back();
        weights[j] = weights.back();
        possible_set_of_chunks_copy.pop_back();
        chunks_level.pop_back();
        weights.pop_back();
        j--;
      }
    }
  }

  ASSERT(possible_set_of_chunks_copy.empty());

  return lightest_chunk_set;
}

bool napsat::NapSAT::conflict_has_one_literal_in_chunks(Tclause conflict, bitset& chunks)
{
  bool found = false;
  Tlit* lits = _clauses[conflict].lits;
  for (unsigned i = 0; i < _clauses[conflict].size; i++) {
    if (chunks.has_intersection(lit_chunks(lits[i]))) {
      if (found)
        return false;
      found = true;
    }
  }
  ASSERT(found);
  return found;
}

bool napsat::NapSAT::conflict_has_one_literal_at_highest_level(Tclause conflict)
{
  ASSERT(conflict != CLAUSE_UNDEF);
  ASSERT(_clauses[conflict].size > 0);
  Tlit* lits = _clauses[conflict].lits;
  Tlevel high_lvl = LEVEL_ROOT;
  unsigned count = 0;
  for (unsigned i = 0; i < _clauses[conflict].size; i++) {
    Tlevel lvl = lit_level(lits[i]);
    if (lit_lazy_reason(lits[i]) != CLAUSE_UNDEF) {
      lvl = lit_lazy_level(lits[i]);
    }
    count += lvl == high_lvl;
    if (lvl > high_lvl) {
      high_lvl = lvl;
      count = 1;
    }
  }
  return count == 1;
}

void napsat::NapSAT::fix_watched_literals(Tclause conflict)
{
  // In chronological backtracking, it might be the case that the second highest literal is not at the second position.
  Tlit* lits = _clauses[conflict].lits;
  // We need to ensure that it becomes the second watched literal
  Tlit* end = lits + _clauses[conflict].size;

  // If could be that the undefined literal is not the first one
  // We need to find the undefined literal and put it at the first position
  Tlit* undef_lit = nullptr;
  for (Tlit* i = lits; i < end; i++) {
    if (lit_undef(*i)) {
      undef_lit = i;
      break;
    }
  }
  ASSERT(undef_lit != nullptr);
  ASSERT(lit_undef(*undef_lit));
  if (undef_lit == lits + 1) {
    // just swap the first two literals
    Tlit tmp = lits[0];
    lits[0] = lits[1];
    lits[1] = tmp;
  } else if (undef_lit > lits + 1) {
    // the undefined literal is somewhere else in the clause. We need to change the watched literals
    stop_watch(lits[0], conflict);
    Tlit tmp = lits[0];
    lits[0] = *undef_lit;
    *undef_lit = tmp;
    watch_lit(lits[0], conflict);
  }

  if (_options.graph_backtracking) {
    // Now, we bring a literal that is at the top of the chunk lattice to the second position
    // This is similar to the highest level in chronological backtracking, but we use the chunk instead
    Tlit* high_lit = lits + 1;
    unsigned chunk_count = lit_chunks(*high_lit).count();
    for (Tlit* i = lits + 2; i < end; i++) {
      unsigned curr_chunk_count = lit_chunks(*i).count();
      if (curr_chunk_count > chunk_count) {
        chunk_count = curr_chunk_count;
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

  } else { // graph backtracking
    // Now, we bring a literal that is at the top of the chunk lattice to the second position
    // This is similar to the highest level in chronological backtracking, but we use the chunk instead
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

}

bool napsat::NapSAT::lit_is_required_in_learned_clause(Tlit lit)
{
  if (lit_decision(lit))
    return true;
  ASSERT(lit_reason(lit) < _clauses.size());
  TSclause& clause = _clauses[lit_reason(lit)];
  ASSERT_MSG(!clause.deleted,
    "Literal: " + lit_to_string(lit) + "\nClause: " + clause_to_string(lit_reason(lit)));
  for (unsigned i = 1; i < clause.size; i++)
    if (!lit_marked(clause.lits[i]))
      return true;
  return false;
}

bool napsat::NapSAT::analyzed_level_or_chunk(Tlit lit, Tlevel level, const bitset& chunks)
{
  return (_options.graph_backtracking && lit_chunks(lit).has_intersection(chunks))
      || (!_options.graph_backtracking && lit_level(lit) == level);
}

void NapSAT::analyze_conflict(Tlevel level, const bitset& chunks) {
  // Computes the UIP at a given level
  // We assume that the level is a top level of the clause. That is, the level is is not reachable from a literal l at a higher level in the clause.
  ASSERT_MSG(level <= solver_level(),
             "Analyzing conflict at level " + to_string(level) + " but current level is " + to_string(solver_level()));

#ifndef NDEBUG
  // check that all variables are unmarked
  for (unsigned i = 0; i < _vars.size(); i++) {
    Tvar var = Tvar(i);
    ASSERT_MSG(!var_marked(var), "Variable " + to_string(var) + " is marked");
  }
#endif

  unsigned count = 0;

  for (unsigned i = 0; i < _next_literal_index; i++) {
    Tlit lit = _literal_buffer[i];
    ASSERT(lit_false(lit));
    if (!lit_marked(lit) && analyzed_level_or_chunk(lit, level, chunks)) {
      // it could be that the literal is duplicated in the buffer
      // in this case, we do not want to count it twice
      count++;
    }
    lit_mark(lit);
  }

  // We need to clear the literal buffer now.
  // The information is held in the "marked" markers
  _next_literal_index = 0;

  ASSERT(count > 0);

  unsigned i = _trail.size();
  while (i > 0) {
    Tlit lit = _trail[--i];
    if (!lit_marked(lit))
      continue;
    if (!analyzed_level_or_chunk(lit, level, chunks))
      continue;
    lit_unmark(lit);
    bump_var_activity(lit_to_var(lit));
    // We already have the FUIP. No need to check the reason
    // We just need to finish collecting the literals that are marked.
    if (count == 1) {
      ASSERT(analyzed_level_or_chunk(lit, level, chunks));
      // this is the UIP
      _literal_buffer[_next_literal_index++] = lit_neg(lit);
      break;
    }
    // mark the literals of the reason
    Tclause reason = lit_reason(lit);
    if (lit_lazy_reason(lit) != CLAUSE_UNDEF) {
      reason = lit_lazy_reason(lit);
      // if we use the lazy reason, we need to ensure that we will look at all the literals in the clause
      // since the missed lower implication does not satisfy the trail invariant, we need to push the reading head to the back
      // note that this may lead to duplicate literals in the learned clause, but this will be cleaned up in the "internal_add_clause" function
      i = _trail.size();
    }
    ASSERT_MSG(reason != CLAUSE_UNDEF, "Literal: " + lit_to_string(lit) + " at level: " + to_string(level) + " has no reason. The count is: " + to_string(count));
    if (_proof)
      _proof->link_resolution(lit_neg(lit), reason);
    ASSERT_MSG(reason != CLAUSE_UNDEF, "Literal: " + lit_to_string(lit) + " at level: " + to_string(level) + " has no reason. The count is: " + to_string(count));
    Tlit* reason_lits = _clauses[reason].lits;
    for (unsigned j = 1; j < _clauses[reason].size; j++) {
      Tlit reason_lit = reason_lits[j];
      if (lit_marked(reason_lit))
        continue;
      lit_mark(reason_lit);
      if (analyzed_level_or_chunk(reason_lit, level, chunks))
        count++;
    }
    count--;
  }
  ASSERT(count == 1);


  // collect all the marked literals
  for (size_t i = 0; i < _trail.size(); i++) {
    Tlit lit = _trail[i];
    if (!lit_marked(lit))
      continue;
    lit_unmark(lit);
    bump_var_activity(lit_to_var(lit));
    if(lit_is_required_in_learned_clause(lit)) {
      _literal_buffer[_next_literal_index++] = lit_neg(lit);
    } else {
      if (_proof) {
        _proof->link_resolution(lit_neg(lit), lit_reason(lit));
      }
    }
  }

  // clean up the literals at level 0
  if (_proof) {
    prove_root_literal_removal(_literal_buffer, _next_literal_index);
  }

  unsigned k = 0;
  for (unsigned i = 0; i < _next_literal_index; i++) {
    Tlit lit = _literal_buffer[i];
    if (lit_level(lit) == LEVEL_ROOT) {
      // we do not want to add the root literals to the learned clause
      continue;
    }
    _literal_buffer[k++] = lit;
  }
  _next_literal_index = k;



  // ensure that the UIP is the first literal in the buffer
  // note that in NCB, this is automatically the case. The loop will fail at the first iteration
  for (unsigned i = 0; i < _next_literal_index; i++) {
    Tlit lit = _literal_buffer[i];
    ASSERT(lit_false(lit));
    if (lit_level(lit) == level) {
      // this is the UIP
      _literal_buffer[i] = _literal_buffer[0];
      _literal_buffer[0] = lit;
      break;
    }
  }
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
    if (!lit_marked(lits[i])) {
      count++;
    }
    lit_mark(lits[i]);
  }

  unsigned i = _trail.size() - 1;
  while (count != 0) {
    while (!lit_marked(_trail[i]))
      i--;
    Tlit lit = _trail[i];
    ASSERT(lit_level(lit) == LEVEL_ROOT);
    ASSERT(lit_reason(lit) != CLAUSE_UNDEF);
    Tclause reason = lit_reason(lit);
    ASSERT(reason != CLAUSE_UNDEF);
    _proof->link_resolution(lit_neg(lit), reason);

    for (unsigned j = 1; j < _clauses[reason].size; j++) {
      Tlit lit = _clauses[reason].lits[j];
      ASSERT(lit_false(lit));
      if (lit_marked(lit))
        continue;
      lit_mark(lit);
      count++;
    }
    count--;
    lit_unmark(lit);
  }
}

void napsat::NapSAT::repair_unary_clause_conflict(Tclause conflict)
{
  ASSERT(conflict != CLAUSE_UNDEF);
  ASSERT(_clauses[conflict].size == 1);
  Tlevel backtrack_level = LEVEL_ROOT;
  Tlit lit = _clauses[conflict].lits[0];
  if (_options.graph_backtracking) {
    vector<bitset> possible_set_of_chunks;
    compute_chunk_combination(conflict, possible_set_of_chunks, bitset(_n_allocated_chunks));
    bitset undone_chunks = choose_analyzed_chunk(conflict, possible_set_of_chunks);
    if (undone_chunks.empty()) {
      // The literal does not belong to any chunk, therefore, it does not depend on a decision and the conflict cannot be repaired
      _status = UNSAT;
      return;
    }
    bitset chunks(_n_allocated_chunks);
    if (_clauses[conflict].size > 1) {
      Tlit* lits = _clauses[conflict].lits;
      lit_cross_chunks(lits[0]) |= undone_chunks; // ensure that the cross chunks are set
      lit_cross_chunks(lits[1]) |= undone_chunks; // ensure that the cross chunks are set
    }
    undo_chunks(undone_chunks);
    ASSERT(lit_undef(lit));
  } else {
    if (_options.chronological_backtracking)
      backtrack_level = lit_level(lit) - 1;
    backtrack(backtrack_level);
    // In strong chronological backtracking, the literal might have been implied again during reimplication
    // Therefore, we might need to trigger another conflict analysis
    ASSERT(_options.lazy_strong_chronological_backtracking || lit_undef(lit));
    if (!lit_undef(lit)) {
      // the problem is unsat
      // The literal could have been propagated by the reimplication
      _status = UNSAT;
      return;
    }
  }
  imply_literal(lit, conflict);
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
  ASSERT_MSG(_options.chronological_backtracking || _options.graph_backtracking
          || _clauses[conflict].external
  || (lit_level(lits[0]) == solver_level()
   && lit_level(lits[1]) == solver_level()),
    "Conflict: " + clause_to_string(conflict) + "\nDecision level: " + to_string(solver_level()));
#ifndef NDEBUG
  for (unsigned i = 0; i < _clauses[conflict].size; i++) {
    ASSERT(lit_false(lits[i]));
    ASSERT(_options.graph_backtracking || lit_level(lits[i]) <= lit_level(lits[0]));
  }
#endif

  NOTIFY_OBSERVER(_observer, new napsat::gui::conflict(conflict));
  if (_status == SAT)
    _status = UNDEF;

  bump_clause_activity(conflict);

  /********** UNIT CLAUSE **********/
  if (_clauses[conflict].size == 1) {
    repair_unary_clause_conflict(conflict);
    return;
  }

  // Check wether there is a unique literal at the highest level. If that is the case, there is no need to trigger conflict analysis

  // Copy the literals of the clause to the literal buffer
  ASSERT(!_writing_clause);
  ASSERT(_next_literal_index == 0);
  _writing_clause = true;
  for (unsigned i = 0; i < _clauses[conflict].size; i++) {
    Tlit lit = lits[i];
    ASSERT(lit_false(lit));
    _literal_buffer[_next_literal_index++] = lit;
  }

  // perform conflict analysis
  if (_proof) {
    _proof->start_resolution_chain();
    _proof->link_resolution(LIT_UNDEF, conflict);
  }

  bitset analyzed_chunks;
  Tclause identical_clause = CLAUSE_ERROR;

  bool identical = false;
  if (_options.graph_backtracking) {
    vector<bitset> possible_set_of_chunks;
    compute_chunk_combination(conflict, possible_set_of_chunks, bitset(_n_allocated_chunks));
    if (possible_set_of_chunks.empty()) {
      // all literals must either be at root level, or the decision is reimplied at level 0
      ASSERT(lit_level(_clauses[conflict].lits[0]) == LEVEL_ROOT);
      _status = UNSAT;
      if (_proof) {
        prove_root_literal_removal(_literal_buffer, _next_literal_index);
        _proof->finalize_resolution(_clauses.size(), nullptr, 0);
      }
      return;
    }

    // compute the highest chunk level
    Tlevel highest_chunk_level = LEVEL_ROOT;
    for (const auto& chunks : possible_set_of_chunks) {
      for (auto i = chunks.cbegin(); i != chunks.cend(); ++i) {
        highest_chunk_level = std::max(highest_chunk_level, var_level(_chunks[*i].decision));
      }
    }

    do {
      analyzed_chunks = choose_analyzed_chunk(conflict, possible_set_of_chunks);
      possible_set_of_chunks.erase(find(possible_set_of_chunks.begin(), possible_set_of_chunks.end(), analyzed_chunks));
      identical_clause = CLAUSE_ERROR;
      // compute the highest level of the chosen chunks
      Tlevel chunk_level = LEVEL_ROOT;
      for (auto i = analyzed_chunks.cbegin(); i != analyzed_chunks.cend(); ++i) {
        chunk_level = std::max(chunk_level, var_level(_chunks[*i].decision));
      }

      if (analyzed_chunks.empty()) {
        // The conflict does not have a literal that belongs to a chunk, therefore, it cannot be repaired
        _status = UNSAT;
        if (_proof) {
          prove_root_literal_removal(_literal_buffer, _next_literal_index);
          _proof->finalize_resolution(_clauses.size(), nullptr, 0);
        }
        return;
      }
      if (!(identical = conflict_has_one_literal_in_chunks(conflict, analyzed_chunks))) {
        analyze_conflict(LEVEL_ROOT, analyzed_chunks);
        _next_literal_index = cleanup_duplicate_literals(_literal_buffer, _next_literal_index);

        // check if the clause already exists
        for (size_t i = 0; i < _next_literal_index; i++) { lit_mark(_literal_buffer[i]); }

        // go thought the watch lists of the literals. If we find a clause that subsumes the learned clause, we do not need to add it
        for (size_t i = 0; i < _next_literal_index && !identical; i++) {
          Tlit lit = _literal_buffer[i];
          for (const TSwatch & watch : _watches[lit]) {
            if (!lit_false(watch.block) || !lit_marked(watch.block)) // all literals should be falsified
              continue;
            bool different = false;
            Tlit* other = _clauses[watch.cl].lits;
            for (size_t j = 0; j < _clauses[watch.cl].size && !different; j++) {
              different = !lit_marked(other[j]) || !lit_false(other[j]);
            }
            if (!different) {
              identical = true;
              identical_clause = watch.cl;
              NOTIFY_OBSERVER(_observer, new napsat::gui::stat("Identical clause found"));
              break;
            }
          }
        }

        for (size_t i = 0; i < _next_literal_index; i++) { lit_unmark(_literal_buffer[i]); }
      }
      if (chunk_level == highest_chunk_level) {
        // we have analyzed the chunks at the highest level, we stop here
        break;
      }
    } while (identical);
  } else if (!(identical = conflict_has_one_literal_at_highest_level(conflict))) {
    do {
      Tlevel conflict_level = lit_level(_literal_buffer[0]);
      if (conflict_level == LEVEL_ROOT) {
        // The conflict does not have a literal that belongs to a chunk, therefore, it cannot be repaired
        _status = UNSAT;
        if (_proof) {
          prove_root_literal_removal(_literal_buffer, _next_literal_index);
          _proof->finalize_resolution(_clauses.size(), nullptr, 0);
        }
        return;
      }

      analyze_conflict(conflict_level, analyzed_chunks);
      // in lazy strong chronological backtracking, if the UIP is a missed lower implication, we need to recalculate the conflict level
      Tclause lazy_reason = lit_lazy_reason(_literal_buffer[0]);
      if (lazy_reason == CLAUSE_UNDEF) {
        // The UIP is not a missed lower implication, we can stop the analysis
        break;
      }

      if (_proof)
        _proof->link_resolution(_literal_buffer[0], lazy_reason);
      // The UIP is a missed lower implication, we need to reanalyze the conflict
      // Replace the UIP by its lazy reason
      ASSERT(lit_level(_literal_buffer[0]) == conflict_level);
      _literal_buffer[0] = _literal_buffer[--_next_literal_index];
      Tlit* uip_lazy_lits = _clauses[lazy_reason].lits;
      // Note that this may introduce duplicate literals in the learned clause, but they will be cleaned up when marking the literals
      for (unsigned i = 1; i < _clauses[lazy_reason].size; i++) {
        _literal_buffer[_next_literal_index++] = uip_lazy_lits[i];
      }
      // find the highest level and bring it to the front
      for (unsigned i = 1; i < _next_literal_index; i++) {
        if (lit_level(_literal_buffer[i]) > lit_level(_literal_buffer[0])) {
          Tlit tmp = _literal_buffer[0];
          _literal_buffer[0] = _literal_buffer[i];
          _literal_buffer[i] = tmp;
        }
      }
      conflict_level = lit_level(_literal_buffer[0]);
    } while (true);
  }
  // Later, if the clause is identical, we do not add it to the clause set.

  if (_options.graph_backtracking) {
    undo_chunks(analyzed_chunks);
  } else {
    // make sure that the second literal is at the second highest level
    for (unsigned i = 2; i < _next_literal_index; i++) {
      if (lit_level(_literal_buffer[i]) > lit_level(_literal_buffer[1])) {
        Tlit tmp = _literal_buffer[1];
        _literal_buffer[1] = _literal_buffer[i];
        _literal_buffer[i] = tmp;
      }
    }
    ASSERT(_next_literal_index <= 1 || lit_level(_literal_buffer[1]) <= lit_level(_literal_buffer[0]));
    Tlevel backtrack_level = choose_backtracked_level(_literal_buffer, _next_literal_index);
    if (backtrack_level == LEVEL_UNDEF) {
      // The conflict cannot be repaired, therefore, we need to stop the search
      _status = UNSAT;
      /**
       * Note that we will still backtrack to the root level, otherwise we might falsify the invariants on watched literals.
       * indeed, the conflict may have swapped the watched literal for an already propagated literal.
       * for example, let the clause a v b v c watched by a and b not propagated.
       * if -c is propagated at level 2, and -a and -b are at level 1, then the
       * clause will change its watched literal to -c, and the clause will be
       * c v a v b, but c is propagated, hence violating strong watched literals.
       *
       * In general this is okay since we will backtrack c, but not if we interrupt
       * and do not backtrack.
       */
      backtrack(LEVEL_ROOT);
      if (_proof) {
        _proof->finalize_resolution(_clauses.size(), _literal_buffer, _next_literal_index);
      }
      return;
    }
    backtrack(backtrack_level);
  }

  if (identical) {
    if (_proof)
      _proof->cancel_resolution_chain();
    _writing_clause = false;
    _next_literal_index = 0;
    if (clause_falsified(conflict)) {
      repair_conflict(conflict);
    } else {
      if (identical_clause == CLAUSE_ERROR) {
        // the clause itself is a UIP
        fix_watched_literals(conflict);
        imply_literal(_clauses[conflict].lits[0], conflict);
      } else {
        // the clause reduces to another known clause after conflict analysis. This other clause is then the UIP cut
        fix_watched_literals(identical_clause);
        imply_literal(_clauses[identical_clause].lits[0], identical_clause);
      }
    }

  } else {
    ASSERT(lit_undef(_literal_buffer[0]));
    ASSERT(_next_literal_index == 1 || !lit_undef(_literal_buffer[1]));
    NOTIFY_OBSERVER(_observer, new napsat::gui::stat("Learned clause: "));
    Tclause learned = internal_add_clause(_literal_buffer, _next_literal_index, true, false);
    if (_proof)
      _proof->finalize_resolution(learned, _literal_buffer, _next_literal_index);

    _writing_clause = false;
    _next_literal_index = 0;
    // finalizing the clause will also imply the first literal of the clause
  }

  _var_activity_increment /= _options.var_activity_decay;
}

void NapSAT::restart()
{
  NOTIFY_OBSERVER(_observer, new napsat::gui::stat("Restart"));
  if (_options.graph_backtracking) {
    unsigned tmp = _n_propagated_lits;
    backtrack(LEVEL_ROOT);
    _n_propagated_lits = min(tmp, _n_propagated_lits);
    size_t i = _trail.size();
    while (i > 0) {
      i--;
      Tlit lit = _trail[i];
      _vars[lit_to_var(lit)].propagated = false;
      if (i < _n_propagated_lits) {
        NOTIFY_OBSERVER(_observer, new napsat::gui::remove_propagation(lit));
      }
    }
    _n_propagated_lits = 0;
  } else {
    backtrack(LEVEL_ROOT);
  }
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

unsigned napsat::NapSAT::cleanup_duplicate_literals(Tlit* lits, unsigned size)
{
#ifndef NDEBUG
  // check that all variables are unmarked
  for (unsigned i = 0; i < _vars.size(); i++) {
    ASSERT(!var_marked(Tvar(i)));
  }
#endif
  Tlit* i = lits;
  Tlit* j = i;
  Tlit* end = i + size;
  while(i < end) {
    if (lit_marked(*i)) {
      i++;
      continue;
    }
    lit_mark(*i);
    *j++ = *i++;
  }
  unsigned new_size = j - lits;
  for (unsigned k = 0; k < new_size; k++) {
    lit_unmark(lits[k]);
  }
  return new_size;
}

Tclause napsat::NapSAT::internal_add_clause(const Tlit* lits_input, const unsigned input_size, bool learned, bool external)
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
      satisfied_at_root |= lit_true(lits_input[i]);
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
    clause->size = input_size - n_removed;
  }

  // Remove duplicate literals
  if (input_size > 1) {
    clause->size = cleanup_duplicate_literals(lits, clause->size);
  }
  clause_size = clause->size;

  if (_proof && external) {
    _proof->input_clause(cl, lits_input, input_size);
    // Remove the literals falsified at level 0 in the proof
    if (n_removed > 0) {
      _proof->remove_root_literals(cl);
    }
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

  if (external && _options.ignore_unused_variables) {
    // mark all the variables in the clause as constrained
    for (unsigned i = 0; i < clause_size; i++)
      var_mark_constrained(lit_to_var(lits[i]));
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
      repair_conflict(cl);
    }
    return cl;
  }
  else if (clause_size == 2) {
    // clause->watched = false;
    _binary_watch[lits[0]].push_back(TSwatch(cl, lits[1]));
    _binary_watch[lits[1]].push_back(TSwatch(cl, lits[0]));
#if NOTIFY_WATCH_CHANGES
    NOTIFY_OBSERVER(_observer, new napsat::gui::watch(cl, lits[0]));
    NOTIFY_OBSERVER(_observer, new napsat::gui::watch(cl, lits[1]));
#endif
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
  if (_options.delete_clauses && _n_learned_clauses >= _next_clause_elimination){
    simplify_clause_set();
    // The clause we just added should not be deleted
    ASSERT(!_clauses[cl].deleted);
  }
  return cl;
}

/*****************************************************************************/
/*                            Public interface                               */
/*****************************************************************************/

napsat::NapSAT::NapSAT(unsigned n_var, unsigned n_clauses, napsat::options& options) :
  _options(options)
{
  // We have to create the observer before allocating the variables. Otherwise the notifications will not be sent
#if USE_OBSERVER
  if (options.interactive || options.observing || options.check_invariants || options.print_stats || options.print_live_stats) {
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

  _vars.resize(1);
  _trail = vector<Tlit>();
  _trail.reserve(n_var);
  _watches.resize(2 * n_var + 2);

  _clauses = vector<TSclause>();
  _clauses.reserve(n_clauses);
  _activities.reserve(n_clauses);

  // TODO replace this with an std::vector
  _literal_buffer = new Tlit[2*n_var+1];
  _next_literal_index = 0;
  var_allocate(n_var + 1);

  if (options.build_proof)
    _proof = new napsat::proof::resolution_proof();
  else
    _proof = nullptr;

  if (_options.graph_backtracking) {
    allocate_chunks(4032);
  }

  _backtrack_cost_estimator = estimate_backtrack_cost;
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
  // ASSERT(watch_lists_complete());
  // ASSERT(watch_lists_minimal());
  if (_status != UNDEF)
    return false;
  while (_n_propagated_lits < _trail.size()) {
    Tlit lit = _trail[_n_propagated_lits];
#ifdef NDEBUG
    if (lit_propagated(lit)) {
      _n_propagated_lits++;
      NOTIFY_OBSERVER(_observer, new napsat::gui::stat("Skipped Propagation"));
      NOTIFY_OBSERVER(_observer, new napsat::gui::propagation(lit));
      continue;
    }
    if (!lit_propagated(lit)) {
      lit_cross_chunks(lit).clear();
    }
#else
    if (!lit_propagated(lit)) {
      lit_cross_chunks(lit).clear();
    }
#endif
    lit_cross_chunks(lit) |= lit_chunks(lit);

    Tclause conflict = propagate_binary_clauses(lit);
    if (conflict == CLAUSE_UNDEF)
      conflict = propagate_lit(lit);
    if (conflict == CLAUSE_UNDEF) {
      _vars[lit_to_var(lit)].propagated = true;
      _n_propagated_lits++;
      NOTIFY_OBSERVER(_observer, new napsat::gui::propagation(lit));
      continue;
    }
    repair_conflict(conflict);
    if (_status == UNSAT)
      return false;
    if (!_options.no_restart &&_luby_counter.increment()) {
      restart();
    }
  }
  if (_trail.size() == _vars.size() - 1) {
    _status = SAT;
    return false;
  }
  return true;
}

static inline void print_bt_option(const options &options) {
  string bt = "non-chronological";
  if (options.chronological_backtracking)
    bt = "chronological";
  else if (options.weak_chronological_backtracking)
    bt = "weak-chronological";
  else if (options.restoring_strong_chronological_backtracking)
    bt = "restoring-strong-chronological";
  else if (options.lazy_strong_chronological_backtracking)
    bt = "lazy-strong-chronological";
  else if (options.graph_backtracking)
    bt = "graph";
  LOG_INFO("Using backtracking strategy: " + bt);
}

status NapSAT::solve()
{
  if (_status != UNDEF)
    return _status;
  print_bt_option(_options);
  while (true) {
    NOTIFY_OBSERVER(_observer, new napsat::gui::check_invariants());
    if (!propagate()) {
      if (_status == UNSAT || !_options.interactive)
        break;
      NOTIFY_OBSERVER(_observer, new napsat::gui::done(_status == SAT));
    }
    ASSERT_MSG(_n_propagated_lits == _trail.size(), "Propagation mismatch"
      "\nPropagated literals: " + to_string(_n_propagated_lits)
      + "\nTrail size: " + to_string(_trail.size()));
    NOTIFY_OBSERVER(_observer, new napsat::gui::check_invariants());
    if (_n_root_lvl_lits >= _purge_threshold
    && ((!_options.weak_chronological_backtracking && !_options.restoring_strong_chronological_backtracking && !_options.graph_backtracking)
       || solver_level() == LEVEL_ROOT)) {
      // in WCB and RSCB, missed lower implications can be a problem when purging clauses.
      // this is the same trick as in CaDiCaL, but we might be able to do better
      purge_clauses();
      _purge_threshold = _n_root_lvl_lits + _purge_inc;
      if (_status == UNSAT)
        return _status;
      // in chronological backtracking, the purge might have implied some literals
      // therefore we cannot take a decision before we propagate
      continue;
    }
    NOTIFY_OBSERVER(_observer, new napsat::gui::check_invariants());
    synchronize();
#if USE_OBSERVER
    if (_observer && _options.interactive)
      _observer->notify(new napsat::gui::checkpoint());
    else
#endif
      decide();
    ASSERT(_status != UNSAT);
    if (_status == SAT)
      break;
  }
  synchronize();
  if (_status == SAT)
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
  ASSERT(!_variable_heap.contains(0));
  while (!_variable_heap.empty() && !var_undef(_variable_heap.top()))
    _variable_heap.pop();
  if (_variable_heap.empty()) {
    _status = SAT;
    return false;
  }
  Tvar var = _variable_heap.top();

  if (var == last_backtracked_decision) {
    NOTIFY_OBSERVER(_observer, new napsat::gui::stat("Re-deciding Last Backtracked"));
  }
  ASSERT(var_constrained(var));
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
  Tclause cl = internal_add_clause(_literal_buffer, _next_literal_index, false, true);
  _writing_clause = false;
  _next_literal_index = 0;
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
  if(!_options.print_stats && !_options.print_live_stats)
    return;
  for (Tvar var = 1; var < _vars.size(); var++) {
    TSvar& v = _vars[var];
    switch (v.synced)
    {
    case 0:
    case 1:
      // do nothing
      break;
    case 2:
      NOTIFY_OBSERVER(_observer, new napsat::gui::stat("Sync unassign"));
      if (v.state == VAR_UNDEF)
        v.synced = 1;
      else {
        v.synced = 0;
        NOTIFY_OBSERVER(_observer, new napsat::gui::stat("Sync assign"));
      }
      break;
    case 3:
      ASSERT (v.state != VAR_UNDEF);
      NOTIFY_OBSERVER(_observer, new napsat::gui::stat("Sync assign"));
      v.synced = 0;
    default:
      break;
    }
  }
  _sync_validity_index = _trail.size();
}

unsigned NapSAT::sync_validity_limit()
{
  return _sync_validity_index;
}

void NapSAT::set_markup(void (*markup_function)(void))
{
  ASSERT(markup_function);
}

const std::vector<Tlit>& NapSAT::trail() const
{
  return _trail;
}

void napsat::NapSAT::print_proof()
{
  ASSERT(_proof);
  ASSERT(_status == UNSAT);
  _proof->print_proof();
}

bool napsat::NapSAT::check_proof()
{
  ASSERT(_proof);
  ASSERT(_status == UNSAT);
  return _proof->check_proof();
}

bool napsat::NapSAT::check_model(const vector<Tlit>& assignment) const
{
  vector<bool> assigned(2* _vars.size() + 2, false);
  for (const auto& lit : assignment) {
    ASSERT(!assigned[lit]);
    ASSERT(!assigned[lit_neg(lit)]);
    assigned[lit] = true;
  }
  // check that all variables are assigned
  for (Tvar var = 1; var < _vars.size(); var++) {
    if (!var_constrained(var))
      continue;
    ASSERT(assigned[literal(var, false)] || assigned[literal(var, true)]);
  }
  for (const auto& cl : _clauses) {
    bool satisfied = false;
    for (unsigned i = 0; i < cl.size && !satisfied; i++) {
      satisfied |= assigned[cl.lits[i]];
    }
    if (!satisfied) return false;
  }
  return true;
}
