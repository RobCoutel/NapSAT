/*
 * This file is part of the source code of the software program
 * NapSAT. It is protected by applicable copyright laws.
 *
 * This source code is protected by the terms of the MIT License.
 */
/**
 * @file src/solver/NapSAT-utils.cpp
 * @author Robin Coutelier
 *
 * @brief This file is part of the NapSAT solver. It implements auxiliary functions for the
 * SAT solver such as printing, parsing,...
 */
#include "NapSAT.hpp"

#include "custom-assert.hpp"
#include "../utils/printer.hpp"
#include "../utils/decoder.hpp"

#include <iostream>
#include <fstream>
#include <iomanip>
#include <algorithm>

#define ORANGE "\033[0;33m"
#define GREEN "\033[0;32m"
#define RED "\033[0;31m"

using namespace napsat;
using namespace std;

bool napsat::NapSAT::parse_dimacs(const char* filename)
{
  bool printed_warning = false;
  // the file is a compressed xz file
  // first decompress it and store it in a temporary file
  istringstream stream;
  if (string(filename).substr(string(filename).size() - 3) == ".xz") {
    // create a virtual file and read the content of the compressed file
    ostringstream decompressed_data;
    if (!decompress_xz(filename, decompressed_data)) {
      LOG_ERROR("The file " << filename << " could not be decompressed.");
      _status = ERROR;
      return false;
    }
    stream = istringstream(decompressed_data.str());
  }
  else {
    ifstream file = ifstream(filename);
    if (!file.is_open()) {
      LOG_ERROR("The file " << filename << " could not be opened.");
      _status = ERROR;
      return false;
    }
    stream = istringstream(string((istreambuf_iterator<char>(file)), istreambuf_iterator<char>()));
  }

  string line;
  while (getline(stream, line)) {
    while (line.size() > 0 && line[0] == ' ')
      line.erase(0, 1);
    if (line.size() == 0 || line[0] == '\n')
      continue;
    if (line[0] == 'c') {
      if (_observer && line.size() > 1 && line[1] == 'o') {
        // parse the alias name of the variable
        // the comment should be of the form:
        // >co <var> <alias>
        string alias = "";
        string var_string = "";
        unsigned i = 3;
        while (i < line.size() && line[i] != '=' && line[i] != ' ')
          var_string += line[i++];
        i++;
        while (i < line.size() && line[i] != ' ' && line[i] != '\n')
          alias += line[i++];
        if (alias != "" && var_string != "") {
          try {
            unsigned var = stoi(var_string);
            if (var >= _vars.size())
              var_allocate(var + 1);
            _observer->set_alias(var, alias);
          }
          catch (invalid_argument e) {
            if (!printed_warning) {
              LOG_WARNING("The comments starting with \'co\' are interpreted as aliases for variables. The format of the comment should be: \'co <var> <alias>\' with alias a string without spaces");
              printed_warning = true;
            }
            continue;
          }
        }
      }
      continue;
    }
    if (line[0] == '%')
      break;
    if (line.find("p cnf") == 0) {
      unsigned n_var, n_clauses;
      sscanf(line.c_str(), "p cnf %u %u", &n_var, &n_clauses);
      if (n_var > _vars.size()) {
        cout << "Allocating " << n_var << " variables" << endl;
        var_allocate(n_var);
      }
      // we ignore the number of clauses
      // TODO preallocated the clauses
      continue;
    }
    start_clause();

    string token = "";
    for (char c : line) {
      if (c == ' ' || c == '\n') {
        if (token == "")
          continue;
        try {
          int lit = stoi(token);
          if (lit == 0)
            break;
          add_literal(napsat::literal(abs(lit), lit > 0));
          token = "";
          continue;
        }
        catch (invalid_argument e) {
          LOG_ERROR("The token " << token << " is not a number.");
          _status = ERROR;
          throw invalid_argument("The token " + token + " is not a number.");
          return false;
        }
      }
      token += c;
    }
    finalize_clause();
    if (_status != UNDEF)
      return true;
  }
  return true;
}

void napsat::NapSAT::bump_var_activity(Tvar var)
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

void napsat::NapSAT::bump_clause_activity(Tclause cl)
{
  _activities[cl] += _clause_activity_increment;
  _clause_activity_increment *= _options.clause_activity_multiplier;
  _max_clause_activity += _clause_activity_increment;
  if (_max_clause_activity > 1e100) {
    for (Tclause i = 0; i < _clauses.size(); i++)
      _activities[i] *= 1e-100;
    _clause_activity_increment *= 1e-100;
    _max_clause_activity *= 1e-100;
  }
}

