#include "modulariT-SAT.hpp"

#include <iostream>
#include <cstring>
#include <cassert>

using namespace sat;
using namespace std;

void modulariT_SAT::stack_lit(Tlit lit, Tclause reason)
{
#if LOG_LEVEL > 0
  cout << "Stacking ";
  print_lit(lit);
  cout << " with reason ";
  print_clause(reason);
  cout << "\n";
#endif
#if STATS
  if (reason == SAT_CLAUSE_UNDEF)
    _stats.inc_decisions();
#endif
  assert(lit_undef(lit));
  Tvar var = lit_to_var(lit);
  _trail.push_back(lit);
  TSvar &svar = _vars[var];
  svar.state = lit_pol(lit);
  svar.waiting = true;
  svar.reason = reason;
  svar.waiting = true;

  _agility *= _agility_decay;
  _agility_threshold *= _threshold_multiplier;

  if (reason == SAT_CLAUSE_UNDEF)
  {
    // Decision
    _decision_index.push_back(_trail.size() - 1);
    svar.level = _decision_index.size();
  }
  else
  {
    // Implied literal
    assert(lit == _clauses[reason].lits[0]);
    if (_clauses[reason].size == 1)
      svar.level = SAT_LEVEL_ROOT;
    else if (_chronological_backtracking)
    {
      // Find the level of the clause
      Tlevel level = SAT_LEVEL_ROOT;
      Tlit *end = _clauses[reason].lits + _clauses[reason].size;
      for (Tlit *i = _clauses[reason].lits; i < end; i++)
        if (lit_level(*i) > level && *i != lit)
          level = lit_level(*i);
      svar.level = level;
    }
    else
    {
      assert(watched_levels(reason));
      assert(most_relevant_watched_literals(reason));
      assert(lit == _clauses[reason].lits[0]);
      svar.level = lit_level(_clauses[reason].lits[1]);
    }
  }
  if (lit_pol(lit) != svar.phase_cache)
    _agility += 1 - _agility_decay;
  svar.phase_cache = lit_pol(lit);

  if (svar.level == SAT_LEVEL_ROOT)
    _purge_counter++;
  assert(svar.level != SAT_LEVEL_UNDEF);
  assert(svar.level <= _decision_index.size());
}

