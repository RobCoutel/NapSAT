/*
 * This file is part of the source code of the software program
 * NapSAT. It is protected by applicable copyright laws.
 *
 * This source code is protected by the terms of the MIT License.
 */
/**
 * @file src/solver/NapSAT-backtrack.cpp
 * @author Robin Coutelier
 *
 * @brief This file is part of the NapSAT solver. It implements the backtracking
 * procedures.
 */
#include "NapSAT.hpp"

#include "custom-assert.hpp"

#include <iostream>
#include <cstring>

using namespace napsat;
using namespace std;

void NapSAT::var_unassign(Tvar var)
{
  ASSERT(!var_undef(var));

  TSvar& v = _vars[var];
  NOTIFY(unassign, Tlit(var, v.state));
  if (v.missed_lower_implication != CLAUSE_UNDEF) {
    v.missed_lower_implication = CLAUSE_UNDEF;
  }
  if (!_variable_heap.contains(var))
    _variable_heap.insert(var, v.activity);

  if (_options->graph_backtracking) {
    if (v.reason == CLAUSE_UNDEF) {
      ASSERT(v.chunks.count() == 1);
      Tchunk ck = *v.chunks.cbegin();
      TSchunk& chunk = _chunks[ck];
      _free_chunks.push_back(ck);
      chunk.decision = Tvar();
      chunk.missed_implication.clear();
      chunk.is_reimplied = false;
    }
    v.chunks.clear();
    v.cross_chunks.clear();
  }
  v.state = Tval::UNDEF;
  v.reason = CLAUSE_UNDEF;
  v.level = LEVEL_UNDEF;
  v.propagated = false;
}

Tlevel NapSAT::choose_backtracked_level(Tlit* learned_lits, unsigned size) const
{
  ASSERT(!_options->graph_backtracking);
#ifndef NDEBUG
  // The first literal of the clause is at the highest level
  for (unsigned i = 1; i < size; i++) {
    ASSERT(lit_level(learned_lits[i]) <= lit_level(learned_lits[0]), "Inconsistent literal levels for learned clause " + clause_to_string(learned_lits, size));
  }
#endif
  if (size == 0) {
    // If the learned clause is empty, we can backtrack to the root level
    return LEVEL_UNDEF;
  }
  if (_options->chronological_backtracking) {
    return lit_level(learned_lits[0]) - 1;
  }
  if (size == 1) {
    // If the learned clause is a unit clause, we can backtrack to the level of the literal
    return LEVEL_ROOT;
  }
  return lit_level(learned_lits[1]);
}

