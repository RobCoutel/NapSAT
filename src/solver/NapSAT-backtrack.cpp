#include "NapSAT.hpp"

#include "custom-assert.hpp"

#include <iostream>
#include <cstring>

using namespace napsat;
using namespace std;

Tlevel napsat::NapSAT::choose_backtracked_level(Tlit* learned_lits, unsigned size)
{
  ASSERT(!_options.graph_backtracking);
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
      ASSERT(var_propagated(var));
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
      { return lit_level(clause_lits(a)[1]) < lit_level(clause_lits(b)[1]); });
    for (Tclause lazy_clause : _reimplication_backtrack_buffer) {
      Tlit reimpl_lit = clause_lits(lazy_clause)[0];
      ASSERT(lit_undef(reimpl_lit));
      imply_literal(reimpl_lit, lazy_clause);
      NOTIFY_OBSERVER(_observer, new napsat::gui::stat("Lazy reimplication used"));
    }
    _reimplication_backtrack_buffer.clear();
  }
}

static Tvar last_backtracked_decision = 0;

void napsat::NapSAT::backtrack(const bitset& backtracked_chunks)
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
  // print_trail();
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
      for (unsigned k = 1; k < clause_size(lazy_reason); k++) {
        Tlit l = clause_lits(lazy_reason)[k];
        if (lit_undef(l) || lit_chunks(l).has_intersection(backtracked_chunks)) {
          lit_lazy_reason(lit) = CLAUSE_UNDEF;
          break;
        }
      }
    }
    Tvar var = lit_to_var(lit);
    if (lit_lvl > min_level) {
      // We need to fix the level of the literal
      var_level(var) = level_transformation[var_level(var)];
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

  // print_trail();

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
      lit_lazy_reason(lit) = CLAUSE_UNDEF;
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
