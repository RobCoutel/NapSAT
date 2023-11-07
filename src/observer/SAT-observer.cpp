#include "SAT-observer.hpp"
#include "../solver/SAT-config.hpp"

#include <typeinfo>
#include <iostream>
#include <fstream>
#include <cassert>

using namespace sat;
using namespace sat::gui;
using namespace std;

long unsigned sat::gui::observer::hash_clause(const std::vector<sat::Tlit> &lits)
{
  // https://stackoverflow.com/questions/20511347/a-good-hash-function-for-a-vector
  std::size_t seed = lits.size();
  for (auto x : lits)
  {
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = (x >> 16) ^ x;
    seed ^= x + 0x9e3779b9 + (seed << 6) + (seed >> 2);
  }
  return seed;
}

sat::gui::observer::observer()
{
  _display = new sat::gui::display(this);
}

void observer::notify(notification *notification)
{
  cout << "notification: " << notification->get_message() << endl;
  assert(_location == _notifications.size());
  _notifications.push_back(notification);
  _location++;
  notification->apply(*this);
  _display->notify_change(notification->get_event_level());
}

unsigned observer::next()
{
  assert(_location < _notifications.size());
  _notifications[_location++]->apply(*this);
  cout << "notification " << _location << "/" << _notifications.size() << endl;
  return _notifications[_location - 1]->get_event_level();
}

unsigned observer::back()
{
  assert(_location > 0);
  _location--;
  notification *notification = _notifications[_location];
  cout << "notification " << _location << "/" << _notifications.size() << endl;
  cout << "notification: " << notification->get_message() << endl;
  notification->rollback(*this);
  return notification->get_event_level();
}

void sat::gui::observer::notify_checkpoint()
{
  while (_commands.size() != 0)
  {
    string command = _commands[0];
    _commands.erase(_commands.begin());
    cout << "Executing command: " << command << endl;
    if (transmit_command(command))
      return;
  }
  _display->notify_checkpoint();
}

void sat::gui::observer::load_commands(std::string filename)
{
  ifstream file(filename);
  if (!file.is_open()) {
    cout << "Error: could not open file \"" << filename << "\"\n";
    return;
  }
  string line;
  while (getline(file, line)) {
    _commands.push_back(line);
  }
  file.close();
}

void sat::gui::observer::set_command_parser(command_parser parser)
{
  _command_parser = parser;
}

bool sat::gui::observer::transmit_command(std::string command)
{
  assert(_command_parser);
  return _command_parser(command);
}

sat::Tval observer::var_value(sat::Tvar var)
{
  if (var >= _variables.size())
    return VAR_ERROR;
  if (!_variables[var].active)
    return VAR_ERROR;
  return _variables[var].value;
}

sat::Tval observer::lit_value(sat::Tlit lit)
{
  sat::Tval value = var_value(lit_to_var(lit));
  if (value == VAR_ERROR)
    return VAR_ERROR;
  if (value == VAR_UNDEF)
    return VAR_UNDEF;
  if (value == VAR_TRUE)
    return lit_pol(lit);
  if (value == VAR_FALSE)
    return lit_pol(lit) ^ 1;
  assert(false);
  return VAR_ERROR;
}

sat::Tlevel observer::var_level(sat::Tvar var)
{
  if (var >= _variables.size())
    return LEVEL_ERROR;
  if (!_variables[var].active)
    return LEVEL_ERROR;
  return _variables[var].level;
}

sat::Tlevel observer::lit_level(sat::Tlit lit)
{
  return var_level(lit_to_var(lit));
}

const std::vector<sat::Tlit> &observer::get_assignment()
{
  return _assignment_stack;
}

std::vector<std::pair<sat::Tclause, const std::vector<sat::Tlit> *>> sat::gui::observer::get_clauses()
{
  vector<pair<Tclause, const vector<Tlit> *>> to_return;
  Tclause cl = 0;
  for (clause *cl_ptr : _active_clauses)
  {
    if (cl_ptr && cl_ptr->active)
    {
      assert(cl_ptr->cl == cl);
      to_return.push_back(make_pair(cl, &cl_ptr->literals));
    }
    cl++;
  }
  return to_return;
}

