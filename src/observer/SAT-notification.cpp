/**
 * @file src/observer/SAT-notification.cpp
 * @author Robin Coutelier
 *
 * @brief This file is part of the observer of the SMT Solver modulariT. It contains the
 * implementation of the notifications' apply and rollback methods.
 */
#include "SAT-notification.hpp"
#include "SAT-observer.hpp"

#include <cassert>
#include <algorithm>
#include <iostream>
#include <fstream>

using namespace sat;
using namespace sat::gui;
using namespace std;

bool sat::gui::notification::_suppress_warning = false;


std::string sat::gui::notification_type_to_string(ENotifType type)
{
  switch (type) {
  case ENotifType::CHECKPOINT:
  return "Checkpoint";
  case ENotifType::DONE:
  return "Done";
  case ENotifType::MARKER:
  return "Marker";
  case ENotifType::NEW_VARIABLE:
  return "New variable";
  case ENotifType::DELETE_VARIABLE:
  return "variable deleted";
  case ENotifType::DECISION:
  return "Decision";
  case ENotifType::IMPLICATION:
  return "Implication";
  case ENotifType::PROPAGATION:
  return "Propagation";
  case ENotifType::CONFLICT:
  return "Conflict";
  case ENotifType::BACKTRACKING_STARTED:
  return "Backtracking started";
  case ENotifType::BACKTRACKING_DONE:
  return "Backtracking completed";
  case ENotifType::UNASSIGNMENT:
  return "Unassignement";
  case ENotifType::NEW_CLAUSE:
  return "New clause";
  case ENotifType::DELETE_CLAUSE:
  return "Clause deleted";
  case ENotifType::WATCH:
  return "Watch";
  case ENotifType::UNWATCH:
  return "Stop watching";
  case ENotifType::REMOVE_LITERAL:
  return "Remove literal";
  case ENotifType::CHECK_INVARIANTS:
  return "Invariants checking";
  case ENotifType::BLOCKER:
  return "Blocker set";
  case ENotifType::MISSED_LOWER_IMPLICATION:
  return "Missed lower implication";
  case ENotifType::REMOVE_LOWER_IMPLICATION:
  return "Remove lower implication";
  default:
  return "UNKNOWN";
  }
}

unsigned sat::gui::new_variable::get_event_level(observer* obs)
{
  assert(obs);
  return obs->is_variable_marked(var) ? 0 : event_level;
}

void sat::gui::new_variable::apply(observer* obs)
{
  assert(obs);
  if (var >= obs->_variables.size()) {
    obs->_variables.resize(var + 1);
    obs->_variables[var].active = true;
    return;
  }
  assert(!obs->_variables[var].active);
  obs->_variables[var].active = true;
}

void sat::gui::new_variable::rollback(observer* obs)
{
  assert(obs);
  assert(obs->_variables.size() > var);
  assert(obs->_variables[var].active);
  obs->_variables[var] = observer::variable();
}

unsigned sat::gui::delete_variable::get_event_level(observer* obs)
{
  assert(obs);
  return obs->is_variable_marked(var) ? 0 : event_level;
}

void sat::gui::delete_variable::apply(observer* obs)
{
  assert(obs);
  assert(obs->_variables.size() > var);
  assert(obs->_variables[var].active);
  obs->_variables[var].active = false;
}

void sat::gui::delete_variable::rollback(observer* obs)
{
  assert(obs);
  assert(obs->_variables.size() > var);
  assert(!obs->_variables[var].active);
  obs->_variables[var].active = true;
}

unsigned sat::gui::decision::get_event_level(observer* obs)
{
  assert(obs);
  return obs->is_variable_marked(lit_to_var(lit)) ? 0 : event_level;
}

void sat::gui::decision::apply(observer* obs)
{
  assert(obs);
  Tvar var = lit_to_var(lit);
  assert(obs->_variables.size() > var);
  assert(obs->_variables[var].active);
  assert(obs->_variables[var].value == VAR_UNDEF);
  obs->_variables[var].value = lit_pol(lit);
  obs->_decision_level++;
  obs->_variables[var].level = obs->_decision_level;
  obs->_variables[var].reason = CLAUSE_UNDEF;
  obs->_assignment_stack.push_back(lit);
}

