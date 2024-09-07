#include "NapSAT.hpp"
#include "custom-assert.hpp"

#include <iostream>
#include <cstring>
#include <algorithm>

using namespace std;

void napsat::NapSAT::repair_watch_lists()
{
  /** REPAIR BINARY WATCH LIST **/
  for (Tlit lit = 2; lit < _watch_lists.size(); lit++) {
    for (unsigned j = 0; j < _binary_clauses[lit].size(); j++) {
      Tclause cl = _binary_clauses[lit][j].second;
      ASSERT_MSG(cl != CLAUSE_UNDEF,
        "Error: binary clause " << lit_to_string(lit) << " <- " << lit_to_string(_binary_clauses[lit][j].first) << " is undefined");
      if (_clauses[cl].deleted) {
        _binary_clauses[lit].erase(_binary_clauses[lit].begin() + j);
        j--;
      }
    }
  }
  /** REPAIR WATCH LISTS **/
  for (Tlit lit = 2; lit < _watch_lists.size(); lit++) {
    vector<Tclause>& watch_list = _watch_lists[lit];
    Tclause* i = watch_list.data();
    Tclause* end = i + watch_list.size();

    while (i < end) {
      TSclause &clause = _clauses[*i];
      if (clause.deleted || !clause.watched
       || (clause.lits[0] != lit && clause.lits[1] != lit)
       || clause.size <= 2) {
#if NOTIFY_WATCH_CHANGES
        if(!clause.deleted && clause.size != 2)
          NOTIFY_OBSERVER(_observer, new napsat::gui::unwatch(*i, lit));
#endif
        *i = *(--end);
        continue;
      }
      i++;
    }
    watch_list.resize(end - watch_list.data());
  }
}


void napsat::NapSAT::purge_root_watch_lists()
{
  ASSERT(_options.weak_chronological_backtracking || _options.restoring_strong_chronological_backtracking);
  // in weak chronological backtracking, a missed lower implication can create a clause that has a watched literal falsified at level 0 while not being satisfied at level 0
  // Therefore we need to clean the watch lists
  for (unsigned i = 0; i < _propagated_literals; i++) {
    Tlit lit = _trail[i];
    if (lit_level(lit) != LEVEL_ROOT)
      continue;

    lit = lit_neg(lit);
    vector<Tclause>& watch_list = _watch_lists[lit];
    Tclause* j = watch_list.data();
    Tclause* k = j;
    // start one before so that we just need to increment at the start of the loop
    j--;
    Tclause* end = j + watch_list.size();
    while (j++ < end) {
      Tclause cl = *j;
      TSclause& clause = _clauses[cl];
      if (clause.deleted) {
        // remove the clause from the watch list
        continue;
      }
      if (lit_reason(clause.lits[0]) == cl) {
        // keep the clause
        *(k++) = cl;
        continue;
      }
#if NOTIFY_WATCH_CHANGES
      NOTIFY_OBSERVER(_observer, new napsat::gui::unwatch(cl, lit));
#endif
      // if the clause is already deleted, do not bother
      if (clause.deleted)
        continue;

      ASSERT_MSG(clause.size > 2,
        "Clause: " + clause_to_string(cl) + "\nLiteral: " + lit_to_string(lit));
      if (lit_true(clause.blocker) && lit_level(clause.blocker) == LEVEL_ROOT) {
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
  ASSERT(watch_lists_complete());
  ASSERT(watch_lists_minimal());
  NOTIFY_OBSERVER(_observer, new napsat::gui::stat("Purging clauses"));
  _purge_threshold = _n_root_lvl_lits + _purge_inc;
  // We assume that all the literals are propagated
  ASSERT(_propagated_literals == _trail.size());

  if (_options.weak_chronological_backtracking || _options.restoring_strong_chronological_backtracking)
    purge_root_watch_lists();

  for (Tclause cl = 0; cl < _clauses.size(); cl++) {
    // Do not remove clauses that are used as reasons
    TSclause& clause = _clauses[cl];
    if (is_protected(cl))
      continue;
    if (clause.deleted || !clause.watched || clause.size <= 2)
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
        // remove the literal and push it to the back
        // we push it to the back so that we can print the clause even after the literal is removed
        NOTIFY_OBSERVER(_observer, new napsat::gui::remove_literal(cl, *i));
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
          ASSERT_MSG(lit_false(lits[i]),
            "Clause: " + clause_to_string(cl) + "\nLiteral: " + lit_to_string(lits[i]));
          ASSERT(lit_level(lits[i]) == LEVEL_ROOT);
        }
#endif
        NOTIFY_OBSERVER(_observer, new napsat::gui::remove_literal(cl, lits[1]));
        clause.size--;
      }
    }

    ASSERT_MSG(!clause.deleted,
               "Clause: " + clause_to_string(cl) + " was deleted.");
    if (_proof && previous_size != clause.size) {
      _proof->start_resolution_chain();
      _proof->link_resolution(LIT_UNDEF, cl);
      prove_root_literal_removal(clause.lits + clause.size, previous_size - clause.size);
      // we need to deactivate the clause to be able to replace it
      _proof->deactivate_clause(cl);
      _proof->finalize_resolution(cl, lits, clause.size);
    }

    if (clause.size == 2) {
      _binary_clauses[lits[0]].push_back(make_pair(lits[1], cl));
      _binary_clauses[lits[1]].push_back(make_pair(lits[0], cl));
      NOTIFY_OBSERVER(_observer, new napsat::gui::stat("Binary clause simplified"));
    }
    if (clause.size == 1) {
      clause.watched = false;
      // The literal might be a missed lower implication
      if (lit_true(lits[0])) {
        ASSERT(_options.chronological_backtracking);
        if (_options.lazy_strong_chronological_backtracking)
          reimply_literal(lits[0], cl);
      }
      else {
        ASSERT(lit_undef(lits[0]));
        imply_literal(lits[0], cl);
      }
      NOTIFY_OBSERVER(_observer, new napsat::gui::stat("Unit clause simplified"));
    }
  }
  // remove the deleted clauses
  repair_watch_lists();
  NOTIFY_OBSERVER(_observer, new napsat::gui::check_invariants());
  ASSERT(watch_lists_complete());
  ASSERT(watch_lists_minimal());
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
      NOTIFY_OBSERVER(_observer, new napsat::gui::stat("Clause deleted"));
    }
  }
  repair_watch_lists();
  ASSERT(watch_lists_complete());
  ASSERT(watch_lists_minimal());
  NOTIFY_OBSERVER(_observer, new napsat::gui::stat("Clause set simplified"));
}
