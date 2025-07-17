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
  ASSERT_MSG(lit_undef(lit),
    "Literal: " + lit_to_string(lit) + "\nReason: " + clause_to_string(reason));
#ifndef NDEBUG
  if (reason != CLAUSE_UNDEF && reason != CLAUSE_LAZY && _clauses[reason].size >= 2) {
    Tlit lit1 = _clauses[reason].lits[1];
    if (_options.graph_backtracking) {
      for (unsigned i = 2; i < _clauses[reason].size; i++) {
        Tlit lit_i = _clauses[reason].lits[i];
        ASSERT(lit_false(lit_i));
        ASSERT_MSG(!(lit_chunks(lit_i) > lit_chunks(lit1)),
                   "Literal: " + lit_to_string(lit_i) +
                   "\nReason: " + clause_to_string(reason) +
                   "\nChunk: " + lit_chunks(lit_i).to_string() +
                   "\nSVar chunks: " + _vars[lit_to_var(lit1)].chunks.to_string());
      }
    } else {
      for (unsigned i = 2; i < _clauses[reason].size; i++) {
        Tlit lit_i = _clauses[reason].lits[i];
        ASSERT(lit_false(lit_i));
        ASSERT(lit_level(lit_i) <= lit_level(lit1));
      }
    }
  }
#endif

  Tvar var = lit_to_var(lit);
  _trail.push_back(lit);
  TSvar& svar = _vars[var];
  svar.state = lit_pol(lit);
  svar.propagated = false;
  svar.reason = reason;

  _agility *= _options.agility_decay;
  _options.agility_threshold *= _options.threshold_multiplier;

  if (reason == CLAUSE_UNDEF) {
    // Decision
    _decision_index.push_back(_trail.size() - 1);
    svar.level = solver_level();
    NOTIFY_OBSERVER(_observer, new napsat::gui::decision(lit));
    if (_options.graph_backtracking) {
      if (_free_chunks.empty()) {
        ASSERT(_chunks.size() == _n_allocated_chunks);
        _chunks.resize(_n_allocated_chunks * 2);

        for (Tchunk i = 0; i < _n_allocated_chunks; i++) {
          _free_chunks.push_back(i + _n_allocated_chunks);
        }
        _n_allocated_chunks *= 2;
        // resize the chunk sets of the variables
        for (Tvar i = 0; i < _vars.size(); i++) {
          _vars[i].chunks.resize(_n_allocated_chunks);
          _vars[i].cross_chunks.resize(_n_allocated_chunks);
        }
      }
      Tchunk chunk_number = _free_chunks.back();
      _free_chunks.pop_back();
      svar.chunks.set(chunk_number, true);
      _chunks[chunk_number].weight = 1;
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
    ASSERT(lit == _clauses[reason].lits[0]);
    if (_clauses[reason].size == 1)
      svar.level = LEVEL_ROOT;
    else {
      ASSERT(lit == _clauses[reason].lits[0]);
      if (_options.graph_backtracking) {
        // In graph backtracking we do not have any information about the levels
        // we need to compute it by going through the clause
        svar.level = lit_level(_clauses[reason].lits[1]);
        for (unsigned i = 2; i < _clauses[reason].size; i++) {
          svar.level = std::max(svar.level, lit_level(_clauses[reason].lits[i]));
        }

        // compute the chunks and cross-chunks of the variable
        bitvector& chunks = svar.chunks;
        ASSERT(chunks.empty());
        for (unsigned i = 1; i < _clauses[reason].size; i++) {
          chunks |= lit_chunks(_clauses[reason].lits[i]);
        }
        bitvector& cross_chunks = svar.cross_chunks;
        ASSERT(cross_chunks.empty());
        cross_chunks = svar.chunks - lit_chunks(_clauses[reason].lits[1]);

        // update the size of the chunk
        for (Tchunk chunk = 0; chunk < _n_allocated_chunks; chunk++) {
          if (svar.chunks[chunk]) {
            _chunks[chunk].weight++;
          }
        }
      } else {
        svar.level = lit_level(_clauses[reason].lits[1]);
      }
#ifndef NDEBUG
      // check that the second literal is at the highest level
      if (_options.graph_backtracking) {
        for (unsigned i = 2; i < _clauses[reason].size; i++) {
          bitvector& lit_chunk = lit_chunks(_clauses[reason].lits[i]);
          bitvector& svar_chunks = svar.chunks;
          ASSERT_MSG(!(lit_chunk > svar_chunks),
            "Literal: " + lit_to_string(_clauses[reason].lits[i]) +
            "\nReason: " + clause_to_string(reason) +
            "\nChunk: " + lit_chunk.to_string() +
            "\nSVar chunks: " + svar_chunks.to_string());
        }
      } else {
        for (unsigned i = 2; i < _clauses[reason].size; i++) {
          ASSERT(lit_level(_clauses[reason].lits[i]) <= svar.level);
        }
      }
#endif
    }
    NOTIFY_OBSERVER(_observer, new napsat::gui::implication(lit, reason, svar.level));
  }

  // phase caching
  if (lit_pol(lit) != svar.phase_cache)
    _agility += 1 - _options.agility_decay;
  svar.phase_cache = lit_pol(lit);

  if (svar.level == LEVEL_ROOT) {
    _n_root_lvl_lits++;
    if (_proof)
      _proof->root_assign(lit, reason);
  }
  ASSERT(svar.level != LEVEL_UNDEF);
  ASSERT(svar.level <= solver_level());

}

