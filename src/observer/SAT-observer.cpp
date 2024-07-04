/**
 * @file src/observer/SAT-observer.cpp
 * @author Robin Coutelier
 *
 * @brief This file is part of the NapSAT solver. It contains the implementation of the
 * class observer.
 */

#include "SAT-observer.hpp"
#include "SAT-config.hpp"

#include <typeinfo>
#include <iostream>
#include <fstream>
#include <cassert>
#include <cmath>

#ifdef __unix__
#include <sys/ioctl.h> //ioctl() and TIOCGWINSZ
#include <unistd.h> // for STDOUT_FILENO
#endif
#ifdef _WIN32
#include <windows.h>
#endif

#define COLORBLIND_MODE 1

static unsigned TERMINAL_WIDTH = 169;

using namespace napsat;
using namespace napsat::gui;
using namespace std;

long unsigned napsat::gui::observer::hash_clause(const std::vector<napsat::Tlit>& lits)
{
  // https://stackoverflow.com/questions/20511347/a-good-hash-function-for-a-vector
  std::size_t seed = lits.size();
  for (auto x : lits) {
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = (x >> 16) ^ x;
    seed ^= x + 0x9e3779b9 + (seed << 6) + (seed >> 2);
  }
  assert(seed != 0);
  return seed;
}

napsat::gui::observer::observer(napsat::options& options) : _options(options)
{
  _display = new napsat::gui::display(this);
  assert(options.interactive || options.observing || options.check_invariants || options.print_stats);
  if (options.interactive || options.observing) {
    notify(new napsat::gui::marker("Start"));
  }
  else {
    if (options.check_invariants) {
      toggle_checking_only(true);
    }
    else {
      assert(options.print_stats);
      toggle_stats_only(true);
    }
  }
  load_invariant_configuration();
  if (options.commands_file != "") {
    load_commands(options.commands_file);
  }
  if (options.save_folder != "") {
    if (options.save_folder[options.save_folder.size() - 1] != '/') {
      options.save_folder += "/";
    }
    string save_folder = options.save_folder;
    for (unsigned i = 0; i < save_folder.size(); i++) {
      if (save_folder[i] == ' ') {
        save_folder.insert(i, "\\");
        i++;
      }
    }
    // create the folder if it does not exist
    if (system(("mkdir -p " + save_folder).c_str()) != 0) {
      cerr << "Error: could not create folder \"" << options.save_folder << "\"\n";
    }
  }
}

napsat::gui::observer::observer(const observer& other) : _options(other._options)
{
  for (auto notification : other._notifications)
    _notifications.push_back(notification->clone());
}

void observer::notify(notification* notification)
{
  if (notification->get_type() == STAT) {
    stat_count[notification->get_message()]++;
    delete notification;
    return;
  }
  else {
    notification_count[notification->get_type()]++;
    if (_stats_only)
      delete notification;
    else
      _notifications.push_back(notification);
  }
  if (_stats_only)
    return;

  _location++;
  assert(_location == _notifications.size());
  // cout << "notification " << _location << "/" << _notifications.size() << endl;
  // cout << "notification: " << notification->get_message() << endl;
  notification->apply(this);
  if (!_check_invariants_only) {
    if (_breakpoints.find(_location) != _breakpoints.end()) {
      cout << "Breakpoint reached" << endl;
      _display->notify_change(1);
    }
    else
      _display->notify_change(notification->get_event_level(this));
  }
}

std::string observer::get_statistics()
{
  string s = "";
  s += "Core Statistics:\n";
  s += "  - Variables: " + to_string(_variables.size()) + "\n";
  unsigned n_clauses = 0;
  for (clause* cl : _active_clauses) {
    if (cl && cl->active)
      n_clauses++;
  }
  s += "  - Clauses: " + to_string(n_clauses) + "\n";
  s += "  - Notifications:\n";
  for (auto pair : notification_count) {
    s += "  - " + notification_type_to_string(pair.first) + ": " + to_string(pair.second) + "\n";
  }

  if (stat_count.size() > 0) {
    s += "Additional Statistics:\n";
    for (auto pair : stat_count) {
      s += "  - " + pair.first + ": " + to_string(pair.second) + "\n";
    }
  }
  return s;
}