Tclause modulariT_SAT::propagate_lit(Tlit lit)
{
#if LOG_LEVEL > 1
  cout << "---- PROPAGATING ";
  print_lit(lit);
  cout << "\n";
#if LOG_LEVEL > 5
  print_trail();
#endif
#endif
#if STATS
  _stats.inc_propagations();
#endif
  lit = lit_neg(lit);
  vector<Tclause> &watch_list = _watch_lists[lit];

  unsigned n = watch_list.size();
  for (unsigned i = 0; i < n; i++)
  {
    Tclause cl = watch_list[i];
    TSclause &clause = _clauses[cl];
#if LOG_LEVEL > 4
    cout << "Checking clause ";
    print_clause(cl);
    cout << endl;
#endif
#if DEBUG_CLAUSE > 0
    if (cl == DEBUG_CLAUSE)
    {
      cout << "Checking clause ";
      print_clause(cl);
      cout << "\n";
    }
#endif
    assert(clause.watched);
    assert(clause.size >= 2);
    Tlit *lits = clause.lits;
    assert(lit == lits[0] || lit == lits[1]);
    Tlit lit2 = lits[0] ^ lits[1] ^ lit;
    lits[0] = lit2;
    lits[1] = lit;
    if (lit_true(lit2))
    {
      // clause is satisfied, move it to the end of the watch list
#if LOG_LEVEL > 4
      cout << "Clause is satisfied\n";
#endif
#if DEBUG_CLAUSE > 0
      if (cl == DEBUG_CLAUSE)
        cout << "Clause is satisfied\n";
#endif
      continue;
    }
    if (lit_true(clause.blocker)
    && (!_chronological_backtracking || lit_level(clause.blocker) <= lit_level(lit)))
    {
      // clause is satisfied, move it to the end of the watch list
#if LOG_LEVEL > 4
      cout << "Clause is blocked by ";
      print_lit(clause.blocker);
      cout << "\n";
#endif
#if DEBUG_CLAUSE > 0
      if (cl == DEBUG_CLAUSE)
      {
        cout << "Clause is blocked by ";
        print_lit(clause.blocker);
        cout << "\n";
      }
#endif
      continue;
    }
    Tlit replacement = SAT_LIT_UNDEF;
    Tlit *end = clause.lits + clause.size;
    Tlit *k = lits + 2;
#if DEBUG_CLAUSE > 0
    if (cl == DEBUG_CLAUSE)
    {
      cout << "Clause is not satisfied\n";
      cout << "Looking for replacement\n";
    }
#endif
    while (k < end)
    {
      if (lit_false(*k))
      {
        k++;
        continue;
      }
      replacement = *k;
#if SAT_BLOCKERS
      if (lit_true(replacement)
      && (!_chronological_backtracking || lit_level(replacement) <= lit_level(lit)))
      {
        clause.blocker = replacement;
#if STATS
        _stats.inc_blocked_clauses();
#endif
#if LOG_LEVEL > 2
        cout << "Clause sets up blocker ";
        print_lit(clause.blocker);
        cout << "\n";
#endif
        goto next_clause;
      }
#endif
      break;
    }
    if (replacement == SAT_LIT_UNDEF)
    {
      assert(!lit_true(lit2));
      if (lit_false(lit2))
      {
        // the clause is conflicting
#if LOG_LEVEL > 2
        cout << "Clause is conflicting\n";
#endif
#if SAT_GREEDY_STOP
#if DEBUG_CLAUSE > 0
        if (cl == DEBUG_CLAUSE)
        {
          cout << "Clause is conflicting\n";
          cout << "Conflict ";
          print_clause(conflict);
          cout << "\n";
        }
#endif
        if (_chronological_backtracking)
        {
          assert(lit2 == lits[0]);
          assert(lit == lits[1]);
          select_watched_literals(lits, _clauses[cl].size);
          if (lit2 != lits[0] && lit2 != lits[1])
            stop_watch(lit2, cl);
          if (lit != lits[0] && lit != lits[1]) {
            watch_list[i--] = watch_list[--n];
            watch_list.pop_back();
          }
          if (lits[0] != lit && lits[0] != lit2)
            watch_lit(lits[0], cl);
          if (lits[1] != lit && lits[1] != lit2)
            watch_lit(lits[1], cl);
        }
        return cl;
#endif
      }
      else
      {
        assert(lit_undef(lit2));
        // clause is unit, propagate
        stack_lit(lit2, cl);
#if LOG_LEVEL > 4
        cout << "Clause is unit\n";
#endif
#if DEBUG_CLAUSE > 0
        if (cl == DEBUG_CLAUSE)
          cout << "Clause is unit\n";
#endif
        continue;
      }
    }
    assert(lit_undef(replacement)
       || (_chronological_backtracking
        && lit_level(replacement) > lit_level(lit)));
    assert(replacement != lits[0] && replacement != lits[1]);
    assert(*k == replacement);
    // watch the replacement and stop watching lit
    lits[1] = replacement;
    *k = lit;
    // watch new literal
    watch_lit(replacement, cl);
    // remove the clause from the watch list
    watch_list[i--] = watch_list[--n];
    watch_list.pop_back();
#if LOG_LEVEL > 2
    cout << "Clause is watched by ";
    print_lit(replacement);
    cout << " and becomes ";
    print_clause(cl);
    cout << "\n";
#endif
  next_clause:;
  }

  _vars[lit_to_var(lit)].waiting = false;
  assert(trail_consistency());
  return SAT_CLAUSE_UNDEF;
}

void modulariT_SAT::backtrack(Tlevel level)
{
  assert(!_chronological_backtracking);
#if LOG_LEVEL > 0
  cout << "---- BACKTRACKING to level " << level << "\n";
#if LOG_LEVEL > 3
  print_trail();
#endif
#endif
  assert(level <= _decision_index.size());
  if (level == _decision_index.size())
    return;
#if STATS
  _stats.add_backtrack_difference(_decision_index.size() - level);
#endif
  while (_decision_index.size() > level)
  {
    Tlit lit = _trail.back();
    _trail.pop_back();
    if (lit_reason(lit) == SAT_CLAUSE_UNDEF)
    {
      assert(lit_level(lit) == _decision_index.size());
      _decision_index.pop_back();
    }
    Tvar var = lit_to_var(lit);
    var_unassign(var);
  }
  _propagated_literals = _trail.size();
#if LOG_LEVEL > 4
  print_trail();
#endif
  assert(trail_consistency());
  assert(trail_monotonicity());
}

