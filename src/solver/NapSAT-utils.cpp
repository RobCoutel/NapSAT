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

static inline void ltrim(string &s) {
  s.erase(s.begin(), find_if(s.begin(), s.end(), [](unsigned char ch) {
    return !isspace(ch);
  }));
}

static inline bool is_prefix(const string &s, const string &prefix) {
  return s.compare(0, prefix.size(), prefix) == 0;
}

bool NapSAT::parse_dimacs(const char* filename)
{
#if USE_OBSERVER
  bool printed_warning = false;
#endif
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
  } else if (string(filename).substr(string(filename).size() - 4) == ".bz2") {
    // create a virtual file and read the content of the compressed file
    ostringstream decompressed_data;
    if (!decompress_bz2(filename, decompressed_data)) {
      LOG_ERROR("The file " << filename << " could not be decompressed.");
      _status = ERROR;
      return false;
    }
    stream = istringstream(decompressed_data.str());
  } else {
    ifstream file = ifstream(filename);
    if (!file.is_open()) {
      LOG_ERROR("The file " << filename << " could not be opened.");
      _status = ERROR;
      return false;
    }
    stream = istringstream(string((istreambuf_iterator<char>(file)), istreambuf_iterator<char>()));
  }

  string line;
  bool first_line = true;
  while (getline(stream, line)) {
    ltrim(line);
    if (line.empty())
      continue;
#if USE_OBSERVER
    if (_observer && is_prefix(line, "co")) {
      // parse the alias name of the variable
      // the comment should be of the form:
      // >co <var> <alias>
      istringstream ss(line);
      string alias, var_string, rest;

      // does not work with `=` anymore
      ss.ignore(2);
      ss >> var_string;
      ss >> alias;
      ss >> rest;

      if (!var_string.empty() && !alias.empty() && rest.empty()) {
        try {
          unsigned var = stoi(var_string);
          if (var >= _vars.size())
            var_allocate(var + 1);
          _observer->set_alias(var, alias);
          // done, next line
          continue;
        }
        catch (invalid_argument &e) {
          // fall through printing the warning
        }
      }
      if (!printed_warning) {
        LOG_WARNING("The comments starting with \'co\' are interpreted as aliases for variables. The format of the comment should be: \'co <var> <alias>\' with alias a string without spaces");
        printed_warning = true;
      }
      // treat it as a regular comment, continue
      continue;
    }
#endif
    if (is_prefix(line, "c"))
      continue;
    if (is_prefix(line, "%"))
      break;
    if (is_prefix(line, "p cnf")) {
      if (first_line) {
        unsigned n_var, n_clauses;
        sscanf(line.c_str(), "p cnf %u %u", &n_var, &n_clauses);
        if (n_var > _vars.size()) {
          LOG_INFO("Allocating " << n_var << " variables and " << n_clauses << " clauses.");
          var_allocate(n_var);
        }
      } else {
        LOG_ERROR("Misplaced string \'p cnf\' found. Must be the first non-comment line.");
      }
      continue;
    }
    first_line = false;

    string token;
    istringstream ss(line);
    start_clause();
    while(getline(ss, token, ' ')) {
      if (token.empty())
        continue;
      try {
        int lit = stoi(token);
        if (lit == 0)
          break;
        add_literal(literal(abs(lit), lit > 0));
      } catch (invalid_argument &e) {
        LOG_ERROR("The token " << token << " is not a number.");
        _status = ERROR;
        throw invalid_argument("The token " + token + " is not a number.");
        return false;
      }
    }
    finalize_clause();
    if (_status != UNKNOWN)
      return true;
  }
  return true;
}

void NapSAT::bump_var_activity(Tvar var)
{
  assert(var < _vars.size());
  TSvar& svar = _vars[var];
  svar.activity += _var_activity_increment;
  if (svar.activity > 1e100) {
    for (Tvar i = 1; i < _vars.size(); i++) {
      _vars[i].activity *= 1e-100;
    }
    _variable_heap.normalize(1e-100);
    _var_activity_increment *= 1e-100;
  }
  if (_variable_heap.contains(var)) {
    _variable_heap.increase_activity(var, svar.activity);
  }
}

