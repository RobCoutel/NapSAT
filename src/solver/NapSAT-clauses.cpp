#include "NapSAT.hpp"

#include "custom-assert.hpp"

#include <iostream>
#include <cstring>

using namespace napsat;
using namespace std;

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
      _conflicts.push_back(cl);
      repair_conflicts();
    }
    return cl;
  }
  else if (clause_size == 2) { // TODO simplify this
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
      else if (lit_false(lits[0])) {
        _conflicts.push_back(cl);
        repair_conflicts();
      }
      else if (_options.lazy_strong_chronological_backtracking) {
        ASSERT(lit_true(lits[0]));
        if (lit_lazy_reason(lits[0]) == CLAUSE_UNDEF || lit_level(clause_lits(lit_lazy_reason(lits[0]))[1]) > lit_level(lits[0]))
          lit_lazy_reason(lits[1]) = cl;
      }
    }
  }
  else {
    select_watched_literals(lits, clause_size);
    watch_lit(lits[0], cl);
    watch_lit(lits[1], cl);
    if (lit_false(lits[0])) {
      _conflicts.push_back(cl);
        repair_conflicts();
    }
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

void napsat::NapSAT::delete_clause(Tclause cl)
{
  // If the clause is the reason for a literal, it cannot be deleted
  ASSERT(cl < _clauses.size());
  TSclause &clause = _clauses[cl];
  ASSERT(!is_protected(cl));
  _n_learned_clauses -= _clauses[cl].learned;
  clause.deleted = true;
  clause.watched = false;
  _deleted_clauses.push_back(cl);
  NOTIFY_OBSERVER(_observer, new napsat::gui::delete_clause(cl));
  if(_proof)
    _proof->deactivate_clause(cl);
}

bitset napsat::NapSAT::clause_chunks(Tclause cl)
{
  bitset chunk(_n_allocated_chunks);
  Tlit* lits = clause_lits(cl);
  unsigned size = clause_size(cl);
  for (unsigned i = 0; i < size; i++) {
    const bitset& lit_chunk = lit_chunks(lits[i]);
    chunk |= lit_chunk;
  }
  return chunk;
}

Tlevel napsat::NapSAT::clause_level(Tclause cl)
{
  ASSERT(cl != CLAUSE_UNDEF);
  ASSERT(cl < _clauses.size());
  ASSERT(clause_size(cl) > 0);
  const Tlit* lits = clause_lits(cl);

  if (!_options.graph_backtracking) {
    ASSERT(lit_is_max_literal(lits[0], lits + 1, clause_size(cl) - 1));
    return lit_level(lits[0]);
  }

  Tlevel level = lit_level(lits[0]);
  for (unsigned i = 1; i < clause_size(cl); i++) {
    Tlevel lit_lvl = lit_level(lits[i]);
    if (lit_lvl > level)
      level = lit_lvl;
  }
  return level;
}