void sat::modulariT_SAT::CB_backtrack(Tlevel level)
{
  assert(_chronological_backtracking);
#if LOG_LEVEL > 0
  cout << "---- CHRONOLOGICAL BACKTRACKING to level " << level << "\n";
#if LOG_LEVEL > 3
  print_trail();
#endif
#endif
  assert(level <= _decision_index.size());
  assert(_backtrack_buffer.empty());
  if (level == _decision_index.size())
    return;
#if STATS
  _stats.add_backtrack_difference(_decision_index.size() - level);
#endif
  unsigned waiting_count = 0;
  while (_decision_index.size() > level)
  {
    Tlit lit = _trail.back();
    _trail.pop_back();
    if (lit_reason(lit) == SAT_CLAUSE_UNDEF)
    {
      assert(lit_level(lit) == _decision_index.size());
      _decision_index.pop_back();
    }
    if (lit_level(lit) > level)
    {
      var_unassign(lit_to_var(lit));
      continue;
    }
    _backtrack_buffer.push_back(lit);
    waiting_count += _vars[lit_to_var(lit)].waiting;
  }
  while (!_backtrack_buffer.empty())
  {
    Tlit lit = _backtrack_buffer.back();
    _backtrack_buffer.pop_back();
    _trail.push_back(lit);
  }
  _propagated_literals = _trail.size() - waiting_count;
#if LOG_LEVEL > 4
  print_trail();
#endif
  assert(trail_consistency());
}

bool sat::modulariT_SAT::lit_is_required_in_learned_clause(Tlit lit)
{
  assert(lit_false(lit));
  if (lit_reason(lit) == SAT_CLAUSE_UNDEF)
    return true;
  assert(lit_reason(lit) < _clauses.size());
  TSclause &clause = _clauses[lit_reason(lit)];
  assert(!clause.deleted);
  for (unsigned i = 1; i < clause.size; i++)
    if (!lit_seen(clause.lits[i]))
      return true;
  return false;
}

void modulariT_SAT::analyze_conflict(Tclause conflict)
{
#if LOG_LEVEL > 0
  cout << "---- ANALYZING CONFLICT ";
  print_clause(conflict);
  cout << endl;
#endif
#if LOG_LEVEL > 3
  print_trail();
#endif
#if DEBUG_CLAUSE > 0
  if (conflict == DEBUG_CLAUSE)
  {
    cout << "Analyzing conflict ";
    print_clause(conflict);
    cout << endl;
  }
#endif
  assert(watch_lists_minimal());
  assert(conflict != SAT_CLAUSE_UNDEF);
  assert(!_writing_clause);
  unsigned count = 0;
  unsigned i = _trail.size() - 1;
  TSclause &clause = _clauses[conflict];
  bump_clause_activity(conflict);
  _next_literal_index = 0;
  Tlevel decision_level = _decision_index.size();
  for (unsigned j = 0; j < clause.size; j++)
  {
    Tlit lit = clause.lits[j];
    assert(lit_false(lit));
    bump_var_activity(lit_to_var(lit));
    if (lit_level(lit) == decision_level)
    {
      lit_mark_seen(lit);
      count++;
    }
    else if(lit_is_required_in_learned_clause(lit))
    {
      assert(lit_level(lit) < decision_level);
      assert(lit_false(lit));
      lit_mark_seen(lit);
      _literal_buffer[_next_literal_index++] = lit;
    }
  }
  assert(count > 1);
  while (count > 1)
  {
    while (!lit_seen(_trail[i]) || lit_level(_trail[i]) != decision_level)
    {
      assert(i > 0);
      i--;
    }
    assert(count <= _trail.size());
    assert(i < _trail.size());
    assert(lit_seen(_trail[i]));
    assert(lit_level(_trail[i]) == decision_level);
    Tlit lit = _trail[i];
    lit_unmark_seen(lit);
    count--;
    Tclause reason = lit_reason(lit);
    bump_clause_activity(reason);
    assert(reason != SAT_CLAUSE_UNDEF);
    TSclause &clause = _clauses[reason];

    assert(lit_true(clause.lits[0]));
    // start at 1 because the first literal is the one that was propagated
    for (unsigned j = 1; j < clause.size; j++)
    {
      Tlit lit = clause.lits[j];
      assert(lit_false(lit));
      bump_var_activity(lit_to_var(lit));
      if (lit_seen(lit))
        continue;
      if (lit_level(lit) == decision_level)
      {
        lit_mark_seen(lit);
        count++;
      }
      else if(lit_is_required_in_learned_clause(lit))
      {
        assert(lit_level(lit) < decision_level);
        assert(lit_false(lit));
        lit_mark_seen(lit);
        _literal_buffer[_next_literal_index++] = lit;
      }
    }
  }
  while (!lit_seen(_trail[i])
       || lit_level(_trail[i]) != decision_level)
  {
    assert(i > 0);
    i--;
  }
  _literal_buffer[_next_literal_index++] = lit_neg(_trail[i]);
  // remove the seen markers on literals in the clause
  for (unsigned j = 0; j < _next_literal_index; j++)
    lit_unmark_seen(_literal_buffer[j]);
  internal_add_clause(_literal_buffer, _next_literal_index, true, false);
}