void NapSAT::bump_clause_activity(Tclause cl)
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

static const char esc_char = 27; // the decimal code for escape character is 27

void NapSAT::watch_lit(Tlit lit, Tclause cl)
{
  const Tlit* lits = clause_lits(cl);
  ASSERT(cl != CLAUSE_UNDEF);
  ASSERT(cl < _clauses.size());
  ASSERT(clause_size(cl) > 2);
  ASSERT(lit == lits[0] || lit == lits[1]);
  _watches[lit].push_back(TSwatch(cl, lits[0] ^ lits[1] ^ lit));
  #if NOTIFY_WATCH_CHANGES
    NOTIFY_OBSERVER(watch, cl, lit);
    NOTIFY_OBSERVER(block, cl, lits[0] ^ lits[1] ^ lit, lit);
  #endif
}

void NapSAT::watch_lit_bin(Tclause cl)
{
  const Tlit* lits = clause_lits(cl);
  ASSERT(cl != CLAUSE_UNDEF);
  ASSERT(cl < _clauses.size());
  ASSERT(clause_size(cl) == 2);
  _binary_watches[lits[0]].push_back(TSwatch(cl, lits[1]));
  _binary_watches[lits[1]].push_back(TSwatch(cl, lits[0]));
  #if NOTIFY_WATCH_CHANGES
    NOTIFY_OBSERVER(watch, cl, lits[0]);
    NOTIFY_OBSERVER(block, cl, lits[1], lits[0]);
    NOTIFY_OBSERVER(watch, cl, lits[1]);
    NOTIFY_OBSERVER(block, cl, lits[0], lits[1]);
  #endif
}

void NapSAT::stop_watch(Tlit lit, Tclause cl)
{
#if NOTIFY_WATCH_CHANGES
  NOTIFY_OBSERVER(unwatch, cl, lit);
#endif
  ASSERT(cl != CLAUSE_UNDEF);
  ASSERT(clause_lits(cl)[0] == lit || clause_lits(cl)[1] == lit);
  ASSERT(clause_size(cl) > 2);
  size_t loc = 0;
  while (loc < _watches[lit].size() && _watches[lit][loc].cl != cl) {
    loc++;
  }
  ASSERT(loc < _watches[lit].size());
  _watches[lit].erase(_watches[lit].begin() + loc);
}

unsigned NapSAT::cleanup_duplicate_literals(Tlit* lits, unsigned size)
{
  ASSERT(all_of(lits, lits + size, [this](Tlit lit) { return !lit_marked(lit); }));
  Tlit* i = lits;
  Tlit* j = i;
  Tlit* end = i + size;
  while(i < end) {
    if (lit_marked(*i)) {
      i++;
      continue;
    }
    lit_mark(*i);
    *j++ = *i++;
  }
  unsigned new_size = j - lits;
  for (unsigned k = 0; k < new_size; k++) {
    lit_unmark(lits[k]);
  }
  return new_size;
}

void NapSAT::var_allocate(Tvar var)
{
  if (_vars.size() >= var + 1)
    return;

  if (_status == SAT) {
    _status = UNKNOWN;
  }

  unsigned old_size = _vars.size();
  _vars.resize(var + 1);
  for (Tvar i = old_size; i <= var; i++) {
    assert(_vars[i].constrained == 0);
    if (!_options.ignore_unused_variables)
      var_mark_constrained(i);
    _vars[i].chunks.resize(_n_allocated_chunks);
    _vars[i].cross_chunks.resize(_n_allocated_chunks);
    NOTIFY_OBSERVER(new_variable, i);
  }

  _watches.resize(2 * var + 2);
  _binary_watches.resize(2 * var + 2);
  // reallocate the literal buffer to make sure it is big enough
  // TODO replace with std::vector
  Tlit* new_literal_buffer = new Tlit[_vars.size() + 1];
  assert(_lit_buffer);
  std::memcpy(new_literal_buffer, _lit_buffer,
              _lit_buffer_size * sizeof(Tlit));
  delete[] _lit_buffer;
  _lit_buffer = new_literal_buffer;
}

