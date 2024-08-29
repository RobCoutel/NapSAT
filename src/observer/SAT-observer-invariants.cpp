/**
 * @file src/observer/SAT-observer-invariants.cpp
 * @author Robin Coutelier
 *
 * @brief This file is part of the observer of the SAT solver NapSAT. It contains the
 * implementation of the invariant checkers in the observer.
 */
#include "SAT-observer.hpp"
#include "../environment.hpp"

#include <iostream>
#include <fstream>

using namespace std;

void napsat::gui::observer::load_invariant_configuration()
{
  string filename = napsat::env::get_invariant_configuration_folder();
  if (_options.lazy_strong_chronological_backtracking)
    filename += "lazy-strong-chronological-backtracking";
  else if (_options.restoring_strong_chronological_backtracking)
    filename += "restoring-strong-chronological-backtracking";
  else if (_options.weak_chronological_backtracking)
    filename += "weak-chronological-backtracking";
  else
    filename += "non-chronological-backtracking";
  filename += ".conf";
  ifstream file(filename);
  if (!file.is_open()) {
    LOG_ERROR("The invariant configuration could not be loaded from file: " + filename);
    return;
  }
  // TODO this is a bit brutal. Do like in the options.
  // reset all the invariants to false
  _check_trail_sanity = false;
  _check_level_ordering = false;
  _check_trail_monotonicity = false;
  _check_no_missed_implications = false;
  _check_topological_order = false;
#if NOTIFY_WATCH_CHANGES
  _check_weak_watched_literals = false;
  _check_strong_watched_literals = false;
  _check_backtrack_compatible_watched_literals = false;
  _check_lazy_backtrack_compatible_watch_literals = false;
#endif
  _check_assignment_coherence = false;
  // read the file
  string line;
  while (getline(file, line)) {
    if (line == "trail_sanity")
      _check_trail_sanity = true;
    else if (line == "level_ordering")
      _check_level_ordering = true;
    else if (line == "trail_monotonicity")
      _check_trail_monotonicity = true;
    else if (line == "no_missed_implications")
      _check_no_missed_implications = true;
    else if (line == "topological_order")
      _check_topological_order = true;
    else if (line == "weak_watched_literals")
      _check_weak_watched_literals = true;
    else if (line == "strong_watched_literals")
      _check_strong_watched_literals = true;
    else if (line == "backtrack_compatible_watched_literals")
      _check_backtrack_compatible_watched_literals = true;
    else if (line == "lazy_backtrack_compatible_watched_literals")
      _check_lazy_backtrack_compatible_watch_literals = true;
    else if (line == "assignment_coherence")
      _check_assignment_coherence = true;
    else
      LOG_WARNING("unknown invariant " << line);
  }
  cout << "Invariants : " << endl;
  cout << "  - trail_sanity: " << _check_trail_sanity << endl;
  cout << "  - level_ordering: " << _check_level_ordering << endl;
  cout << "  - trail_monotonicity: " << _check_trail_monotonicity << endl;
  cout << "  - no_missed_implications: " << _check_no_missed_implications << endl;
  cout << "  - topological_order: " << _check_topological_order << endl;
#if NOTIFY_WATCH_CHANGES
  cout << "  - weak_watched_literals: " << _check_weak_watched_literals << endl;
  cout << "  - strong_watched_literals: " << _check_strong_watched_literals << endl;
  cout << "  - backtrack_compatible_watched_literals: " << _check_backtrack_compatible_watched_literals << endl;
  cout << "  - lazy_backtrack_compatible_watched_literals: " << _check_lazy_backtrack_compatible_watch_literals << endl;
#endif
  cout << "  - assignment_coherence: " << _check_assignment_coherence << endl;

  file.close();
}

bool napsat::gui::observer::check_invariants()
{
  bool success = true;
  success &= !_check_trail_sanity || check_trail_sanity();
  success &= !_check_level_ordering || check_level_ordering();
  success &= !_check_trail_monotonicity || check_trail_monotonicity();
  success &= !_check_no_missed_implications || check_no_missed_implications();
  success &= !_check_topological_order || check_topological_order();
#if NOTIFY_WATCH_CHANGES
  success &= check_watched_literals();
#endif
  success &= !_check_assignment_coherence || check_assignment_coherence();
  return success;
}

bool napsat::gui::observer::check_relevant_invariant(napsat::gui::notification *notification)
{
  return false;
}

