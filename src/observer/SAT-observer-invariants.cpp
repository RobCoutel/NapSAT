#include "SAT-observer.hpp"

#include <iostream>
#include <fstream>

using namespace std;

static const string error = "\033[1;31m" + string("Error: ") + "\033[0m";

void sat::gui::observer::load_invariant_configuration(std::string filename)
{
  ifstream file(filename);
  if (!file.is_open())
  {
    cerr << "Error: could not load the invariant configuration file." << endl;
    return;
  }
  // reset all the invariants to false
  _check_trail_sanity = false;
  _check_level_ordering = false;
  _check_trail_monoticity = false;
  _check_no_missed_implications = false;
  _check_topological_order = false;
#if NOTIFY_WATCH_CHANGE
  _check_strong_watch_literals = false;
  _check_blocked_watch_literals = false;
  _check_weak_watch_literals = false;
  _check_weak_blocked_watch_literals = false;
#endif
  _check_assignment_coherence = false;
  // read the file
  string line;
  while (getline(file, line))
  {
    if (line == "trail_sanity")
      _check_trail_sanity = true;
    else if (line == "level_ordering")
      _check_level_ordering = true;
    else if (line == "trail_monoticity")
      _check_trail_monoticity = true;
    else if (line == "no_missed_implications")
      _check_no_missed_implications = true;
    else if (line == "topological_order")
      _check_topological_order = true;
#if NOTIFY_WATCH_CHANGE
    else if (line == "strong_watch_literals")
      _check_strong_watch_literals = true;
    else if (line == "blocked_watch_literals")
      _check_blocked_watch_literals = true;
    else if (line == "weak_watch_literals")
      _check_weak_watch_literals = true;
    else if (line == "weak_blocked_watch_literals")
      _check_weak_blocked_watch_literals = true;
#else
    else if (line == "strong_watch_literals" || line == "blocked_watch_literals" || line == "weak_watch_literals" || line == "weak_blocked_watch_literals")
      // do nothing
      ;
#endif
    else if (line == "assignment_coherence")
      _check_assignment_coherence = true;
    else
    {
      cerr << "Error: unknown invariant " << line << endl;
    }
  }
  cout << "Invariant configuration loaded from " << filename << endl;
  file.close();

  cout << "The following invariants are enabled:" << endl;
  if (_check_trail_sanity)
    cout << "trail sanity, " << endl;
  if (_check_level_ordering)
    cout << "level ordering, " << endl;
  if (_check_trail_monoticity)
    cout << "trail monoticity, " << endl;
  if (_check_no_missed_implications)
    cout << "no missed implications, " << endl;
  if (_check_topological_order)
    cout << "topological order, " << endl;
#if NOTIFY_WATCH_CHANGE
  if (_check_strong_watch_literals)
    cout << "strong watch literals, " << endl;
  if (_check_blocked_watch_literals)
    cout << "blocked watch literals, " << endl;
  if (_check_weak_watch_literals)
    cout << "weak watch literals, " << endl;
  if (_check_weak_blocked_watch_literals)
    cout << "weak blocked watch literals, " << endl;
#endif
  if (_check_assignment_coherence)
    cout << "assignment coherence" << endl;
  cout << endl;
}

bool sat::gui::observer::check_invariants()
{
  bool success = true;
  success &= !_check_trail_sanity || check_trail_sanity();
  success &= !_check_level_ordering || check_level_ordering();
  success &= !_check_trail_monoticity || check_trail_monoticity();
  success &= !_check_no_missed_implications || check_no_missed_implications();
  success &= !_check_topological_order || check_topological_order();
#if NOTIFY_WATCH_CHANGE
  success &= !_check_strong_watch_literals || check_strong_watch_literals();
  success &= !_check_blocked_watch_literals || check_blocked_watch_literals();
  success &= !_check_weak_watch_literals || check_weak_watch_literals();
  success &= !_check_weak_blocked_watch_literals || check_weak_blocked_watch_literals();
  success &= !_check_watch_literals_levels || heck_watch_literals_levels();
#endif
  success &= !_check_assignment_coherence || check_assignment_coherence();
  return success;
}

