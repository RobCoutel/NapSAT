/**
 * @file src/observer/SAT-notification.cpp
 * @author Robin Coutelier
 *
 * @brief This file is part of the NapSAT solver. It contains the
 * implementation of the notifications' apply and rollback methods.
 */
#include "SAT-notification.hpp"
#include "SAT-observer.hpp"

#include <cassert>
#include <algorithm>
#include <iostream>
#include <fstream>

using namespace napsat;
using namespace napsat::gui;
using namespace std;

#ifndef NDEBUG
#define ASSERT_OBS(notif, x) \
if (!(x)) { \
  if (!notification::_suppress_warning) \
    LOG_ERROR("Assertion failed: " << #x << " in notification " << notif->get_message()); \
  return false; \
}
#else
#define ASSERT_OBS(this, x) assert(x)
#endif

bool napsat::gui::notification::_suppress_warning = false;


std::string napsat::gui::notification_type_to_string(ENotifType type)
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
  case ENotifType::REMOVE_PROPAGATION:
  return "Removed propagation";
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

unsigned napsat::gui::new_variable::get_event_level(observer* obs)
{
  ASSERT_OBS(this, obs);
  return obs->is_variable_marked(var) ? 0 : event_level;
}

bool napsat::gui::new_variable::apply(observer* obs)
{
  ASSERT_OBS(this, obs);
  if (var >= obs->_variables.size()) {
    obs->_variables.resize(var + 1);
    obs->_variables[var].active = true;
    return true;
  }
  ASSERT_OBS(this, !obs->_variables[var].active);
  obs->_variables[var].active = true;
  return true;
}

bool napsat::gui::new_variable::rollback(observer* obs)
{
  ASSERT_OBS(this, obs);
  ASSERT_OBS(this, obs->_variables.size() > var);
  ASSERT_OBS(this, obs->_variables[var].active);
  obs->_variables[var] = observer::variable();
  return true;
}

unsigned napsat::gui::delete_variable::get_event_level(observer* obs)
{
  ASSERT_OBS(this, obs);
  return obs->is_variable_marked(var) ? 0 : event_level;
}

bool napsat::gui::delete_variable::apply(observer* obs)
{
  ASSERT_OBS(this, obs);
  ASSERT_OBS(this, obs->_variables.size() > var);
  ASSERT_OBS(this, obs->_variables[var].active);
  obs->_variables[var].active = false;
  return true;
}

bool napsat::gui::delete_variable::rollback(observer* obs)
{
  ASSERT_OBS(this, obs);
  ASSERT_OBS(this, obs->_variables.size() > var);
  ASSERT_OBS(this, !obs->_variables[var].active);
  obs->_variables[var].active = true;
  return true;
}

unsigned napsat::gui::decision::get_event_level(observer* obs)
{
  ASSERT_OBS(this, obs);
  return obs->is_variable_marked(lit_to_var(lit)) ? 0 : event_level;
}

bool napsat::gui::decision::apply(observer* obs)
{
  ASSERT_OBS(this, obs);
  Tvar var = lit_to_var(lit);
  ASSERT_OBS(this, obs->_variables.size() > var);
  ASSERT_OBS(this, obs->_variables[var].active);
  ASSERT_OBS(this, obs->_variables[var].value == VAR_UNDEF);
  obs->_variables[var].value = lit_pol(lit);
  obs->_decision_level++;
  obs->_variables[var].level = obs->_decision_level;
  obs->_variables[var].reason = CLAUSE_UNDEF;
  obs->_assignment_stack.push_back(lit);
  return true;
}

bool napsat::gui::decision::rollback(observer* obs)
{
  ASSERT_OBS(this, obs);
  Tvar var = lit_to_var(lit);
  ASSERT_OBS(this, obs->_variables.size() > var);
  ASSERT_OBS(this, obs->_variables[var].active);
  ASSERT_OBS(this, obs->_variables[var].value == lit_pol(lit));
  ASSERT_OBS(this, obs->_variables[var].level == obs->_decision_level);
  ASSERT_OBS(this, obs->_assignment_stack.back() == lit);
  obs->_variables[var].value = VAR_UNDEF;
  obs->_decision_level--;
  obs->_assignment_stack.pop_back();
  return true;
}

unsigned napsat::gui::implication::get_event_level(observer* obs)
{
  ASSERT_OBS(this, obs);
  return obs->is_variable_marked(lit_to_var(lit)) || obs->is_clause_marked(reason) ? 0 : event_level;
}

bool napsat::gui::implication::apply(observer* obs)
{
  ASSERT_OBS(this, obs);
  Tvar var = lit_to_var(lit);
  ASSERT_OBS(this, obs->_variables.size() > var);
  ASSERT_OBS(this, obs->_variables[var].active);
  ASSERT_OBS(this, obs->_variables[var].value == VAR_UNDEF);
  ASSERT_OBS(this, reason != CLAUSE_UNDEF);
  ASSERT_OBS(this, reason != CLAUSE_LAZY);
  ASSERT_OBS(this, reason <= obs->_active_clauses.size());
  ASSERT_OBS(this, obs->_active_clauses[reason]->active);
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
    // TODO: Make those errors instead of warnings
    if (!found) {
      LOG_WARNING("The clause " << reason << " does not contain the literal " << lit_to_int(lit));
      success = false;
    }
    if (level_check == LEVEL_ERROR || level_check == LEVEL_UNDEF) {
      LOG_WARNING("The clause " << reason << " seems to contain a literal that is not assigned different from " << lit_to_int(lit));
      success = false;
    }
    else if (level_check != level) {
      LOG_WARNING("level of variable " << var << " is " << level_check << " but was given as " << level << " by the solver");
      success = false;
    }
    if (!success) {
      event_level = 0;
      obs->_assignment_stack.push_back(lit);
      cout << "Notification number " << obs->_notifications.size() << endl;
      return false;
    }
  }
  ASSERT_OBS(this, obs->_variables[var].level <= obs->_decision_level);
  obs->_assignment_stack.push_back(lit);
  return true;
}