void NapSAT::backtrack(Tlevel level)
{
  ASSERT(level <= solver_level());
  if (_status == status::SAT) {
    _status = status::UNKNOWN;
  }
  if (level >= solver_level())
    return;
  unsigned waiting_count = 0;

  // the restore point is the location of the lowest decision that gets undone. That is, one level above the backtracking level.
  const size_t restore_point = _decision_index[level];
  ASSERT(restore_point <= _trail.size());
  ASSERT(lit_decision(_trail[restore_point]),
    "The restore point should be a decision literal. Restore point: " + to_string(restore_point) + "\nLevel: " + to_string(level));
  ASSERT(lit_level(_trail[restore_point]) == level + 1,
    "The restore point should be at level one above the backtracking level. Restore point: " + to_string(restore_point) + "\nLevel: " + to_string(level));
  size_t j = restore_point;
  _sync_validity_index = min(_sync_validity_index, restore_point);

  ASSERT(_backtracked_variables.empty());

  for (unsigned i = restore_point; i < _trail.size(); i++) {
    Tlit lit = _trail[i];
    Tvar var = lit.var();
    if (lit_level(lit) > level) {
      ASSERT(_options->lazy_strong_chronological_backtracking || _options->graph_backtracking || lit_lazy_reason(lit) == CLAUSE_UNDEF);
      if (lit_lazy_level(lit) <= level) {
        // look if the literal can be reimplied at a lower level
        ASSERT(_options->lazy_strong_chronological_backtracking || _options->lazy_chunk_merging);
        Tclause lazy_reason = lit_lazy_reason(lit);
        ASSERT(lazy_reason != CLAUSE_UNDEF);
        ASSERT(clause_lits(lazy_reason)[0] == lit);
        ASSERT(lit_true(clause_lits(lazy_reason)[0]));
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
    } else { // lit_level(lit) <= level
      ASSERT(_options->chronological_backtracking || _options->graph_backtracking,
        "The literal " + lit_to_string(lit) + " at level " + lit_level(lit).to_string() + " at position " + to_string(i) + " should be backtracked. Backtracking level: " + to_string(level));
      _trail[j++] = lit;
      waiting_count += (i >= _n_propagated_lits);
    }
    if (lit_lazy_reason(lit) != CLAUSE_UNDEF && lit_lazy_level(lit) > level) {
      // the lazy reason is not valid anymore
      lit_lazy_reason(lit) = CLAUSE_UNDEF;
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

  ASSERT(_options->chronological_backtracking || _options->graph_backtracking || waiting_count == 0,
             "Waiting count: " + to_string(waiting_count) + "\nLevel: " + to_string(level) + "\nRestore point: " + to_string(restore_point));
  _n_propagated_lits = _trail.size() - waiting_count;
  ASSERT(_options->chronological_backtracking || _options->graph_backtracking || _n_propagated_lits == restore_point,
    "Propagated literals: " + to_string(_n_propagated_lits) + "\nRestore point: " + to_string(restore_point));
  // in RSCB we need to move the propagation head back to the location of the first literal that moved
  // that is, the location of the first literal that was unassigned.
  if (_options->chronological_backtracking) {
    while (_n_propagated_lits > restore_point) {
      Tlit lit = _trail[_n_propagated_lits - 1];
      Tvar var = lit.var();
      ASSERT(var_propagated(var));
      _vars[var].propagated = false;
      _n_propagated_lits--;
      NOTIFY(unpropagate, lit);
    }
  }
  if (_reimplication_backtrack_buffer.size() > 0) {
    ASSERT(_options->lazy_strong_chronological_backtracking || _options->lazy_chunk_merging);
    // adds the literals on the lazy reimplication buffer to the trail by order of increasing level
    // Sort the literals by increasing level. It is not necessary, but it probably is more effective
    // TODO evaluate the performance of this. Is sorting useful?
    // The topological order will automatically be respected because the reimplied literals cannot depend on each other.
    // see Theorem 17 in [Lazy Reimplication in Chronological Backtracking, Robin Coutelier and Mathias Fleury and Laura Kovács]
    for (Tclause lazy_clause : _reimplication_backtrack_buffer) {
      Tlit reimpl_lit = clause_lits(lazy_clause)[0];
      ASSERT(lit_undef(reimpl_lit));
      imply(reimpl_lit, lazy_clause);
      NOTIFY_STAT(_n_lazy_reimplication_used);
    }
    _reimplication_backtrack_buffer.clear();
  }
}

static Tvar last_backtracked_decision = 0;

void NapSAT::backtrack(const bitset& backtracked_chunks)
{
  ASSERT(_options->graph_backtracking);
  ASSERT(!backtracked_chunks.empty());
  ASSERT(!backtracked_chunks.empty());
  ASSERT(_backtracked_variables.empty());
  // Mapping to the new level of literals after backtracking
  indexed_vector<Tlevel, Tlevel> level_transformation;
  level_transformation.resize(solver_level() + 1);
  Tlevel real_level = 1;
  Tlevel min_level = LEVEL_UNDEF;
  for (Tlevel lvl = 1; lvl <= solver_level(); lvl++) {
    Tlit decision = decision_lit(lvl);

    if (lit_chunks(decision).has_intersection(backtracked_chunks)) {
      min_level = min(min_level, lvl);
      level_transformation[lvl] = LEVEL_ERROR;
    } else {
      level_transformation[lvl] = real_level.value++;
    }
  }

  Tlit* i = _trail.data();
  Tlit* j = i + _decision_index[min_level - 1];
  Tlit* end = _trail.data() + _trail.size();
  Tlevel decision_counter = min_level - 1;
  unsigned new_propagation_head = _n_propagated_lits;
  // print_trail();
  while (i < end) {
    Tlit lit = *i;
    Tvar var = lit.var();
    bitset& chunks = var_chunks(var);
    Tlevel lit_lvl = var_level(var);

    // belonging to one of the deleted levels is a sufficient condition, faster to compute than the chunk intersection
    bool backtracked = i >= j &&  (level_transformation[lit_lvl] == LEVEL_ERROR || chunks.has_intersection(backtracked_chunks));

    /** CROSS-CHUNKS **/
    if (!backtracked && lit_propagated(lit) && lit_cross_chunks(lit).has_intersection(backtracked_chunks)) {
      unsigned loc = min(i, j) - _trail.data();
      _vars[var].propagated = false;
      NOTIFY_STAT(_n_propagation_replayed);
      NOTIFY(unpropagate, lit);
      new_propagation_head = min(new_propagation_head, loc);
    }

    // check if the lazy reimplication still holds
    Tclause lazy_reason = lit_lazy_reason(lit);
    if (lazy_reason != CLAUSE_UNDEF) {
      ASSERT(lit_decision(lit));
      TSchunk& chunk = _chunks[*var_chunks(var).cbegin()];
      if (chunk.missed_implication.has_intersection(backtracked_chunks)) {
        lit_lazy_reason(lit) = CLAUSE_UNDEF;
        chunk.missed_implication.clear();
        chunk.is_reimplied = false;
      }
    }

    if (i < j) {
      i++;
      continue;
    }


    /** ACTUAL BACKTRACKING **/
    // only for after the start position

    if (backtracked) {
      if(lit_decision(lit)) {
        last_backtracked_decision = lit.var();
      }
      // we need to unassign the variable
      var_unassign(lit.var());
      _sync_validity_index = min(_sync_validity_index, (size_t) (j - _trail.data()));
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

    if (lit_lvl > min_level) {
      // We need to fix the level of the literal
      var_level(var) = level_transformation[var_level(var)];
      ASSERT(var_level(var) != LEVEL_ERROR);
      NOTIFY(update_level, lit, var_level(var));
    }
    *(j++) = *(i++);
  }
  ASSERT(decision_counter == _decision_index.size() - backtracked_chunks.count());
  _decision_index.resize(decision_counter);
  _trail.resize(j - _trail.data());

  ASSERT(_n_propagated_lits <= _trail.size());
  _n_propagated_lits = new_propagation_head;

  ASSERT(all_of(_trail.begin(), _trail.end(), [this,backtracked_chunks](Tlit l){
    return !lit_chunks(l).has_intersection(backtracked_chunks); }));
  ASSERT(all_of(_trail.begin(), _trail.end(), [this,backtracked_chunks](Tlit l){
    return !lit_propagated(l) || !lit_cross_chunks(l).has_intersection(backtracked_chunks); }));
}