bool napsat::gui::observer::check_trail_sanity()
{
  const string error_header = ERROR_HEAD + "Invariant violation (trail sanity): ";
  bool success = true;
  for (Tclause cl = 0; cl < _active_clauses.size(); cl++) {
    clause *c = _active_clauses[cl];
    if (!c->active)
      continue;
    unsigned i;
    for (i = 0; i < c->literals.size(); i++)
      if (lit_value(c->literals[i]) != VAR_FALSE || !lit_propagated(c->literals[i]))
        break;
    if (i == c->literals.size()) {
      success = false;
      _error_message += error_header + "clause " + clause_to_string(cl) + " is falsified by the trail.\n";
    }
  }
  return success;
}

bool napsat::gui::observer::check_level_ordering()
{
  const string error_header = ERROR_HEAD + "Invariant violation (level ordering): ";
  bool success = true;
  for (Tlit lit : _assignment_stack) {
    if (lit_reason(lit) == CLAUSE_UNDEF || lit_reason(lit) == CLAUSE_LAZY)
      continue;
    clause *c = _active_clauses[lit_reason(lit)];
    if (!c->active) {
      success = false;
      _error_message += error_header + "clause " + clause_to_string(lit_reason(lit)) + " is not active.\n";
      continue;
    }
    for (Tlit lit2 : c->literals) {
      if (lit_level(lit2) > lit_level(lit)) {
        success = false;
        _error_message += error_header + "clause " + clause_to_string(lit_reason(lit)) + " has a literal " + lit_to_string(lit2) + " with a higher level than " + lit_to_string(lit) + ".\n";
      }
    }
  }
  return success;
}

bool napsat::gui::observer::check_trail_monotonicity()
{
  const string error_header = ERROR_HEAD + "Invariant violation (trail monotonicity): ";
  bool success = true;
  Tlevel last_level = 0;
  for (Tlit lit : _assignment_stack) {
    if (lit_level(lit) < last_level) {
      success = false;
      _error_message += error_header + "literal " + lit_to_string(lit) + " has a lower level than the previous literal " + lit_to_string(_assignment_stack[_assignment_stack.size() - 1]) + ".\n";
    }
    last_level = lit_level(lit);
  }
  return success;
}

bool napsat::gui::observer::check_no_missed_implications()
{
  const string error_header = ERROR_HEAD + "Invariant violation (no missed implications): ";
  bool success = true;
  for (Tclause cl = 0; cl < _active_clauses.size(); cl++) {
    clause *c = _active_clauses[cl];
    if (!c->active)
      continue;
    unsigned n_undef = 0;
    Tlit last_undef = LIT_UNDEF;
    for (Tlit watched : c->watched)
      if (lit_value(watched) == VAR_TRUE || !lit_propagated(watched))
        goto next_clause;
    for (Tlit lit : c->literals) {
      if (lit_value(lit) == VAR_TRUE || !lit_propagated(lit))
        goto next_clause;
      if (lit_value(lit) == VAR_UNDEF) {
        n_undef++;
        last_undef = lit;
      }
    }
    if (n_undef == 1) {
      success = false;
      _error_message += error_header + "clause " + clause_to_string(cl) + " has only one undefined literal " + lit_to_string(last_undef) + ".\n";
    }
  next_clause:;
  }
  return success;
}

bool napsat::gui::observer::check_topological_order()
{
  const string error_header = ERROR_HEAD + "Invariant violation (topological order): ";
  bool success = true;
  vector<bool> visited(_variables.size(), false);
  for (Tlit lit : _assignment_stack) {
    assert(lit_to_var(lit) < visited.size());
    visited[lit_to_var(lit)] = true;
    if (lit_reason(lit) == CLAUSE_UNDEF || lit_reason(lit) == CLAUSE_LAZY)
      continue;
    clause *c = _active_clauses[lit_reason(lit)];
    if (!c->active) {
      success = false;
      _error_message += error_header + "clause " + clause_to_string(lit_reason(lit)) + " is not active.\n";
      continue;
    }
    for (Tlit lit2 : c->literals) {
      if (!visited[lit_to_var(lit2)]) {
        success = false;
        _error_message += error_header + "the reason clause " + clause_to_string(lit_reason(lit)) + " for the implication of literal " + lit_to_string(lit) + " has a literal " + lit_to_string(lit2) + " that is not visited yet.\n";
      }
    }
  }
  return success;
}