void napsat::NapSAT::delete_clause(Tclause cl)
{
  // If the clause is the reason for a literal, it cannot be deleted
  ASSERT(cl < _clauses.size());
  TSclause &clause = _clauses[cl];
  ASSERT(!is_protected(cl));
  _n_learned_clauses -= _clauses[cl].learned;
  clause.deleted = true;
  clause.watched = false;
  _deleted_clauses.push_back(cl);
  NOTIFY_OBSERVER(_observer, new napsat::gui::delete_clause(cl));
  if(_proof)
    _proof->deactivate_clause(cl);
}

static const char esc_char = 27; // the decimal code for escape character is 27

void napsat::NapSAT::watch_lit(Tlit lit, Tclause cl)
{
#if NOTIFY_WATCH_CHANGES
  NOTIFY_OBSERVER(_observer, new napsat::gui::watch(cl, lit));
#endif
  ASSERT(cl != CLAUSE_UNDEF);
  ASSERT(cl < _clauses.size());
  ASSERT(_clauses[cl].size > 2);
  ASSERT(lit == _clauses[cl].lits[0] || lit == _clauses[cl].lits[1]);
  _watch_lists[lit].push_back(cl);
}

void napsat::NapSAT::stop_watch(Tlit lit, Tclause cl)
{
#if NOTIFY_WATCH_CHANGES
  NOTIFY_OBSERVER(_observer, new napsat::gui::unwatch(cl, lit));
#endif
  ASSERT(cl != CLAUSE_UNDEF);
  ASSERT(_clauses[cl].lits[0] == lit || _clauses[cl].lits[1] == lit);
  ASSERT(_clauses[cl].size > 2);
  auto location = find(_watch_lists[lit].begin(), _watch_lists[lit].end(), cl);
  ASSERT(location != _watch_lists[lit].end());
  _watch_lists[lit].erase(location);
}

unsigned napsat::NapSAT::utility_heuristic(Tlit lit)
{
  return (lit_true(lit) * (2 * solver_level() - lit_level(lit) + 1)) + (lit_undef(lit) * (solver_level() + 1)) + (lit_false(lit) * (lit_level(lit)));
}

void napsat::NapSAT::print_lit(Tlit lit)
{
  if (lit_seen(lit))
    cout << "M";
  if (lit_undef(lit))
    cout << ORANGE;
  else if (lit_true(lit))
    cout << GREEN;
  else { // lit_false(lit)
    ASSERT(lit_false(lit));
    cout << RED;
  }
  if (!lit_undef(lit) && lit_reason(lit) == CLAUSE_UNDEF)
    cout << "\033[4m";
  if (!lit_pol(lit))
    cout << "-";
  cout << lit_to_var(lit);

  cout << esc_char << "[0m";
  cout << "\033[0m";
}

string NapSAT::lit_to_string(Tlit lit)
{
  string s = "";
  if (lit_seen(lit))
    s += "M";
  if (lit_undef(lit))
    s += ORANGE;
  else if (lit_true(lit))
    s += GREEN;
  else { // lit_false(lit)
    ASSERT(lit_false(lit));
    s += RED;
  }
  if (!lit_undef(lit) && lit_reason(lit) == CLAUSE_UNDEF)
    s += "\033[4m";
  if (!lit_pol(lit))
    s += "-";
  s += to_string(lit_to_var(lit));

  s += "\033[0m";
  return s;
}

string NapSAT::clause_to_string(Tclause cl)
{
  string s = "";
  if (cl == CLAUSE_UNDEF)
    return "undef";

  if (_clauses[cl].deleted) {
    s += "d";
  }
  s += to_string(cl) + ": ";
  for (Tlit* i = _clauses[cl].lits; i < _clauses[cl].lits + _clauses_sizes[cl]; i++) {
    if (i == _clauses[cl].lits + _clauses[cl].size)
      s += "| ";
    if (*i == _clauses[cl].blocker)
      s += "\033[3mb";
    s += lit_to_string(*i);
    if (*i == _clauses[cl].blocker)
      s += "\033[0m";
    s += " ";
  }
  return s;
}

void NapSAT::print_clause(Tclause cl)
{
  cout << clause_to_string(cl);
}