bool sat::gui::observer::check_relevant_invariant(sat::gui::notification *notification)
{
  return false;
}

bool sat::gui::observer::check_trail_sanity()
{
  const string error_header = error + "Invariant violation (trail sanity): ";
  bool success = true;
  for (Tclause cl = 0; cl < _active_clauses.size(); cl++)
  {
    clause *c = _active_clauses[cl];
    if (!c->active)
    {
      continue;
    }
    unsigned i;
    for (i = 0; i < c->literals.size(); i++)
      if (lit_value(c->literals[i]) != VAR_FALSE || !lit_propagated(c->literals[i]))
        break;
    if (i == c->literals.size())
    {
      success = false;
      _error_message += error_header + "clause " + clause_to_string(cl) + " is falsified by the trail.\n";
    }
  }
  return success;
}

bool sat::gui::observer::check_level_ordering()
{
  const string error_header = error + "Invariant violation (level ordering): ";
  bool success = true;
  for (Tlit lit : _assignment_stack)
  {
    if (lit_reason(lit) == CLAUSE_UNDEF || lit_reason(lit) == CLAUSE_LAZY)
    {
      continue;
    }
    clause *c = _active_clauses[lit_reason(lit)];
    if (!c->active)
    {
      success = false;
      _error_message += error_header + "clause " + clause_to_string(lit_reason(lit)) + " is not active.\n";
      continue;
    }
    for (Tlit lit2 : c->literals)
    {
      if (lit_level(lit2) > lit_level(lit))
      {
        success = false;
        _error_message += error_header + "clause " + clause_to_string(lit_reason(lit)) + " has a literal " + lit_to_string(lit2) + " with a higher level than " + lit_to_string(lit) + ".\n";
      }
    }
  }
  return success;
}

bool sat::gui::observer::check_trail_monoticity()
{
  const string error_header = error + "Invariant violation (trail monoticity): ";
  bool success = true;
  Tlevel last_level = 0;
  for (Tlit lit : _assignment_stack)
  {
    if (lit_level(lit) < last_level)
    {
      success = false;
      _error_message += error_header + "literal " + lit_to_string(lit) + " has a lower level than the previous literal " + lit_to_string(_assignment_stack[_assignment_stack.size() - 1]) + ".\n";
    }
    last_level = lit_level(lit);
  }
  return success;
}

bool sat::gui::observer::check_no_missed_implications()
{
  const string error_header = error + "Invariant violation (no missed implications): ";
  bool success = true;
  for (Tclause cl = 0; cl < _active_clauses.size(); cl++)
  {
    clause *c = _active_clauses[cl];
    if (!c->active)
    {
      continue;
    }
    unsigned n_undef = 0;
    Tlit last_undef = LIT_UNDEF;
    for (Tlit watched : c->watched)
      if (lit_value(watched) == VAR_TRUE || !lit_propagated(watched))
        goto next_clause;
    for (Tlit lit : c->literals)
    {
      if (lit_value(lit) == VAR_TRUE)
      {
        goto next_clause;
      }
      if (lit_value(lit) == VAR_UNDEF)
      {
        n_undef++;
        last_undef = lit;
      }
    }
    if (n_undef == 1)
    {
      success = false;
      _error_message += error_header + "clause " + clause_to_string(cl) + " has only one undefined literal " + lit_to_string(last_undef) + ".\n";
    }
  next_clause:;
  }
  return success;
}

bool sat::gui::observer::check_topological_order()
{
  const string error_header = error + "Invariant violation (topological order): ";
  bool success = true;
  vector<bool> visited(_active_clauses.size(), false);
  for (Tlit lit : _assignment_stack)
  {
    visited[lit_to_var(lit)] = true;
    if (lit_reason(lit) == CLAUSE_UNDEF || lit_reason(lit) == CLAUSE_LAZY)
    {
      continue;
    }
    clause *c = _active_clauses[lit_reason(lit)];
    if (!c->active)
    {
      success = false;
      _error_message += error_header + "clause " + clause_to_string(lit_reason(lit)) + " is not active.\n";
      continue;
    }
    for (Tlit lit2 : c->literals)
    {
      if (!visited[lit_to_var(lit2)])
      {
        success = false;
        _error_message += error_header + "the reason clause " + clause_to_string(lit_reason(lit)) + " for the implication of literal " + lit_to_string(lit) + " has a literal " + lit_to_string(lit2) + " that is not visited yet.\n";
      }
    }
  }
  return success;
}