void NapSAT::allocate_chunks(size_t n_chunks)
{
  ASSERT(n_chunks > _n_allocated_chunks);
  ASSERT(_chunks.size() == _n_allocated_chunks);
  _chunks.resize(n_chunks);

  for (Tchunk i = 1; i <= n_chunks; i++) {
    _free_chunks.push_back(_n_allocated_chunks + n_chunks - i);
    NOTIFY_STAT(_n_allocated_chunks);
  }
  _n_allocated_chunks = n_chunks;
  // resize the chunk sets of the variables
  for (Tvar i = 0; i < _vars.size(); i++) {
    _vars[i].chunks.resize(_n_allocated_chunks);
    _vars[i].cross_chunks.resize(_n_allocated_chunks);
  }

  for (Tchunk i = _n_allocated_chunks; i < _chunks.size(); i++) {
    _chunks[i].missed_implication.resize(_n_allocated_chunks);
  }
}

unsigned NapSAT::utility_heuristic(Tlit lit)
{
  // In graph backtracking, we cannot use a utility function anymore, because the literals are
  // now in a lattice, where all literals are not necessarily comparable.
  // We can however approximate the utility with the number of non-zero chunks of the literal.
  unsigned level_weight;
  if (_options.graph_backtracking) {
    level_weight = lit_chunks(lit).count();
  } else {
    level_weight = lit_level(lit);
  }
  return (lit_true(lit)  * (2 * solver_level() - level_weight + 3))
       + (lit_undef(lit) * (solver_level() + 2))
       + (lit_false(lit) * level_weight + 1);
}

unsigned NapSAT::max_utility_heuristic()
{
  return (2 * solver_level() + 3);
}


void NapSAT::defragment_order()
{
  _current_order = 0;
  for (size_t i = 0; i < _trail.size(); i++) {
    lit_order(_trail[i]) = _current_order++;
  }
}

void napsat::NapSAT::defragment_trail()
{
  // TODO is there a faster way to do this?
  // Probably, but algorithmically complicated.
  // Change if bottleneck.

  // store all the decisions
  vector<Tlit> decisions;
  for (Tlevel level = 1; level <= solver_level(); level++) {
    decisions.push_back(decision_lit_ptr(level)[0]);
  }
  restart();
  propagate();
  ASSERT(_conflicts.empty());
  for (Tlit decision : decisions) {
    imply_literal(decision, CLAUSE_UNDEF);
    propagate();
    ASSERT(_conflicts.empty());
  }
  ASSERT(check_trail_monotonicity());
}

size_t napsat::NapSAT::find_literal_in_trail(Tlit lit) const
{
  // the literal must be located between the decision of it's decision level and the end of the trail
  // if in NCB, it must be located between the decision of it's decision level and the next decision of a different level
  Tlevel level = lit_level(lit);
  const Tlit* left = decision_lit_ptr(level);
  const Tlit* right = _trail.data() + _trail.size();
  if (lit_decision(lit)) {
    return left - _trail.data();
  }
  if (!_options.chronological_backtracking && !_options.graph_backtracking
   && level < solver_level()) {
    right = decision_lit_ptr(level + 1);
  }

  size_t target_order = lit_order(lit);
  while(left < right) {
    const Tlit* mid = left + (right - left) / 2;
    if (lit_order(*mid) == target_order) {
      return mid - _trail.data();
    }
    if (lit_order(*mid) < target_order) {
      left = mid + 1;
    } else {
      right = mid;
    }
  }
  ASSERT_MSG(false,
             "The literal " << lit_to_string(lit) << " with order " << target_order
          << " was not found in the trail between " << (left - _trail.data())
          << " and " << (right - _trail.data()) << ".");
  return _trail.size();
}



void NapSAT::print_lit(Tlit lit) const
{
  cout << lit_to_string(lit);
}