void sat::gui::decision::rollback(observer* obs)
{
  assert(obs);
  Tvar var = lit_to_var(lit);
  assert(obs->_variables.size() > var);
  assert(obs->_variables[var].active);
  assert(obs->_variables[var].value == lit_pol(lit));
  assert(obs->_variables[var].level == obs->_decision_level);
  assert(obs->_assignment_stack.back() == lit);
  obs->_variables[var].value = VAR_UNDEF;
  obs->_decision_level--;
  obs->_assignment_stack.pop_back();
}

unsigned sat::gui::implication::get_event_level(observer* obs)
{
  assert(obs);
  return obs->is_variable_marked(lit_to_var(lit)) || obs->is_clause_marked(reason) ? 0 : event_level;
}

void sat::gui::implication::apply(observer* obs)
{
  assert(obs);
  Tvar var = lit_to_var(lit);
  assert(obs->_variables.size() > var);
  assert(obs->_variables[var].active);
  assert(obs->_variables[var].value == VAR_UNDEF);
  assert(reason != CLAUSE_UNDEF);
  assert(reason != CLAUSE_LAZY);
  assert(reason <= obs->_active_clauses.size());
  obs->_variables[var].value = lit_pol(lit);
  obs->_variables[var].reason = reason;
  obs->_variables[var].level = level;

  if (!notification::_suppress_warning) {
    Tlevel level_check = 0;
    bool found = false;
    for (Tlit l : obs->_active_clauses[reason]->literals) {
      if (lit == l) {
        found = true;
        continue;
      }
      Tvar v = lit_to_var(l);
      if (obs->_variables[v].level > level_check)
        level_check = obs->_variables[v].level;
    }
    bool success = true;
    if (!found) {
      cerr << "\033[0;33mWARNING\033[0m: The clause " << reason << " does not contain the literal " << lit_to_int(lit) << endl;
      success = false;
    }
    if (level_check == LEVEL_ERROR || level_check == LEVEL_UNDEF) {
      cerr << "\033[0;33mWARNING\033[0m: The clause " << reason << " seems to contain a literal that is not assigned different from " << lit_to_int(lit) << endl;
      success = false;
    }
    else if (level_check != level) {
      cerr << "\033[0;33mWARNING\033[0m: level of variable " << var << " is " << level_check << " but was given as " << level << " by the solver " << endl;
      success = false;
    }
    if (!success) {
      event_level = 0;
      obs->_assignment_stack.push_back(lit);
      cout << "Notification number " << obs->_notifications.size() << endl;
      return;
    }
  }
  assert(obs->_variables[var].level <= obs->_decision_level);
  obs->_assignment_stack.push_back(lit);
}

void sat::gui::implication::rollback(observer* obs)
{
  assert(obs);
  Tvar var = lit_to_var(lit);
  assert(obs->_variables.size() > var);
  assert(obs->_variables[var].active);
  assert(obs->_variables[var].value == lit_pol(lit));
  assert(obs->_variables[var].level <= obs->_decision_level);
  assert(obs->_assignment_stack.back() == lit);
  obs->_variables[var].value = VAR_UNDEF;
  obs->_assignment_stack.pop_back();
}

unsigned sat::gui::propagation::get_event_level(observer* obs)
{
  assert(obs);
  return obs->is_variable_marked(lit_to_var(lit)) ? 0 : event_level;
}

void sat::gui::propagation::apply(observer* obs)
{
  assert(obs);
  assert(obs->_variables.size() > lit_to_var(lit));
  assert(obs->_variables[lit_to_var(lit)].active);
  assert(obs->_variables[lit_to_var(lit)].value != VAR_UNDEF);
  assert(obs->_assignment_stack[obs->_n_propagated] == lit);
  obs->_n_propagated++;
  obs->_variables[lit_to_var(lit)].propagated = true;

#if NOTIFY_WATCH_CHANGE
  assert(obs->check_watch_literals_levels());
  assert(obs->check_strong_watch_literals());
  assert(obs->check_blocked_watch_literals());
  assert(obs->check_weak_watch_literals());
#endif
}

