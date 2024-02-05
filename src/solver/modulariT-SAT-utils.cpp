#include "modulariT-SAT.hpp"

#include <iostream>
#include <fstream>
#include <cassert>
#include <iomanip>

using namespace sat;
using namespace std;

void sat::modulariT_SAT::parse_dimacs(const char* filename)
{
  ifstream file(filename);
  if (!file.is_open()) {
    cout << "Error: could not open file " << filename << endl;
    return;
  }

  string line;
  while (getline(file, line)) {
    while (line.size() > 0 && line[0] == ' ')
      line.erase(0, 1);
    if (line.size() == 0 || line[0] == 'c' || line[0] == 'p' || line[0] == '\n')
      continue;
    if (line[0] == '%')
      break;
    start_clause();

    string token = "";
    for (char c : line) {
      if (c == ' ' || c == '\n') {
        if (token == "")
          continue;
        int lit = stoi(token);
        if (lit == 0)
          break;
        add_literal(sat::literal(abs(lit), lit > 0));
        token = "";
      }
      else
        token += c;
    }
    finalize_clause();
  }
}

void sat::modulariT_SAT::bump_var_activity(Tvar var)
{
  _vars.at(var).activity += _var_activity_increment;
  if (_vars.at(var).activity > 1e100) {
    for (Tvar i = 1; i < _vars.size(); i++) {
      _vars.at(i).activity *= 1e-100;
    }
    _variable_heap.normalize(1e-100);
    _var_activity_increment *= 1e-100;
  }
  if (_variable_heap.contains(var))
    _variable_heap.increase_activity(var, _vars.at(var).activity);
}

void sat::modulariT_SAT::bump_clause_activity(Tclause cl)
{
  _clauses.at(cl).activity += _clause_activity_increment;
  _clause_activity_increment *= _options.clause_activity_multiplier;
  _max_clause_activity += _clause_activity_increment;
  if (_max_clause_activity > 1e100) {
    for (Tclause i = 0; i < _clauses.size(); i++) {
      _clauses.at(i).activity *= 1e-100;
    }
    _clause_activity_increment *= 1e-100;
    _max_clause_activity *= 1e-100;
  }
}

void sat::modulariT_SAT::delete_clause(Tclause cl)
{
  _n_learned_clauses -= _clauses[cl].learned;
  _clauses[cl].deleted = true;
  _clauses[cl].watched = false;
  _deleted_clauses.push_back(cl);
  notify_obs(new sat::gui::delete_clause(cl));
}

static const char esc_char = 27; // the decimal code for escape character is 27

void sat::modulariT_SAT::repair_watch_lists()
{
  for (Tlit lit = 2; lit < _watch_lists.size(); lit++) {
    unsigned j = 0;
    for (unsigned i = 0; i < _watch_lists[lit].size(); i++) {
      Tclause cl = _watch_lists[lit][i];
      assert(cl < _clauses.size());
      TSclause& clause = _clauses[cl];
      if (clause.deleted || !clause.watched || clause.size < 2 || (clause.lits[0] != lit && clause.lits[1] != lit)) {
#if NOTIFY_WATCH_CHANGES
        notify_obs(new sat::gui::unwatch(cl, lit));
#endif
        continue;
      }
      _watch_lists[lit][j++] = cl;
    }
    _watch_lists[lit].resize(j);
  }
}

unsigned sat::modulariT_SAT::utility_heuristic(Tlit lit)
{
  return (lit_true(lit) * (2 * _decision_index.size() - lit_level(lit) + 1)) + (lit_undef(lit) * (_decision_index.size() + 1)) + (lit_false(lit) * (lit_level(lit)));
}

void sat::modulariT_SAT::print_lit(Tlit lit)
{
  if (lit_seen(lit))
    cout << "M";
  if (lit_undef(lit))
    cout << "\033[0;33m";
  else if (lit_true(lit))
    cout << "\033[0;32m";
  else { // lit_false(lit)
    assert(lit_false(lit));
    cout << "\033[0;31m";
  }
  if (!lit_undef(lit) && lit_reason(lit) == CLAUSE_UNDEF)
    cout << esc_char << "[4m";
  if (!lit_pol(lit))
    cout << "-";
  cout << lit_to_var(lit);

  cout << esc_char << "[0m";
  cout << "\033[0m";
}

string modulariT_SAT::literal_to_string(Tlit lit)
{
  string s = "";
  if (lit_seen(lit))
    s += "M";
  if (lit_undef(lit))
    s += "\033[0;33m";
  else if (lit_true(lit))
    s += "\033[0;32m";
  else { // lit_false(lit)
    assert(lit_false(lit));
    s += "\033[0;31m";
  }
  if (!lit_undef(lit) && lit_reason(lit) == CLAUSE_UNDEF) {
    s += esc_char;
    s += "[4m";
  }
  if (!lit_pol(lit))
    s += "-";
  s += to_string(lit_to_var(lit));

  s += esc_char;
  s += "[0m";
  s += "\033[0m";
  return s;
}

