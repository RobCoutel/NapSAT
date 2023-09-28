#include "modulariT-SAT.hpp"

#include <iostream>
#include <fstream>
#include <cassert>

using namespace sat;
using namespace std;

void sat::modulariT_SAT::parse_dimacs(const char *filename)
{
  ifstream file(filename);
  if (!file.is_open())
  {
    cout << "Error: could not open file " << filename << endl;
    return;
  }

  string line;
  while (getline(file, line))
  {
    while(line.size() > 0 && line[0] == ' ')
      line.erase(0, 1);
    if (line.size() == 0 || line[0] == 'c' || line[0] == 'p'|| line[0] == '\n')
      continue;
    if (line[0] == '%')
      break;
    start_clause();

    string token = "";
    for (char c : line)
    {
      if (c == ' ' || c == '\n')
      {
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
  if (_vars.at(var).activity > 1e100)
  {
    for (Tvar i = 1; i < _vars.size(); i++)
    {
      _vars.at(i).activity *= 1e-100;
    }
    _variable_heap.normalize(1e-100);
    _var_activity_increment *= 1e-100;
  }
  if(_variable_heap.contains(var))
    _variable_heap.increase_activity(var, _vars.at(var).activity);
}

void sat::modulariT_SAT::bump_clause_activity(Tclause cl)
{
  _clauses.at(cl).activity += _clause_activity_increment;
  _clause_activity_increment *= _clause_activity_multiplier;
  _max_clause_activity += _clause_activity_increment;
  if (_max_clause_activity > 1e100)
  {
    for (Tclause i = 0; i < _clauses.size(); i++)
    {
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
}

static const char esc_char = 27; // the decimal code for escape character is 27

void sat::modulariT_SAT::repair_watch_lists()
{
  for (Tlit lit = 2; lit<_watch_lists.size(); lit++)
  {
    unsigned j = 0;
    for (unsigned i = 0; i < _watch_lists[lit].size(); i++)
    {
      Tclause cl = _watch_lists[lit][i];
      assert(cl < _clauses.size());
      TSclause &clause = _clauses[cl];
      if (clause.deleted || !clause.watched
       || clause.size < 2
       || (clause.lits[0] != lit && clause.lits[1] != lit))
      {
        continue;
      }
      _watch_lists[lit][j++] = cl;
    }
    _watch_lists[lit].resize(j);
  }
}

unsigned sat::modulariT_SAT::utility_heuristic(Tlit lit)
{
  return (lit_true(lit) * (2 * _decision_index.size() - lit_level(lit) + 1))
       + (lit_undef(lit) * _decision_index.size())
       + (lit_false(lit) * (lit_level(lit)));
}

void sat::modulariT_SAT::print_lit(Tlit lit)
{
  if (lit_seen(lit))
    cout << "M";
  if (lit_undef(lit))
    cout << "\033[0;33m";
  else if (lit_true(lit))
    cout << "\033[0;32m";
  else
  { // lit_false(lit)
    assert(lit_false(lit));
    cout << "\033[0;31m";
  }
  if (!lit_undef(lit) && lit_reason(lit) == SAT_CLAUSE_UNDEF)
    cout << esc_char << "[4m";
  if (!lit_pol(lit))
    cout << "-";
  cout << lit_to_var(lit);

  cout << esc_char << "[0m";
  cout << "\033[0m";
}

void modulariT_SAT::print_clause(Tclause cl)
{
  if (cl == SAT_CLAUSE_UNDEF)
  {
    cout << "undef";
    return;
  }
  if (_clauses[cl].watched)
  {
    cout << "w";
  }
  if (_clauses[cl].deleted)
  {
    cout << "d";
  }
  cout << cl << ": ";
  for (Tlit *i = _clauses[cl].lits; i < _clauses[cl].lits + _clauses[cl].original_size; i++)
  {
    if (i == _clauses[cl].lits + _clauses[cl].size)
      cout << "| ";
    print_lit(*i);
    cout << " ";
  }
}

void modulariT_SAT::print_trail()
{
  cout << "trail: " << _propagated_literals << " - " << _trail.size() - _propagated_literals << "\n";
  for (unsigned int i = 0; i < _trail.size(); i++)
  {
    Tlit lit = _trail[i];
    if (i == _propagated_literals)
    {
      cout << "-------- waiting queue --------\n";
    }
    assert(!lit_undef(lit));
    cout << lit_level(lit) << ": ";
    for (Tlevel i = 0; i < lit_level(lit); i++)
    {
      cout << " ";
    }
    print_lit(lit);
    cout << " --> reason: ";
    print_clause(lit_reason(lit));
    cout << "\n";
  }
  cout << "\n";
}

void sat::modulariT_SAT::print_clause_set()
{
  Tclause i = 0;
  while (i < _clauses.size())
  {
    print_clause(i++);
    cout << "\n";
  }
}

void sat::modulariT_SAT::print_watch_lists()
{
  for (unsigned int i = 0; i < _watch_lists.size(); i++)
  {
    cout << "watch list for ";
    print_lit(i);
    cout << ": ";
    for (Tclause cl : _watch_lists[i])
      cout << cl << " ";
    cout << "\n";
  }
}