void NapSAT::var_unassign(Tvar var)
{
  TSvar& v = _vars[var];
  NOTIFY_OBSERVER(_observer,
                  new napsat::gui::unassignment(literal(var, v.state)));
                  if (v.missed_lower_implication != CLAUSE_UNDEF) {
                    NOTIFY_OBSERVER(_observer,
                    new napsat::gui::remove_lower_implication(var));
    v.missed_lower_implication = CLAUSE_UNDEF;
  }
  if (!_variable_heap.contains(var))
    _variable_heap.insert(var, v.activity);

  if (_options.graph_backtracking) {
    // decrement the weight of the chunks and reset the value
    for (Tchunk i = 0; i < v.chunks.size(); i++) {
      if (v.chunks[i]) {
        _chunks[i].weight--;
        if (_chunks[i].weight == 0) {
          ASSERT(v.reason == CLAUSE_UNDEF);
          _free_chunks.push_back(i);
          _chunks[i].decision = LIT_UNDEF;
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

Tlit* napsat::NapSAT::quick_replacement(Tlit* lits, unsigned size) {
  ASSERT(size >= 2);
  ASSERT(lit_false(lits[1]));
  // This must be as efficient as possible!
  Tlit* end = lits + size;
  Tlit* k = lits + 1;
  while (++k < end)
    if (!lit_false(*k))
      return k;
  return lits + 1;
}

Tlit* napsat::NapSAT::graph_replacement(Tlit* lits, unsigned size)
{
  Tlit* end = lits + size;
  Tlit* top_element = lits + 1;
  Tlit* k = lits + 1;
  const bitvector* top_elem_chunk = &lit_chunks(lits[1]);
  while (++k < end) {
    ASSERT(lit_false(*k));
    const bitvector* dep = &lit_chunks(*k);
    if (*dep > *top_elem_chunk) {
      top_element = k;
      top_elem_chunk = dep;
    }
  }
  return top_element;
}

Tlit* napsat::NapSAT::advanced_replacement(Tlit* lits, unsigned size)
{
  /**
   * Pre conditions:
   * - The set of literals C = {c₂, ¬ℓ, ..., cₙ} has more than two literals
   *    |C| > 2
   * - The second literal ¬ℓ of the clause is falsified but not yet propagated
   *    ℓ ∈ ω
   */
  if (!_options.chronological_backtracking) {
    return lits + 1;
  }
  ASSERT(size >= 2);
  ASSERT(lit_false(lits[1]));
  // This must be as efficient as possible!
  Tlit* end = lits + size;
  Tlit* k = lits + 2;
  Tlevel high_lvl = lit_level(lits[1]);
  Tlit* high_lit = lits + 1;
  while (k < end) {
    ASSERT(lit_false(*k))
    if (lit_level(*k) > high_lvl) {
      // in non-chronological backtracking, the watched literals are always at the highest level
      ASSERT(_options.chronological_backtracking);
      high_lvl = lit_level(*k);
      high_lit = k;
    }
    k++;
  }
  return high_lit;
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
      if (_options.graph_backtracking &&
          !(lit_chunks(bin.first) <= lit_chunks(lit))) {
        lit_cross_chunks(lit) |= (lit_chunks(lit) - lit_chunks(bin.first));
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
    ASSERT(_options.chronological_backtracking || _options.graph_backtracking
           || lit_level(bin.first) == lit_level(lit));
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
    if (_options.graph_backtracking) {
      // make sure that the first element is at the top of the lattice
      if (lit_chunks(bin.first) < lit_chunks(lit)) {
        // in place swapping
        bin.first ^= lit;
        lit ^= bin.first;
        bin.first ^= lit;
        // we do not need to update the next watched clause because the clause is binary
      }
    }
    ASSERT(_options.graph_backtracking || lit_level(_clauses[bin.second].lits[0]) >= lit_level(_clauses[bin.second].lits[1]));
    return bin.second;
  }
  return CLAUSE_UNDEF;
}

Tclause NapSAT::propagate_lit(Tlit lit)
{
  // ASSERT(watch_lists_complete());
  // ASSERT(watch_lists_minimal());
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
   * - NCB:  ¬c₁ ∈ τ ⇒ c₂ ∈ π
   *                 ∨ b ∈ π
   *        δ(b) ≤ δ(c₂) is trivially true in NCB since the levels are monotonically increasing
   * - WCB:  ¬c₁ ∈ τ ⇒  ¬c₂ ∉ τ
   *                 ∨ [b ∈ π ∧ δ(b) ≤ δ(c₁)]
   *        We actually want to enforce ¬c₁ ∈ τ ⇒ c₂ ∈ π ∨ [b ∈ π ∧ δ(b) ≤ δ(c₁)] but cannot
   *        guarantee that this will hold after backtracking.
   * - LSCB: ¬c₁ ∈ τ ⇒ [c₂ ∈ π ∧ [δ(c₂) ≤ δ(c₁) ∨ δ(λ(c₂) \ {c₂}) ≤ δ(c₁)]
   *                 ∨ [b ∈ π ∧ δ(b) ≤ δ(c₁)]
   * - GB:   ¬c₁ ∈ τ ⇒ [c₂ ∈ π ∧ γ(c₁) ⊆ γ(c₂) ∪ η(c₂)]
   *                 ∨ [b ∈ π ∧ γ(b) ⊆ γ(ℓ)]
   *
   * We initialise F* with all the clauses that are not watched by ¬ℓ. If they satisfied the invariant
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
   * - GB:   ¬c₁ ∈ (τ ⋅ ℓ) ⇒ [c₂ ∈ π ∧ γ(c₁) ⊆ γ(c₂) ∪ η(c₂)]
   *                       ∨ [b ∈ π ∧ γ(b) ⊆ γ(ℓ)]
   *
   * If Q is true, then we can add C to F* and we know that the invariants are preserved.
   * If we cannot make Q true, then there is a conflict and we can return C.
   *
   * If we have not returned a conflict, at then end of the loop, we will have explored
   * all the clauses watched by ¬ℓ and F* = F. Therefore, we satisfy our contract.
   */
  const bitvector& chunks_lit = lit_chunks(lit);
  while (i < end) {
    Tclause cl = *i;
    TSclause& clause = _clauses[cl];
    ASSERT(clause.watched);
    ASSERT(clause.size >= 2);
    // Skip condition before dereferencing the pointers
    if (lit_true(clause.blocker)
      && (!_options.chronological_backtracking || lit_level(clause.blocker) <= lvl)
      && (!_options.graph_backtracking || lit_chunks(clause.blocker) <= chunks_lit)) {
      /**
       * NCB: b ∈ π
       * WCB: b ∈ π ∧ δ(b) ≤ δ(c₁)
       * SCB: b ∈ π ∧ δ(b) ≤ δ(c₁)
       * GB:  b ∈ π ∧ γ(b) ⊆ γ(ℓ)
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
      && (!_options.lazy_strong_chronological_backtracking || lit_level(lit2) <= lvl)
      && (!_options.graph_backtracking || lit_chunks(lit2) <= chunks_lit)) {
      /**
       * NCB: c₂ ∈ π
       * WCB: c₂ ∈ π
       * SCB: c₂ ∈ π ∧ δ(c₂) ≤ δ(c₁)
       * GB:  c₂ ∈ π ∧ γ(c₁) ⊆ γ(c₂) ∪ η(c₂)
       * the invariants are preserved without any action
       */
      i++;
      continue;
    }
    /** SEARCH REPLACEMENT **/
    Tlit* replacement = quick_replacement(lits, clause.size);
    /**
     * Quick replacement returns a non-falsified literal r ∈ C \ {c₂} if such a literal exists.
     */

    /**
     * Search replacement returns a literal r ∈ C \ {c₂} such that it either is a good replacement
     * such that
     *   ¬r ∈ (τ ⋅ ¬c₁) ⇒ C \ {c₂}, π ⊧ ⊥
     * NCB: If c₂ ∈ π, then we would have stopped at the skip conditions
     *      We know that [C \ {c₂}, π ⊧ ⊥] ⇒ δ(c₁) = δ(c₂) = δ(C) and c₁ will be returned
     *      if C \ {c₂} is conflicting
     *
     * We know that
     * ALL: [¬r ∈ (τ ⋅ ¬c₁) ⇒ C \ {c₂}, π ⊧ ⊥]
     * NCB:  c₂ ∉ π                  ∧  b ∉ π
     * WCB:  c₂ ∉ π                  ∧ [b ∉ π ∨ δ(b) > δ(c₁)]
     * SCB: [c₂ ∉ π ∨ δ(c₂) > δ(c₁)] ∧ [b ∉ π ∨ δ(b) > δ(c₁)]
    */
    ASSERT(replacement != nullptr);

    Tlevel replacement_lvl = lit_level(*replacement);

    ASSERT_MSG(_options.chronological_backtracking || _options.graph_backtracking
           || (!lit_true(*replacement) || replacement_lvl <= lvl),
      "Clause: " + clause_to_string(cl) + "\nLiteral: " + lit_to_string(lit) + "\nReplacement: " + lit_to_string(*replacement) + "\nLevel: " + to_string(lvl));
    /** TRUE literal **/
    if (lit_true(*replacement)
    && ((!_options.graph_backtracking && replacement_lvl <= lvl)
     || (_options.graph_backtracking && lit_chunks(*replacement) <= chunks_lit))) {
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
     * We now perform a more advanced search for a replacement.
     * In case of NCB, this is not necessary because we are certain that c₂ is at the highest level
     */
    if (_options.graph_backtracking) {
      replacement = graph_replacement(lits, clause.size);
    }
    else if (_options.chronological_backtracking) {
      replacement = advanced_replacement(lits, clause.size);
    }
    replacement_lvl = lit_level(*replacement);
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
      ASSERT(_options.chronological_backtracking || _options.graph_backtracking);
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
     * NCB:  c₂ ∉ π                  ∧  b ∉ π
     * WCB:  c₂ ∉ π                  ∧ [b ∉ π ∨ δ(b) > δ(c₁)]
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
      // ASSERT(watch_lists_complete());
      // ASSERT(watch_lists_minimal());
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
    if (lit_level(lit2) <= replacement_lvl
    && (!_options.graph_backtracking || lit_chunks(lit2) <= lit_chunks(lit))) {
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
    ASSERT(_options.lazy_strong_chronological_backtracking || _options.graph_backtracking);
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
    if (_options.lazy_strong_chronological_backtracking) {
      reimply_literal(lit2, cl);
    }
    else {
      // We are in graph backtracking, so we do not need to reimply the literal
      // but we will need to repropagate the literal if one of the other literals in the clause are backtracked
      bitvector chunks_to_update(_n_allocated_chunks);
      for (unsigned i = 0; i < clause.size; i++) {
        chunks_to_update |= lit_chunks(lits[i]);
      }
      // TODO: the setminus is not necessary, but is good for debugging
      lit_cross_chunks(lit) |= chunks_to_update - lit_chunks(lit);
    }
    /**
     * We now have in addition that δ(λ(c₂) \ {c₂}) ≤ δ(c₁)
     * that satisfies
     * ¬c₁ ∈ (τ ⋅ ℓ) ⇒ [c₂ ∈ π ∧ [δ(c₂) ≤ δ(c₁) ∨ δ(λ(c₂) \ {c₂}) ≤ δ(c₁)]
     *               ∨ [b ∈ π ∧ δ(b) ≤ δ(c₁)]
     */
    // don't increment. We would have done so earlier if we did not change the watched literals
  }

  watch_list.resize(end - watch_list.data());
  // ASSERT(watch_lists_complete());
  // ASSERT(watch_lists_minimal());
  return CLAUSE_UNDEF;
}

void napsat::NapSAT::backtrack(Tlevel level)
{
  ASSERT_MSG(level <= solver_level(),
             "Backtracking to level " + to_string(level) + " but current level is " + to_string(solver_level()));
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
      waiting_count += !_vars[var].propagated;
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

  ASSERT_MSG(_options.chronological_backtracking || waiting_count == 0,
             "Waiting count: " + to_string(waiting_count) + "\nLevel: " + to_string(level) + "\nRestore point: " + to_string(restore_point));
  _propagated_literals = _trail.size() - waiting_count;
  ASSERT_MSG(_options.chronological_backtracking || _options.graph_backtracking || _propagated_literals == restore_point,
    "Propagated literals: " + to_string(_propagated_literals) + "\nRestore point: " + to_string(restore_point));
  // in RSCB we need to move the propagation head back to the location of the first literal that moved
  // that is, the location of the first literal that was unassigned.
  if (_options.restoring_strong_chronological_backtracking) {
    while (_propagated_literals > restore_point) {
      Tlit lit = _trail[_propagated_literals - 1];
      Tvar var = lit_to_var(lit);
      ASSERT_MSG(_vars[var].propagated,
                  "Literal: " + lit_to_string(lit) + "\nLevel: " + to_string(lit_level(lit)));
      _vars[var].propagated = false;
      _propagated_literals--;
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

void napsat::NapSAT::undo_chunk(Tchunk chunk)
{
  ASSERT(chunk < _n_allocated_chunks);
  ASSERT(_chunks[chunk].weight > 0);
  ASSERT(_backtracked_variables.empty());

  Tvar chunk_decision = _chunks[chunk].decision;
  Tlevel decision_level = var_level(chunk_decision);

  Tlit* i = _trail.data();
  Tlit* j = i;
  Tlit* end = i + _trail.size();
  Tlevel decision_counter = LEVEL_ROOT;
  while (i < end) {
    bitvector& chunks = lit_chunks(*i);
    if (chunks.get(chunk)) {
      // we need to unassign the variable
      _backtracked_variables.push_back(lit_to_var(*i));
      if (lit_propagated(*i) && j - _trail.data() < _propagated_literals) {
        // it could be that a variable is marked as propagated but is behind the propagation head
        // in that case, we do not need to move the propagation head
        _propagated_literals--;
        ASSERT(_propagated_literals <= _trail.size());
      }
      i++;
      continue;
    }
    if (lit_reason(*i) == CLAUSE_UNDEF) {
      _decision_index[decision_counter++] = j - _trail.data();
    }
    *(j++) = *(i++);
  }
  ASSERT_MSG(decision_counter == _decision_index.size() - 1,
    "Decision counter: " + to_string(decision_counter) + "\nDecision index size: " + to_string(_decision_index.size()));
  _decision_index.resize(decision_counter);

  while(!_backtracked_variables.empty()) {
    Tvar var = _backtracked_variables.back();
    _backtracked_variables.pop_back();
    var_unassign(var);
  }

  _trail.resize(j - _trail.data());
  // The _trail should not be reallocated, so we should be able to use the pointers
  ASSERT(j >= _trail.data());
  ASSERT(j <= _trail.data() + _trail.size());

  // We need to fix the levels of all the literals above the decision level of the chunk
  for (Tlit* k = _trail.data() + _trail.size() - 1; k >= _trail.data(); k--) {
  // for (Tlit* k = _trail.data(); k < _trail.data() + _trail.size(); k++) {
    ASSERT_MSG(lit_level(*k) != decision_level,
      "Literal: " + lit_to_string(*k) + "\nLevel: " + to_string(lit_level(*k)) + "\nDecision level: " + to_string(decision_level));
    if (lit_level(*k) > decision_level) {
      // We need to fix the level of the literal
      Tvar var = lit_to_var(*k);
      ASSERT(var_level(var) > decision_level);
      _vars[var].level--;
      NOTIFY_OBSERVER(_observer, new napsat::gui::update_level(*k, _vars[var].level));
    }
    // We need to fix the propagation head
    if (lit_propagated(*k) && lit_cross_chunks(*k).get(chunk)) {
      unsigned location = k - _trail.data();
      while(_propagated_literals > location) {
        _propagated_literals--;
        Tlit lit = _trail[_propagated_literals];
        _vars[lit_to_var(lit)].propagated = false;
        NOTIFY_OBSERVER(_observer, new napsat::gui::remove_propagation(lit));
      }
      ASSERT(_propagated_literals <= _trail.size());
    }
  }
}

Tlevel napsat::NapSAT::choose_backtracked_level(Tlit* learned_lits, unsigned size)
{
  ASSERT(!_options.graph_backtracking)
#ifndef NDEBUG
  // The first literal of the clause is at the highest level
  for (unsigned i = 1; i < size; i++) {
    ASSERT(lit_level(learned_lits[i]) <= lit_level(learned_lits[0]));
  }
#endif
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

Tchunk napsat::NapSAT::choose_analyzed_chunk(Tclause conflict) {
  ASSERT(conflict != CLAUSE_UNDEF);
  ASSERT(_options.graph_backtracking);
  Tlit* lits = _clauses[conflict].lits;
  vector<bool> seen_chunks(_n_allocated_chunks, false);
  ASSERT(_clauses[conflict].size > 0);
  unsigned min_chunk_weight = UINT32_MAX;
  Tchunk min_chunk = CHUNK_UNDEF;

  for (unsigned i = 0; i < _clauses[conflict].size; i++) {
    Tlit lit = lits[i];
    bitvector& chunks = lit_chunks(lit);
    for (unsigned j = 0; j < _n_allocated_chunks; j++) {
      Tchunk chunk = Tchunk(j);
      if (seen_chunks[chunk] || !chunks.get(chunk))
        continue;
      seen_chunks[chunk] = true;
      ASSERT(_chunks[chunk].weight > 0);
      if (_chunks[chunk].weight < min_chunk_weight) {
        min_chunk_weight = _chunks[chunk].weight;
        min_chunk = chunk;
      }
    }
  }
  return min_chunk;
}

bool napsat::NapSAT::lit_is_required_in_learned_clause(Tlit lit)
{
  if (lit_reason(lit) == CLAUSE_UNDEF)
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

bool napsat::NapSAT::analyzed_level_or_chunk(Tlit lit, Tlevel level, Tchunk chunk)
{
  return (_options.graph_backtracking && lit_chunks(lit)[chunk])
      || (!_options.graph_backtracking && lit_level(lit) == level);
}

void NapSAT::analyze_conflict_level(Tlevel level) {
  // Computes the UIP at a given level
  // We assume that the level is a top level of the clause. That is, the level is is not reachable from a literal l at a higher level in the clause.
  ASSERT(level <= solver_level());

#ifndef NDEBUG
  // check that all variables are unmarked
  for (unsigned i = 0; i < _vars.size(); i++) {
    Tvar var = Tvar(i);
    ASSERT_MSG(!var_marked(var), "Variable " + to_string(var) + " is marked");
  }
#endif

  unsigned count = 0;

  Tchunk chunk = CHUNK_UNDEF;
  if (_options.graph_backtracking) {
    // we need to choose a chunk to analyze
    // unfortunately, we need a to do a linear seach to find the chunk
    for (unsigned i = 0; i < _n_allocated_chunks; i++) {
      if (var_level(_chunks[i].decision) == level){
        ASSERT_MSG(_chunks[i].weight > 0, "Chunk " + to_string(i) + " has weight " + to_string(_chunks[i].weight) + " but has variable " + to_string(_chunks[i].decision));
        chunk = Tchunk(i);
        break;
      }
    }
    ASSERT(chunk != CHUNK_UNDEF);
  }

  for (unsigned i = 0; i < _next_literal_index; i++) {
    Tlit lit = _literal_buffer[i];
    ASSERT(lit_false(lit));
    if (analyzed_level_or_chunk(lit, level, chunk)
    && !lit_marked(lit)) {
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

    ASSERT(lit_marked(lit));
    lit_unmark(lit);
    bump_var_activity(lit_to_var(lit));
    if (!analyzed_level_or_chunk(lit, level, chunk)) {
      if(lit_is_required_in_learned_clause(lit)) {
        _literal_buffer[_next_literal_index++] = lit_neg(lit);
      } else {
        if (_proof) {
          _proof->link_resolution(lit_neg(lit), lit_reason(lit));
        }
        if (_options.lazy_strong_chronological_backtracking) {
          // it could be that the literal was added earlier in the analysis (since we can go back to the end with the lazy reason)
          // we need to ensure to remove it from the buffer
          for (unsigned j = 0; j < _next_literal_index; j++) {
            if (_literal_buffer[j] == lit_neg(lit)) {
              _literal_buffer[j] = _literal_buffer[--_next_literal_index];
            }
          }
        }
      }
      continue;
    }
    // We already have the FUIP. No need to check the reason
    // We just need to finish collecting the literals that are marked.
    if (count == 1) {
      ASSERT(analyzed_level_or_chunk(lit, level, chunk));
      // this is the UIP
      _literal_buffer[_next_literal_index++] = lit_neg(lit);
      continue;
    }
    // mark the literals of the reason
    Tclause reason = lit_reason(lit);
    ASSERT_MSG(reason != CLAUSE_UNDEF, "Literal: " + lit_to_string(lit) + " at level: " + to_string(level) + " has no reason. The count is: " + to_string(count));
    if (lit_lazy_reason(lit) != CLAUSE_UNDEF) {
      reason = lit_lazy_reason(lit);
      // if we use the lazy reason, we need to ensure that we will look at all the literals in the clause
      // since the missed lower implication does not satisfy the trail invariant, we need to push the reading head to the back
      // note that this may lead to duplicate literals in the learned clause, but this will be cleaned up in the "internal_add_clause" function
      i = _trail.size() - 1;
    }
    if (_proof)
      _proof->link_resolution(lit_neg(lit), reason);
    ASSERT_MSG(reason != CLAUSE_UNDEF, "Literal: " + lit_to_string(lit) + " at level: " + to_string(level) + " has no reason. The count is: " + to_string(count));
    Tlit* reason_lits = _clauses[reason].lits;
    for (unsigned j = 1; j < _clauses[reason].size; j++) {
      Tlit reason_lit = reason_lits[j];
      if (lit_marked(reason_lit))
        continue;
      lit_mark(reason_lit);
      if (analyzed_level_or_chunk(reason_lit, level, chunk))
        count++;
    }
    count--;
  }
  ASSERT(count == 1);

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
    Tchunk undone_chunk = choose_analyzed_chunk(conflict);
    if (undone_chunk == CHUNK_UNDEF) {
      // The literal does not belong to any chunk, therefore, it does not depend on a decision and the conflict cannot be repaired
      _status = UNSAT;
      return;
    }
    ASSERT(_chunks[undone_chunk].weight > 0);
    undo_chunk(undone_chunk);
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

  Tchunk analyzed_chunk = CHUNK_UNDEF;
  if (_options.graph_backtracking) {
    analyzed_chunk = choose_analyzed_chunk(conflict);
    if (analyzed_chunk == CHUNK_UNDEF) {
      // The conflict does not have a literal that belongs to a chunk, therefore, it cannot be repaired
      _status = UNSAT;
      if (_proof) {
        prove_root_literal_removal(_literal_buffer, _next_literal_index);
        _proof->finalize_resolution(_clauses.size(), nullptr, 0);
      }
      return;
    }
    ASSERT(_chunks[analyzed_chunk].weight > 0);
    Tlevel chunk_level = var_level(_chunks[analyzed_chunk].decision);
    analyze_conflict_level(chunk_level);
  } else {
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

      analyze_conflict_level(conflict_level);
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
    } while (true);
  }

  // Check wether the clause in _literal_buffer is identical to the conflict clause
  // We do a quadratic search, but it is not a problem since the number of literals in the conflict clause is small
  bool identical = true;
  for (unsigned i = 0; identical && i < _next_literal_index; i++) {
    Tlit lit = _literal_buffer[i];
    unsigned j = 0;
    for (; j < _clauses[conflict].size; j++) {
      if (lit == _clauses[conflict].lits[j])
        break;
    }
    if (j == _clauses[conflict].size) {
      identical = false;
      break;
    }
  }
  for (unsigned j = 0; identical && j < _clauses[conflict].size; j++) {
    Tlit lit = _clauses[conflict].lits[j];
    unsigned i = 0;
    for (; i < _next_literal_index; i++) {
      if (lit == _literal_buffer[i])
        break;
    }
    if (i == _next_literal_index) {
      identical = false;
      break;
    }
  }
  // Later, if the clause is identical, we do not add it to the clause set.

  if (_options.graph_backtracking) {
    undo_chunk(analyzed_chunk);
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
    ASSERT(lit_undef(_literal_buffer[0]));
    ASSERT_MSG(_next_literal_index == 1 || !lit_undef(_literal_buffer[1]),
               "Literal buffer: " + to_string(_next_literal_index) + " literals, but the second literal is undefined: " + lit_to_string(_literal_buffer[1]) +
      "\nConflict clause: " + clause_to_string(conflict));
  }

  if (identical) {
    if (_proof)
      _proof->cancel_resolution_chain();
    _writing_clause = false;
    _next_literal_index = 0;

    // In chronological backtracking, it might be the case that the second highest literal is not at the second position.
    // We need to ensure that it becomes the second watched literal
    Tlit* end = lits + _clauses[conflict].size;
    Tlit* high_lit = lits + 1;
    // TODO: In graph backtracking, we should use a different criteria to select the watched literals.
    if (_options.graph_backtracking) {
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
      // Now, we bring a literal that is at the top of the chunk lattice to the second position
      // This is similar to the highest level in chronological backtracking, but we use the chunk instead
      bitvector* highest_chunks = &lit_chunks(*high_lit);
      for (Tlit* i = lits + 2; i < end; i++) {
        if (lit_chunks(*i) > *highest_chunks) {
          highest_chunks = &lit_chunks(*i);
          high_lit = i;
        }
      }
    } else {
      Tlevel high_lvl = lit_level(*high_lit);
      for (Tlit* i = lits + 2; i < end; i++) {
        if (lit_level(*i) > high_lvl) {
          high_lvl = lit_level(*i);
          high_lit = i;
        }
      }
    }
    if (high_lit > lits + 1) {
      stop_watch(lits[1], conflict);
      Tlit tmp = lits[1];
      lits[1] = *high_lit;
      *high_lit = tmp;
      watch_lit(lits[1], conflict);
    }

    // we need to imply the literal
    imply_literal(_clauses[conflict].lits[0], conflict);
  } else {
    Tclause learned = internal_add_clause(_literal_buffer, _next_literal_index, true, false);

    if (_proof)
      _proof->finalize_resolution(learned, _literal_buffer, _next_literal_index);

    _writing_clause = false;
    _next_literal_index = 0;
    // finalizing the clause will also imply the first literal of the clause
  }

  // bool unique = true;
  // for (unsigned i = 1; unique && i < _clauses[conflict].size; i++)
  //   unique = lit_level(lits[i]) != lit_level(lits[0]);

  /********** CLAUSES WITH ONE LITERAL AT MAX LEVEL **********/
//   if (unique && lit_lazy_reason(lits[0]) == CLAUSE_UNDEF) {
//     NOTIFY_OBSERVER(_observer, new napsat::gui::stat("One literal at highest level"));
//     ASSERT(_options.chronological_backtracking || _clauses[conflict].external);

//     Tlevel backtrack_level = lit_level(lits[1]);
//     if (_options.chronological_backtracking)
//       backtrack_level = lit_level(lits[0]) - 1;
// #ifndef NDEBUG
//     else {
//       // In NCB, we need that the second highest level is at the level of C \ {c₁}
//       //    δ(c₂) = δ(C \ {c₁})
//       // Such that we can backtrack to the second highest level
//       for (unsigned i = 2; i < _clauses[conflict].size; i++)
//         ASSERT(lit_level(lits[i]) <= lit_level(lits[1]));
//     }
// #endif
//     backtrack(backtrack_level);
//     ASSERT(lit_undef(lits[0]));
// #ifndef NDEBUG
//     for (unsigned i = 1; i < _clauses[conflict].size; i++)
//       ASSERT_MSG(lit_false(lits[i]),
//         "Conflict: " + clause_to_string(conflict) + "\nLiteral: " + lit_to_string(lits[i]));
// #endif

//     if (_options.chronological_backtracking) {
//       // In chronological backtracking, it might be the case that the second highest literal is not at the second position.
//       // We need to ensure that it becomes the second watched literal
//       Tlit* end = lits + _clauses[conflict].size;
//       Tlit* high_lit = lits + 1;
//       Tlevel high_lvl = lit_level(*high_lit);
//       for (Tlit* i = lits + 2; i < end; i++) {
//         if (lit_level(*i) > high_lvl) {
//           high_lvl = lit_level(*i);
//           high_lit = i;
//         }
//       }
//       if (high_lit > lits + 1) {
//         stop_watch(lits[1], conflict);
//         Tlit tmp = lits[1];
//         lits[1] = *high_lit;
//         *high_lit = tmp;
//         watch_lit(lits[1], conflict);
//       }
//     }
//     imply_literal(lits[0], conflict);
//     return;
//   }

  // analyze_conflict(conflict);

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
      // TODO should be taken into account in the proof
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
  }

  // Remove duplicate literals
  if (input_size > 1) {
    sort(lits, lits + clause_size);
    unsigned j = 1;
    for (unsigned i = 1; i < clause_size; i++) {
      if (lits[i] == lits[i - 1]) {
        // This could happen if the clause is generated using a missed lower implication
        ASSERT_MSG(external || _options.lazy_strong_chronological_backtracking, "Duplicate literal " + lit_to_string(lits[i]) + " in internal clause");
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

  var_allocate(n_var + 1);
  _trail = vector<Tlit>();
  _trail.reserve(n_var);
  _watch_lists.resize(2 * n_var + 2);

  _clauses = vector<TSclause>();
  _clauses.reserve(n_clauses);
  _activities.reserve(n_clauses);

  _literal_buffer = new Tlit[n_var];
  _next_literal_index = 0;

  if (options.build_proof)
    _proof = new napsat::proof::resolution_proof();
  else
    _proof = nullptr;

  if (_options.graph_backtracking) {
    _chunks.resize(_n_allocated_chunks);
    for (unsigned i = 0; i < _n_allocated_chunks; i++) {
      _free_chunks.push_back(i);
    }
  }
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
  while (_propagated_literals < _trail.size()) {
    Tlit lit = _trail[_propagated_literals];
    lit_cross_chunks(lit).clear();
    Tclause conflict = propagate_binary_clauses(lit);
    if (conflict == CLAUSE_UNDEF)
      conflict = propagate_lit(lit);
    if (conflict == CLAUSE_UNDEF) {
      _vars[lit_to_var(lit)].propagated = true;
      _propagated_literals++;
      NOTIFY_OBSERVER(_observer, new napsat::gui::propagation(lit));
      continue;
    }
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
    ASSERT_MSG(_propagated_literals == _trail.size(), "Propagation mismatch"
      "\nPropagated literals: " + to_string(_propagated_literals)
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
  while (!_variable_heap.empty() && !var_undef(_variable_heap.top()))
    _variable_heap.pop();
  if (_variable_heap.empty()) {
    _status = SAT;
    return false;
  }
  Tvar var = _variable_heap.top();
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
