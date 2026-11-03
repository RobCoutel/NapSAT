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
      free_chunk(var);
    }
    v.chunks.clear();
    v.cross_chunks.clear();
  }
  v.state = Tval::UNDEF;
  v.reason = CLAUSE_UNDEF;
  v.level = LEVEL_UNDEF;
  v.trail_pos = TRAIL_POS_UNDEF;
}

Tlevel NapSAT::choose_backtracked_level(Tlit* learned_lits, unsigned size) const
{
  ASSERT(!_options->graph_backtracking);
#ifndef NDEBUG
  // The first literal of the clause is at the highest level
  for (unsigned i = 1; i < size; i++) {
    ASSERT(level(learned_lits[i]) <= level(learned_lits[0]), "Inconsistent literal levels for learned clause " + clause_to_string(learned_lits, size));
  }
#endif
  if (size == 0) {
    // If the learned clause is empty, we can backtrack to the root level
    return LEVEL_UNDEF;
  }
  if (_options->chronological_backtracking) {
    return level(learned_lits[0]) - 1;
  }
  if (size == 1) {
    // If the learned clause is a unit clause, we can backtrack to the level of the literal
    return LEVEL_ROOT;
  }
  return level(learned_lits[1]);
}

void NapSAT::backtrack(Tlevel lvl)
{
  ASSERT(lvl <= solver_level());
  if (_status == status::SAT) {
    _status = status::UNKNOWN;
  }
  if (lvl >= solver_level())
    return;
  unsigned waiting_count = 0;

  // the restore point is the location of the lowest decision that gets undone. That is, one level above the backtracking level.
  const size_t restore_point = _decision_index[lvl];
  ASSERT(restore_point <= _trail.size());
  ASSERT(is_decision(_trail[restore_point]),
    "The restore point should be a decision literal. Restore point: " + to_string(restore_point) + "\nLevel: " + to_string(lvl));
  ASSERT(level(_trail[restore_point]) == lvl + 1,
    "The restore point should be at level one above the backtracking level. Restore point: " + to_string(restore_point) + "\nLevel: " + to_string(lvl));
  size_t j = restore_point;
  _sync_validity_index = min(_sync_validity_index, restore_point);

  ASSERT(_backtracked_variables.empty());

  for (unsigned i = restore_point; i < _trail.size(); i++) {
    Tlit lit = _trail[i];
    Tvar var = lit.var();
    if (level(lit) > lvl) {
      ASSERT(_options->lazy_strong_chronological_backtracking || _options->graph_backtracking || lazy_reason(lit) == CLAUSE_UNDEF);
      if (lit_lazy_level(lit) <= lvl) {
        // look if the literal can be reimplied at a lower level
        ASSERT(_options->lazy_strong_chronological_backtracking || _options->lazy_chunk_merging);
        Tclause l_reason = lazy_reason(lit);
        ASSERT(l_reason != CLAUSE_UNDEF);
        ASSERT(clause_lits(l_reason)[0] == lit);
        ASSERT(lit_true(clause_lits(l_reason)[0]));
        _reimplication_backtrack_buffer.push_back(l_reason);
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
    } else { // level(lit) <= level
      ASSERT(_options->chronological_backtracking || _options->graph_backtracking,
        "The literal " + lit_to_string(lit) + " at level " + level(lit).to_string() + " at position " + to_string(i) + " should be backtracked. Backtracking level: " + to_string(lvl));
      lit_trail_pos(lit) = j;
      _trail[j++] = lit;
      waiting_count += (i >= _n_propagated_lits);
    }
    if (lazy_reason(lit) != CLAUSE_UNDEF && lit_lazy_level(lit) > lvl) {
      // the lazy reason is not valid anymore
      lazy_reason(lit) = CLAUSE_UNDEF;
    }
  }
  // Here we unassign the literals as mentioned above
  while(!_backtracked_variables.empty()) {
    Tvar var = _backtracked_variables.back();
    _backtracked_variables.pop_back();
    var_unassign(var);
  }
  _trail.resize(j);
  _decision_index.resize(lvl);

  ASSERT(_options->chronological_backtracking || _options->graph_backtracking || waiting_count == 0,
             "Waiting count: " + to_string(waiting_count) + "\nLevel: " + to_string(lvl) + "\nRestore point: " + to_string(restore_point));
  _n_propagated_lits = _trail.size() - waiting_count;
  ASSERT(_options->chronological_backtracking || _options->graph_backtracking || _n_propagated_lits == restore_point,
    "Propagated literals: " + to_string(_n_propagated_lits) + "\nRestore point: " + to_string(restore_point));
  // in RSCB we need to move the propagation head back to the location of the first literal that moved
  // that is, the location of the first literal that was unassigned.
  if (_options->chronological_backtracking) {
    while (_n_propagated_lits > restore_point) {
      Tlit lit = _trail[_n_propagated_lits - 1];
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
  ASSERT(check_trail_position_consistency());
}

static Tvar last_backtracked_decision = 0;

void NapSAT::backtrack(const bitset& backtracked_chunks)
{
  ASSERT(_options->graph_backtracking);
  ASSERT(!backtracked_chunks.empty());
  ASSERT(!backtracked_chunks.empty());
  ASSERT(_backtracked_variables.empty());

  // record the recently backtracked chunks to avoid a full scan of the trail for repropagation
  _recently_backtracked_chunks |= backtracked_chunks;

  // Mapping to the new level of literals after backtracking
  indexed_vector<Tlevel, Tlevel> level_transformation;
  level_transformation.resize(solver_level() + 1);
  Tlevel real_level = 1;
  Tlevel min_level = LEVEL_UNDEF;
  level_transformation[LEVEL_ROOT] = LEVEL_ROOT;
  for (Tlevel lvl = 1; lvl <= solver_level(); lvl++) {
    Tlit decision = decision_lit(lvl);

    if (lit_chunks(decision).has_intersection(backtracked_chunks)) {
      min_level = min(min_level, lvl);
      level_transformation[lvl] = LEVEL_ERROR;
    } else {
      level_transformation[lvl] = real_level.value++;
    }
  }

  Tlit* i = _trail.data() + _decision_index[min_level - 1];
  Tlit* j = i;
  Tlit* end = _trail.data() + _trail.size();
  Tlevel decision_counter = min_level - 1;

  while (i < end) {
    Tlit lit = *i;
    Tvar var = lit.var();
    bitset& chunks = var_chunks(var);
    Tlevel lvl = level(var);

    if (level_transformation[lvl] == LEVEL_ERROR || chunks.has_intersection(backtracked_chunks)) {
      if(is_decision(lit)) {
        last_backtracked_decision = lit.var();
      }
      // we need to unassign the variable
      var_unassign(lit.var());
      _sync_validity_index = min(_sync_validity_index, (size_t) (j - _trail.data()));
      unsigned loc = j - _trail.data();
      _n_propagated_lits   -= loc < _n_propagated_lits;
      ASSERT(_n_propagated_lits <= _trail.size());
      i++;
      continue;
    }

    if (is_decision(lit)) {
      _decision_index[decision_counter++] = j - _trail.data();
    }

    if (lvl > min_level) {
      // We need to fix the level of the literal
      level(var) = level_transformation[level(var)];
      ASSERT(level(var) != LEVEL_ERROR);
      NOTIFY(update_level, lit, level(var));
    }
    trail_pos(var) = j - _trail.data();
    *(j++) = *(i++);
  }
  ASSERT(decision_counter == _decision_index.size() - backtracked_chunks.count());
  _decision_index.resize(decision_counter);
  _trail.resize(j - _trail.data());
  ASSERT(check_trail_position_consistency());

  ASSERT(_n_propagated_lits <= _trail.size());
#if OBSERVER_ENABLED
  if (_sentinel) {
    while (_n_propagated_lits > 0) {
      NOTIFY(unpropagate, _trail[--_n_propagated_lits]);
    }
  }
#endif
  _n_propagated_lits = 0;

  // check if the lazy reimplications still hold
  for (Tchunk c = 0; c < _n_allocated_chunks; c++) {
    if (backtracked_chunks.has_intersection(_chunks[c].missed_implication)) {
      _chunks[c].missed_implication.clear();
      _chunks[c].is_reimplied = false;
      lazy_reason(_chunks[c].decision) = CLAUSE_UNDEF;
    }
  }
}
