/*
 * This file is part of the source code of the software program
 * NapSAT. It is protected by applicable copyright laws.
 *
 * This source code is protected by the terms of the MIT License.
 */
/**
 * @file src/solver/NapSAT-purge.cpp
 * @author Robin Coutelier
 * @brief This file is part of the NapSAT solver. It implements the clause purging mechanism of the NapSAT solver.
 * @details Clauses are purged when i) the solver propagated literals at root level, ii) there are too many clauses.
 * In the first case, the function purge_clauses removes clauses that are satisfied at root level and removes literals that are falsified at root level.
 * In the second case, the function simplify_clause_set removes clauses that are not active enough.
 */
#include "NapSAT.hpp"

#include "custom-assert.hpp"

#include <iostream>
#include <cstring>
#include <algorithm>

using namespace std;

void napsat::NapSAT::repair_watch_lists()
{
  /** REPAIR BINARY WATCH LIST **/
  for (Tlit lit = 2; lit < _watches.size(); lit.value++) {
    for (unsigned j = 0; j < _binary_watches[lit].size(); j++) {
      Tclause cl = _binary_watches[lit][j].cl;
      ASSERT(cl != CLAUSE_UNDEF,
        "Error: binary clause " << lit_to_string(lit) << " <- " << lit_to_string(_binary_watches[lit][j].block) << " is undefined");
      if (_clauses[cl].deleted) {
        _binary_watches[lit].erase(_binary_watches[lit].begin() + j);
        j--;
      }
    }
  }
  /** REPAIR WATCH LISTS **/
  for (Tlit lit = 2; lit < _watches.size(); lit.value++) {
    vector<TSwatch>& watch_list = _watches[lit];
    TSwatch* i = watch_list.data();
    TSwatch* end = i + watch_list.size();

    while (i < end) {
      TSclause &clause = _clauses[i->cl];
      if (clause.deleted || !clause.watched
       || (clause.lits[0] != lit && clause.lits[1] != lit)
       || clause.size <= 2) {
        if(!clause.deleted && clause.size != 2) {
          NOTIFY_WATCH(unwatch, i->cl, lit);
        }
        *i = *(--end);
        continue;
      }
      i++;
    }
    watch_list.resize((size_t) (end - watch_list.data()));
  }
}


void napsat::NapSAT::purge_root_watch_lists()
{
  ASSERT(_options.chronological_backtracking);
  // in weak chronological backtracking, a missed lower implication can create a clause that has a watched literal falsified at level 0 while not being satisfied at level 0
  // Therefore we need to clean the watch lists
  for (unsigned i = 0; i < _n_propagated_lits; i++) {
    Tlit lit = _trail[i];
    if (lit_level(lit) != LEVEL_ROOT)
      continue;

    lit = ~lit;
    vector<TSwatch>& watch_list = _watches[lit];
    TSwatch* j = watch_list.data();
    TSwatch* k = j;
    // start one before so that we just need to increment at the start of the loop
    j--;
    TSwatch* end = j + watch_list.size();
    while (j++ < end) {
      TSwatch &w = *j;
      Tclause cl = w.cl;
      TSclause& clause = _clauses[cl];
      if (clause.deleted) {
        // remove the clause from the watch list
        continue;
      }
      if (is_protected(cl)) {
        // keep the clause
        *(k++) = *j;
        continue;
      }
      NOTIFY_WATCH(unwatch, cl, lit);
      // if the clause is already deleted, do not bother
      if (clause.deleted)
        continue;

      ASSERT(clause.size > 2,
        "Clause: " + clause_to_string(cl) + "\nLiteral: " + lit_to_string(lit));
      if (lit_true(w.block) && lit_level(w.block) == LEVEL_ROOT) {
        // delete the clause. repair_watch_lists will take care of the rest
        delete_clause(cl);
        continue;
      }
      Tlit* lits = clause.lits;
      Tlit lit2 = lits[0] ^ lits[1] ^ lit;
      ASSERT(lit2 == lits[0] || lit2 == lits[1]);
      lits[0] = lit2;
      lits[1] = lit;

      if (lit_true(lits[0]) && lit_level(lits[0]) == LEVEL_ROOT) {
        // delete the clause. repair_watch_lists will take care of the rest
        delete_clause(cl);
        continue;
      }
      for (unsigned i = 2; i < clause.size; i++) {
        if (lit_level(lits[i]) != LEVEL_ROOT || lit_true(lits[i])) {
          lits[1] ^= lits[i];
          lits[i] ^= lits[1];
          lits[1] ^= lits[i];
          watch_lit(lits[1], cl);
          break;
        }
      }
    }
    watch_list.resize(k - watch_list.data());
    // Free the memory. We will not push new clauses in the watch list
    watch_list.shrink_to_fit();
  }
}