void NapSAT::print_trail()
{
  cout << "trail: " << _propagated_literals << " - " << _trail.size() - _propagated_literals << "\n";
  for (unsigned int i = 0; i < _trail.size(); i++) {
    Tlit lit = _trail[i];
    if (i == _propagated_literals) {
      cout << "-------- waiting queue --------\n";
    }
    ASSERT(!lit_undef(lit));
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

void napsat::NapSAT::print_trail_simple()
{
  cout << "trail :\n";
  for (Tlevel lvl = solver_level(); lvl <= solver_level(); lvl--) {
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

void napsat::NapSAT::print_clause_set()
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

      ASSERT(string_length_escaped(clause_str) <= max_clause_width);
      for (unsigned k = 0; k < max_clause_width - string_length_escaped(clause_str); k++)
        spaces += " ";
      cout << spaces;
      j += max_clause_width;
    }
    cout << endl;
  }
}

void napsat::NapSAT::print_watch_lists(Tlit lit)
{
  Tlit i = 1;
  Tlit end = _watch_lists.size();
  if (lit != LIT_UNDEF) {
    i = lit;
    end = lit + 1;
  }
  for (; i < end; i++) {
    cout << "watch list for ";
    if (lit_pol(i))
      cout << " ";
    print_lit(i);
    cout << ": ";
    // print the binary list
    cout << "binary: ";
    for (pair<Tlit, Tclause> p : _binary_clauses[i]) {
      print_lit(p.first);
      cout << " <- " << p.second << " ";
    }
    cout << "\n                non-binary: ";

    for (Tclause cl : _watch_lists[i])
      cout << cl << " ";
    cout << "\n";
  }
}

bool napsat::NapSAT::parse_command(std::string input)
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
    LOG_WARNING("Empty command. Try \"HELP\" to get the list of commands.");
    return false;
  }
  if (tokens[0] == "DECIDE") {
    if (tokens.size() == 1)
      decide();
    else if (tokens.size() == 2) {
      int lit_int = stoi(tokens[1]);
      Tlit lit = literal(abs(lit_int), lit_int > 0);
      if (!lit_undef(lit)) {
        LOG_WARNING("The literal " << lit_to_string(lit) << " is not undefined. This command is ignored.");
        return false;
      }
      decide((lit));
    }
    else {
      LOG_WARNING("Wrong number of arguments (expected 0 or 1). This command is ignored.");
      return false;
    }
  }
  else if (tokens[0] == "HINT") {
    if (tokens.size() == 2) {
      int lit = stoi(tokens[1]);
      if (!lit_undef(lit)) {
        LOG_WARNING("The literal " << lit_to_string(lit) << " is not undefined. This command is ignored.");
        return false;
      }
      hint(literal(abs(lit), lit > 0));
    }
    else if (tokens.size() == 3) {
      int lit = stoi(tokens[1]);
      if (!lit_undef(lit)) {
        LOG_WARNING("The literal " << lit_to_string(lit) << " is not undefined. This command is ignored.");
        return false;
      }
      Tlevel level = stoi(tokens[2]);
      hint(literal(abs(lit), lit > 0), level);
    }
    else {
      LOG_WARNING("Wrong number of arguments (expected 0 or 1). This command is ignored.");
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
        LOG_WARNING("unknown argument \"" << tokens[1] << "\"");
    }
    else
      LOG_WARNING("Wrong number of arguments (expected 1). This command is ignored.");
  }
  else if (tokens[0] == "DELETE_CLAUSE") {
    if (tokens.size() != 2) {
      LOG_WARNING("Wrong number of arguments (expected 1). This command is ignored.");
      return false;
    }
    int cl = stoi(tokens[1]);
    if (cl < 0 || (unsigned) cl >= _clauses.size()) {
      LOG_WARNING("The clause " << cl << " does not exist. This command is ignored.");
      return false;
    }
    if (_clauses[cl].deleted) {
      LOG_WARNING("The clause " << cl << " is already deleted\n");
      return false;
    }
    delete_clause(cl);
  }
  else if (tokens[0] == "HELP") {
    // print the content of the help file
    string man_file = napsat::env::get_man_page_folder() + "man-sat.txt";
    ifstream file(man_file);
    if (file.is_open()) {
      string line;
      while (getline(file, line))
        cout << line << endl;
      file.close();
    }
    else {
      LOG_ERROR("The manual page could not be loaded.");
    }
  }
  else {
    LOG_WARNING("unknown command \"" << tokens[0] << "\"; try \"HELP\" to get the list of commands");
    return false;
  }
  return true;
}