bool napsat::gui::implication::rollback(observer* obs)
{
  ASSERT_OBS(this, obs);
  Tvar var = lit_to_var(lit);
  ASSERT_OBS(this, obs->_variables.size() > var);
  ASSERT_OBS(this, obs->_variables[var].active);
  ASSERT_OBS(this, obs->_variables[var].value == lit_pol(lit));
  ASSERT_OBS(this, obs->_variables[var].level <= obs->_decision_level);
  ASSERT_OBS(this, obs->_assignment_stack.back() == lit);
  obs->_variables[var].value = VAR_UNDEF;
  obs->_assignment_stack.pop_back();
  return true;
}

unsigned napsat::gui::propagation::get_event_level(observer* obs)
{
  ASSERT_OBS(this, obs);
  return obs->is_variable_marked(lit_to_var(lit)) ? 0 : event_level;
}

bool napsat::gui::propagation::apply(observer* obs)
{
  ASSERT_OBS(this, obs);
  ASSERT_OBS(this, obs->_variables.size() > lit_to_var(lit));
  ASSERT_OBS(this, obs->_variables[lit_to_var(lit)].active);
  ASSERT_OBS(this, obs->_variables[lit_to_var(lit)].value != VAR_UNDEF);
  ASSERT_OBS(this, obs->_assignment_stack[obs->_n_propagated] == lit);
  obs->_n_propagated++;
  obs->_variables[lit_to_var(lit)].propagated = true;
  return true;
}

bool napsat::gui::propagation::rollback(observer* obs)
{
  ASSERT_OBS(this, obs);
  ASSERT_OBS(this, obs->_n_propagated > 0);
  obs->_n_propagated--;
  ASSERT_OBS(this, obs->_variables.size() > lit_to_var(lit));
  ASSERT_OBS(this, obs->_variables[lit_to_var(lit)].active);
  ASSERT_OBS(this, obs->_variables[lit_to_var(lit)].value != VAR_UNDEF);
  // ASSERT_OBS(this, obs->_assignment_stack[obs->_n_propagated] == lit);
  obs->_variables[lit_to_var(lit)].propagated = false;
  return true;
}