string modulariT_SAT::clause_to_string(Tclause cl)
{
  string s = "";
  if (cl == CLAUSE_UNDEF)
    return "undef";
  // if (_clauses[cl].watched)
  // {
  //   s += "w";
  // }
  // if (_clauses[cl].deleted)
  // {
  //   s += "d";
  // }
  s += to_string(cl) + ": ";
  for (Tlit* i = _clauses[cl].lits; i < _clauses[cl].lits + _clauses[cl].original_size; i++) {
    if (i == _clauses[cl].lits + _clauses[cl].size)
      s += "| ";
    s += literal_to_string(*i);
    s += " ";
  }
  return s;
}

/**
 * @brief Returns the length of the string when escaped.
 */
static unsigned string_length_escaped(string str)
{
  unsigned length = 0;
  for (unsigned i = 0; i < str.length(); i++) {
    if (str[i] == esc_char) {
      while (str[i] != 'm')
        i++;
      continue;
    }
    length++;
  }
  return length;
}

void modulariT_SAT::print_clause(Tclause cl)
{
  if (cl == CLAUSE_UNDEF) {
    cout << "undef";
    return;
  }
  // if (_clauses[cl].watched) {
  //   cout << "w";
  // }
  if (_clauses[cl].deleted) {
    cout << "d";
  }
  cout << cl << ": ";
  for (Tlit* i = _clauses[cl].lits; i < _clauses[cl].lits + _clauses[cl].original_size; i++) {
    if (i == _clauses[cl].lits + _clauses[cl].size)
      cout << "| ";
    print_lit(*i);
    cout << " ";
  }
}

void modulariT_SAT::print_trail()
{
  cout << "trail: " << _propagated_literals << " - " << _trail.size() - _propagated_literals << "\n";
  for (unsigned int i = 0; i < _trail.size(); i++) {
    Tlit lit = _trail[i];
    if (i == _propagated_literals) {
      cout << "-------- waiting queue --------\n";
    }
    assert(!lit_undef(lit));
    cout << lit_level(lit) << ": ";
    for (Tlevel i = 0; i < lit_level(lit); i++) {
      cout << " ";
    }
    print_lit(lit);
    cout << " --> reason: ";
    print_clause(lit_reason(lit));
    cout << "\n";
  }
  cout << "\n";
}

/**
 * @brief Adds spaces to the left of the number to make it have as many digits as the maximum number of digits in the given range.
 */
static void pad(int n, int max_int)
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
  for (int i = 0; i < max_digits - digits; i++)
    cout << " ";
}

void sat::modulariT_SAT::print_trail_simple()
{
  cout << "trail :\n";
  for (Tlevel lvl = _decision_index.size(); lvl <= _decision_index.size(); lvl--) {
    cout << lvl << ": ";
    for (unsigned i = 0; i < _trail.size(); i++) {
      if (i == _propagated_literals)
        cout << "| ";
      Tlit lit = _trail[i];
      if (lit_level(lit) == lvl) {
        if (lit_pol(lit))
          cout << " ";
        pad(lit_to_var(lit), _vars.size());
        print_lit(lit);
        cout << " ";
      }
      else {
        pad(0, _vars.size());
        cout << "  ";
      }
    }
    cout << "\n";
  }
}

const static unsigned TERMINAL_WIDTH = 120;