string NapSAT::lit_to_string(Tlit lit) const
{
  string s = "";
  if (lit_marked(lit))
    s += "M";
  if (lit_undef(lit))
    s += ORANGE;
  else if (lit_true(lit))
    s += GREEN;
  else { // lit_false(lit)
    ASSERT(lit_false(lit));
    s += RED;
  }
  if (lit_decision(lit))
    s += "\033[4m";
  if (!lit_pol(lit))
    s += "-";
  s += to_string(lit_to_var(lit));
  if (lit_locked(lit))
    s += "🔒";

  s += "\033[0m";
  return s;
}

std::string napsat::NapSAT::lit_to_md_string(Tlit lit) const
{
  string s = "";
  if (lit_undef(lit))
    s += "<span style=\"color:rgb(200, 200, 0)\">";
  else if (lit_true(lit))
    s += "<span style=\"color:rgb(0, 176, 80)\">";
  else { // lit_false(lit)
    ASSERT(lit_false(lit));
    s += "<span style=\"color:rgb(255, 0, 0)\">";
  }
  if (lit_decision(lit))
    s += "<u>";
  s += to_string(lit_to_int(lit));
  if (lit_locked(lit))
    s += "🔒";

  if (lit_decision(lit))
    s += "</u>";

  s += "</span>";

  return s;
}

std::string napsat::NapSAT::lit_to_md_info_string(Tlit lit) const
{
//   ---
// tags:
//   - decision
// ---
  string s = "---\n";
  if (lit_decision(lit)) {
    s += "  tags:\n   - decision\n";
  } else {
    s += "  tags:\n   - implied\n";
  }
  // check if the literal is part of a conflict
  bool in_conflict = false;
  for (Tclause conflict : _conflicts) {
    const Tlit* lits = clause_lits(conflict);
    for (const Tlit* i = lits; i < lits + clause_size(conflict); i++) {
      if (lit_to_var(*i) == lit_to_var(lit)) {
        in_conflict = true;
        break;
      }
    }
  }
  if (in_conflict) {
    s += "   - conflicting\n";
  }

  s += "  level: "      + to_string(lit_level(lit)) + "\n";
  s += "  propagated: " + (string) (lit_propagated(lit) ? "true" : "false") + "\n";
  s += "  sync: "       + (string) (lit_synced(lit) ? "true" : "false") + "\n";
  s += "  locked: "     + (string) (lit_locked(lit) ? "true" : "false") + "\n";
  s += "  marked: "     + (string) (lit_marked(lit) ? "true" : "false") + "\n";
  s += "  vsids: "      + to_string(_vars[lit_to_var(lit)].activity) + "\n";

  // compute the number of clauses that contain this variable
  unsigned n_clauses = 0;
  for (TSclause cl : _clauses) {
    if (cl.deleted)
      continue;
    const Tlit* lits = cl.lits;
    for (const Tlit* i = lits; i < lits + cl.size; i++) {
      if (lit_to_var(*i) == lit_to_var(lit)) {
        n_clauses++;
        break;
      }
    }
  }
  s += "  n_clauses: " + to_string(n_clauses) + "\n";

  s += "---\n";
  s += "$\\ell$: " + lit_to_md_string(lit) + "\n";
  s += "$\\delta(\\ell)$: " + to_string(lit_level(lit)) + "\n";
  s += "$\\rho(\\ell)$: " + clause_to_md_string(lit_reason(lit)) + "\n";
  if (lit_lazy_reason(lit) != CLAUSE_UNDEF) {
    s += "$\\lambda(\\ell)$: " + clause_to_md_string(lit_lazy_reason(lit)) + "\n";
  }
  if (_options.graph_backtracking) {
    s += "$\\gamma(\\ell)$: " + lit_chunks(lit).to_string() + "\n";
    s += "$\\eta(\\ell)$: " + (lit_cross_chunks(lit) - lit_chunks(lit)).to_string() + "\n";
    s += "$\\zeta(\\ell)$: " + to_string(literal_cost(lit)) + "\n";
  }

  s += "---\n";
  s += "Watch list for " + lit_to_md_string(lit) + ":\n";
  for (const TSwatch& watch : _watches[lit]) {
    s += "  Clause " + clause_to_md_string(watch.cl) + " with blocking literal " + lit_to_md_string(watch.block) + "\n";
  }
  s += "---\n";
  s += "Watch list for " + lit_to_md_string(lit_neg(lit)) + ":\n";
  for (const TSwatch& watch : _watches[lit_neg(lit)]) {
    s += "  Clause " + clause_to_md_string(watch.cl) + " with blocking literal " + lit_to_md_string(watch.block) + "\n";
  }
  s += "---\n";
  return s;
}