unsigned observer::next()
{
  if (_stats_only) {
    cerr << "\033[0;33mWARNING\033[0m: trying to navigate in statistics only mode" << endl;
  }
  assert(_location < _notifications.size());
  _notifications[_location++]->apply(this);
  if (_breakpoints.find(_location) != _breakpoints.end()) {
    cout << "Breakpoint reached" << endl;
    return 1;
  }
  return _notifications[_location - 1]->get_event_level(this);
}

unsigned observer::back()
{
  assert(_location > 0);
  _location--;
  notification* notification = _notifications[_location];
  notification->rollback(this);
  if (_breakpoints.find(_location) != _breakpoints.end()) {
    cout << "Breakpoint reached" << endl;
    return 1;
  }
  return notification->get_event_level(this);
}

std::string napsat::gui::observer::last_message()
{
  if (_location == 0)
    return "Initial state";
  return _notifications[_location - 1]->get_message();
}

void napsat::gui::observer::mark_variable(napsat::Tvar var)
{
  _marked_variables.insert(var);
}

void napsat::gui::observer::mark_clause(napsat::Tclause cl)
{
  _marked_clauses.insert(cl);
}

void napsat::gui::observer::unmark_variable(napsat::Tvar var)
{
  _marked_variables.erase(var);
}

void napsat::gui::observer::unmark_clause(napsat::Tclause cl)
{
  _marked_clauses.erase(cl);
}

bool napsat::gui::observer::is_variable_marked(napsat::Tvar var)
{
  return _marked_variables.find(var) != _marked_variables.end();
}

bool napsat::gui::observer::is_clause_marked(napsat::Tclause cl)
{
  return _marked_clauses.find(cl) != _marked_clauses.end();
}

void napsat::gui::observer::set_breakpoint(unsigned n_notifications)
{
  _breakpoints.insert(n_notifications);
}

void napsat::gui::observer::unset_breakpoint(unsigned n_notifications)
{
  _breakpoints.erase(n_notifications);
}

void napsat::gui::observer::notify_checkpoint()
{
  while (_commands.size() != 0) {
    string command = _commands[0];
    _commands.erase(_commands.begin());
    cout << "Executing command: " << command << endl;
    if (transmit_command(command))
      return;
  }
  _display->notify_checkpoint();
}

void napsat::gui::observer::load_commands(std::string filename)
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

void napsat::gui::observer::set_command_parser(command_parser parser)
{
  _command_parser = parser;
}

bool napsat::gui::observer::transmit_command(std::string command)
{
  assert(_command_parser);
  return _command_parser(command);
}

napsat::Tval observer::var_value(napsat::Tvar var)
{
  if (var >= _variables.size())
    return VAR_ERROR;
  if (!_variables[var].active)
    return VAR_ERROR;
  return _variables[var].value;
}

napsat::Tval observer::lit_value(napsat::Tlit lit)
{
  napsat::Tval value = var_value(lit_to_var(lit));
  assert(value == VAR_TRUE || value == VAR_FALSE || value == VAR_UNDEF || value == VAR_ERROR);
  if (value == VAR_ERROR)
    return VAR_ERROR;
  if (value == VAR_UNDEF)
    return VAR_UNDEF;
  return lit_pol(lit) == value;
}

napsat::Tlevel observer::var_level(napsat::Tvar var)
{
  if (var >= _variables.size())
    return LEVEL_ERROR;
  if (!_variables[var].active)
    return LEVEL_ERROR;
  return _variables[var].level;
}

napsat::Tclause napsat::gui::observer::var_reason(napsat::Tvar var)
{
  if (var >= _variables.size())
    return CLAUSE_ERROR;
  if (!_variables[var].active)
    return CLAUSE_ERROR;
  return _variables[var].reason;
}

napsat::Tclause napsat::gui::observer::var_lazy_reason(napsat::Tvar var)
{
  if (var >= _variables.size())
    return CLAUSE_ERROR;
  if (!_variables[var].active)
    return CLAUSE_ERROR;
  return _variables[var].lazy_reason;
}

napsat::Tclause napsat::gui::observer::lit_lazy_reason(napsat::Tvar var)
{
  return var_lazy_reason(lit_to_var(var));
}