unsigned napsat::gui::remove_propagation::get_event_level(observer* obs)
{
  ASSERT_OBS(this, obs);
  return obs->is_variable_marked(lit_to_var(lit)) ? 0 : event_level;
}

bool napsat::gui::remove_propagation::apply(observer* obs)
{
  ASSERT_OBS(this, obs);
  ASSERT_OBS(this, obs->_n_propagated > 0);
  obs->_n_propagated--;
  ASSERT_OBS(this, obs->_variables.size() > lit_to_var(lit));
  ASSERT_OBS(this, obs->_variables[lit_to_var(lit)].active);
  ASSERT_OBS(this, obs->_variables[lit_to_var(lit)].value != VAR_UNDEF);
  ASSERT_OBS(this, obs->_assignment_stack[obs->_n_propagated] == lit);
  ASSERT_OBS(this, obs->_variables[lit_to_var(lit)].propagated);
  obs->_variables[lit_to_var(lit)].propagated = false;
  return true;
}

bool napsat::gui::remove_propagation::rollback(observer* obs)
{
  ASSERT_OBS(this, obs);
  ASSERT_OBS(this, obs->_variables.size() > lit_to_var(lit));
  ASSERT_OBS(this, obs->_variables[lit_to_var(lit)].active);
  ASSERT_OBS(this, obs->_variables[lit_to_var(lit)].value != VAR_UNDEF);
  ASSERT_OBS(this, !obs->_variables[lit_to_var(lit)].propagated);
  obs->_n_propagated++;
  obs->_variables[lit_to_var(lit)].propagated = true;
  return true;
}

unsigned napsat::gui::unassignment::get_event_level(observer* obs)
{
  ASSERT_OBS(this, obs);
  return obs->is_variable_marked(lit_to_var(lit)) ? 0 : event_level;
}

bool napsat::gui::unassignment::apply(observer* obs)
{
  ASSERT_OBS(this, obs);
  Tvar var = lit_to_var(lit);
  ASSERT_OBS(this, obs->_variables.size() > var);
  ASSERT_OBS(this, obs->_variables[var].active);
  ASSERT_OBS(this, obs->_variables[var].value != VAR_UNDEF);
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
    ASSERT_OBS(this, i < obs->_assignment_stack.size() + 1);
  }
  else {
    ASSERT_OBS(this, location < obs->_assignment_stack.size());
    ASSERT_OBS(this, lit_to_var(obs->_assignment_stack[location]) == var);
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
  return true;
}

bool napsat::gui::unassignment::rollback(observer* obs)
{
  ASSERT_OBS(this, obs);
  Tvar var = lit_to_var(lit);
  // print all the variables involved in that method
  ASSERT_OBS(this, obs->_variables.size() > var);
  ASSERT_OBS(this, obs->_variables[var].active);
  ASSERT_OBS(this, obs->_variables[var].value == VAR_UNDEF);
  ASSERT_OBS(this, location <= obs->_assignment_stack.size());
  obs->_assignment_stack.insert(obs->_assignment_stack.begin() + location, lit);
  obs->_variables[var].value = lit_pol(lit);
  obs->_variables[var].level = level;
  obs->_variables[var].reason = reason;
  obs->_decision_level = max(obs->_decision_level, level);

  if (propagated) {
    obs->_n_propagated++;
    obs->_variables[lit_to_var(lit)].propagated = true;
  }

  ASSERT_OBS(this, lit_to_var(obs->_assignment_stack[location]) == var);
  return true;
}

unsigned napsat::gui::new_clause::get_event_level(observer* obs)
{
  ASSERT_OBS(this, obs);
  return obs->is_clause_marked(cl) ? 0 : event_level;
}