string NapSAT::clause_to_string(Tclause cl) const
{
  string s = "";
  if (cl == CLAUSE_UNDEF)
    return "undef";

  if (_clauses[cl].deleted) {
    s += "d";
  }
  if (_clauses[cl].external) {
    s += "e";
  }
  if (_clauses[cl].learned) {
    s += "l";
  }
  s += to_string(cl) + ": ";
  ASSERT(clause_size(cl) > 0);
  ASSERT(clause_size(cl) < 1000000);
  for (const Tlit* i = clause_lits(cl); i < clause_lits(cl) + clause_size(cl); i++) {
    if (i == clause_lits(cl) + clause_size(cl))
      s += "| ";
    s += lit_to_string(*i);
    s += " ";
  }
  return s;
}

std::string napsat::NapSAT::clause_to_md_string(Tclause cl) const
{
  string s = "";
  if (cl == CLAUSE_UNDEF)
    return "undef";
  if (_clauses[cl].deleted) {
    s += "~~";
  }
  if (_clauses[cl].external) {
    s += "**";
  }
  s += to_string(cl);
  if (_clauses[cl].deleted) {
    s += "~~";
  }
  if (_clauses[cl].external) {
    s += "**";
  }
  s += ": ";
  ASSERT(clause_size(cl) > 0);
  ASSERT(clause_size(cl) < 1000000);
  for (const Tlit* i = clause_lits(cl); i < clause_lits(cl) + clause_size(cl); i++) {
    if (i == clause_lits(cl) + clause_size(cl))
      s += "| ";
    s += lit_to_md_string(*i);
    s += " ";
  }
  return s;
}

std::string NapSAT::clause_to_string(const Tlit* lits, size_t size) const
{
  string s = "";
  s += "{ ";
  for (const Tlit* i = lits; i < lits + size; i++) {
    s += lit_to_string(*i);
    s += " ";
  }
  s += "}";
  return s;
}

std::string napsat::NapSAT::clause_to_md_string(const Tlit* lits, size_t size) const
{
  string s = "";
  s += "{ ";
  for (const Tlit* i = lits; i < lits + size; i++) {
    s += lit_to_md_string(*i);
    s += " ";
  }
  s += "}";
  return s;
}

void NapSAT::print_clause(Tclause cl) const
{
  cout << clause_to_string(cl);
}

void NapSAT::print_trail() const
{
  cout << "trail: " << _n_propagated_lits << " - " << _trail.size() - _n_propagated_lits;
  if (_n_assumptions > 0) {
    cout << " (assumptions: " << _n_assumptions << ")";
  }
  cout << "\n";
  for (unsigned int i = 0; i < _trail.size(); i++) {
    Tlit lit = _trail[i];
    if (i == _n_propagated_lits) {
      cout << "^ τ -------- propagation head -------- ω v\n";
    }
    ASSERT(!lit_undef(lit));
    cout << pad(i, _trail.size()) << i;
    cout << ": δ = " <<  pad(lit_level(lit), solver_level()) << lit_level(lit) << " ";
    if (solver_level() < 500) {
      for (Tlevel i = 0; i < lit_level(lit); i++) {
        cout << " ";
      }
    }
    print_lit(lit);
    cout << " --> ρ = ";
    print_clause(lit_reason(lit));
    if (lit_lazy_reason(lit) != CLAUSE_UNDEF) {
      cout << " / (λ = ";
      print_clause(lit_lazy_reason(lit));
      cout << ")";
    }
    if (_options.graph_backtracking) {
      cout << " (γ = " << lit_chunks(lit).to_string() << ", ";
      cout <<   "η = " << (lit_cross_chunks(lit) - lit_chunks(lit)).to_string();
      if (lit_decision(lit) && lit_lazy_reason(lit) != CLAUSE_UNDEF) {
        cout << ", ξ = " << _chunks[lit_to_var(lit)].missed_implication.to_string();
      }
      cout << ")";
    }
    // if (lit_propagated(lit)) {
    //   cout << " (propagated)";
    // }
    cout << "\n";
  }
  for (Tclause conflict : _conflicts) {
    cout << "conflict: ";
    print_clause(conflict);
    if (_options.graph_backtracking)
      cout << " (" << clause_chunks(conflict).to_string() << ")";
    cout << "\n";
  }
  cout << endl;
}