void modulariT_SAT::repair_conflict(Tclause conflict)
{
#if LOG_LEVEL > 0
  cout << "---- REPAIRING CONFLICT ";
  print_clause(conflict);
  cout << endl;
#endif
#if STATS
  _stats.inc_conflicts();
#endif
  assert(most_relevant_watched_literals(conflict));
  assert(_clauses[conflict].size > 0);
  assert(lit_false(_clauses[conflict].lits[0]));
  if (lit_level(_clauses[conflict].lits[0]) == SAT_LEVEL_ROOT)
  {
#if LOG_LEVEL > 0
    cout << "UNSAT - Clause ";
    print_clause(conflict);
    cout << " is falsified at level 0\n";
#if LOG_LEVEL > 3
    print_trail();
#endif
#endif
    _status = UNSAT;
    return;
  }
  if (_clauses[conflict].size == 1)
  {
    if (_chronological_backtracking)
      CB_backtrack(lit_level(_clauses[conflict].lits[0]) - 1);
    else
      backtrack(SAT_LEVEL_ROOT);
    stack_lit(_clauses[conflict].lits[0], conflict);
    return;
  }
  assert(_chronological_backtracking
      || lit_level(_clauses[conflict].lits[0]) == _decision_index.size());
  if (lit_level(_clauses[conflict].lits[0]) != lit_level(_clauses[conflict].lits[1]))
  {
    if (_chronological_backtracking)
      CB_backtrack(lit_level(_clauses[conflict].lits[0]) - 1);
    else
      backtrack(lit_level(_clauses[conflict].lits[1]));
    stack_lit(_clauses[conflict].lits[0], conflict);
    return;
  }
  if (_chronological_backtracking)
    CB_backtrack(lit_level(_clauses[conflict].lits[0]));
  analyze_conflict(conflict);
  _var_activity_increment /= _var_activity_decay;
}

void modulariT_SAT::restart()
{
#if LOG_LEVEL > 0
  cout << "RESTART\n";
#endif
#if STATS
  _stats.inc_restarts();
#endif
  _agility = 1;
  _agility_threshold *= _threshold_decay;
  if (_chronological_backtracking)
    CB_backtrack(SAT_LEVEL_ROOT);
  else
    backtrack(SAT_LEVEL_ROOT);
  assert(trail_consistency());
}