#if NOTIFY_WATCH_CHANGE

bool sat::gui::observer::check_strong_watch_literals()
{
  const string error_header = error + "Invariant violation (strong watch literals): ";
  bool success = true;
  for (Tclause cl = 0; cl < _active_clauses.size(); cl++)
  {
    clause *c = _active_clauses[cl];
    if (!c->active || c->literals.size() < 2)
      continue;
    if (lit_value(c->watched[0]) == VAR_TRUE || lit_value(c->watched[1]) == VAR_TRUE)
      continue;

    if ((lit_value(c->watched[0]) == VAR_FALSE && lit_propagated(c->watched[0])) || (lit_value(c->watched[1]) == VAR_FALSE && lit_propagated(c->watched[1])))
    {
      success = false;
      if (lit_value(c->watched[0]) == VAR_FALSE && lit_propagated(c->watched[0]))
      {
        _error_message += error_header + "clause " + clause_to_string(cl) + " has a falsified watched literal " + lit_to_string(c->watched[0]) + " that is propagated.\n";
      }
      if (lit_value(c->watched[1]) == VAR_FALSE && lit_propagated(c->watched[1]))
      {
        _error_message += error_header + "clause " + clause_to_string(cl) + " has a falsified watched literal " + lit_to_string(c->watched[1]) + " that is propagated.\n";
      }
      continue;
    }
  }
  return success;
}

bool sat::gui::observer::check_blocked_watch_literals()
{
  const string error_header = error + "Invariant violation (blocked watch literals): ";
  bool success = true;
  for (Tclause cl = 0; cl < _active_clauses.size(); cl++)
  {
    clause *c = _active_clauses[cl];
    if (!c->active)
      continue;
    vector<Tlit> lits = c->literals;
    vector<Tlit> watched = c->watched;
    if (watched.size() != 2)
    {
      _error_message += error_header + "clause " + clause_to_string(cl) + " has " + to_string(watched.size()) + " watched literals.\n";
    }
    // check that one of the watched literals is false and propagated
    if (lit_value(watched[0]) == VAR_FALSE && lit_propagated(watched[0]) || lit_value(watched[1]) == VAR_FALSE && lit_propagated(watched[1]))
    {
      // the clause should be satisfied
      for (Tlit lit : lits)
        if (lit_value(lit) == VAR_TRUE)
          goto next_clause;
      _error_message += error_header + "clause " + clause_to_string(cl) + " is not satisfied but ";
      if (lit_value(watched[0]) == VAR_FALSE && lit_propagated(watched[0]))
        _error_message += "watched literal " + lit_to_string(watched[0]) + " is false and propagated";
      if (lit_value(watched[0]) == VAR_FALSE && lit_propagated(watched[0]) && lit_value(watched[1]) == VAR_FALSE && lit_propagated(watched[1]))
        _error_message += " and ";
      if (lit_value(watched[1]) == VAR_FALSE && lit_propagated(watched[1]))
        _error_message += "watched literal " + lit_to_string(watched[1]) + " is false and propagated";
      _error_message += ".\n";
    }
  next_clause:
  }
  return success;
}

