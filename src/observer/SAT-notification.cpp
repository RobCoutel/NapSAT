#include "SAT-notification.hpp"
#include "SAT-observer.hpp"

#include <cassert>
#include <iostream>

using namespace sat;
using namespace sat::gui;
using namespace std;

bool sat::gui::notification::_suppress_warning = false;

void sat::gui::new_variable::apply(observer &obs)
{
  if (var >= obs._variables.size())
  {
    obs._variables.resize(var + 1);
    obs._variables[var].active = true;
    return;
  }
  assert(!obs._variables[var].active);
  obs._variables[var].active = true;
}

void sat::gui::new_variable::rollback(observer &obs)
{
  assert(obs._variables.size() > var);
  assert(obs._variables[var].active);
  obs._variables[var] = observer::variable();
}

void sat::gui::delete_variable::apply(observer &obs)
{
  assert(obs._variables.size() > var);
  assert(obs._variables[var].active);
  obs._variables[var].active = false;
}

void sat::gui::delete_variable::rollback(observer &obs)
{
  assert(obs._variables.size() > var);
  assert(!obs._variables[var].active);
  obs._variables[var].active = true;
}

void sat::gui::decision::apply(observer &obs)
{
  Tvar var = lit_to_var(lit);
  assert(obs._variables.size() > var);
  assert(obs._variables[var].active);
  assert(obs._variables[var].value == VAR_UNDEF);
  obs._variables[var].value = lit_pol(lit);
  obs._decision_level++;
  obs._variables[var].level = obs._decision_level;
  obs._variables[var].reason = CLAUSE_UNDEF;
  obs._assignment_stack.push_back(lit);
}

void sat::gui::decision::rollback(observer &obs)
{
  Tvar var = lit_to_var(lit);
  assert(obs._variables.size() > var);
  assert(obs._variables[var].active);
  assert(obs._variables[var].value == lit_pol(lit));
  assert(obs._variables[var].level == obs._decision_level);
  assert(obs._assignment_stack.back() == lit);
  obs._variables[var].value = VAR_UNDEF;
  obs._decision_level--;
  obs._assignment_stack.pop_back();
}

void sat::gui::implication::apply(observer &obs)
{
  Tvar var = lit_to_var(lit);
  assert(obs._variables.size() > var);
  assert(obs._variables[var].active);
  assert(obs._variables[var].value == VAR_UNDEF);
  assert(reason != CLAUSE_UNDEF);
  assert(reason != CLAUSE_LAZY);
  assert(reason <= obs._active_clauses.size());
  obs._variables[var].value = lit_pol(lit);
  obs._variables[var].reason = reason;
  obs._variables[var].level = level;

  if (!notification::_suppress_warning)
  {
    Tlevel level_check = 0;
    bool found = false;
    for (Tlit l : obs._active_clauses[reason]->literals)
    {
      if (lit == l)
      {
        found = true;
        continue;
      }
      Tvar v = lit_to_var(l);
      if (obs._variables[v].level > level_check)
        level_check = obs._variables[v].level;
    }
    if (!notification::_suppress_warning && !found)
    {
      cerr << "\033[0;33mWARNING\033[0m: The clause " << reason << " does not contain the literal " << lit_to_int(lit) << endl;
    }
    if (!notification::_suppress_warning && (level_check == LEVEL_ERROR || level_check == LEVEL_UNDEF))
    {
      cerr << "\033[0;33mWARNING\033[0m: The clause " << reason << " seems to contain a literal that is not assigned different from " << lit_to_int(lit) << endl;
    }
    else if (!notification::_suppress_warning && level_check != level)
    {
      cerr << "\033[0;33mWARNING\033[0m: level of variable " << var << " is " << level_check << " but was given as " << level << " by the solver " << endl;
    }
  }
  assert(obs._variables[var].level <= obs._decision_level);
  obs._assignment_stack.push_back(lit);
}

void sat::gui::implication::rollback(observer &obs)
{
  Tvar var = lit_to_var(lit);
  assert(obs._variables.size() > var);
  assert(obs._variables[var].active);
  assert(obs._variables[var].value == lit_pol(lit));
  assert(obs._variables[var].level <= obs._decision_level);
  assert(obs._assignment_stack.back() == lit);
  obs._variables[var].value = VAR_UNDEF;
  obs._assignment_stack.pop_back();
}

void sat::gui::propagation::apply(observer &obs)
{
  assert(obs._variables.size() > lit_to_var(lit));
  assert(obs._variables[lit_to_var(lit)].active);
  assert(obs._variables[lit_to_var(lit)].value != VAR_UNDEF);
  assert(obs._assignment_stack[obs._n_propagated] == lit);
  obs._n_propagated++;
}