void sat::modulariT_SAT::purge_clauses()
{
#if LOG_LEVEL > 0
  cout << "PURGING\n";
#if LOG_LEVEL > 3
  print_trail();
#endif
#endif
  _purge_threshold += _purge_inc;
  for (Tclause cl = 0; cl < _clauses.size(); cl++)
  {
    // Do not remove clauses that are used as reasons
    if (cl == lit_reason(_clauses[cl].lits[0]))
      continue;
    if (_clauses[cl].deleted || !_clauses[cl].watched)
      continue;
    for (Tlit *i = _clauses[cl].lits; i < _clauses[cl].lits + _clauses[cl].size; i++)
    {
      if (lit_level(*i) > SAT_LEVEL_ROOT)
        continue;
      if (lit_false(*i))
      {
        Tlit tmp = *i;
        *i = _clauses[cl].lits[_clauses[cl].size - 1];
        _clauses[cl].lits[_clauses[cl].size - 1] = tmp;
        _clauses[cl].size--;
        if (i < _clauses[cl].lits + 2 && _clauses[cl].size >= 2)
          watch_lit(*i, cl);
        i--;
#if LOG_LEVEL > 3
        cout << "Removing literal ";
        print_lit(tmp);
        cout << " from clause ";
        print_clause(cl);
        cout << endl;
#endif
      }
      else
      {
        assert(lit_true(*i));
        delete_clause(cl);
#if LOG_LEVEL > 3
        cout << "Removing clause ";
        print_clause(cl);
        cout << endl;
#endif
#if STATS
        _stats.inc_purged_clauses();
#endif
        break;
      }
    }
    if (_clauses[cl].watched && _clauses[cl].size == 1)
    {
      _clauses[cl].watched = false;
#if LOG_LEVEL > 3
      cout << "Clause ";
      print_clause(cl);
      cout << " is unit after purge\n";
#endif
      if (lit_undef(_clauses[cl].lits[0]))
        stack_lit(_clauses[cl].lits[0], cl);
      else
      {
        assert(_chronological_backtracking);
#if LOG_LEVEL > 3
        cout << "TODO: missed lower implication\n";
#endif
        CB_backtrack(SAT_LEVEL_ROOT);
        stack_lit(_clauses[cl].lits[0], cl);
      }
    }
    else if (_clauses[cl].size == 0)
    {
      _status = UNSAT;
#if LOG_LEVEL > 0
      cout << "UNSAT - Clause ";
      print_clause(cl);
      cout << " is empty after purge\n";
#endif
      return;
    }
  }
  repair_watch_lists();
  assert(watch_lists_complete());
  assert(watch_lists_minimal());
}

void sat::modulariT_SAT::simplify_clause_set()
{
#if LOG_LEVEL > 0
  cout << "SIMPLIFYING\n";
#endif
  _next_clause_elimination *= _clause_elimination_multiplier;
  _clause_activity_threshold *= _clause_activity_threshold_decay;
  double threshold = _max_clause_activity * _clause_activity_threshold;
  for (Tclause cl = 0; cl < _clauses.size(); cl++)
  {
    assert(_clauses[cl].activity <= _max_clause_activity);
    assert(_clauses[cl].size > 0);
    if (_clauses[cl].deleted || !_clauses[cl].watched || !_clauses[cl].learned)
      continue;
    if (_clauses[cl].size <= 2)
      continue;
    if (lit_reason(_clauses[cl].lits[0]) == cl)
      continue;
    if (_clauses[cl].activity < threshold)
    {
      delete_clause(cl);
#if LOG_LEVEL > 3
      cout << "Removing clause ";
      print_clause(cl);
      cout << endl;
#endif
#if STATS
      _stats.inc_deleted_clauses();
#endif
    }
  }
  repair_watch_lists();
#if LOG_LEVEL > 0
  cout << "c " << _n_learned_clauses << " learned clauses left\n";
#endif
}

void sat::modulariT_SAT::order_trail()
{
}

void sat::modulariT_SAT::select_watched_literals(Tlit *lits, unsigned size)
{
  unsigned high_index = 0;
  unsigned second_index = 1;
  unsigned hight_utility = utility_heuristic(lits[0]);
  unsigned second_utility = utility_heuristic(lits[1]);
  if (hight_utility < second_utility)
  {
    high_index = 1;
    second_index = 0;
    unsigned tmp = hight_utility;
    hight_utility = second_utility;
    second_utility = tmp;
  }

  for (unsigned i = 2; i < size; i++)
  {
    if (lit_undef(lits[second_index]))
      break;
    unsigned lit_utility = utility_heuristic(lits[i]);
    if (lit_utility > hight_utility)
    {
      second_index = high_index;
      second_utility = hight_utility;
      high_index = i;
      hight_utility = lit_utility;
    }
    else if (lit_utility > second_utility)
    {
      second_index = i;
      second_utility = lit_utility;
    }
  }

  Tlit tmp = lits[0];
  lits[0] = lits[high_index];
  lits[high_index] = tmp;
  if (second_index == 0)
  {
    tmp = lits[1];
    lits[1] = lits[high_index];
    lits[high_index] = tmp;
    return;
  }
  tmp = lits[1];
  lits[1] = lits[second_index];
  lits[second_index] = tmp;
}