bool sat::gui::observer::check_weak_watch_literals()
{
  const string error_header = error + "Invariant violation (weak watch literals): ";
  bool success = true;
  for (Tclause cl = 0; cl < _active_clauses.size(); cl++)
  {
    clause *c = _active_clauses[cl];
    if (!c->active || c->literals.size() < 2)
      continue;
    if (lit_value(c->watched[0]) == VAR_TRUE && lit_level(c->watched[0]) < lit_level(c->watched[1]))
    {
      continue;
    }
    if (lit_value(c->watched[1]) == VAR_TRUE && lit_level(c->watched[1]) < lit_level(c->watched[0]))
    {
      continue;
    }
    if ((lit_value(c->watched[0]) == VAR_FALSE && lit_propagated(c->watched[0])) || (lit_value(c->watched[1]) == VAR_FALSE && lit_propagated(c->watched[1])))
    {
      success = false;
      if (lit_value(c->watched[0]) == VAR_FALSE && lit_propagated(c->watched[0]))
      {
        _error_message += error_header + "clause " + clause_to_string(cl) + " has a falsified watched literal " + lit_to_string(c->watched[0]) + " that is propagated.\n";
      }
      if (lit_value(c->watched[1]) == VAR_FALSE && lit_propagated(c->watched[1]))
      {
        _error_message += error_header + "clause " + clause_to_string(cl) + " has a falsified watched literal " + lit_to_string(c->watched[1]) + " that is propagated.\n";
      }
      continue;
    }
  }
  return success;
}

bool sat::gui::observer::check_weak_blocked_watch_literals()
{
  const string error_header = error + "Invariant violation (weak watch literals): ";
  bool success = true;
  for (Tclause cl = 0; cl < _active_clauses.size(); cl++)
  {
    clause *c = _active_clauses[cl];
    if (!c->active)
      continue;
    vector<Tlit> lits = c->literals;
    vector<Tlit> watched = c->watched;
    if (watched.size() != 2)
    {
      _error_message += error_header + "clause " + clause_to_string(cl) + " has " + to_string(watched.size()) + " watched literals.\n";
    }
    // check that one of the watched literals is false and propagated
    if (lit_value(watched[0]) == VAR_FALSE && lit_propagated(watched[0]) || lit_value(watched[1]) == VAR_FALSE && lit_propagated(watched[1]))
    {
      Tlit highest_unsatisfied_watched = LIT_UNDEF;
      for (Tlit lit : watched)
        if (lit_value(lit) == VAR_FALSE && lit_propagated(lit))
          if (lit_level(lit) > lit_level(highest_unsatisfied_watched))
            highest_unsatisfied_watched = lit;
      // the clause should be satisfied
      Tlit satisfied_lit = LIT_UNDEF;
      for (Tlit lit : lits)
        if (lit_value(lit) == VAR_TRUE)
          if (lit_level(lit) < lit_level(satisfied_lit))
            satisfied_lit = lit;

      if (satisfied_lit == LIT_UNDEF)
      {
        _error_message += error_header + "clause " + clause_to_string(cl) + " has a watched literal falsified and propagated but is not satisfied at a lower level.\n";
      }
    }
  next_clause:
  }
  return success;
}

#endif

bool sat::gui::observer::check_assignment_coherence()
{
  const string error_header = error + "Invariant violation (assignment coherence): ";
  bool success = true;
  vector<bool> visited(_active_clauses.size(), false);
  for (Tlit lit : _assignment_stack)
  {
    if (visited[lit_to_var(lit)])
    {
      success = false;
      _error_message += error_header + "variable " + to_string(lit_to_var(lit)) + " is visited more than once.\n";
    }
    if (lit_value(lit) == VAR_UNDEF)
    {
      success = false;
      _error_message += error_header + "variable " + to_string(lit_to_var(lit)) + " is undefined.\n";
    }
    if (lit_value(lit) == VAR_FALSE)
    {
      success = false;
      _error_message += error_header + "variable " + to_string(lit_to_var(lit)) + " is false in the assignment.\n";
    }
    visited[lit_to_var(lit)] = true;

    Tclause reason = lit_reason(lit);
    if (reason == CLAUSE_UNDEF || reason == CLAUSE_LAZY)
    {
      continue;
    }
    clause *c = _active_clauses[reason];
    if (!c->active)
    {
      success = false;
      _error_message += error_header + "clause " + clause_to_string(reason) + " is not active.\n";
      continue;
    }
    // check that the reason is only satisfied by one literal, that is "lit"
    for (Tlit l : c->literals)
    {
      if (l == lit)
        continue;
      if (lit_value(l) != VAR_FALSE)
      {
        success = false;
        _error_message += error_header + "clause " + clause_to_string(reason) + " is satisfied by literal " + lit_to_string(l) + " but not by " + lit_to_string(lit) + ".\n";
      }
    }
  }
  return success;
}