void napsat::NapSAT::purge_clauses()
{
  ASSERT(check_watch_lists_complete());
  ASSERT(check_watch_lists_minimal());
  NOTIFY_STAT(_n_purged_clauses);
  _purge_threshold = _n_root_lvl_lits + _purge_inc;
  // We assume that all the literals are propagated
  ASSERT(_n_propagated_lits == _trail.size());

  if (_options.chronological_backtracking)
    purge_root_watch_lists();

  for (Tclause cl = 0; cl < _clauses.size(); cl++) {
    // Do not remove clauses that are used as reasons
    TSclause& clause = _clauses[cl];
    if (clause.deleted || !clause.watched || clause.size <= 2)
      continue;
    if (is_protected(cl))
      continue;
    // Since all literals are propagated, if a clause has a watched literal falsified at level 0, then the other must be satisfied.
    // In strong chronological backtracking, the other watched literal must be satisfied at level 0 too.
    Tlit* lits = clause.lits;
    if ((lit_true(lits[0]) && lit_level(lits[0]) == LEVEL_ROOT)
      || (lit_true(lits[1]) && lit_level(lits[1]) == LEVEL_ROOT)) {
      delete_clause(cl);
      continue;
    }

    Tlit* i = lits + 2;
    Tlit* end = lits + clause.size - 1;
    unsigned previous_size = clause.size;
    while (i <= end) {
      if (lit_level(*i) != LEVEL_ROOT) {
        i++;
        continue;
      }
      if (lit_false(*i)) {
        // In Graph bracktracking, a missed cross-implication can create a unit clause satisfied a a level not zero.
        // We can prevent this by not deleting literals that have a non-empty cross-chunk set.
        if (_options.graph_backtracking) {
          i++;
          continue;
        }
        // remove the literal and push it to the back
        // we push it to the back so that we can print the clause even after the literal is removed
        NOTIFY(shrink_clause, cl, *i);
        Tlit tmp = *i;
        *i = *end;
        *end = tmp;
        end--;
        continue;
      }
      ASSERT(lit_true(*i));
      delete_clause(cl);
      break;
    }
    clause.size = end - lits + 1;

    if (clause.deleted)
      continue;

    if (lit_level(lits[1]) == LEVEL_ROOT) {
      if (lit_true(lits[1])) {
        delete_clause(cl);
        continue;
      }
      else if (lit_propagated(lits[1])) {
#ifndef NDEBUG
        for (unsigned i = 2; i < clause.size; i++) {
          ASSERT(lit_false(lits[i]),
            "Clause: " + clause_to_string(cl) + "\nLiteral: " + lit_to_string(lits[i]));
          ASSERT(lit_level(lits[i]) == LEVEL_ROOT,
            "Clause: " + clause_to_string(cl) + "\nLiteral: " + lit_to_string(lits[i]) + "\nLevel: " + lit_level(lits[i]).to_string());
        }
#endif
        NOTIFY(shrink_clause, cl, lits[1]);
        clause.size--;
      }
    }

    ASSERT(!clause.deleted,
               "Clause: " + clause_to_string(cl) + " was deleted.");
    if (_proof && previous_size != clause.size) {
      _proof->start_resolution_chain();
      _proof->link_resolution(LIT_UNDEF, cl);
      prove_root_literal_removal(clause.lits + clause.size, previous_size - clause.size);
      // we need to deactivate the clause to be able to replace it
      _proof->deactivate_clause(cl);
      _proof->finalize_resolution(cl, lits, clause.size);
    }
    if (_dependency_tracker && previous_size != clause.size) {
      for (unsigned j = clause.size; j < previous_size; j++) {
        _dependency_tracker->link_dependencies(cl, lit_reason(clause.lits[j]));
      }
    }

    if (clause.size == 2) {
      _binary_watches[lits[0]].push_back(TSwatch(cl, lits[1]));
      NOTIFY(block, cl, lits[1], lits[0]);
      _binary_watches[lits[1]].push_back(TSwatch(cl, lits[0]));
      NOTIFY(block, cl, lits[0], lits[1]);
      NOTIFY_STAT(_n_binary_clause_simplified);
    }
    if (clause.size == 1) {
      clause.watched = false;
      // The literal might be a missed lower implication
      if (lit_true(lits[0])) {
        ASSERT(_options.chronological_backtracking || _options.graph_backtracking);
        if (_options.lazy_strong_chronological_backtracking)
          reimply_literal(lits[0], cl);
      }
      else {
        ASSERT(lit_undef(lits[0]));
        imply_literal(lits[0], cl);
      }
      NOTIFY_STAT(_n_unit_clause_simplified);
    }
  }
  // remove the deleted clauses
  repair_watch_lists();
  NOTIFY(check_invariants);
  ASSERT(check_watch_lists_complete());
  ASSERT(check_watch_lists_minimal());
}

void napsat::NapSAT::simplify_clause_set()
{
  _next_clause_elimination *= _options.clause_elimination_multiplier;
  _clause_activity_threshold *= _options.clause_activity_threshold_decay;
  double threshold = _max_clause_activity * _clause_activity_threshold;
  for (Tclause cl = 0; cl < _clauses.size(); cl++) {
    ASSERT(_activities[cl] <= _max_clause_activity);
    ASSERT(_clauses[cl].size > 0);
    if (_clauses[cl].deleted || !_clauses[cl].watched || !_clauses[cl].learned)
      continue;
    if (_clauses[cl].size <= 2)
      continue;
    if (is_protected(cl))
      continue;
    if (_activities[cl] < threshold) {
      delete_clause(cl);
      NOTIFY_STAT(_n_redundant_clause);
    }
  }
  repair_watch_lists();
  ASSERT(check_watch_lists_complete());
  ASSERT(check_watch_lists_minimal());
  NOTIFY_STAT(_n_clause_set_simplified);
}