bool napsat::gui::observer::var_propagated(napsat::Tvar var)
{
  return _variables[var].propagated;
}

napsat::Tlevel observer::lit_level(napsat::Tlit lit)
{
  return var_level(lit_to_var(lit));
}

napsat::Tlevel napsat::gui::observer::clause_level(napsat::Tclause cl)
{
  if (cl == CLAUSE_UNDEF)
    return LEVEL_UNDEF;
  Tlevel level = 0;
  for (Tlit lit : _active_clauses[cl]->literals)
    level = max(level, lit_level(lit));
  return level;
}

napsat::Tclause napsat::gui::observer::lit_reason(napsat::Tlit lit)
{
  return var_reason(lit_to_var(lit));
}

bool napsat::gui::observer::lit_propagated(napsat::Tlit lit)
{
  return var_propagated(lit_to_var(lit));
}

const std::vector<napsat::Tlit>& observer::get_assignment()
{
  return _assignment_stack;
}

std::vector<std::pair<napsat::Tclause, const std::vector<napsat::Tlit>*>> napsat::gui::observer::get_clauses()
{
  vector<pair<Tclause, const vector<Tlit>*>> to_return;
  Tclause cl = 0;
  for (clause* cl_ptr : _active_clauses) {
    if (cl_ptr && cl_ptr->active) {
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
  for (unsigned i = 0; i < str.length(); i++) {
    if (str[i] == ESC_CHAR) {
      while (str[i] != 'm')
        i++;
      continue;
    }
    length++;
  }
  return length;
}

std::string napsat::gui::observer::lit_to_string(napsat::Tlit lit)
{
  string s = "";
  Tvar var = lit_to_var(lit);
  if (lit_value(lit) == VAR_UNDEF)
    s += "\033[0;33m";
  else if (lit_value(lit) == VAR_TRUE)
    s += "\033[0;32m";
  else
    s += "\033[0;31m";
  if (lit_value(lit) != VAR_UNDEF && _variables[var].reason == CLAUSE_UNDEF) {
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
  while (max_int > 0) {
    max_int /= 10;
    max_digits++;
  }
  int digits = 0;
  while (n > 0) {
    n /= 10;
    digits++;
  }
  string s = "";
  for (int i = 0; i < max_digits - digits; i++)
    s += " ";
  return s;
}

std::string napsat::gui::observer::variable_to_string(napsat::Tvar var)
{
  string s = "";
  s += std::to_string(var) + ": ";
  s += pad(var, _variables.size());
  if (_variables[var].active) {
    if (_variables[var].value == VAR_UNDEF)
      s += "\033[0;33mundef\033[0m";
    else if (_variables[var].value == VAR_TRUE)
      s += "\033[0;32mtrue\033[0m";
    else if (_variables[var].value == VAR_FALSE)
      s += "\033[0;31mfalse\033[0m";
    else
      s += "error";
    s += " @ ";
    if (_variables[var].level == LEVEL_UNDEF)
      s += "inf";
    else
      s += std::to_string(_variables[var].level);
    s += " by ";
    if (_variables[var].reason == CLAUSE_UNDEF && _variables[var].value != VAR_UNDEF)
      s += "decision";
    else if (_variables[var].reason == CLAUSE_UNDEF)
      s += "undef";
    else if (_variables[var].reason == CLAUSE_LAZY)
      s += "lazy";
    else
      s += std::to_string(_variables[var].reason);
    if (_variables[var].lazy_reason != CLAUSE_UNDEF)
      s += "/" + std::to_string(_variables[var].lazy_reason);
  }
  else
    s += "deleted";
  return s;
}

bool napsat::gui::observer::enable_sorting = false;

void napsat::gui::observer::sort_clauses(Tclause cl)
{
  if (!enable_sorting)
    return;
  vector<Tlit>& lits = _active_clauses[cl]->literals;
  unsigned n_sat = 0;
  unsigned n_undef = 0;
  for (Tlit lit : lits) {
    if (lit_value(lit) == VAR_TRUE)
      n_sat++;
    else if (lit_value(lit) == VAR_UNDEF)
      n_undef++;
  }
  unsigned i = 0;
  unsigned j = n_sat;
  unsigned k = n_sat + n_undef;
  while (i < n_sat) {
    if (lit_value(lits[i]) == VAR_TRUE)
      i++;
    else if (lit_value(lits[i]) == VAR_UNDEF) {
      assert(j < n_sat + n_undef);
      swap(lits[i], lits[j++]);
    }
    else {
      assert(lit_value(lits[i]) == VAR_FALSE);
      assert(k < lits.size());
      swap(lits[i], lits[k++]);
    }
  }
  while (j < n_sat + n_undef) {
    if (lit_value(lits[j]) == VAR_UNDEF)
      j++;
    else {
      assert(k < lits.size());
      swap(lits[j], lits[k++]);
    }
  }
}

std::string napsat::gui::observer::clause_to_string(Tclause cl)
{
  string s = "";
  if (cl == CLAUSE_UNDEF)
    return "undef";
  if (enable_sorting) {
    if (_active_clauses[cl]->literals.size() > 0 && lit_value(_active_clauses[cl]->literals[0]) == VAR_UNDEF)
      s += "\033[0;33m";
    else if (_active_clauses[cl]->literals.size() > 0 && lit_value(_active_clauses[cl]->literals[0]) == VAR_TRUE)
      s += "\033[0;32m";
    else
      s += "\033[0;31m";
    s += std::to_string(cl) + "\033[0m: ";
  }
  else {
    s += std::to_string(cl) + ": ";
  }
  vector<Tlit> lits = _active_clauses[cl]->literals;

  for (unsigned i = 0; i < lits.size(); i++) {
    if (i + _active_clauses[cl]->n_deleted_literals == lits.size())
      s += "| ";
    Tlit lit = lits[i];
    assert(i + _active_clauses[cl]->n_deleted_literals < lits.size() || lit_value(lit) == VAR_FALSE);
    if (_active_clauses[cl]->watched.find(lit) != _active_clauses[cl]->watched.end())
      s += "w";
    if (_active_clauses[cl]->blocker == lit)
      s += "b";
    s += lit_to_string(lit) + " ";
  }
  return s;
}

void napsat::gui::observer::print_clause_set()
{
#ifdef __unix__
  struct winsize size;
  ioctl(STDOUT_FILENO, TIOCGWINSZ, &size);
  TERMINAL_WIDTH = size.ws_col;
#endif
#ifdef _WIN32
  CONSOLE_SCREEN_BUFFER_INFO csbi;
  GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
  TERMINAL_WIDTH = csbi.srWindow.Right - csbi.srWindow.Left + 1;
#endif

  unsigned longest_clause = 0;
  cout << "Clauses: " << _clauses_dict.size() << "\n";
  for (Tclause cl = 0; cl < _active_clauses.size(); cl++) {
    if (_active_clauses[cl] == nullptr || !_active_clauses[cl]->active)
      continue;
    if (_active_clauses[cl]->literals.size() > longest_clause)
      longest_clause = _active_clauses[cl]->literals.size();
  }
  unsigned longest_var = 1; // 1 for the sign
  Tvar max_var = _variables.size();
  while (max_var > 0) {
    max_var /= 10;
    longest_var++;
  }

  unsigned max_clause_width = (longest_clause + 2) * (longest_var + 1) + 3;
  unsigned current_position = 0;
  for (unsigned i = 0; i < _active_clauses.size(); i++) {
    if (!_active_clauses[i]->active)
      continue;
    if (current_position + max_clause_width > TERMINAL_WIDTH) {
      cout << "\n";
      current_position = 0;
    }
    sort_clauses(i);
    string clause_str = clause_to_string(i);
    cout << clause_str;
    string spaces = "";

    assert(string_length_escaped(clause_str) <= max_clause_width);
    unsigned k = string_length_escaped(clause_str);
    for (; k < max_clause_width && k < TERMINAL_WIDTH; k++)
      spaces += " ";
    cout << spaces;
    current_position += max_clause_width;
  }
  cout << "\n";
  for (unsigned i = 0; i < TERMINAL_WIDTH; i++)
    cout << "*";
  cout << endl;
}

void napsat::gui::observer::print_deleted_clauses()
{}

void napsat::gui::observer::print_assignment()
{
#ifdef __unix__
  struct winsize size;
  ioctl(STDOUT_FILENO, TIOCGWINSZ, &size);
  TERMINAL_WIDTH = size.ws_col;
#endif
#ifdef _WIN32
  CONSOLE_SCREEN_BUFFER_INFO csbi;
  GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
  TERMINAL_WIDTH = csbi.srWindow.Right - csbi.srWindow.Left + 1;
#endif
  cout << "trail :\n";

  unsigned max_n_digits_level = 1;
  Tlevel max_level = _decision_level;
  while (max_level > 0) {
    max_level /= 10;
    max_n_digits_level++;
  }

  for (Tlevel lvl = _decision_level; lvl <= _decision_level; lvl--) {
    // pad the level with spaces
    string level_str = to_string(lvl);
    while(level_str.size() < max_n_digits_level)
      level_str = " " + level_str;
    cout << level_str + ": ";
    for (unsigned i = 0; i < _assignment_stack.size(); i++) {
      if (i == _n_propagated)
        cout << "| ";
      Tlit lit = _assignment_stack[i];
      if (lit_level(lit) == lvl)
        cout << lit_to_string(lit) << " ";
      else {
        cout << " ";
        for (unsigned j = 0; j < to_string(lit_to_int(lit)).size(); j++)
          cout << " ";
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

void napsat::gui::observer::print_variables()
{
#ifdef __unix__
  struct winsize size;
  ioctl(STDOUT_FILENO, TIOCGWINSZ, &size);
  TERMINAL_WIDTH = size.ws_col;
#endif
#ifdef _WIN32
  CONSOLE_SCREEN_BUFFER_INFO csbi;
  GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
  TERMINAL_WIDTH = csbi.srWindow.Right - csbi.srWindow.Left + 1;
#endif
  const unsigned MAX_VARS_PER_LINE = 30;
  unsigned current_position = 0;
  cout << "variables :\n";
  for (Tvar var = 0; var < _variables.size(); var++) {
    if (current_position + MAX_VARS_PER_LINE > TERMINAL_WIDTH) {
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

std::string napsat::gui::observer::literal_to_latex(Tlit lit, bool watched, bool blocked)
{
  string s = "";
  if (lit_value(lit) == VAR_FALSE)
    s += "\\red{";
  else if (lit_value(lit) == VAR_TRUE)
#if COLORBLIND_MODE
    s += "\\blue{";
#else
    s += "\\green{";
#endif
  if (!lit_pol(lit))
    s += "\\neg ";
  // if (lit_reason(lit) == CLAUSE_UNDEF && lit_value(lit) != VAR_UNDEF)
  //   s += "\\bm{v}";
  // else
  s += "v";
  s += "_{" + to_string(lit_to_var(lit)) + "}";
  if (lit_value(lit) == VAR_FALSE || lit_value(lit) == VAR_TRUE)
    s += "}";
  return s;
}

std::string napsat::gui::observer::literal_to_aligned_latex(Tlit lit, bool watched, bool blocked)
{
  string s = "";
  if (lit_value(lit) == VAR_FALSE)
    s += "\\red{";
  else if (lit_value(lit) == VAR_TRUE)
#if COLORBLIND_MODE
    s += "\\blue{";
#else
    s += "\\green{";
#endif
  if (!lit_pol(lit))
    s += "\\neg ";
  else
    s += "\\phantom{\\neg} ";

  if (watched)
    s += "\\underline{";
  if (blocked)
    s += "\\boxed{";
  s += "v";
  s += "_{" + to_string(lit_to_var(lit));

  unsigned n_digits = floor(log10(lit_to_var(lit)));
  unsigned max_digits = floor(log10(_variables.size()));
  if (n_digits < max_digits) {
    s += "\\phantom{";
    while (n_digits < max_digits) {
      s += "0";
      n_digits++;
    }
    s += "}";
  }

  s += "}";
  if (watched)
    s += "}";
  if (blocked)
    s += "}";
  if (lit_value(lit) == VAR_FALSE || lit_value(lit) == VAR_TRUE)
    s += "}";
  return s;
}

static const unsigned MAX_LITS_PER_LINE = 3;

std::string napsat::gui::observer::clause_to_latex(Tclause cl)
{
  if (cl == CLAUSE_UNDEF)
    return "decision";
  string s = "$";
  // If there are some watched literals, print those first
  assert(_active_clauses[cl]->watched.size() == 0 || _active_clauses[cl]->watched.size() == 2);
  bool printed = false;
  set<Tlit>& watched_lit = _active_clauses[cl]->watched;
  if (watched_lit.size() > 0) {
    Tlit first_watched = *watched_lit.begin();
    Tlit second_watched = *watched_lit.rbegin();
    if (lit_value(first_watched) == VAR_FALSE) {
      // swap the literals
      Tlit tmp = first_watched;
      first_watched = second_watched;
      second_watched = tmp;
    }
    s += literal_to_latex(first_watched, true, false);
    s += " \\lor ";
    s += literal_to_latex(second_watched, true, false);
    printed = true;
  }

  for (unsigned i = 0; i < _active_clauses[cl]->literals.size(); i++) {
    clause* cl_ptr = _active_clauses[cl];
    Tlit lit = cl_ptr->literals[i];
    if (_active_clauses[cl]->watched.find(lit) != _active_clauses[cl]->watched.end())
      continue;
    if (i > 0 || printed)
      s += "\\lor ";
    if (i == MAX_LITS_PER_LINE - 1 && i < _active_clauses[cl]->literals.size() - 1) {
      s += "\\red{\\dots}";
      break;
    }
    bool watched = cl_ptr->watched.find(lit) != cl_ptr->watched.end();
    bool blocked = cl_ptr->blocker == lit;
    s += literal_to_latex(lit, watched, blocked);
  }
  s += "$";
  return s;
}

std::string napsat::gui::observer::clause_to_aligned_latex(Tclause cl)
{
  if (cl == CLAUSE_UNDEF)
    return "decision";
  string s = "";
  // If there are some watched literals, print those first
  assert(_active_clauses[cl]->watched.size() == 0 || _active_clauses[cl]->watched.size() == 2);
  bool printed = false;
  set<Tlit>& watched_lit = _active_clauses[cl]->watched;

  if (watched_lit.size() > 0) {
    Tlit first_watched = *watched_lit.begin();
    Tlit second_watched = *watched_lit.rbegin();
    if (lit_value(first_watched) == VAR_FALSE) {
      // swap the literals
      Tlit tmp = first_watched;
      first_watched = second_watched;
      second_watched = tmp;
    }
    s += literal_to_aligned_latex(first_watched, true, false);
    s += " \\lor ";
    s += literal_to_aligned_latex(second_watched, true, false);
    printed = true;
  }

  for (unsigned i = 0; i < _active_clauses[cl]->literals.size(); i++) {
    if (find(watched_lit.begin(), watched_lit.end(), _active_clauses[cl]->literals[i]) != watched_lit.end())
      continue;
    if (i > 0 || printed)
      s += " \\lor ";
    clause* cl_ptr = _active_clauses[cl];
    Tlit lit = cl_ptr->literals[i];
    bool watched = cl_ptr->watched.find(lit) != cl_ptr->watched.end();
    bool blocked = cl_ptr->blocker == lit;
    s += literal_to_aligned_latex(lit, watched, blocked);
  }
  return s;
}

static inline std::string pair_to_latex(int a, double b)
{
  return "(" + std::to_string(a) + ", " + std::to_string(b) + ")";
}

std::string napsat::gui::observer::trail_to_latex()
{
  double spacing = 0.75;
  string s = "";

  unsigned x = 0;
  Tlevel y = 0;

  // print the literals
  for (Tlit lit : _assignment_stack) {
    Tlevel level = lit_level(lit);
    s += "\\draw[thick] (" + to_string(x) + ", 0) -- node[below, yshift = -0.1cm] {";
    s += "\\rotatebox{270}{";
    s += clause_to_latex(lit_reason(lit));
    s += "}} (" + to_string(x + 1) + ", 0);\n";
    if (y != level) {
      s += "\\draw[thick] " + pair_to_latex(x, y * spacing) + " -- " + pair_to_latex(x, level * spacing) + ";\n";
      y = level;
    }
    s += "\\draw[thick] " + pair_to_latex(x, y * spacing) + " -- ";
    if (lit_reason(lit) == CLAUSE_UNDEF)
      s += "node[below] {$\\delta = " + to_string(level) + "$} ";
    s += "node[above] {$";
    s += literal_to_latex(lit);
    s += "$} " + pair_to_latex(x + 1, y * spacing) + ";\n";
    x++;
  }

  // print the conflicts
  for (clause* cl : _active_clauses) {
    bool falsified = true;
    for (Tlit lit : cl->literals) {
      if (lit_value(lit) != VAR_FALSE) {
        falsified = false;
        break;
      }
    }
    if (!falsified)
      continue;
    Tlevel level = 0;
    for (Tlit lit : cl->literals) {
      if (lit_level(lit) > level)
        level = lit_level(lit);
    }
    // print in red
    s += "\\draw[thick, red] (" + to_string(x) + ", 0) -- node[below, yshift = -0.1cm] {";
    s += "\\rotatebox{270}{";
    s += clause_to_latex(cl->cl);
    s += "}} (" + to_string(x + 1) + ", 0);\n";
    if (y != level) {
      s += "\\draw[thick, red] " + pair_to_latex(x, y * spacing) + " -- " + pair_to_latex(x, level * spacing) + ";\n";
      y = level;
    }
    s += "\\draw[thick, red] " + pair_to_latex(x, y * spacing) + " -- ";
    s += "node[above, red] {$\\bot$} " + pair_to_latex(x + 1, y * spacing) + ";\n";
    x++;
  }

  s += "\n\\draw[thick, dotted] ";
  s += pair_to_latex(_n_propagated, -3) + " -- ";
  s += pair_to_latex(_n_propagated, (_decision_level + 1) * spacing + 0.5);
  s += " node[right, yshift=-0.2cm] {$\\q \\rightarrow$}";
  s += " node[left, yshift=-0.2cm] {$\\leftarrow \\trail$};\n\n";

  // print the ticks
  if (x > 0) {
    s += "\\foreach \\x in {0,1,...," + to_string(x) + "}\n";
    s += "  \\draw[thick] (\\x,3pt)--(\\x,-3pt);\n";
  }
  else {
    s += "\\draw[thick] (0,3pt)--(0,-3pt);\n";
  }
  return s;
}

std::string napsat::gui::observer::clause_set_to_latex()
{
  string s = "\\begin{tabular}{l}\n";
  for (Tclause cl = 0; cl < _active_clauses.size(); cl++) {
    if (_active_clauses[cl] == nullptr || !_active_clauses[cl]->active)
      continue;
    s += "  $C_{" + to_string(cl + 1);
    unsigned n_digits = floor(log10(cl + 1));
    unsigned max_digits = floor(log10(_active_clauses.size()));
    if (n_digits < max_digits) {
      s += "\\phantom{";
      while (n_digits < max_digits) {
        s += "0";
        n_digits++;
      }
      s += "}";
    }
    s += "} = ";
    s += clause_to_aligned_latex(cl) + "$\\\\\n";
  }
  s += "\\end{tabular}\n";
  return s;
}

std::string napsat::gui::observer::used_clauses_to_latex()
{
  cout << "used clauses to latex" << endl;
  string s = "\\begin{tabular}{l}\n";
  for (Tclause cl = 0; cl < _active_clauses.size(); cl++) {
    bool falsified = true;
    for (Tlit lit : _active_clauses[cl]->literals) {
      if (lit_value(lit) != VAR_FALSE) {
        falsified = false;
        break;
      }
    }
    bool propagating = false;
    for (Tlit lit : _active_clauses[cl]->literals) {
      if (lit_reason(lit) == cl) {
        propagating = true;
        break;
      }
    }
    if (propagating || falsified) {
      s += "  $C_{" + to_string(cl + 1);
      unsigned n_digits = floor(log10(cl + 1));
      unsigned max_digits = floor(log10(_active_clauses.size()));
      if (n_digits < max_digits) {
        s += "\\phantom{";
        while (n_digits < max_digits) {
          s += "0";
          n_digits++;
        }
        s += "}";
      }
      s += "} = ";
      s += clause_to_aligned_latex(cl) + "$\\\\\n";
    }
  }
  s += "\\end{tabular}\n";
  return s;
}

std::string napsat::gui::observer::implication_graph_to_latex()
{
  string s = "";
  s += "\\tikzstyle{vertex}=[draw,minimum size=24pt,inner sep=1pt]\n";
  s += "\\tikzstyle{propagated}=[circle]\n";
  s += "\\tikzstyle{decision}=[rectangle]\n";
  s += "\\tikzstyle{myarr}=[shorten >=1pt,->,>=stealth]\n";
  s += "\\tikzstyle{currentclause}=[fill=blue!15]\n";

  for (Tlevel lvl = 0; lvl <= _decision_level; lvl++) {
    unsigned x = 0;
    for (Tlit lit : _assignment_stack) {
      if (lit_level(lit) == lvl) {
        s += "\\node[vertex";
        if (lit_reason(lit) == CLAUSE_UNDEF)
          s += ", decision]";
        else
          s += ", propagated]";
        s += "(v" + to_string(lit_to_var(lit)) + ") at (";
        s += to_string(x) + ", " + to_string((-2) * (int) lvl) + ") {$";
        s += literal_to_latex(lit);
        s += "$};\n";
        x += 2;
      }
    }
  }
  s += "\n";
  for (unsigned i = 1; i < _assignment_stack.size(); i++) {
    Tlit lit = _assignment_stack[i];
    Tclause reason = lit_reason(lit);
    if (reason != CLAUSE_UNDEF) {
      for (unsigned j = 0; j < _active_clauses[reason]->literals.size(); j++) {
        Tlit lit2 = _active_clauses[reason]->literals[j];
        if (lit2 == lit)
          continue;
        // cout << "lit2: " << lit2 << endl;
        // cout << "lit: " << lit << endl;
        if (lit_level(lit2) != lit_level(lit) || lit_to_var(lit2) == lit_to_var(_assignment_stack[i - 1])) {
          s += "\\draw (v" + to_string(lit_to_var(lit2)) + ") edge[myarr] (v" + to_string(lit_to_var(lit)) + ");";
        }
        else {
          s += "\\draw (v" + to_string(lit_to_var(lit2)) + ") edge[myarr, bend right=30] (v" + to_string(lit_to_var(lit)) + ");";
        }
        s += "\n";
      }
    }
    reason = _variables[lit_to_var(lit)].lazy_reason;
    if (reason != CLAUSE_UNDEF) {
      for (unsigned j = 0; j < _active_clauses[reason]->literals.size(); j++) {
        Tlit lit2 = _active_clauses[reason]->literals[j];
        if (lit2 == lit)
          continue;
        // cout << "lit2: " << lit2 << endl;
        // cout << "lit: " << lit << endl;
        assert(lit_level(lit2) != lit_level(lit) || lit_to_var(lit2) == lit_to_var(_assignment_stack[i - 1]));
        s += "\\draw (v" + to_string(lit_to_var(lit2)) + ") edge[myarr, dashed] (v" + to_string(lit_to_var(lit)) + ");";
        s += "\n";
      }
    }
    s += "\n";
  }
  return s;
}

void napsat::gui::observer::save_state()
{
  if (!_recording)
    return;
  string filename = _options.save_folder + "/trail-" + to_string(file_number) + ".tex";
  string trail_latex = trail_to_latex();
  ofstream file(filename);
  if (!file.is_open()) {
    cout << "Could not open file " << filename << endl;
  }
  else {
    file << trail_latex;
    file.close();
  }
  filename = _options.save_folder + "/clauses-" + to_string(file_number) + ".tex";
  string clause_latex;
  if (_active_clauses.size() > 20)
    clause_latex = used_clauses_to_latex();
  else
    clause_latex = clause_set_to_latex();
  file.open(filename);
  if (!file.is_open()) {
    cout << "Could not open file " << filename << endl;
  }
  else {
    file << clause_latex;
    file.close();
  }
  file_number++;
}

napsat::gui::observer::~observer()
{
  for (auto notification : _notifications)
    delete notification;
  for (auto clause : _clauses_dict)
    delete clause.second;
  delete _display;
}