void sat::gui::propagation::rollback(observer* obs)
{
  assert(obs);
  assert(obs->_n_propagated > 0);
  obs->_n_propagated--;
  assert(obs->_variables.size() > lit_to_var(lit));
  assert(obs->_variables[lit_to_var(lit)].active);
  assert(obs->_variables[lit_to_var(lit)].value != VAR_UNDEF);
  assert(obs->_assignment_stack[obs->_n_propagated] == lit);
  obs->_variables[lit_to_var(lit)].propagated = false;
}

unsigned sat::gui::unassignment::get_event_level(observer* obs)
{
  assert(obs);
  return obs->is_variable_marked(lit_to_var(lit)) ? 0 : event_level;
}

void sat::gui::unassignment::apply(observer* obs)
{
  assert(obs);
  Tvar var = lit_to_var(lit);
  assert(obs->_variables.size() > var);
  assert(obs->_variables[var].active);
  assert(obs->_variables[var].value != VAR_UNDEF);
  obs->_variables[lit_to_var(lit)].propagated = false;
  if (obs->_variables[var].reason == CLAUSE_UNDEF)
    obs->_decision_level--;
  obs->_variables[var].value = VAR_UNDEF;

  if (location == MAX_UNSIGNED) {
    unsigned i;
    for (i = obs->_assignment_stack.size() - 1; i < obs->_assignment_stack.size(); i--) {
      Tvar v = lit_to_var(obs->_assignment_stack[i]);
      if (v == var) {
        obs->_assignment_stack.erase(obs->_assignment_stack.begin() + i);
        location = i;
        break;
      }
    }
    assert(i < obs->_assignment_stack.size() + 1);
  }
  else {
    assert(location < obs->_assignment_stack.size());
    assert(lit_to_var(obs->_assignment_stack[location]) == var);
    obs->_assignment_stack.erase(obs->_assignment_stack.begin() + location);
  }
  if (location < obs->_n_propagated) {
    obs->_n_propagated--;
    propagated = true;
  }

  level = obs->_variables[var].level;
  reason = obs->_variables[var].reason;

  obs->_variables[var].level = LEVEL_UNDEF;
  obs->_variables[var].reason = CLAUSE_UNDEF;

#if NOTIFY_WATCH_CHANGE
  assert(obs->check_watch_literals_levels());
  assert(obs->check_strong_watch_literals());
  assert(obs->check_blocked_watch_literals());
  assert(obs->check_weak_watch_literals());
#endif
}

void sat::gui::unassignment::rollback(observer* obs)
{
  assert(obs);
  Tvar var = lit_to_var(lit);
  // print all the variables involved in that method
  assert(obs->_variables.size() > var);
  assert(obs->_variables[var].active);
  assert(obs->_variables[var].value == VAR_UNDEF);
  assert(location <= obs->_assignment_stack.size());
  obs->_assignment_stack.insert(obs->_assignment_stack.begin() + location, lit);
  obs->_variables[var].value = lit_pol(lit);
  obs->_variables[var].level = level;
  obs->_variables[var].reason = reason;
  obs->_decision_level = max(obs->_decision_level, level);

  if (propagated) {
    obs->_n_propagated++;
    obs->_variables[lit_to_var(lit)].propagated = true;
  }

  assert(lit_to_var(obs->_assignment_stack[location]) == var);
}

unsigned sat::gui::new_clause::get_event_level(observer* obs)
{
  assert(obs);
  return obs->is_clause_marked(cl) ? 0 : event_level;
}