#if NOTIFY_WATCH_CHANGES
bool napsat::gui::observer::check_watched_literals()
{
  if (!_check_weak_watched_literals && !_check_strong_watched_literals && !_check_lazy_backtrack_compatible_watch_literals && !_check_backtrack_compatible_watched_literals)
    return true;
  const string error_header = ERROR_HEAD + "Invariant violation (watch literals): ";
  bool success = true;
  for (Tclause cl = 0; cl < _active_clauses.size(); cl++) {
    clause *c = _active_clauses[cl];
    // cout << "checking clause " << clause_to_string(cl) << endl;
    if (!c->active || c->literals.size() - c->n_deleted_literals < 2)
      continue;
    if (c->literals.size() - c->n_deleted_literals == 2) {
      c->watched.insert(c->literals[0]);
      c->watched.insert(c->literals[1]);
    }
    if (c->watched.size() != 2) {
      _error_message += error_header + "clause " + clause_to_string(cl) + " has " + to_string(c->watched.size()) + " watched literals.\n";
      _error_message += error_header + "watched literals: ";
      for (Tlit lit : c->watched)
        _error_message += lit_to_string(lit) + " ";
      _error_message += "\n";
      success = false;
      continue;
    }


    for (Tlit lit : c->watched) {
      Tlit other = LIT_UNDEF;
      for (Tlit l : c->watched) {
        if (l != lit) {
          other = l;
          break;
        }
      }

      // weak watched literals
      // ¬c₁ ∈ τ ⇒ c₂ ∉ τ ∨ [b ∈ π ∧ δ(b) ≤ δ(c₂)]
      if (_check_weak_watched_literals && !weak_watched_literals(lit, other, c->blocker))  {
        success = false;
        _error_message += ERROR_HEAD + "¬c₁ ∈ τ ⇒ [c₂ ∉ τ ∨ b ∈ π]  --  Weak watched literals invariant violation: \n";
        _error_message += ERROR_HEAD + "clause " + clause_to_string(cl) + " does not satisfy the invariant if c₁ is " + lit_to_string(lit) + " and c₂ is " + lit_to_string(other) + ".\n";
      }

      // strong watched literals
      // ¬c₁ ∈ τ ⇒ c₂ ∈ π ∨ [b ∈ π ∧ δ(b) ≤ δ(c₂)]
      if (_check_strong_watched_literals && !strong_watched_literals(lit, other, c->blocker)) {
        success = false;
        _error_message += ERROR_HEAD + "¬c₁ ∈ τ ⇒ [c₂ ∈ π ∨ b ∈ π]  --  Strong watched literals invariant violation: \n";
        _error_message += ERROR_HEAD + "clause " + clause_to_string(cl) + " does not satisfy the invariant if c₁ is " + lit_to_string(lit) + " and c₂ is " + lit_to_string(other) + ".\n";
      }

      // lazy backtrack compatible watched literals
      // ¬c₁ ∈ τ ⇒ [c₂ ∈ π ∧ [δ(c₂) ≤ δ(c₁) ∨ δ(λ(c₂) \ {c₂}) ≤ δ(c₁)]
      //          ∨ [b ∈ π ∧ δ(b) ≤ δ(c₁)]
      if (_check_lazy_backtrack_compatible_watch_literals && !lazy_backtrack_compatible_watched_literals(lit, other, c->blocker)) {
        success = false;
        _error_message += ERROR_HEAD + "¬c₁ ∈ τ ⇒ [c₂ ∈ π ∧ [δ(c₂) ≤ δ(c₁) ∨ δ(λ(c₂) \\ {c₂}) ≤ δ(c₁)] ∨ [b ∈ π ∧ δ(b) ≤ δ(c₁)]  --  Lazy backtrack compatible watched literals invariant violation: \n";
        _error_message += ERROR_HEAD + "clause " + clause_to_string(cl) + " does not satisfy the invariant if c₁ is " + lit_to_string(lit) + " and c₂ is " + lit_to_string(other) + ".\n";
      }

      // backward compatible watched literals
      // ¬c₁ ∈ τ ⇒ [c₂ ∈ π ∧ δ(c₂) ≤ δ(c₁)] ∨ [b ∈ π ∧ δ(b) ≤ δ(c₂)]
      if (_check_backtrack_compatible_watched_literals && !backward_compatible_watched_literals(lit, other, c->blocker)) {
        success = false;
        _error_message += ERROR_HEAD + "¬c₁ ∈ τ ⇒ [c₂ ∈ π ∧ δ(c₂) ≤ δ(c₁)] ∨ [b ∈ π ∧ δ(b) ≤ δ(c₂)]  --  Backward compatible watched literals invariant violation: \n";
        _error_message += ERROR_HEAD + "clause " + clause_to_string(cl) + " does not satisfy the invariant if c₁ is " + lit_to_string(lit) + " and c₂ is " + lit_to_string(other) + ".\n";
      }
    }
  }
  return success;
}