void NapSAT::print_trail_simple() const
{
  cout << "trail :\n";
  for (Tlevel lvl = solver_level(); lvl <= solver_level(); lvl--) {
    cout << lvl << ": ";
    for (unsigned i = 0; i < _trail.size(); i++) {
      if (i == _n_propagated_lits)
        cout << "| ";
      Tlit lit = _trail[i];
      if (lit_level(lit) == lvl) {
        if (lit_pol(lit))
          cout << " ";
        cout << pad(lit_to_var(lit), _vars.size());
        print_lit(lit);
        cout << " ";
      }
      else {
        cout << pad(0, _vars.size());
        cout << "  ";
      }
    }
    cout << "\n";
  }
}

void NapSAT::print_chunks() const
{
  for (Tchunk chunk = 0; chunk < _chunks.size(); chunk++) {
    cout << "Chunk " << chunk << ": ";
    cout << "decision = " << _chunks[chunk].decision << endl;
  }
}

const static unsigned TERMINAL_WIDTH = 120;

void NapSAT::print_clause_set() const
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
  Tclause i = 0;
  while (i < _clauses.size()) {
    unsigned j = max_clause_width;
    do {
      if (_clauses[i].deleted) {
        i++;
        continue;
      }
      string clause_str = clause_to_string(i++);
      cout << clause_str;
      string spaces = "";

      ASSERT(string_length_escaped(clause_str) <= max_clause_width);
      ASSERT(max_clause_width >= string_length_escaped(clause_str));
      for (unsigned k = 0; k < max_clause_width - string_length_escaped(clause_str); k++)
        spaces += " ";
      if (j + max_clause_width < TERMINAL_WIDTH)
        cout << spaces;
      j += max_clause_width;
    } while (j < TERMINAL_WIDTH && i < _clauses.size());
    cout << endl;
  }
}

void NapSAT::print_watch_lists(Tlit lit) const
{
  Tlit i = 1;
  Tlit end = _watches.size();
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
    for (TSwatch w : _binary_watches[i]) {
      print_lit(w.block);
      cout << " <- " << w.cl << " ";
    }
    cout << "\n                non-binary: ";

    for (TSwatch w : _watches[i])
      cout << w.cl << " ";
    cout << "\n";
  }
}

bool NapSAT::parse_command(std::string input)
{
  if (input == "") {
    if (_status == SAT || _status == UNSAT)
      return true;
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
  else if (tokens[0] == "ASSUME") {
    if (tokens.size() != 2) {
      LOG_WARNING("Wrong number of arguments (expected 1). This command is ignored.");
      return false;
    }
    int lit_int = stoi(tokens[1]);
    Tlit lit = literal(abs(lit_int), lit_int > 0);
    if (!assume(lit)) {
      LOG_WARNING("The assumption of literal " << lit_to_string(lit) << " failed.");
      return false;
    }
  }
  else if (tokens[0] == "FORGET") {
    if (tokens.size() != 2) {
      LOG_WARNING("Wrong number of arguments (expected 1). This command is ignored.");
      return false;
    }
    int lit_int = stoi(tokens[1]);
    Tlit lit = literal(abs(lit_int), lit_int > 0);
    if (!forget_assumption(lit)) {
      LOG_WARNING("The forgetting of assumption of literal " << lit_to_string(lit) << " failed.");
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
    string man_file = env::get_man_page_folder() + "man-sat.txt";
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

void napsat::NapSAT::save_state()
{
  SAVE_STATE;
}