bool napsat::gui::new_clause::apply(observer* obs)
{
  ASSERT_OBS(this, obs);
  ASSERT_OBS(this, obs->_active_clauses.size() <= cl || obs->_active_clauses[cl] == nullptr || !obs->_active_clauses[cl]->active);

  if (hash == 0) {
    // sort the literals
    std::sort(lits.begin(), lits.end(), [&](Tlit l1, Tlit l2)
      { return lit_to_var(l1) < lit_to_var(l2); });
    hash = obs->hash_clause(lits);
    while (obs->_clauses_dict.find(hash) != obs->_clauses_dict.end()) {
      // if the clause are identical, send a warning message
      if (!notification::_suppress_warning && obs->_clauses_dict[hash]->literals == lits) {
        if (!obs->_clauses_dict[hash]->active) {
          // The clause was deleted. This is not a big problem.
          LOG_INFO("(at notification number " << obs->_notifications.size() << "): The clause " << cl << " is identical to the clause " << obs->_clauses_dict[hash]->cl << " that was deleted earlier");
        }
        else {
          LOG_WARNING("(at notification number " << obs->_notifications.size() << "): The clause " << cl << " is identical to the clause " << obs->_clauses_dict[hash]->cl);
          event_level = 0;
        }
      }
      hash = (hash * lits.size() + 1);
    }

    observer::clause* c = new observer::clause(lits, cl, learnt, external);
    c->active = true;
    obs->_clauses_dict.insert({ hash, c });
    if (obs->_active_clauses.size() <= cl)
      obs->_active_clauses.resize(cl + 1);
  }
  ASSERT_OBS(this, obs->_active_clauses.size() > cl);
  obs->_active_clauses[cl] = obs->_clauses_dict[hash];
  obs->_active_clauses[cl]->active = true;
  // check that the sorted literals are the same
  ASSERT_OBS(this, obs->hash_clause(obs->_active_clauses[cl]->literals) == obs->hash_clause(lits));
  ASSERT_OBS(this, obs->hash_clause(obs->_clauses_dict[hash]->literals) == obs->hash_clause(lits));
  return true;
}

bool napsat::gui::new_clause::rollback(observer* obs)
{
  ASSERT_OBS(this, obs);
  ASSERT_OBS(this, obs->_active_clauses.size() > cl);
  ASSERT_OBS(this, obs->_active_clauses[cl]->active);
  obs->_active_clauses[cl]->active = false;
  return true;
}

unsigned napsat::gui::delete_clause::get_event_level(observer* obs)
{
  ASSERT_OBS(this, obs);
  return obs->is_clause_marked(cl) ? 0 : event_level;
}

bool napsat::gui::delete_clause::apply(observer* obs)
{
  ASSERT_OBS(this, obs);
  ASSERT_OBS(this, obs->_active_clauses.size() > cl);
  ASSERT_OBS(this, obs->_active_clauses[cl]->active);
  obs->_active_clauses[cl]->active = false;
  vector<Tlit>& lits = obs->_active_clauses[cl]->literals;
  hash = obs->hash_clause(lits);
  return true;
}

bool napsat::gui::delete_clause::rollback(observer* obs)
{
  ASSERT_OBS(this, obs);
  ASSERT_OBS(this, obs->_active_clauses.size() > cl);
  ASSERT_OBS(this, obs->_clauses_dict[hash]);
  // the clause cannot be active before. It would mean it was not deleted.
  ASSERT_OBS(this, !obs->_active_clauses[cl]->active);
  obs->_active_clauses[cl] = obs->_clauses_dict[hash];
  ASSERT_OBS(this, !obs->_active_clauses[cl]->active);
  ASSERT_OBS(this, obs->_active_clauses[cl] != nullptr);
  obs->_active_clauses[cl]->active = true;
  return true;
}

bool napsat::gui::checkpoint::apply(observer* obs)
{
  ASSERT_OBS(this, obs);
  // you can do thins only once. During replay, no command can be parsed.
  if (applied)
    return true;
  obs->notify_checkpoint();
  applied = true;
  return true;
}

