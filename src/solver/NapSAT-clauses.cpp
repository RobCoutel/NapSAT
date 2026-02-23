/*
 * This file is part of the source code of the software program
 * NapSAT. It is protected by applicable copyright laws.
 *
 * This source code is protected by the terms of the MIT License.
 */
/**
 * @file src/solver/NapSAT-clauses.cpp
 * @author Robin Coutelier
 * @brief This file is part of the NapSAT solver. It implements functions to manage clauses.
 */
#include "NapSAT.hpp"

#include "custom-assert.hpp"
#include "../proof/dependency_tracker.hpp"

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
  unsigned max_utility = max_utility_heuristic();

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
    if (second_utility == max_utility) {
      // we cannot do better
      break;
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

Tclause napsat::NapSAT::next_clause_id(size_t size)
{
  if (_deleted_clauses.empty()) {
    Tlit* lits = new Tlit[size];
    TSclause added(lits, size, false, false);
    _clauses.push_back(added);
    _clauses_sizes.push_back(size);
    _activities.push_back(_max_clause_activity);
    return _clauses.size() - 1;
  }
  Tclause cl = _deleted_clauses.back();
  ASSERT(cl < _clauses.size());
  _deleted_clauses.pop_back();
  TSclause& clause = _clauses[cl];
  ASSERT(clause.deleted);
  ASSERT(!clause.watched);
  if (_clauses_sizes[cl] < size) {
    delete[] clause.lits;
    clause.lits = new Tlit[size];
    _clauses_sizes[cl] = size;
  }
  clause.deleted = true;
  clause.learned = true;
  clause.watched = false;
  clause.external = false;
  // fill the end of the clause with LIT_UNDEF for printing purposes
  // Note that this is not necessary for the solver
  for (unsigned i = size; i < _clauses_sizes[cl]; i++)
    clause.lits[i] = LIT_UNDEF;

  return cl;
}

Tclause napsat::NapSAT::internal_add_clause(const Tlit* lits_input, const unsigned input_size, bool learned, bool external, Tclause id)
{
  ASSERT(lits_input != nullptr);
  if (external) {
    for (unsigned i = 0; i < input_size; i++)
      bump_var_activity(lit_to_var(lits_input[i]));
  }

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
      // ASSERT(external);
      satisfied_at_root |= lit_true(lits_input[i]);
      n_removed++;
    }
  }
  // If the clause is satisfied at level 0, we do not need to add it
  // No need to justify it in the proof since clauses satisfied at level 0 and not propagating are not necessary for the proof
  // If it is satisfied already here, it means another clauses propagates the literal at level 0
  if (satisfied_at_root) {
    if (id != CLAUSE_UNDEF) {
      NOTIFY_OBSERVER(new_clause, id, vector<Tlit>(lits_input, lits_input + input_size), learned, external);
      NOTIFY_OBSERVER(delete_clause, id);
      _clauses[id].deleted = true;
      _clauses[id].watched = false;
      if (_proof)
        _proof->deactivate_clause(id);
      if (_dependency_tracker)
        _dependency_tracker->delete_clause(id);

      _deleted_clauses.push_back(id);
    }
    return CLAUSE_UNDEF;
  }


  unsigned clause_size = input_size - n_removed;

  bool non_allocated_clause = id == CLAUSE_UNDEF;

  if (non_allocated_clause) {
    id = next_clause_id(clause_size);
  }

  TSclause& clause = _clauses[id];
  ASSERT(clause.deleted);
  clause.deleted = false;
  clause.learned = learned;
  clause.external = external;
  clause.watched = clause_size >= 2;
  clause.size = clause_size;
  clause.last_looked = 1;
  Tlit* lits = clause.lits;


  // copy the literals to the clause
  if (n_removed == 0)
    memcpy(lits, lits_input, input_size * sizeof(Tlit));
  else {
    // cannot use memcpy because we skip the literals falsified at level 0
    for (unsigned i = 0, j = 0; i < input_size; i++) {
      if (lit_level(lits_input[i]) == LEVEL_ROOT) {
        ASSERT(lit_true(lits_input[i]));
        ASSERT(external);
        continue;
      }
      lits[j++] = lits_input[i];
    }
    clause.size = input_size - n_removed;
  }

  // if (external) {
  //   cout << "Added external clause " << clause_to_string(id) << endl;
  // } else {
  //   cout << "Learned clause " << clause_to_string(id) << endl;
  // }

  // Remove duplicate literals
  if (input_size > 1) {
    unsigned new_size = cleanup_duplicate_literals(lits, clause.size);

    // if the size is 0, it means that the clause was a tautology.
    if (new_size == 0) {
      ASSERT(external);
      if (id != CLAUSE_UNDEF) {
        NOTIFY_OBSERVER(new_clause, id, vector<Tlit>(lits_input, lits_input + input_size), learned, external);
        NOTIFY_OBSERVER(delete_clause, id);
        if (_proof && !non_allocated_clause)
          _proof->deactivate_clause(id);
        if (_dependency_tracker && !non_allocated_clause)
          _dependency_tracker->delete_clause(id);
        _clauses[id].deleted = true;
        _clauses[id].watched = false;
        _deleted_clauses.push_back(id);
      }
      string warning = "The clause: ";
      for (unsigned k = 0; k < input_size; k++) {
        warning += lit_to_string(lits_input[k]) + " ";
      }
      warning += "is a tautology and will be ignored.";
      LOG_WARNING(warning);
      return CLAUSE_UNDEF;
    }

    clause.size = new_size;
  }

  if (_proof && external) {
    _proof->input_clause(id, lits_input, input_size);
    // Remove the literals falsified at level 0 in the proof
    if (n_removed > 0) {
      _proof->remove_root_literals(id);
    }
  }
  if (_dependency_tracker && external) {
    _dependency_tracker->add_input(id);
    // Remove the literals falsified at level 0 in the dependencies
    if (n_removed > 0) {
      for (unsigned i = 0; i < input_size; i++) {
        if (lit_level(lits_input[i]) == LEVEL_ROOT) {
          _dependency_tracker->link_dependencies(id, lit_reason(lits_input[i]));
        }
      }
    }
  }
  // sort the literals
  sort(lits, lits + clause_size);
  // cout << "Added clause " << clause_to_string(id) << endl;

  #if USE_OBSERVER
  if (_observer) {
    vector<Tlit> lits_vector;
    for (unsigned i = 0; i < clause_size; i++)
      lits_vector.push_back(lits[i]);
    NOTIFY_OBSERVER(new_clause, id, lits_vector, learned, external);
  }
  #else
    NOTIFY_STAT(new_clause)
  #endif

  if (clause_size == 0) {
    _status = UNSAT;
    return id;
  }

  if (learned) {
    NOTIFY_STAT_N(_a_learned_clause_size, clause_size);
  }

  if (external && _options.ignore_unused_variables) {
    // mark all the variables in the clause as constrained
    for (unsigned i = 0; i < clause_size; i++)
      var_mark_constrained(lit_to_var(lits[i]));
  }

  if (clause_size == 1) {
    if (lit_undef(lits[0])) {
      if (!_options.chronological_backtracking && !_options.graph_backtracking) {
        // we need to ensure monotonicity of the trail. For that reason, we have to backtrack everything above the implied level
        backtrack(LEVEL_ROOT);
      }
      imply_literal(lits[0], id);
    }
    if (lit_true(lits[0]) && lit_level(lits[0]) != LEVEL_ROOT) {
      reimply_literal_root(lits[0], id);
      return id;
    }
    if (lit_false(lits[0])) {
      if (_status == SAT)
        _status = UNKNOWN;

      _conflicts.push_back(id);
      _lit_buffer_size = 0;
      _just_learned_from_user = true;
      repair_conflicts();
    }
    return id;
  }
  select_watched_literals(lits, clause_size);
  if (clause_size == 2) {
    watch_lit_bin(id);
  } else {
    watch_lit(lits[0], id);
    watch_lit(lits[1], id);
  }

  // cout << "Added clause " << clause_to_string(id) << endl;

  // The clause is not unit or conflicting. Nothing special to do
  if (!lit_false(lits[1])) {
    return id;
  }

  // fix the trail if the clause is unit/conflicting
  if (lit_undef(lits[0])) {

    if (!_options.chronological_backtracking && !_options.graph_backtracking) {
      // we need to ensure monotonicity of the trail. For that reason, we have to backtrack everything above the implied level
      Tlevel backtrack_level = lit_level(lits[1]);
      backtrack(backtrack_level);
    }

    ASSERT(check_clause_unit(id));
    imply_literal(lits[0], id);
    return id;
  }
  if (lit_false(lits[0])) {
    ASSERT(check_clause_falsified(id));
    _conflicts.push_back(id);
    if (_status == SAT)
      _status = UNKNOWN;
    if (!_options.graph_backtracking) {
      // except in graph backtracking, there is no reason to not handle the conflict eagerly
      // in GB, we want to select the best chunks at the end of the procedure.
      _lit_buffer_size = 0;
      _just_learned_from_user = true;
      repair_conflicts();
    }

    return id;
  }

  ASSERT(lit_true(lits[0]));
  ASSERT(check_clause_implying(id));
  ASSERT(lit_level(lits[1]) > LEVEL_ROOT);

  // This might be a missed lower implication
  // check if we need to reimply the literal at a lower level (or fix the cross-chunks)
  reimply_literal(lits[0], id);
  return id;
}

void napsat::NapSAT::delete_clause(Tclause cl)
{
  // If the clause is the reason for a literal, it cannot be deleted
  ASSERT(cl < _clauses.size());
  ASSERT(!_clauses[cl].deleted);
  ASSERT(!_dependency_tracker || !_clauses[cl].external);
  TSclause &clause = _clauses[cl];
  ASSERT(!is_protected(cl));
  _n_learned_clauses -= _clauses[cl].learned;
  clause.deleted = true;
  clause.watched = false;
  _deleted_clauses.push_back(cl);
  NOTIFY_OBSERVER(delete_clause, cl);
  if(_proof)
    _proof->deactivate_clause(cl);
  if (_dependency_tracker)
    _dependency_tracker->delete_clause(cl);
}

bitset napsat::NapSAT::clause_chunks(Tclause cl) const
{
  bitset chunk(_n_allocated_chunks);
  const Tlit* lits = clause_lits(cl);
  unsigned size = clause_size(cl);
  for (unsigned i = 0; i < size; i++) {
    chunk |= lit_chunks(lits[i]);
  }
  return chunk;
}

Tlevel napsat::NapSAT::clause_level(Tclause cl) const
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