void sat::modulariT_SAT::print_clause_set()
{
  unsigned longest_clause = 0;
  for (Tclause cl = 0; cl < _clauses.size(); cl++) {
    if (_clauses[cl].deleted)
      continue;
    if (_clauses[cl].size > longest_clause)
      longest_clause = _clauses[cl].size;
  }
  unsigned longest_var = 1; // 1 for the sign
  Tvar max_var = _vars.size();
  while (max_var > 0) {
    max_var /= 10;
    longest_var++;
  }

  unsigned max_clause_width = (longest_clause + 2) * (longest_var + 1) + 3;
  cout << "max_clause_width = " << max_clause_width << endl;
  Tclause i = 0;
  while (i < _clauses.size()) {
    unsigned j = max_clause_width;
    while (j < TERMINAL_WIDTH && i < _clauses.size()) {
      if (_clauses[i].deleted) {
        i++;
        continue;
      }
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
}

void sat::modulariT_SAT::print_watch_lists()
{
  for (unsigned int i = 0; i < _watch_lists.size(); i++) {
    cout << "watch list for ";
    print_lit(i);
    cout << ": ";
    for (Tclause cl : _watch_lists[i])
      cout << cl << " ";
    cout << "\n";
  }
}

bool sat::modulariT_SAT::parse_command(std::string input)
{
  if (input == "") {
    decide();
    return true;
  }
  string tmp = "";
  vector<string> tokens;

  tokens.clear();
  tmp = "";
  for (unsigned i = 0; i < input.length(); i++) {
    char c = input[i];
    if (c == ' ' || c == '\n' || c == '\0' || c == '\r' || c == '\t') {
      if (tmp != "") {
        tokens.push_back(tmp);
        tmp = "";
      }
    }
    else {
      tmp += c;
    }
  }
  if (tmp != "") {
    tokens.push_back(tmp);
  }
  if (tokens.size() == 0) {
    cout << "Error: empty command\n";
    return false;
  }
  if (tokens[0] == "DECIDE") {
    if (tokens.size() == 1)
      decide();
    else if (tokens.size() == 2) {
      int lit_int = stoi(tokens[1]);
      Tlit lit = literal(abs(lit_int), lit_int > 0);
      if (!lit_undef(lit)) {
        cout << "Error: literal " << lit << " is not undefined\n";
        return false;
      }
      decide((lit));
    }
    else {
      cout << "Error: too many arguments (expected 0 or 1)\n";
      return false;
    }
  }
  else if (tokens[0] == "HINT") {
    if (tokens.size() == 2) {
      int lit = stoi(tokens[1]);
      if (!lit_undef(lit)) {
        cout << "Error: literal " << lit << " is not undefined\n";
        return false;
      }
      hint(literal(abs(lit), lit > 0));
    }
    else if (tokens.size() == 3) {
      int lit = stoi(tokens[1]);
      if (!lit_undef(lit)) {
        cout << "Error: literal " << lit << " is not undefined\n";
        return false;
      }
      Tlevel level = stoi(tokens[2]);
      hint(literal(abs(lit), lit > 0), level);
    }
    else {
      cout << "Error: wrong number of arguments (expected 1 or 2)\n";
      return false;
    }
  }
  else if (tokens[0] == "LEARN") {
    start_clause();
    for (unsigned i = 1; i < tokens.size(); i++) {
      int lit = stoi(tokens[i]);
      add_literal(literal(abs(lit), lit > 0));
    }
    finalize_clause();
  }
  else if (tokens[0] == "EXIT") {
    exit(0);
  }
  else if (tokens[0] == "PRINT") {
    if (tokens.size() == 2) {
      if (tokens[1] == "trail")
        print_trail();
      else if (tokens[1] == "trail-simple")
        print_trail_simple();
      else if (tokens[1] == "clause-set")
        print_clause_set();
      else if (tokens[1] == "watch-lists")
        print_watch_lists();
      else
        cout << "Error: unknown argument \"" << tokens[1] << "\"\n";
    }
    else
      cout << "Error: wrong number of arguments (expected 1)\n";
  }
  else if (tokens[0] == "DELETE_CLAUSE") {
    if (tokens.size() != 2) {
      cout << "Error: wrong number of arguments (expected 1)\n";
      return false;
    }
    int cl = stoi(tokens[1]);
    if (cl < 0 || (unsigned) cl >= _clauses.size()) {
      cout << "Error: clause " << cl << " does not exist\n";
      return false;
    }
    if (_clauses[cl].deleted) {
      cout << "Error: clause " << cl << " is already deleted\n";
      return false;
    }
    delete_clause(cl);
  }
  // else if (tokens[0] == "DELETE_VARIABLE")
  // {
  //   if (tokens.size() != 2) {
  //     cout << "Error: wrong number of arguments (expected 1)\n";
  //     return false;
  //   }
  //   int var = stoi(tokens[1]);
  //   if (var < 0 || var >= _vars.size()) {
  //     cout << "Error: variable " << var << " does not exist\n";
  //     return false;
  //   }
  //   if (!_vars[var].active) {
  //     cout << "Error: variable " << var << " is already deleted\n";
  //     return false;
  //   }
  // }
  // else if (tokens[0] == "COMMAND")
  // {
  //   if (tokens.size() == 2) {
  //     // open the the file provided as second argument and store the commands in a vector

  //     ifstream file(tokens[1]);
  //     if (!file.is_open()) {
  //       cout << "Error: could not open file \"" << tokens[1] << "\"\n";
  //       return false;
  //     }
  //     string line;
  //     while (getline(file, line)) {
  //       commands.push_back(line);
  //     }
  //     file.close();
  //   }
  // }
  else if (tokens[0] == "HELP") {
    cout << "DECIDE [lit]: decide the value of a literal\n";
    cout << "HINT <lit> [level]: hint the value of a literal\n";
    cout << "LEARN <lit1> [lit2] [...] [litn]: learn a new clause\n";
    cout << "PRINT <trail/clause-set/watch-lists>: print the trail, the clause set or the watch lists\n";
    // cout << "COMMAND <file>: read commands from a file\n";
    cout << "HELP: print this help\n";
    cout << "EXIT: exit the program\n";
  }
  else {
    cout << "Error: unknown command \"" << tokens[0] << "\"; try \"HELP\" to get the list of commands\n";
    return false;
  }
  return true;
}