unsigned napsat::gui::conflict::get_event_level(observer* obs)
{
  ASSERT_OBS(this, obs);
  return obs->is_clause_marked(cl) ? 0 : event_level;
}

unsigned napsat::gui::watch::get_event_level(observer* obs)
{
  ASSERT_OBS(this, obs);
  return obs->is_variable_marked(lit_to_var(lit)) || obs->is_clause_marked(cl) ? 0 : event_level;
}

bool napsat::gui::watch::apply(observer* obs)
{
  ASSERT_OBS(this, obs);
  ASSERT_OBS(this, obs->_active_clauses.size() > cl);
  napsat::gui::observer::clause* c = obs->_active_clauses[cl];
  ASSERT_OBS(this, c != nullptr);
  ASSERT_OBS(this, c->active);
  // the watched literal must be in the clause
  ASSERT_OBS(this, !obs->is_watching(cl, lit));
  c->watched.insert(lit);
  return true;
}

bool napsat::gui::watch::rollback(observer* obs)
{
  ASSERT_OBS(this, obs);
  ASSERT_OBS(this, obs->_active_clauses.size() > cl);
  ASSERT_OBS(this, obs->_active_clauses[cl] != nullptr);
  napsat::gui::observer::clause* c = obs->_active_clauses[cl];
  // the literal must be watched
  ASSERT_OBS(this, obs->is_watching(cl, lit));
  c->watched.erase(lit);
  return true;
}

unsigned napsat::gui::unwatch::get_event_level(observer* obs)
{
  ASSERT_OBS(this, obs);
  return obs->is_variable_marked(lit_to_var(lit)) || obs->is_clause_marked(cl) ? 0 : event_level;
}

bool napsat::gui::unwatch::apply(observer* obs)
{
  ASSERT_OBS(this, obs);
  ASSERT_OBS(this, obs->_active_clauses.size() > cl);
  ASSERT_OBS(this, obs->_active_clauses[cl] != nullptr);
  ASSERT_OBS(this, obs->_active_clauses[cl]->active);
  napsat::gui::observer::clause* c = obs->_active_clauses[cl];
  ASSERT_OBS(this, obs->is_watching(cl, lit));
  c->watched.erase(lit);
  return true;
}

bool napsat::gui::unwatch::rollback(observer* obs)
{
  ASSERT_OBS(this, obs);
  ASSERT_OBS(this, obs->_active_clauses.size() > cl);
  ASSERT_OBS(this, obs->_active_clauses[cl] != nullptr);
  napsat::gui::observer::clause* c = obs->_active_clauses[cl];
  ASSERT_OBS(this, !obs->is_watching(cl, lit));
  c->watched.insert(lit);
  return true;
}

unsigned napsat::gui::remove_literal::get_event_level(observer* obs)
{
  ASSERT_OBS(this, obs);
  return obs->is_variable_marked(lit_to_var(lit)) || obs->is_clause_marked(cl) ? 0 : event_level;
}

bool napsat::gui::remove_literal::apply(observer* obs)
{
  ASSERT_OBS(this, obs);
  ASSERT_OBS(this, obs->_active_clauses.size() > cl);
  ASSERT_OBS(this, obs->_active_clauses[cl] != nullptr);
  observer::clause* c = obs->_active_clauses[cl];
  ASSERT_OBS(this, c->active);
  ASSERT_OBS(this, find(c->literals.begin(), c->literals.end(), lit) != c->literals.end());
  // bring the literal to the end of the clause
  unsigned last_literal_location = c->literals.size() - 1 - c->n_deleted_literals;
  unsigned deleted_literal_location = find(c->literals.begin(), c->literals.end(), lit) - c->literals.begin();
  c->literals[deleted_literal_location] = c->literals[last_literal_location];
  c->literals[last_literal_location] = lit;
  c->n_deleted_literals++;
  return true;
}