void sat::gui::propagation::rollback(observer &obs)
{
  assert(obs._n_propagated > 0);
  obs._n_propagated--;
  assert(obs._variables.size() > lit_to_var(lit));
  assert(obs._variables[lit_to_var(lit)].active);
  assert(obs._variables[lit_to_var(lit)].value != VAR_UNDEF);
  assert(obs._assignment_stack[obs._n_propagated] == lit);
}

void sat::gui::unassignment::apply(observer &obs)
{
  Tvar var = lit_to_var(lit);
  assert(obs._variables.size() > var);
  assert(obs._variables[var].active);
  assert(obs._variables[var].value != VAR_UNDEF);
  if (obs._variables[var].reason == CLAUSE_UNDEF)
    obs._decision_level--;
  obs._variables[var].value = VAR_UNDEF;

  if (location == MAX_UNSIGNED)
  {
    unsigned i;
    for (i = obs._assignment_stack.size() - 1; i < obs._assignment_stack.size(); i--)
    {
      Tvar v = lit_to_var(obs._assignment_stack[i]);
      if (v == var)
      {
        obs._assignment_stack.erase(obs._assignment_stack.begin() + i);
        location = i;
        break;
      }
    }
    assert(i < obs._assignment_stack.size() + 1);
  }
  else
  {
    assert(location < obs._assignment_stack.size());
    assert(lit_to_var(obs._assignment_stack[location]) == var);
    obs._assignment_stack.erase(obs._assignment_stack.begin() + location);
  }
  if (location < obs._n_propagated) {
    obs._n_propagated--;
    propagated = true;
  }

  level = obs._variables[var].level;
  reason = obs._variables[var].reason;

  obs._variables[var].level = LEVEL_UNDEF;
  obs._variables[var].reason = CLAUSE_UNDEF;
}

void sat::gui::unassignment::rollback(observer &obs)
{
  Tvar var = lit_to_var(lit);
  // print all the variables involved in that method
  assert(obs._variables.size() > var);
  assert(obs._variables[var].active);
  assert(obs._variables[var].value == VAR_UNDEF);
  assert(location <= obs._assignment_stack.size());
  obs._assignment_stack.insert(obs._assignment_stack.begin() + location, lit);
  obs._variables[var].value = lit_pol(lit);
  obs._variables[var].level = level;
  obs._variables[var].reason = reason;
  obs._decision_level = max(obs._decision_level, level);

  if (propagated)
    obs._n_propagated++;

  assert(lit_to_var(obs._assignment_stack[location]) == var);
}

void sat::gui::new_clause::apply(observer &obs)
{
  assert(obs._active_clauses.size() <= cl
      || obs._active_clauses[cl] == nullptr
      || !obs._active_clauses[cl]->active);
  if (hash == 0) {
    hash = obs.hash_clause(lits);
    observer::clause* c = new observer::clause(lits, cl, learnt, external);
    c->active = true;
    obs._clauses_dict.insert({hash, c});
    if (obs._active_clauses.size() <= cl)
      obs._active_clauses.resize(cl + 1);
  }
  assert(obs._active_clauses.size() > cl);
  obs._active_clauses[cl] = obs._clauses_dict[hash];
  obs._active_clauses[cl]->active = true;
}

void sat::gui::new_clause::rollback(observer &obs)
{
  assert(obs._active_clauses.size() > cl);
  assert(obs._active_clauses[cl]->active);
  obs._active_clauses[cl]->active = false;
}

void sat::gui::delete_clause::apply(observer &obs)
{
  assert(obs._active_clauses.size() > cl);
  assert(obs._active_clauses[cl]->active);
  obs._active_clauses[cl]->active = false;
  vector<Tlit>& lits = obs._active_clauses[cl]->literals;

  hash = obs.hash_clause(lits);
}

void sat::gui::delete_clause::rollback(observer &obs)
{
  assert(obs._active_clauses.size() > cl);
  assert(!obs._active_clauses[cl]->active);
  obs._active_clauses[cl]->active = true;
  assert(obs._active_clauses[cl] != nullptr);
  obs._active_clauses[cl] = obs._clauses_dict[hash];
}

void sat::gui::checkpoint::apply(observer &obs)
{
  // you can do thins only once. During replay, no command can be parsed.
  if (applied)
    return;
  obs.notify_checkpoint();
  applied = true;
}

void sat::gui::checkpoint::rollback(observer &obs)
{
}