void sat::gui::new_clause::apply(observer* obs)
{
  assert(obs);
  assert(obs->_active_clauses.size() <= cl || obs->_active_clauses[cl] == nullptr || !obs->_active_clauses[cl]->active);
  // sort the literals
  if (hash == 0) {
    std::sort(lits.begin(), lits.end(), [&](Tlit l1, Tlit l2)
      { return lit_to_var(l1) < lit_to_var(l2); });
    hash = obs->hash_clause(lits);
    if (obs->_clauses_dict.find(hash) != obs->_clauses_dict.end()) {
      // if the clause are identical, send a warning message
      if (!notification::_suppress_warning && obs->_clauses_dict[hash]->literals == lits) {
        cerr << "\033[0;33mWARNING\033[0m (at notification number " << obs->_notifications.size() << "): ";
        cerr << "The clause " << cl << " is identical to the clause " << obs->_clauses_dict[hash]->cl << endl;
        event_level = 0;
      }
    }
    while (obs->_clauses_dict.find(hash) != obs->_clauses_dict.end()) {
      // todo find a way to have fewer collisions
      hash++;
      if (hash == 0)
        hash = 1;
    }
    observer::clause* c = new observer::clause(lits, cl, learnt, external);
    c->active = true;
    obs->_clauses_dict.insert({ hash, c });
    if (obs->_active_clauses.size() <= cl)
      obs->_active_clauses.resize(cl + 1);
  }
  assert(obs->_active_clauses.size() > cl);
  obs->_active_clauses[cl] = obs->_clauses_dict[hash];
  obs->_active_clauses[cl]->active = true;
  for (Tlit l : obs->_active_clauses[cl]->watched)
    cout << obs->lit_to_string(l) << endl;
}

void sat::gui::new_clause::rollback(observer* obs)
{
  assert(obs);
  assert(obs->_active_clauses.size() > cl);
  assert(obs->_active_clauses[cl]->active);
  obs->_active_clauses[cl]->active = false;
}

unsigned sat::gui::delete_clause::get_event_level(observer* obs)
{
  assert(obs);
  return obs->is_clause_marked(cl) ? 0 : event_level;
}

void sat::gui::delete_clause::apply(observer* obs)
{
  assert(obs);
  assert(obs->_active_clauses.size() > cl);
  assert(obs->_active_clauses[cl]->active);
  obs->_active_clauses[cl]->active = false;
  vector<Tlit>& lits = obs->_active_clauses[cl]->literals;

  hash = obs->hash_clause(lits);
}

void sat::gui::delete_clause::rollback(observer* obs)
{
  assert(obs);
  assert(obs->_active_clauses.size() > cl);
  assert(!obs->_active_clauses[cl]->active);
  obs->_active_clauses[cl]->active = true;
  assert(obs->_active_clauses[cl] != nullptr);
  obs->_active_clauses[cl] = obs->_clauses_dict[hash];
}

void sat::gui::checkpoint::apply(observer* obs)
{
  assert(obs);
  // you can do thins only once. During replay, no command can be parsed.
  if (applied)
    return;
  obs->notify_checkpoint();
  applied = true;
}

void sat::gui::checkpoint::rollback(observer* obs)
{}

void sat::gui::done::apply(observer* obs)
{
  assert(obs);
  cout << "Done" << endl;
  cout << obs->_notifications.size() << " notifications" << endl;
}

unsigned sat::gui::conflict::get_event_level(observer* obs)
{
  assert(obs);
  return obs->is_clause_marked(cl) ? 0 : event_level;
}

void sat::gui::conflict::apply(observer* obs)
{}

unsigned sat::gui::watch::get_event_level(observer* obs)
{
  assert(obs);
  return obs->is_variable_marked(lit_to_var(lit)) || obs->is_clause_marked(cl) ? 0 : event_level;
}

void sat::gui::watch::apply(observer* obs)
{
  assert(obs);
  assert(obs->_active_clauses.size() > cl);
  sat::gui::observer::clause* c = obs->_active_clauses[cl];
  assert(c != nullptr);
  assert(c->active);
  // do not watch the same literal twice
  assert(c->watched.find(lit) == c->watched.end());
  c->watched.insert(lit);
}

void sat::gui::watch::rollback(observer* obs)
{
  assert(obs);
  assert(obs->_active_clauses.size() > cl);
  assert(obs->_active_clauses[cl] != nullptr);
  sat::gui::observer::clause* c = obs->_active_clauses[cl];
  // the literal must be watched
  assert(c->watched.find(lit) != c->watched.end());
  c->watched.erase(lit);
}

unsigned sat::gui::unwatch::get_event_level(observer* obs)
{
  assert(obs);
  return obs->is_variable_marked(lit_to_var(lit)) || obs->is_clause_marked(cl) ? 0 : event_level;
}