bool napsat::gui::observer::weak_watched_literals(napsat::Tlit c1, napsat::Tlit c2, napsat::Tlit blocker)
{
  // ¬c₁ ∈ τ ⇒ [¬c₂ ∉ τ ∨ b ∈ π]
  bool success = !lit_propagated(c1) || lit_value(c1) != VAR_FALSE;
  success |= !lit_propagated(c2) || lit_value(c2) != VAR_FALSE;
  success |= lit_value(blocker) == VAR_TRUE;
  return success;
}

bool napsat::gui::observer::strong_watched_literals(napsat::Tlit c1, napsat::Tlit c2, napsat::Tlit blocked_lit)
{
  // ¬c₁ ∈ τ ⇒ [c₂ ∈ π ∨ b ∈ π]
  bool success = !lit_propagated(c1) || lit_value(c1) != VAR_FALSE;
  success |= lit_value(c2) == VAR_TRUE;
  success |= lit_value(blocked_lit) == VAR_TRUE;
  return success;
}

bool napsat::gui::observer::lazy_backtrack_compatible_watched_literals(napsat::Tlit c1, napsat::Tlit c2, napsat::Tlit blocker)
{
  // ¬c₁ ∈ τ ⇒ [c₂ ∈ π ∧ [δ(c₂) ≤ δ(c₁) ∨ δ(λ(c₂) \ {c₂}) ≤ δ(c₁)]
  //          ∨ [b ∈ π ∧ δ(b) ≤ δ(c₁)]
  Tclause lazy_reason = lit_lazy_reason(c2);
  Tlevel lazy_level = LEVEL_UNDEF;
  if (lazy_reason != CLAUSE_UNDEF) {
    lazy_level = 0;
    for (Tlit lit : _active_clauses[lazy_reason]->literals) {
      if (lit_value(lit) == VAR_FALSE) {
        lazy_level = max(lazy_level, lit_level(lit));
      }
    }
  }
  bool success = !lit_propagated(c1) || lit_value(c1) != VAR_FALSE;
  success |= lit_value(c2) == VAR_TRUE && (lit_level(c2) <= lit_level(c1) || lazy_level <= lit_level(c1));
  success |= lit_value(blocker) == VAR_TRUE && lit_level(blocker) <= lit_level(c1);
  return success;
}

bool napsat::gui::observer::backward_compatible_watched_literals(napsat::Tlit c1, napsat::Tlit c2, napsat::Tlit blocker)
{
  // ¬c₁ ∈ τ ⇒ [c₂ ∈ π ∧ δ(c₂) ≤ δ(c₁)] ∨ [b ∈ π ∧ δ(b) ≤ δ(c₂)]
  bool success = !lit_propagated(c1) || lit_value(c1) != VAR_FALSE;
  success |= lit_value(c2) == VAR_TRUE && lit_level(c2) <= lit_level(c1);
  success |= lit_value(blocker) == VAR_TRUE && lit_level(blocker) <= lit_level(c2);
  return success;
}
#endif

bool napsat::gui::observer::check_assignment_coherence()
{
  const string error_header = ERROR_HEAD + "Invariant violation (assignment coherence): ";
  bool success = true;
  vector<bool> visited(_active_clauses.size(), false);
  for (Tlit lit : _assignment_stack) {
    if (visited[lit_to_var(lit)]) {
      success = false;
      _error_message += error_header + "variable " + to_string(lit_to_var(lit)) + " is visited more than once.\n";
    }
    if (lit_value(lit) == VAR_UNDEF) {
      success = false;
      _error_message += error_header + "variable " + to_string(lit_to_var(lit)) + " is undefined.\n";
    }
    if (lit_value(lit) == VAR_FALSE) {
      success = false;
      _error_message += error_header + "variable " + to_string(lit_to_var(lit)) + " is false in the assignment.\n";
    }
    visited[lit_to_var(lit)] = true;

    Tclause reason = lit_reason(lit);
    if (reason == CLAUSE_UNDEF || reason == CLAUSE_LAZY)
      continue;
    clause *c = _active_clauses[reason];
    if (!c->active) {
      success = false;
      _error_message += error_header + "clause " + clause_to_string(reason) + " is not active.\n";
      continue;
    }
    // check that the reason is only satisfied by one literal, that is "lit"
    for (Tlit l : c->literals) {
      if (l == lit)
        continue;
      if (lit_value(l) != VAR_FALSE) {
        success = false;
        _error_message += error_header + "clause " + clause_to_string(reason) + " is satisfied by literal " + lit_to_string(l) + " but not by " + lit_to_string(lit) + ".\n";
      }
    }
  }
  return success;
}