Tclause sat::modulariT_SAT::internal_add_clause(Tlit *lits_input, unsigned size, bool learned, bool external)
{
#if LOG_LEVEL > 2
  cout << "Adding clause ";
  for (unsigned i = 0; i < size; i++)
  {
    print_lit(lits_input[i]);
    cout << " ";
  }
  cout << endl;
#endif
#if STATS
  if (external)
    _stats.inc_external_clauses();
  if (learned)
    _stats.inc_learned_clauses();
#endif
  Tlit *lits;
  Tclause cl;
  TSclause *clause;
  if (learned)
    _n_learned_clauses++;
  if (external)
    _next_clause_elimination++;

  if (_deleted_clauses.empty())
  {
    lits = new Tlit[size];
    memcpy(lits, lits_input, size * sizeof(Tlit));

    TSclause added(lits, size, learned, external);
    _clauses.push_back(added);
    clause = &_clauses.back();
    cl = _clauses.size() - 1;
  }
  else
  {
    cl = _deleted_clauses.back();
    assert(cl < _clauses.size());
    _deleted_clauses.pop_back();
    clause = &_clauses[cl];
    assert(clause->deleted);
    assert(!clause->watched);
    if (clause->original_size < size)
    {
      delete[] clause->lits;
      clause->lits = new Tlit[size];
    }
    lits = clause->lits;
    memcpy(lits, lits_input, size * sizeof(Tlit));
    clause->size = size;
    clause->original_size = size;
    clause->deleted = false;
    clause->watched = true;
    clause->blocker = SAT_LIT_UNDEF;
  }
  clause->activity = _max_clause_activity;

  if (_n_learned_clauses >= _next_clause_elimination)
    simplify_clause_set();

  assert(clause_soundness(cl));
  if (size == 0)
  {
    clause->watched = false;
    _status = UNSAT;
    return cl;
  }
  if (size == 1)
  {
    clause->watched = false;
    if (lit_true(lits[0]))
      return cl;
    else if (lit_false(lits[0]))
      repair_conflict(cl);
    else
      stack_lit(lits[0], cl);
    return cl;
  }

  select_watched_literals(lits, size);
  assert(most_relevant_watched_literals(cl));

  watch_lit(lits[0], cl);
  watch_lit(lits[1], cl);

  if (lit_false(lits[0]))
    repair_conflict(cl);
  else if (lit_false(lits[1]) && lit_undef(lits[0]))
    stack_lit(lits[0], cl);
  else if (lit_false(lits[1]) && lit_true(lits[0]) && (lit_level(lits[1]) < lit_level(lits[0])))
  {
    // missed lower implication
    // TODO handle this case in CB
  }
  assert(watch_lists_complete());
  assert(watch_lists_minimal());
  return cl;
}

/*****************************************************************************/
/*                            Public interface                               */
/*****************************************************************************/

modulariT_SAT::modulariT_SAT(unsigned int n_var, unsigned int n_clauses)
{
  _vars = vector<TSvar>(n_var + 1);
  _trail = vector<Tlit>();
  _trail.reserve(n_var);
  _watch_lists = vector<vector<Tclause>>(2 * n_var + 2);

  for (Tvar var = 1; var <= n_var; var++)
    _variable_heap.insert(var, 0);

  _clauses = vector<TSclause>();
  _clauses.reserve(n_clauses);

  _literal_buffer = new Tlit[n_var];
  _next_literal_index = 0;
}

modulariT_SAT::~modulariT_SAT()
{
  for (unsigned i = 0; i < _clauses.size(); i++)
    delete[] _clauses[i].lits;
}

void modulariT_SAT::toggle_chronological_backtracking(bool on)
{
  if (!on)
    order_trail();
  _chronological_backtracking = on;
}

void modulariT_SAT::toggle_proofs(bool on)
{
  MARKUSED(on);
}