static const char ESC_CHAR = 27; // the decimal code for escape character is 27

/**
 * @brief Returns the length of the string when escaped.
 */
static unsigned string_length_escaped(string str)
{
  unsigned length = 0;
  for (unsigned i = 0; i < str.length(); i++)
  {
    if (str[i] == ESC_CHAR)
    {
      while (str[i] != 'm')
        i++;
      continue;
    }
    length++;
  }
  return length;
}

std::string sat::gui::observer::literal_to_string(sat::Tlit lit)
{
  string s = "";
  Tvar var = lit_to_var(lit);
  if (lit_value(lit) == VAR_UNDEF)
    s += "\033[0;33m";
  else if (lit_value(lit) == VAR_TRUE)
    s += "\033[0;32m";
  else
    s += "\033[0;31m";
  if (lit_value(lit) != VAR_UNDEF && _variables[var].reason == CLAUSE_UNDEF)
  {
    s += ESC_CHAR;
    s += "[4m";
  }
  s += std::to_string(lit_to_int(lit));
  s += ESC_CHAR;
  s += "[0m";
  s += "\033[0m";
  return s;
}

/**
 * @brief Adds spaces to the left of the number to make it have as many digits as the maximum number of digits in the given range.
 */
static string pad(int n, int max_int)
{
  int max_digits = 0;
  while (max_int > 0)
  {
    max_int /= 10;
    max_digits++;
  }
  int digits = 0;
  while (n > 0)
  {
    n /= 10;
    digits++;
  }
  string s = "";
  for (int i = 0; i < max_digits - digits; i++)
    s += " ";
  return s;
}

std::string sat::gui::observer::variable_to_string(sat::Tvar var)
{
  string s = "";
  s += std::to_string(var) + ": ";
  s += pad(var, _variables.size());
  if (_variables[var].active)
  {
    if (_variables[var].value == VAR_UNDEF)
      s += "\033[0;33mundef\033[0m";
    else if (_variables[var].value == VAR_TRUE)
      s += "\033[0;32mtrue\033[0m";
    else
      s += "\033[0;31mfalse\033[0m";
    s += " @ ";
    if (_variables[var].level == LEVEL_UNDEF)
      s += "inf";
    else
      s += std::to_string(_variables[var].level);
    s += " by ";
    if (_variables[var].reason == CLAUSE_UNDEF)
      s += "decision";
    else if (_variables[var].reason == CLAUSE_LAZY)
      s += "lazy";
    else
      s += std::to_string(_variables[var].reason);
  }
  else
    s += "deleted";
  return s;
}

void sat::gui::observer::sort_clauses(Tclause cl)
{
  vector<Tlit> &lits = _active_clauses[cl]->literals;
  unsigned n_sat = 0;
  unsigned n_undef = 0;
  for (Tlit lit : lits)
  {
    if (lit_value(lit) == VAR_TRUE)
      n_sat++;
    else if (lit_value(lit) == VAR_UNDEF)
      n_undef++;
  }
  unsigned i = 0;
  unsigned j = n_sat;
  unsigned k = n_sat + n_undef;
  while (i < n_sat)
  {
    if (lit_value(lits[i]) == VAR_TRUE)
      i++;
    else if (lit_value(lits[i]) == VAR_UNDEF)
    {
      assert(j < n_sat + n_undef);
      swap(lits[i], lits[j++]);
    }
    else
    {
      assert(lit_value(lits[i]) == VAR_FALSE);
      assert(k < lits.size());
      swap(lits[i], lits[k++]);
    }
  }
  while (j < n_sat + n_undef)
  {
    if (lit_value(lits[j]) == VAR_UNDEF)
      j++;
    else
    {
      assert(k < lits.size());
      swap(lits[j], lits[k++]);
    }
  }
}

