#include "modulariT-SAT.hpp"

#include <iostream>
#include <cstring>
#include <cassert>
#include <functional>

using namespace sat;
using namespace std;

#ifdef NDEBUG
#if OBSERVED_ASSERTS
#define ASSERT(cond)                                                \
  if (_observer) {                                                  \
    if (!(cond))  {                                                 \
      NOTIFY_OBSERVER(_observer, new sat::gui::marker("Assetion failed: " #cond));  \
      assert(cond);                                                 \
    }                                                               \
  }
#else
#define ASSERT(cond) assert(cond)
#endif
#else
#define ASSERT(cond)
#endif

void modulariT_SAT::stack_lit(Tlit lit, Tclause reason)
{
  ASSERT(lit_undef(lit));
  Tvar var = lit_to_var(lit);
  _trail.push_back(lit);
  TSvar& svar = _vars[var];
  svar.state = lit_pol(lit);
  svar.waiting = true;
  svar.reason = reason;
  svar.waiting = true;

  _agility *= _options.agility_decay;
  _options.agility_threshold *= _options.threshold_multiplier;

  if (reason == CLAUSE_UNDEF || reason == CLAUSE_LAZY) {
    // Decision
    // TODO this is going to bug if hints are not pushed at the highest level
    _decision_index.push_back(_trail.size() - 1);
    svar.level = _decision_index.size();
    NOTIFY_OBSERVER(_observer, new sat::gui::decision(lit));
  }
  else {
    // Implied literal
    ASSERT(lit == _clauses[reason].lits[0]);
    if (_clauses[reason].size == 1)
      svar.level = LEVEL_ROOT;
    else if (_options.chronological_backtracking) {
      // Find the level of the clause
      Tlevel level = LEVEL_ROOT;
      Tlit* end = _clauses[reason].lits + _clauses[reason].size;
      for (Tlit* i = _clauses[reason].lits; i < end; i++)
        if (lit_level(*i) > level && *i != lit)
          level = lit_level(*i);
      svar.level = level;
    }
    else {
      ASSERT(watched_levels(reason));
      ASSERT(most_relevant_watched_literals(reason));
      ASSERT(lit == _clauses[reason].lits[0]);
      svar.level = lit_level(_clauses[reason].lits[1]);
    }
    NOTIFY_OBSERVER(_observer, new sat::gui::implication(lit, reason, svar.level));
  }
  if (lit_pol(lit) != svar.phase_cache)
    _agility += 1 - _options.agility_decay;
  svar.phase_cache = lit_pol(lit);

  if (svar.level == LEVEL_ROOT)
    _purge_counter++;
  ASSERT(svar.level != LEVEL_UNDEF);
  ASSERT(svar.level <= _decision_index.size());
}

Tclause modulariT_SAT::propagate_lit(Tlit lit)
{
  lit = lit_neg(lit);

  // go through the binary clauses
  for (pair<Tlit, Tclause> bin : _binary_clauses[lit]) {
    if (lit_undef(bin.first)) {
      // ensure that the stacked literal is positioned at the first position
      Tlit* lits = _clauses[bin.second].lits;
      ASSERT(lits[0] == lit || lits[1] == lit);
      ASSERT(lits[0] == bin.first || lits[1] == bin.first);
      lits[0] = bin.first;
      lits[1] = lit;
      stack_lit(bin.first, bin.second);
    }
    else if (lit_false(bin.first)) {
      // make sure that the highest literal is at the first position
      Tlit* lits = _clauses[bin.second].lits;
      if (lit_level(lits[0]) < lit_level(lits[1])) {
        // in place swapping
        lits[0] = lits[0] ^ lits[1];
        lits[1] = lits[0] ^ lits[1];
        lits[0] = lits[0] ^ lits[1];
      }
      return bin.second;
    }
    // missed lower implications are not possible for binary clauses
    ASSERT(lit_true(bin.first));
    ASSERT(lit_level(bin.first) <= lit_level(lit));
  }

  // level of the propagation
  Tlevel level = lit_level(lit);
  vector<Tclause>& watch_list = _watch_lists[lit];
  unsigned n = watch_list.size();
  for (unsigned i = 0; i < n; i++) {
    Tclause cl = watch_list[i];
    TSclause& clause = _clauses[cl];
    ASSERT(clause.watched);
    ASSERT(clause.size >= 2);
    Tlit* lits = clause.lits;
    ASSERT(lit == lits[0] || lit == lits[1]);
    Tlit lit2 = lits[0] ^ lits[1] ^ lit;
    lits[0] = lit2;
    lits[1] = lit;
    if (lit_true(clause.blocker) && (!_options.chronological_backtracking || lit_level(clause.blocker) <= level))
      continue; // clause is satisfied by the blocker, do not do anything
    if (lit_true(lit2) && (!_options.strong_chronological_backtracking || lit_level(lit2) <= level))
      continue; // clause is satisfied, do not do anything

    // Tlit replacement = LIT_UNDEF;
    Tlit* end = clause.lits + clause.size;
    Tlit* k = lits + 2;

    ASSERT(lit2 == *lits);
    Tlevel lowest_satisfied_level = lit_true(lit2) ? lit_level(lit2) : LEVEL_UNDEF;
    Tlit* lowest_satisfied_literal = lit_true(lit2) ? lits : nullptr;
    ASSERT(lit_false(lit));
    ASSERT(lit == *(lits + 1));
    Tlevel highest_non_satisfied_level = level;
    Tlit* highest_non_satisfied_literal = lits + 1;
    unsigned n_satisfied_literals = lit_true(lit2);

    while (k < end) {
      // we assume that undefined literals have an infinite decision level.
      ASSERT(!lit_undef(*k) || lit_level(*k) > _decision_index.size());
      Tlevel curr_level = lit_level(*k);
      if (lit_true(*k)) {
        if (!_options.strong_chronological_backtracking || curr_level <= highest_non_satisfied_level)
          break;
        lowest_satisfied_level = lit_level(*k);
        lowest_satisfied_literal = k;
        n_satisfied_literals++;
      }
      else if (curr_level > highest_non_satisfied_level) {
        highest_non_satisfied_level = curr_level;
        highest_non_satisfied_literal = k;
        continue;
      }
      if (lowest_satisfied_level <= highest_non_satisfied_level ||
        n_satisfied_literals > 1) {
        // If the lowest satisfied literal is lower than the highest falsified literal, then we might still have a missed lower implication.
        // Here, we know that this is not the case, and we can swap lit with either the highest falsified literal or the lowest satisfied literal.
        // If we swap with the highest falsified literal, we will need to set the blocker to the lowest satisfied literal.
        ASSERT(k == highest_non_satisfied_literal || k == lowest_satisfied_literal);
        break;
      }
      k++;
    }

    if (lowest_satisfied_literal && lowest_satisfied_level <= level) {
      // set blocker and go next
      clause.blocker = *lowest_satisfied_literal;
      continue;
    }

    if (k >= end) {
      /**
       * If we reach this point, then we have either:
       * - only one satisfied literal at a level higher than the falsified literals => missed lower implication
       *    - if lit2 is true, then lit is swapped with the highest falsified literal
       *    - otherwise (lit2 is false): lit is swapped with the highest falsified literal and lit2 is swapped with the lowest satisfied literal
       *        - in both cases, the clause is added to the lazy reimplication buffer if the highest falsified literal is lower than the current reimplication level
       * - no satisfied literal in lit[2:end]
       *    - if lit2 is false, then the clause is conflicting
       *    - if lit2 is undef, then the clause is unit
       */
      if (n_satisfied_literals > 0) {
        // Maybe missed lower implication
        ASSERT(_options.strong_chronological_backtracking);
        ASSERT(lowest_satisfied_literal != nullptr);
        ASSERT(lowest_satisfied_level > highest_non_satisfied_level);
        ASSERT(n_satisfied_literals == 1);
        /**
         * If we have only one satisfied literal that does not quality as blocker, then we know that it must be the first one, or the first literal is not propagated yet.
         *
         * Indeed, in the case where a missed lower implication is detected, the satisfied literal is put as a watched literal. Therefore, when we ensure that lit is at the second possition, we know that the satisfied literal is at the first position (lit2).
         *
         * We just need to make sure that the second watched literal gets the higest level in the clause, such that future routines can detect the level the missed lower implication.
         *
         * We are garanteed that the clause will not change anymore, because we just finished propagating the falsified literal and the other one is the satisfied literal.
         */
        if (!lit_true(lit2)) {
          // just swap lit with the satisfied literal
          // we know that propagating the second literal will detect the missed lower implication if there is one.
          ASSERT(lits != lowest_satisfied_literal);
          lits[1] = *lowest_satisfied_literal;
          *lowest_satisfied_literal = lit;

          // update the watched literal
#if NOTIFY_WATCH_CHANGES
          NOTIFY_OBSERVER(_observer, new sat::gui::unwatch(cl, lit));
#endif
          // remove the clause from the watch list
          watch_list[i--] = watch_list[--n];
          watch_list.pop_back();
          // watch new literal
          watch_lit(lits[1], cl);
          continue;
        }
        ASSERT(lit_true(lit2));
        ASSERT(lowest_satisfied_literal == lits);
        if (highest_non_satisfied_literal != lits + 1) {
          // bring the highest falsified literal to the front such that when reimplication happen, we know which level the reimplication is at
          lits[1] = *highest_non_satisfied_literal;
          *highest_non_satisfied_literal = lit;
#if NOTIFY_WATCH_CHANGES
          NOTIFY_OBSERVER(_observer, new sat::gui::unwatch(cl, lit));
#endif
          // remove the clause from the watch list
          watch_list[i--] = watch_list[--n];
          watch_list.pop_back();
          // watch new literal
          watch_lit(lits[1], cl);
        }
        // Definition of a missed lower implication:
        ASSERT(lit_level(lits[0]) > lit_level(lits[1]));
        ASSERT(lit_true(lits[0]));
        ASSERT(lit_false(lits[1]));

        // push the missed lower implication to the lazy reimplication buffer
        ASSERT(lit2 == lits[0]);
        Tclause lazy_reason = _lazy_reimplication_buffer[lit_to_var(lit2)];
        ASSERT(lit_true(_clauses[cl].lits[0]));
        ASSERT(lit_false(_clauses[cl].lits[1]));
        ASSERT(lazy_reason == CLAUSE_UNDEF || lazy_reason < _clauses.size());
        if (lazy_reason == CLAUSE_UNDEF || lit_level(_clauses[lazy_reason].lits[1]) > highest_non_satisfied_level) {
          _lazy_reimplication_buffer[lit_to_var(lits[0])] = cl;
          NOTIFY_OBSERVER(_observer, new sat::gui::stat("Lazy reimplication detected"));
        }
        continue;
      }
      if (lit_false(lit2)) {
        // the clause is conflicting
        if (_options.chronological_backtracking) {
          // Replace the watched literal by the highest falsified literal
          // It is not really necessary, since if it is not backtracked after conflict analysis, it will be propagated again.
          // But since we already know the best candidate for replacement, we might as well do it now instead of waiting for the next propagation.
          // However, we do not touch the other watched literal because it will be propagated later anyway if it is not backtracked.
          if (highest_non_satisfied_literal > lits + 1) {
            lits[1] = *highest_non_satisfied_literal;
            *highest_non_satisfied_literal = lit;
#if NOTIFY_WATCH_CHANGES
            NOTIFY_OBSERVER(_observer, new sat::gui::unwatch(cl, lit));
#endif
            // remove the clause from the watch list
            watch_list[i--] = watch_list[--n];
            watch_list.pop_back();
            // watch new literal
            watch_lit(lits[1], cl);
            // swap the first and second literals
            // the highest falsified literal is now at the second position
          }
          if (lit_level(lits[0]) < lit_level(lits[1])) {
            lits[1] = lits[0] ^ lits[1];
            lits[0] = lits[0] ^ lits[1];
            lits[1] = lits[0] ^ lits[1];
          }
        }
        ASSERT(lit_level(lits[0]) >= lit_level(lits[1]));
        return cl;
      }
      ASSERT(lit_false(*highest_non_satisfied_literal));
      if (highest_non_satisfied_literal != lits + 1) {
        // Replace the literal with the highest falsified literal
        lits[1] = *highest_non_satisfied_literal;
        *highest_non_satisfied_literal = lit;
#if NOTIFY_WATCH_CHANGES
        NOTIFY_OBSERVER(_observer, new sat::gui::unwatch(cl, lit));
#endif
        // remove the clause from the watch list
        watch_list[i--] = watch_list[--n];
        watch_list.pop_back();
        // watch new literal
        watch_lit(lits[1], cl);
        ASSERT(lit_undef(lit2));
      }
      // clause is unit, propagate
      stack_lit(lit2, cl);
      continue;
    }

    // watch the replacement and stop watching lit
    lits[1] = *k;
    *k = lit;
#if NOTIFY_WATCH_CHANGES
    NOTIFY_OBSERVER(_observer, new sat::gui::unwatch(cl, lit));
#endif
    // remove the clause from the watch list
    watch_list[i--] = watch_list[--n];
    watch_list.pop_back();
    // watch new literal
    watch_lit(lits[1], cl);
  }

  _vars[lit_to_var(lit)].waiting = false;
  return CLAUSE_UNDEF;
}

void modulariT_SAT::backtrack(Tlevel level)
{
  ASSERT(!_options.chronological_backtracking);
  ASSERT(level <= _decision_index.size());
  if (level == _decision_index.size())
    return;
  while (_decision_index.size() > level) {
    Tlit lit = _trail.back();
    _trail.pop_back();
    if (lit_reason(lit) == CLAUSE_UNDEF) {
      ASSERT(lit_level(lit) == _decision_index.size());
      _decision_index.pop_back();
    }
    Tvar var = lit_to_var(lit);
    var_unassign(var);
  }
  _propagated_literals = _trail.size();
  ASSERT(trail_consistency());
  ASSERT(trail_monotonicity());
}

void sat::modulariT_SAT::CB_backtrack(Tlevel level)
{
  ASSERT(_options.chronological_backtracking);
  ASSERT(level <= _decision_index.size());
  ASSERT(_backtrack_buffer.empty());
  if (level == _decision_index.size())
    return;
  NOTIFY_OBSERVER(_observer, new sat::gui::backtracking_started(level));
  unsigned waiting_count = 0;

  while (_decision_index.size() > level) {
    Tlit lit = _trail.back();
    _trail.pop_back();
    if (lit_reason(lit) == CLAUSE_UNDEF) {
      ASSERT(lit_level(lit) == _decision_index.size());
      _decision_index.pop_back();
    }
    if (lit_level(lit) > level) {
      var_unassign(lit_to_var(lit));
      if (_options.strong_chronological_backtracking) {
        // look if the literal can be reimplied at a lower level
        Tclause lazy_reason = _lazy_reimplication_buffer[lit_to_var(lit)];
        if (lazy_reason != CLAUSE_UNDEF && lit_level(_clauses[lazy_reason].lits[1]) <= level) {
          ASSERT(_clauses[lazy_reason].lits[0] == lit);
          ASSERT(lit_undef(_clauses[lazy_reason].lits[0]));
          _reimplication_backtrack_buffer.push_back(lazy_reason);
        }
        // we need to reset it regardless of whether it was useful or not
        _lazy_reimplication_buffer[lit_to_var(lit)] = CLAUSE_UNDEF;
      }
      continue;
    }
    _backtrack_buffer.push_back(lit);
    waiting_count += _vars[lit_to_var(lit)].waiting;
  }
  while (!_backtrack_buffer.empty()) {
    Tlit lit = _backtrack_buffer.back();
    _backtrack_buffer.pop_back();
    _trail.push_back(lit);
  }
  _propagated_literals = _trail.size() - waiting_count;
  if (_options.strong_chronological_backtracking && _reimplication_backtrack_buffer.size() > 0) {
    // adds the literals on the lazy reimplication buffer to the trail by order of increasing level
    // Sort the literals by increasing level
    // The topological order will automatically be respected because the reimplied literals cannot depend on each other.
    // todo maybe we want a stable sort for level collapsing. In this case, the literals will actually depend on each other, and their relative order should be preserved.
    sort(_reimplication_backtrack_buffer.begin(), _reimplication_backtrack_buffer.end(), [this](Tclause a, Tclause b)
      { return lit_level(_clauses[a].lits[1]) < lit_level(_clauses[b].lits[1]); });
    for (Tclause lazy_clause : _reimplication_backtrack_buffer) {
      Tlit reimpl_lit = _clauses[lazy_clause].lits[0];
      ASSERT(lit_undef(reimpl_lit));
      stack_lit(reimpl_lit, lazy_clause);
      NOTIFY_OBSERVER(_observer, new sat::gui::stat("Lazy reimplication used"));
    }
    _reimplication_backtrack_buffer.clear();
  }
}

bool sat::modulariT_SAT::lit_is_required_in_learned_clause(Tlit lit)
{
  ASSERT(lit_false(lit));
  if (lit_reason(lit) == CLAUSE_UNDEF)
    return true;
  ASSERT(lit_reason(lit) < _clauses.size());
  TSclause& clause = _clauses[lit_reason(lit)];
  ASSERT(!clause.deleted);
  for (unsigned i = 1; i < clause.size; i++)
    if (!lit_seen(clause.lits[i]))
      return true;
  return false;
}

void modulariT_SAT::analyze_conflict_reimply(Tclause conflict)
{
  ASSERT(watch_lists_minimal());
  ASSERT(conflict != CLAUSE_UNDEF);
  ASSERT(!_writing_clause);
  unsigned count = 0;
  TSclause& clause = _clauses[conflict];
  bump_clause_activity(conflict);
  _next_literal_index = 0;
  Tlevel conflict_level = lit_level(clause.lits[0]);
  assert(conflict_level > LEVEL_ROOT);
  Tlevel second_highest_level = LEVEL_ROOT;
  // prepare the learn clause in the literal buffer
  for (unsigned j = 0; j < clause.size; j++) {
    Tlit lit = clause.lits[j];
    ASSERT(lit_false(lit));
    ASSERT(lit_level(lit) <= conflict_level);
    ASSERT(!lit_seen(lit));
    bump_var_activity(lit_to_var(lit));
    lit_mark_seen(lit);
    ASSERT(lit_false(lit));
    ASSERT(lit_level(lit) <= conflict_level);
    if (lit_level(lit) == conflict_level)
      count++;
    else {
      _literal_buffer[_next_literal_index++] = lit;
      second_highest_level = max(second_highest_level, lit_level(lit));
    }
  }

  ASSERT(count > 1);
  // replace the literals in the clause by their reason
  unsigned i = _trail.size() - 1;

  while (count > 0) {
    // search the next literal involved in the conflict (in topological order)
    while (!lit_seen(_trail[i]) || lit_level(_trail[i]) != conflict_level)
      i--;
    ASSERT(i < _trail.size());
    Tlit lit = _trail[i];
    /*************************************************************************/
    /*                         LAZY RE-IMPLICATION                           */
    /*************************************************************************/
    if (count == 1) {
      Tclause lazy_reason = _lazy_reimplication_buffer[lit_to_var(lit)];
      if (lazy_reason == CLAUSE_UNDEF) {
        _literal_buffer[_next_literal_index++] = lit_neg(_trail[i]);
        break;
      }
      else {
        NOTIFY_OBSERVER(_observer, new sat::gui::stat("Lazy reimplication used"));
      }
      // The missed lower implication will be propagated again after backtracking.
      // So we anticipate and continue conflict analysis directly
      count = 0;
      for (unsigned j = 1; j < _clauses[lazy_reason].size; j++) {
        Tlit lazy_lit = _clauses[lazy_reason].lits[j];
        bump_var_activity(lit_to_var(lazy_lit));
        ASSERT(lit_false(lazy_lit));
        if (lit_seen(lazy_lit))
          continue;
        lit_mark_seen(lazy_lit);
        Tlevel curr_lit_level = lit_level(lazy_lit);
        ASSERT(curr_lit_level <= conflict_level);
        if (curr_lit_level == conflict_level)
          count++;
        else {
          second_highest_level = max(second_highest_level, curr_lit_level);
          _literal_buffer[_next_literal_index++] = lazy_lit;
        }
      }
      lit_unmark_seen(lit);
      // clean up the literals at level conflict_level
      conflict_level = second_highest_level;
      second_highest_level = LEVEL_ROOT;
      unsigned k = 0;
      for (unsigned j = 0; j < _next_literal_index; j++) {
        ASSERT(lit_false(_literal_buffer[j]));
        ASSERT(lit_level(_literal_buffer[j]) <= conflict_level);
        if (lit_level(_literal_buffer[j]) == conflict_level) {
          count++;
          continue;
        }
        _literal_buffer[k++] = _literal_buffer[j];
        second_highest_level = max(second_highest_level, lit_level(_literal_buffer[j]));
      }
      _next_literal_index = k;
      // reset the seach head since we changed the level
      i = _trail.size() - 1;
      continue;
    } // end if (count == 1)
    /*************************************************************************/
    /*                             STANDARD FUIP                             */
    /*************************************************************************/
    // process the literals at highest level
    Tclause reason = _lazy_reimplication_buffer[lit_to_var(lit)];
    if (reason == CLAUSE_UNDEF)
      reason = lit_reason(lit);
    else {
      NOTIFY_OBSERVER(_observer, new sat::gui::stat("Lazy reimplication used"));
    }
    ASSERT(reason != CLAUSE_UNDEF);
    TSclause& clause = _clauses[reason];
    ASSERT(lit_true(clause.lits[0]));
    // start at 1 because the first literal is the one that was propagated
    ASSERT(lit == clause.lits[0]);
    for (unsigned j = 1; j < clause.size; j++) {
      Tlit curr_lit = clause.lits[j];
      bump_var_activity(lit_to_var(curr_lit));
      if (lit_seen(curr_lit))
        continue;
      lit_mark_seen(curr_lit);
      Tlevel curr_lit_level = lit_level(curr_lit);
      ASSERT(curr_lit_level <= conflict_level);
      if (curr_lit_level == conflict_level)
        count++;
      else {
        second_highest_level = max(second_highest_level, curr_lit_level);
        _literal_buffer[_next_literal_index++] = curr_lit;
      }
    }
    // unmark the replaced literal
    lit_unmark_seen(lit);
    count--;
  } // end while (count > 0)

  // remove the seen markers and the literals that can be removed with subsumption resolution
  unsigned k = 0;
  for (unsigned j = 0; j < _next_literal_index; j++) {
    Tlit lit = _literal_buffer[j];
    ASSERT(lit_false(lit));
    ASSERT(lit_level(lit) <= conflict_level);
    // TODO check if removing them in the other direct is not better
    if (lit_is_required_in_learned_clause(lit))
      _literal_buffer[k++] = _literal_buffer[j];
    else
      lit_unmark_seen(_literal_buffer[j]);
  }
  _next_literal_index = k;
  for (unsigned j = 0; j < _next_literal_index; j++)
    lit_unmark_seen(_literal_buffer[j]);
  if (conflict_level != LEVEL_ROOT) {
    CB_backtrack(conflict_level - 1);
  }
  internal_add_clause(_literal_buffer, _next_literal_index, true, false);
}

void modulariT_SAT::analyze_conflict(Tclause conflict)
{
  ASSERT(watch_lists_minimal());
  ASSERT(conflict != CLAUSE_UNDEF);
  ASSERT(!_writing_clause);
  unsigned count = 0;
  unsigned i = _trail.size() - 1;
  TSclause& clause = _clauses[conflict];
  bump_clause_activity(conflict);
  _next_literal_index = 0;
  Tlevel conflict_level = lit_level(clause.lits[0]);
  Tlevel second_highest_level = LEVEL_ROOT;
  for (unsigned j = 0; j < clause.size; j++) {
    Tlit lit = clause.lits[j];
    ASSERT(lit_false(lit));
    bump_var_activity(lit_to_var(lit));
    if (lit_level(lit) == conflict_level) {
      lit_mark_seen(lit);
      count++;
    }
    else if (lit_is_required_in_learned_clause(lit)) {
      ASSERT(lit_level(lit) < conflict_level);
      ASSERT(lit_false(lit));
      lit_mark_seen(lit);
      _literal_buffer[_next_literal_index++] = lit;
      second_highest_level = max(second_highest_level, lit_level(lit));
    }
  }
  ASSERT(count > 1);
  while (count > 1) {
    while (!lit_seen(_trail[i]) || lit_level(_trail[i]) != conflict_level) {
      ASSERT(i > 0);
      i--;
    }
    ASSERT(count <= _trail.size());
    ASSERT(i < _trail.size());
    ASSERT(lit_seen(_trail[i]));
    ASSERT(lit_level(_trail[i]) == conflict_level);
    Tlit lit = _trail[i];
    lit_unmark_seen(lit);
    count--;
    Tclause reason = lit_reason(lit);
    bump_clause_activity(reason);
    ASSERT(reason != CLAUSE_UNDEF);
    TSclause& clause = _clauses[reason];

    ASSERT(lit_true(clause.lits[0]));
    // start at 1 because the first literal is the one that was propagated
    for (unsigned j = 1; j < clause.size; j++) {
      Tlit lit = clause.lits[j];
      ASSERT(lit_false(lit));
      bump_var_activity(lit_to_var(lit));
      if (lit_seen(lit))
        continue;
      if (lit_level(lit) == conflict_level) {
        lit_mark_seen(lit);
        count++;
      }
      else if (lit_is_required_in_learned_clause(lit)) {
        ASSERT(lit_level(lit) < conflict_level);
        ASSERT(lit_false(lit));
        lit_mark_seen(lit);
        _literal_buffer[_next_literal_index++] = lit;
        second_highest_level = max(second_highest_level, lit_level(lit));
      }
    }
  }
  while (!lit_seen(_trail[i]) || lit_level(_trail[i]) != conflict_level) {
    ASSERT(i > 0);
    i--;
  }
  _literal_buffer[_next_literal_index++] = lit_neg(_trail[i]);
  // remove the seen markers on literals in the clause
  for (unsigned j = 0; j < _next_literal_index; j++)
    lit_unmark_seen(_literal_buffer[j]);
  // backtrack depending on the chronological backtracking strategy
  if (_options.chronological_backtracking)
    CB_backtrack(conflict_level - 1);
  else {
    Tlevel second_highest_level = LEVEL_ROOT;
    for (unsigned j = 0; j < _next_literal_index - 1; j++) {
      Tlit lit = _literal_buffer[j];
      ASSERT(lit_false(lit));
      ASSERT(lit_level(lit) <= conflict_level);
      second_highest_level = max(second_highest_level, lit_level(lit));
    }
    backtrack(second_highest_level);
  }
  internal_add_clause(_literal_buffer, _next_literal_index, true, false);
}

void modulariT_SAT::repair_conflict(Tclause conflict)
{
  NOTIFY_OBSERVER(_observer, new sat::gui::conflict(conflict));
  if (_status == SAT)
    _status = UNDEF;
  ASSERT(_clauses[conflict].size > 0);
  ASSERT(lit_false(_clauses[conflict].lits[0]));
  if (lit_level(_clauses[conflict].lits[0]) == LEVEL_ROOT) {
    _status = UNSAT;
    return;
  }
  if (_clauses[conflict].size == 1) {
    if (_options.chronological_backtracking)
      CB_backtrack(lit_level(_clauses[conflict].lits[0]) - 1);
    else
      backtrack(LEVEL_ROOT);
    // In strong chronological backtracking, the literal might have been propagated again during reimplication
    // Therefore, we might need to trigger another conflict analysis
    ASSERT(_options.strong_chronological_backtracking || lit_undef(_clauses[conflict].lits[0]));
    if (!lit_undef(_clauses[conflict].lits[0])) {
      // the problem is unsat
      // The literal could have been propagated by the reimplication
      _status = UNSAT;
      return;
    }
    stack_lit(_clauses[conflict].lits[0], conflict);

    return;
  }
  ASSERT(_options.chronological_backtracking || lit_level(_clauses[conflict].lits[0]) == _decision_index.size());
  if (_options.chronological_backtracking) {
    unsigned n_literal_at_highest_level = 1;
    Tlevel highest_level = lit_level(_clauses[conflict].lits[0]);
    for (unsigned i = 1; i < _clauses[conflict].size; i++) {
      Tlevel level = lit_level(_clauses[conflict].lits[i]);
      ASSERT(level <= highest_level);
      if (level == highest_level) {
        n_literal_at_highest_level++;
        break;
      }
    }
    if (n_literal_at_highest_level == 1) {
      CB_backtrack(lit_level(_clauses[conflict].lits[0]) - 1);
      // In strong chronological backtracking, the literal might have been propagated again during reimplication
      // Therefore, we might need to trigger another conflict analysis
      ASSERT(_options.strong_chronological_backtracking || lit_undef(_clauses[conflict].lits[0]));
      if (!lit_undef(_clauses[conflict].lits[0])) {
        // the first literal might not be at the highest level anymore
        // therefore we want to bring the highest level literal to the front
        // and then we can restart conflict analysis
        Tlit* lits = _clauses[conflict].lits;
        Tlit* end = lits + _clauses[conflict].size;
        Tlit* highest_level_literal = lits;
        Tlevel highest_level = lit_level(*lits);
        for (Tlit* i = lits + 1; i < end; i++) {
          if (lit_level(*i) > highest_level) {
            highest_level = lit_level(*i);
            highest_level_literal = i;
          }
        }
        Tlit tmp = *lits;
        *lits = *highest_level_literal;
        *highest_level_literal = tmp;
        if (highest_level_literal != lits + 1) {
          stop_watch(*highest_level_literal, conflict);
          watch_lit(*lits, conflict);
        }
        repair_conflict(conflict);
      }
      else
        stack_lit(_clauses[conflict].lits[0], conflict);
      return;
    }
  }
  if (_options.strong_chronological_backtracking)
    analyze_conflict_reimply(conflict);
  else {
    analyze_conflict(conflict);
  }
  _var_activity_increment /= _options.var_activity_decay;
}

void modulariT_SAT::restart()
{
  _agility = 1;
  _options.agility_threshold *= _options.agility_threshold_decay;
  if (_options.chronological_backtracking)
    CB_backtrack(LEVEL_ROOT);
  else
    backtrack(LEVEL_ROOT);
  ASSERT(trail_consistency());
  NOTIFY_OBSERVER(_observer, new sat::gui::stat("Restart"));
}

void sat::modulariT_SAT::purge_clauses()
{
  _purge_threshold += _purge_inc;

  for (Tclause cl = 0; cl < _clauses.size(); cl++) {
    // Do not remove clauses that are used as reasons
    if (cl == lit_reason(_clauses[cl].lits[0]))
      continue;
    if (_clauses[cl].deleted || !_clauses[cl].watched)
      continue;
    for (Tlit* i = _clauses[cl].lits; i < _clauses[cl].lits + _clauses[cl].size; i++) {
      if (lit_level(*i) > LEVEL_ROOT)
        continue;
      if (lit_false(*i)) {
        Tlit tmp = *i;
        *i = _clauses[cl].lits[_clauses[cl].size - 1];
        _clauses[cl].lits[_clauses[cl].size - 1] = tmp;
        _clauses[cl].size--;
        if (i < _clauses[cl].lits + 2 && _clauses[cl].size >= 2) {
          // the temporary set the clause to be in more than two watch lists.
          // The function repair_watch_lists will fix this.
          watch_lit(*i, cl);
        }
        i--;
      }
      else {
        ASSERT(lit_true(*i));
        delete_clause(cl);
        break;
      }
    }
    if (_clauses[cl].watched && _clauses[cl].size == 1) {
      _clauses[cl].watched = false;
      if (lit_undef(_clauses[cl].lits[0]))
        stack_lit(_clauses[cl].lits[0], cl);
      else {
        ASSERT(_options.chronological_backtracking);
        CB_backtrack(LEVEL_ROOT);
        if (lit_false(_clauses[cl].lits[0])) {
          _status = UNSAT;
          return;
        }
        if (lit_undef(_clauses[cl].lits[0]))
          stack_lit(_clauses[cl].lits[0], cl);
        // the clause is satisfied at level 0
        // delete it if it is not the current reason
        else if (cl != lit_reason(_clauses[cl].lits[0]))
          delete_clause(cl);
      }
    }
    else if (_clauses[cl].watched && _clauses[cl].size == 1) {
      _clauses[cl].watched = false;
      // insert in the binary clause watch list
      Tlit* lits = _clauses[cl].lits;
      NOTIFY_OBSERVER(_observer, new sat::gui::stat("Binary clause created"));
      _binary_clauses[lits[0]].push_back(make_pair(lits[1], cl));
      _binary_clauses[lits[1]].push_back(make_pair(lits[0], cl));
    }
    else if (_clauses[cl].size == 0) {
      _status = UNSAT;
      return;
    }
  }
  repair_watch_lists();
  ASSERT(watch_lists_complete());
  ASSERT(watch_lists_minimal());
}

void sat::modulariT_SAT::simplify_clause_set()
{
  _next_clause_elimination *= _options.clause_elimination_multiplier;
  _clause_activity_threshold *= _options.clause_activity_threshold_decay;
  double threshold = _max_clause_activity * _clause_activity_threshold;
  for (Tclause cl = 0; cl < _clauses.size(); cl++) {
    ASSERT(_clauses[cl].activity <= _max_clause_activity);
    ASSERT(_clauses[cl].size > 0);
    if (_clauses[cl].deleted || !_clauses[cl].watched || !_clauses[cl].learned)
      continue;
    if (_clauses[cl].size <= 2)
      continue;
    if (lit_reason(_clauses[cl].lits[0]) == cl)
      continue;
    if (_clauses[cl].activity < threshold) {
      delete_clause(cl);
      NOTIFY_OBSERVER(_observer, new sat::gui::stat("Clause deleted"));
    }
  }
  repair_watch_lists();
  ASSERT(watch_lists_complete());
  NOTIFY_OBSERVER(_observer, new sat::gui::stat("Clause set simplified"));
}

void sat::modulariT_SAT::order_trail()
{}

void sat::modulariT_SAT::select_watched_literals(Tlit* lits, unsigned size)
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

Tclause sat::modulariT_SAT::internal_add_clause(Tlit* lits_input, unsigned size, bool learned, bool external)
{
  for (unsigned i = 0; i < size; i++)
    bump_var_activity(lit_to_var(lits_input[i]));
  Tlit* lits;
  Tclause cl;
  TSclause* clause;
  if (learned)
    _n_learned_clauses++;
  if (external)
    _next_clause_elimination++;

  if (_deleted_clauses.empty()) {
    lits = new Tlit[size];
    memcpy(lits, lits_input, size * sizeof(Tlit));

    TSclause added(lits, size, learned, external);
    _clauses.push_back(added);
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
    if (clause->original_size < size) {
      delete[] clause->lits;
      clause->lits = new Tlit[size];
    }
    lits = clause->lits;
    memcpy(lits, lits_input, size * sizeof(Tlit));
    clause->size = size;
    clause->original_size = size;
    clause->deleted = false;
    clause->watched = true;
    clause->blocker = LIT_UNDEF;
  }
  clause->activity = _max_clause_activity;
  if (_observer) {
    vector<Tlit> lits_vector;
    for (unsigned i = 0; i < size; i++)
      lits_vector.push_back(lits[i]);
    _observer->notify(new sat::gui::new_clause(cl, lits_vector, learned, external));
  }

  ASSERT(clause_soundness(cl));
  if (size == 0) {
    clause->watched = false;
    _status = UNSAT;
    return cl;
  }
  if (size == 1) {
    clause->watched = false;
    if (lit_true(lits[0]))
      return cl;
    if (lit_false(lits[0]))
      repair_conflict(cl);
    else
      stack_lit(lits[0], cl);
    return cl;
  }
  else if (size == 2) {
    NOTIFY_OBSERVER(_observer, new sat::gui::stat("Binary clause added"));
    clause->watched = false;
    _binary_clauses[lits[0]].push_back(make_pair(lits[1], cl));
    _binary_clauses[lits[1]].push_back(make_pair(lits[0], cl));
    if (lit_false(lits[0]) && !lit_false(lits[1])) {
      // swap the literals so that the false literal is at the second position
      lits[1] = lits[0] ^ lits[1];
      lits[0] = lits[0] ^ lits[1];
      lits[1] = lits[0] ^ lits[1];
    }
    if (lit_false(lits[1])) {
      if (lit_undef(lits[0]))
        stack_lit(lits[0], cl);
      else
        repair_conflict(cl);
    }
  }
  else {
    select_watched_literals(lits, size);
    watch_lit(lits[0], cl);
    watch_lit(lits[1], cl);
    if (lit_false(lits[0]))
      repair_conflict(cl);
    else if (lit_false(lits[1]) && lit_undef(lits[0]))
      stack_lit(lits[0], cl);
    else if (lit_false(lits[1]) && lit_true(lits[0]) && (lit_level(lits[1]) < lit_level(lits[0]))) {
      Tclause lazy_reason = _lazy_reimplication_buffer[lit_to_var(lits[0])];
      if (lazy_reason == CLAUSE_UNDEF || lit_level(_clauses[lazy_reason].lits[1]) > lit_level(lits[1])) {
        _lazy_reimplication_buffer[lit_to_var(lits[0])] = cl;
        NOTIFY_OBSERVER(_observer, new sat::gui::stat("Lazy reimplication detected"));
      }
    }
    ASSERT(watch_lists_complete());
    ASSERT(watch_lists_minimal());
  }
  if (_options.delete_clauses && _n_learned_clauses >= _next_clause_elimination)
    simplify_clause_set();
  return cl;
}

/*****************************************************************************/
/*                            Public interface                               */
/*****************************************************************************/

sat::modulariT_SAT::modulariT_SAT(unsigned n_var, unsigned n_clauses, sat::options& options) :
  _options(options)
{
  _vars = vector<TSvar>(n_var + 1);
  _trail = vector<Tlit>();
  _trail.reserve(n_var);
  _watch_lists = vector<vector<Tclause>>(2 * n_var + 2);

  for (Tvar var = 1; var <= n_var; var++) {
    NOTIFY_OBSERVER(_observer, new sat::gui::new_variable(var));
    _variable_heap.insert(var, 0);
  }

  _clauses = vector<TSclause>();
  _clauses.reserve(n_clauses);

  _literal_buffer = new Tlit[n_var];
  _next_literal_index = 0;

  if (options.interactive) {
    _observer = new sat::gui::observer();
  }
  else if (options.observing) {
    _observer = new sat::gui::observer();
  }
  else if (options.check_invariants) {
    _observer = new sat::gui::observer();
    _observer->toggle_checking_only(true);
  }
  else if (options.print_stats) {
    _observer = new sat::gui::observer();
    _observer->toggle_stats_only(true);
  }
  else
    _observer = nullptr;
}

modulariT_SAT::~modulariT_SAT()
{
  for (unsigned i = 0; i < _clauses.size(); i++)
    delete[] _clauses[i].lits;
  if (_observer)
    delete _observer;
  delete[] _literal_buffer;
}

void modulariT_SAT::set_proof_callback(void (*proof_callback)(void))
{
  _proof_callback = proof_callback;
}

bool sat::modulariT_SAT::is_observing() const
{
  return _observer != nullptr;
}

sat::gui::observer* sat::modulariT_SAT::get_observer() const
{
  return _observer;
}

bool modulariT_SAT::propagate()
{
  if (_status != UNDEF)
    return false;
  while (_propagated_literals < _trail.size()) {
    Tlit lit = _trail[_propagated_literals];
    Tclause conflict = propagate_lit(lit);
    if (conflict != CLAUSE_UNDEF) {
      NOTIFY_OBSERVER(_observer, new sat::gui::conflict(conflict));
      repair_conflict(conflict);
      if (_status == UNSAT)
        return false;
      if (_agility < _options.agility_threshold)
        restart();
      continue;
    }
    _propagated_literals++;
    NOTIFY_OBSERVER(_observer, new sat::gui::propagation(lit));
    if (_purge_counter >= _purge_threshold) {
      purge_clauses();
      _purge_counter = 0;
      if (_status == UNSAT)
        return false;
    }
  }
  ASSERT(trail_consistency());
  ASSERT(no_unit_clauses());
  if (_trail.size() == _vars.size() - 1) {
    _status = SAT;
    return false;
  }
  return true;
}

status modulariT_SAT::solve()
{
  if (_status != UNDEF)
    return _status;
  while (true) {
    NOTIFY_OBSERVER(_observer, new sat::gui::check_invariants());
    if (!propagate()) {
      if (_status == UNSAT || !_options.interactive)
        break;
      NOTIFY_OBSERVER(_observer, new sat::gui::done(_status == SAT));
    }
    ASSERT(trail_consistency());
    if (_observer && _options.interactive)
      _observer->notify(new sat::gui::checkpoint());
    else
      decide();
  }
  ASSERT(trail_consistency());
  NOTIFY_OBSERVER(_observer, new sat::gui::done(_status == SAT));
  return _status;
}

status modulariT_SAT::get_status()
{
  return _status;
}

bool modulariT_SAT::decide()
{
  while (!_variable_heap.empty() && !var_undef(_variable_heap.top()))
    _variable_heap.pop();
  if (_variable_heap.empty()) {
    _status = SAT;
    return false;
  }
  Tvar var = _variable_heap.top();
  Tlit lit = literal(var, _vars[var].phase_cache);
  stack_lit(lit, CLAUSE_UNDEF);
  return true;
}

bool sat::modulariT_SAT::decide(Tlit lit)
{
  ASSERT(lit_undef(lit));
  stack_lit(lit, CLAUSE_UNDEF);
  return true;
}

void modulariT_SAT::start_clause()
{
  ASSERT(!_writing_clause);
  _writing_clause = true;
  _next_literal_index = 0;
}

void modulariT_SAT::add_literal(Tlit lit)
{
  ASSERT(_writing_clause);
  Tvar var = lit_to_var(lit);
  var_allocate(var);
  ASSERT(_next_literal_index < _vars.size());
  _literal_buffer[_next_literal_index++] = lit;
}

status modulariT_SAT::finalize_clause()
{
  ASSERT(_writing_clause);
  _writing_clause = false;
  internal_add_clause(_literal_buffer, _next_literal_index, false, true);
  propagate();
  return _status;
}

status sat::modulariT_SAT::add_clause(Tlit* lits, unsigned size)
{
  Tvar max_var = 0;
  for (unsigned i = 0; i < size; i++)
    if (lit_to_var(lits[i]) > max_var)
      max_var = lit_to_var(lits[i]);
  var_allocate(max_var);
  internal_add_clause(lits, size, false, true);
  propagate();
  return _status;
}

const Tlit* sat::modulariT_SAT::get_clause(Tclause cl) const
{
  assert(cl < _clauses.size());
  return _clauses[cl].lits;
}

unsigned sat::modulariT_SAT::get_clause_size(Tclause cl) const
{
  assert(cl < _clauses.size());
  return _clauses[cl].size;
}

void modulariT_SAT::hint(Tlit lit)
{
  ASSERT(lit_to_var(lit) < _vars.size());
  ASSERT(!_writing_clause);
  ASSERT(lit_undef(lit));
  stack_lit(lit, CLAUSE_LAZY);
}

void modulariT_SAT::hint(Tlit lit, unsigned int level)
{
  ASSERT(lit_to_var(lit) < _vars.size());
  ASSERT(!_writing_clause);
  ASSERT(lit_undef(lit));
  ASSERT(level <= _decision_index.size() + 1);
  hint(lit);
  _vars[lit_to_var(lit)].level = level;
}

void modulariT_SAT::synchronize()
{
  _number_of_valid_literals = _trail.size();
  for (Tvar var : _touched_variables)
    _vars[var].state_last_sync = _vars[var].state;

  _touched_variables.clear();
}

unsigned modulariT_SAT::sync_validity_limit()
{
  return _number_of_valid_literals;
}

unsigned modulariT_SAT::sync_color(Tvar var)
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

void modulariT_SAT::set_markup(void (*markup_function)(void))
{
  ASSERT(markup_function);
}

const std::vector<Tlit>& modulariT_SAT::trail() const
{
  return _trail;
}

Tlevel modulariT_SAT::decision_level() const
{
  return _decision_index.size();
}