void sat::gui::unwatch::apply(observer* obs)
{
  assert(obs);
  // cout << "unwatch " << cl << " " << obs->lit_to_string(lit) << endl;
  assert(obs->_active_clauses.size() > cl);
  assert(obs->_active_clauses[cl] != nullptr);
  sat::gui::observer::clause* c = obs->_active_clauses[cl];
  assert(c->watched.find(lit) != c->watched.end());
  c->watched.erase(lit);
}

void sat::gui::unwatch::rollback(observer* obs)
{
  assert(obs);
  assert(obs->_active_clauses.size() > cl);
  assert(obs->_active_clauses[cl] != nullptr);
  sat::gui::observer::clause* c = obs->_active_clauses[cl];
  assert(c->watched.find(lit) == c->watched.end());
  c->watched.insert(lit);
}

unsigned sat::gui::remove_literal::get_event_level(observer* obs)
{
  assert(obs);
  return obs->is_variable_marked(lit_to_var(lit)) || obs->is_clause_marked(cl) ? 0 : event_level;
}

void sat::gui::remove_literal::apply(observer* obs)
{}

void sat::gui::remove_literal::rollback(observer* obs)
{}

void sat::gui::check_invariants::apply(observer* obs)
{
  assert(obs);
  bool success = obs->check_invariants();
  if (!success) {
    obs->print_clause_set();
    obs->print_variables();
    obs->print_assignment();
    cout << "\033[0;31mERROR\033[0m: Invariants are not satisfied" << endl;
    cout << obs->get_error_message() << endl;
    event_level = 0;
  }
}

void sat::gui::check_invariants::rollback(observer* obs)
{
  assert(obs);
  obs->check_invariants();
}

unsigned sat::gui::block::get_event_level(observer* obs)
{
  assert(obs);
  return obs->is_variable_marked(lit_to_var(lit)) || obs->is_clause_marked(cl) ? 0 : event_level;
}

void sat::gui::block::apply(observer* obs)
{
  assert(obs);
  assert(obs->_active_clauses.size() > cl);
  assert(obs->_active_clauses[cl] != nullptr);
  observer::clause* c = obs->_active_clauses[cl];
  assert(c->active);
  assert(find(c->literals.begin(), c->literals.end(), lit) != c->literals.end());
  previous_blocker = c->blocker;
  c->blocker = lit;
}

void sat::gui::block::rollback(observer* obs)
{
  assert(obs);
  assert(obs->_active_clauses.size() > cl);
  assert(obs->_active_clauses[cl] != nullptr);
  observer::clause* c = obs->_active_clauses[cl];
  assert(c->active);
  assert(c->blocker == lit);
  c->blocker = previous_blocker;
}

void sat::gui::missed_lower_implication::apply(observer* obs)
{
  assert(obs);
  assert(obs->_variables.size() > var);
  assert(obs->_active_clauses.size() > cl);
  assert(obs->_active_clauses[cl] != nullptr);
  last_cl = obs->_variables[var].lazy_reason;
  obs->_variables[var].lazy_reason = cl;
}

void sat::gui::missed_lower_implication::rollback(observer* obs)
{
  assert(obs);
  assert(obs->_variables.size() > var);
  // assert(obs->_active_clauses.size() > cl);
  assert(obs->_active_clauses[cl] != nullptr);
  assert(obs->_variables[var].lazy_reason == cl);
  obs->_variables[var].lazy_reason = last_cl;
}

void sat::gui::remove_lower_implication::apply(observer* obs)
{
  last_cl = obs->_variables[var].lazy_reason;
  obs->_variables[var].lazy_reason = CLAUSE_UNDEF;
}

void sat::gui::remove_lower_implication::rollback(observer* obs)
{
  assert(obs);
  assert(obs->_variables.size() > var);
  assert(obs->_active_clauses.size() > last_cl);
  assert(obs->_active_clauses[last_cl] != nullptr);
  assert(obs->_variables[var].lazy_reason == CLAUSE_UNDEF);
  obs->_variables[var].lazy_reason = last_cl;
}