const static unsigned TERMINAL_WIDTH = 158;
std::string sat::gui::observer::clause_to_string(Tclause cl)
{
  string s = "";
  if (cl == CLAUSE_UNDEF)
    return "undef";
  if (_active_clauses[cl]->literals.size() > 0
  && lit_value(_active_clauses[cl]->literals[0]) == VAR_UNDEF)
    s += "\033[0;33m";
  else if (_active_clauses[cl]->literals.size() > 0
  && lit_value(_active_clauses[cl]->literals[0]) == VAR_TRUE)
    s += "\033[0;32m";
  else
    s += "\033[0;31m";
  s += std::to_string(cl) + "\033[0m: ";
  vector<Tlit> lits = _active_clauses[cl]->literals;

  for (Tlit lit : lits)
    s += literal_to_string(lit) + " ";
  return s;
}

void sat::gui::observer::print_clause_set()
{
  unsigned longest_clause = 0;
  cout << "Clauses: " << _clauses_dict.size() << "\n";
  for (Tclause cl = 0; cl < _active_clauses.size(); cl++)
  {
    if (_active_clauses[cl] == nullptr || !_active_clauses[cl]->active)
      continue;
    if (_active_clauses[cl]->literals.size() > longest_clause)
      longest_clause = _active_clauses[cl]->literals.size();
  }
  unsigned longest_var = 1; // 1 for the sign
  Tvar max_var = _variables.size();
  while (max_var > 0)
  {
    max_var /= 10;
    longest_var++;
  }

  unsigned max_clause_width = (longest_clause + 2) * (longest_var + 1) + 3;
  Tclause i = 0;
  while (i < _active_clauses.size())
  {
    unsigned j = max_clause_width;
    while (j < TERMINAL_WIDTH && i < _active_clauses.size())
    {
      if (!_active_clauses[i]->active)
      {
        i++;
        continue;
      }
      sort_clauses(i);
      string clause_str = clause_to_string(i++);
      cout << clause_str;
      string spaces = "";

      assert(string_length_escaped(clause_str) <= max_clause_width);
      for (unsigned k = 0; k < max_clause_width - string_length_escaped(clause_str); k++)
        spaces += " ";
      cout << spaces;
      j += max_clause_width;
    }
    cout << endl;
  }
  for (unsigned i = 0; i < TERMINAL_WIDTH; i++)
    cout << "*";
  cout << "\n";
}

void sat::gui::observer::print_deleted_clauses()
{
}

void sat::gui::observer::print_assignment()
{
  cout << "trail :\n";
  for (Tlevel lvl = _decision_level; lvl <= _decision_level; lvl--)
  {
    cout << lvl << ": ";
    for (unsigned i = 0; i < _assignment_stack.size(); i++)
    {
      if (i == _n_propagated)
        cout << "| ";
      Tlit lit = _assignment_stack[i];
      Tvar var = lit_to_var(lit);
      if (_variables[var].level == lvl)
      {
        cout << literal_to_string(lit) << " ";
        cout << pad(lit_to_var(lit), _variables.size());
        if (lit_pol(lit))
          cout << " ";
      }
      else
      {
        cout << "  " + pad(0, _variables.size());
        ;
      }
    }
    if (_n_propagated == _assignment_stack.size())
      cout << "| ";
    cout << "\n";
  }
  for (unsigned i = 0; i < TERMINAL_WIDTH; i++)
    cout << "*";
  cout << "\n";
}

void sat::gui::observer::print_variables()
{
  const unsigned MAX_VARS_PER_LINE = 30;
  unsigned current_position = 0;
  cout << "variables :\n";
  for (Tvar var = 0; var < _variables.size(); var++)
  {
    if (current_position + MAX_VARS_PER_LINE > TERMINAL_WIDTH)
    {
      cout << "\n";
      current_position = 0;
    }
    current_position += MAX_VARS_PER_LINE;
    string variable_str = variable_to_string(var);
    cout << variable_str;
    string spaces = "";
    assert(string_length_escaped(variable_str) <= MAX_VARS_PER_LINE);
    for (unsigned k = 0; k < MAX_VARS_PER_LINE - string_length_escaped(variable_str); k++)
      spaces += " ";
    cout << spaces;
  }
  cout << "\n";
  for (unsigned i = 0; i < TERMINAL_WIDTH; i++)
    cout << "*";
  cout << "\n";
}

sat::gui::observer::~observer()
{
  for (auto notification : _notifications)
    delete notification;
  for (auto clause : _clauses_dict)
    delete clause.second;
  delete _display;
}