bool modulariT_SAT::propagate()
{
#if LOG_LEVEL > 0
  cout << "Propagating\n";
#if LOG_LEVEL > 2
  cout << _trail.size() - _propagated_literals << " literals to propagate\n";
#endif
#if LOG_LEVEL > 3
  print_trail();
#endif
#endif
  while (_propagated_literals < _trail.size())
  {
    Tlit lit = _trail[_propagated_literals];
    Tclause conflict = propagate_lit(lit);
    if (conflict != SAT_CLAUSE_UNDEF)
    {
#if LOG_LEVEL > 1
      cout << "Conflict ";
      print_clause(conflict);
      cout << "\n";
#endif
      repair_conflict(conflict);
      if (_status == UNSAT)
        return false;
      if (_agility < _agility_threshold)
        restart();
      continue;
    }
    _propagated_literals++;
    if (_purge_counter >= _purge_threshold)
    {
      purge_clauses();
      _purge_counter = 0;
      if (_status == UNSAT)
        return false;
    }
  }
  assert(trail_consistency());
  assert(no_unit_clauses());
  if (_trail.size() == _vars.size() - 1)
  {
    _status = SAT;
    return false;
  }
  return true;
}

modulariT_SAT::status modulariT_SAT::solve()
{
  if (_status != UNDEF)
    return _status;
  while (propagate())
  {
    assert(trail_consistency());
    decide();
  }
  assert(trail_consistency());
#if STATS
  _stats.print();
#endif
  return _status;
}

modulariT_SAT::status modulariT_SAT::get_status()
{
  return _status;
}

bool modulariT_SAT::decide()
{
  while(!_variable_heap.empty() && !var_undef(_variable_heap.top()))
    _variable_heap.pop();
  if (_variable_heap.empty())
  {
    _status = SAT;
    return false;
  }
  Tvar var = _variable_heap.top();
#if LOG_LEVEL > 1
  cout << "Deciding ";
  print_lit(literal(var, 0));
  cout << " with activity " << _vars[var].activity;
  cout << endl;
#endif
  stack_lit(literal(var, _vars[var].phase_cache), SAT_CLAUSE_UNDEF);
  return true;
}

bool sat::modulariT_SAT::decide(Tlit lit)
{
  assert(lit_undef(lit));
  stack_lit(lit, SAT_CLAUSE_UNDEF);
  return true;
}

void modulariT_SAT::start_clause()
{
  assert(!_writing_clause);
  _writing_clause = true;
  _next_literal_index = 0;
}

void modulariT_SAT::add_literal(Tlit lit)
{
  assert(_writing_clause);
  Tvar var = lit_to_var(lit);
  var_allocate(var);
  // bump_var_activity(var);
  assert(_next_literal_index < _vars.size());
  _literal_buffer[_next_literal_index++] = lit;
}

modulariT_SAT::status modulariT_SAT::finalize_clause()
{
  assert(_writing_clause);
  _writing_clause = false;
  internal_add_clause(_literal_buffer, _next_literal_index, false, true);
  propagate();
  return _status;
}

modulariT_SAT::status sat::modulariT_SAT::add_clause(Tlit *lits, unsigned size)
{
  Tvar max_var = 0;
  for (unsigned i = 0; i < size; i++)
    if (lit_to_var(lits[i]) > max_var)
      max_var = lit_to_var(lits[i]);
  var_allocate(max_var);
  // for (unsigned i = 0; i < size; i++)
    // bump_var_activity(lit_to_var(lits[i]));
  internal_add_clause(lits, size, false, true);
  propagate();
  return _status;
}

const Tlit *sat::modulariT_SAT::get_clause(Tclause cl) const
{
  assert(cl < _clauses.size());
  return _clauses[cl].lits;
}

unsigned sat::modulariT_SAT::get_clause_size(Tclause cl) const
{
  assert(cl < _clauses.size());
  return _clauses[cl].size;
}

void modulariT_SAT::hint(Tvar var, bool pol)
{
  MARKUSED(var);
  MARKUSED(pol);
}

void modulariT_SAT::hint(Tvar var, bool pol, unsigned int level)
{
  MARKUSED(var);
  MARKUSED(pol);
  MARKUSED(level);
}

void modulariT_SAT::synchronized()
{
}

unsigned modulariT_SAT::sync_validity()
{
  return 0;
}

unsigned modulariT_SAT::sync_color(Tvar var)
{
  MARKUSED(var);
  return 0;
}

void modulariT_SAT::set_markup(void (*markup_function)(void))
{
  MARKUSED(markup_function);
}

const std::vector<Tlit> &modulariT_SAT::trail() const
{
  return *(new std::vector<Tlit>());
}

Tlevel modulariT_SAT::decision_level() const
{
  return Tlevel();
}