bool napsat::gui::remove_literal::rollback(observer* obs)
{
  ASSERT_OBS(this, obs);
  ASSERT_OBS(this, obs->_active_clauses.size() > cl);
  ASSERT_OBS(this, obs->_active_clauses[cl] != nullptr);
  observer::clause* c = obs->_active_clauses[cl];
  ASSERT_OBS(this, c->active);
  // if the literal was watched, add it to the watched literals

  c->n_deleted_literals--;
  ASSERT_OBS(this, c->literals[c->literals.size() - c->n_deleted_literals - 1] == lit);
  return true;
}

bool napsat::gui::check_invariants::apply(observer* obs)
{
  ASSERT_OBS(this, obs);
  if (!obs->check_invariants()) {
    LOG_ERROR("Invariants are not satisfied");
    cerr << obs->get_error_message() << endl;
    event_level = 0;
  }
  return true;
}

bool napsat::gui::check_invariants::rollback(observer* obs)
{
  ASSERT_OBS(this, obs);
  if (!obs->check_invariants()) {
    LOG_ERROR("Invariants are not satisfied");
    cerr << obs->get_error_message() << endl;
    event_level = 0;
  }
  return true;
}

unsigned napsat::gui::block::get_event_level(observer* obs)
{
  ASSERT_OBS(this, obs);
  return obs->is_variable_marked(lit_to_var(lit)) || obs->is_clause_marked(cl) ? 0 : event_level;
}

bool napsat::gui::block::apply(observer* obs)
{
  ASSERT_OBS(this, obs);
  ASSERT_OBS(this, obs->_active_clauses.size() > cl);
  ASSERT_OBS(this, obs->_active_clauses[cl] != nullptr);
  observer::clause* c = obs->_active_clauses[cl];
  ASSERT_OBS(this, c->active);
  ASSERT_OBS(this, find(c->literals.begin(), c->literals.end(), lit) != c->literals.end());
  previous_blocker = c->blocker;
  c->blocker = lit;
  return true;
}

bool napsat::gui::block::rollback(observer* obs)
{
  ASSERT_OBS(this, obs);
  ASSERT_OBS(this, obs->_active_clauses.size() > cl);
  ASSERT_OBS(this, obs->_active_clauses[cl] != nullptr);
  observer::clause* c = obs->_active_clauses[cl];
  ASSERT_OBS(this, c->active);
  ASSERT_OBS(this, c->blocker == lit);
  c->blocker = previous_blocker;
  return true;
}

bool napsat::gui::missed_lower_implication::apply(observer* obs)
{
  ASSERT_OBS(this, obs);
  ASSERT_OBS(this, obs->_variables.size() > var);
  ASSERT_OBS(this, obs->_active_clauses.size() > cl);
  ASSERT_OBS(this, obs->_active_clauses[cl] != nullptr);
  last_cl = obs->_variables[var].lazy_reason;
  obs->_variables[var].lazy_reason = cl;
  return true;
}

bool napsat::gui::missed_lower_implication::rollback(observer* obs)
{
  ASSERT_OBS(this, obs);
  ASSERT_OBS(this, obs->_variables.size() > var);
  // ASSERT_OBS(this, obs->_active_clauses.size() > cl);
  ASSERT_OBS(this, obs->_active_clauses[cl] != nullptr);
  ASSERT_OBS(this, obs->_variables[var].lazy_reason == cl);
  obs->_variables[var].lazy_reason = last_cl;
  return true;
}

bool napsat::gui::remove_lower_implication::apply(observer* obs)
{
  last_cl = obs->_variables[var].lazy_reason;
  obs->_variables[var].lazy_reason = CLAUSE_UNDEF;
  return true;
}

bool napsat::gui::remove_lower_implication::rollback(observer* obs)
{
  ASSERT_OBS(this, obs);
  ASSERT_OBS(this, obs->_variables.size() > var);
  ASSERT_OBS(this, obs->_active_clauses.size() > last_cl);
  ASSERT_OBS(this, obs->_active_clauses[last_cl] != nullptr);
  ASSERT_OBS(this, obs->_variables[var].lazy_reason == CLAUSE_UNDEF);
  obs->_variables[var].lazy_reason = last_cl;
  return true;
}
