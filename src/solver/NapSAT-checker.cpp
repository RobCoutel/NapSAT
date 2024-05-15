/**
 * @file src/solver/NapSAT-checker.cpp
 * @author Robin Coutelier
 *
 * @brief This file is part of the SAT solver NapSAT. It implements some invariant checkers
 * for the solver.
 * @details Not all the invariants are checked here since some are verified by the observer.
 * Only invariants on the integrity of the solver's data structure are checked here.
 */
#include "NapSAT.hpp"
#include "custom-assert.hpp"

#include <unordered_set>

using namespace std;
using namespace napsat;

static const string error = "\033[1;31m" + string("Error: ") + "\033[0m";

bool napsat::NapSAT::trail_variable_consistency()
{
  bool success = true;
  for (Tlit lit : _trail) {
    if (!lit_true(lit)) {
      success = false;
      cerr << error << "Invariant violation: Trail variable consistency: literal " << lit << " is in the trail but not true\n";
      cout << error << "Invariant violation: literal " << lit << " is in the trail but not true" << endl;
    }
  }

  for (Tvar var = 1; var < _vars.size(); var++) {
    if (!var_undef(var)) {
      bool found = false;
      for (Tlit lit : _trail) {
        if (lit_to_var(lit) == var) {
          found = true;
          break;
        }
      }
      if (!found) {
        success = false;
        cerr << error << "Invariant violation: Trail variable consistency: variable " << var << " is assigned " << var_true(var) << " but its literal " << literal(var, var_true(var)) << " is in the trail\n";
        cout << error << "Invariant violation: variable " << var << " is assigned " << var_true(var) << " but its literal " << literal(var, var_true(var)) << " is in the trail" << endl;
      }
    }
  }
  return success;
}

bool napsat::NapSAT::is_watched(Tlit lit, Tclause cl)
{
  if (_clauses[cl].size == 2) {
    // check the binary clause list
    for (pair bin : _binary_clauses[lit])
      if (bin.second == cl)
        return true;
    return false;
  }
  vector<Tclause>& watch_list = _watch_lists[lit];
  return find(watch_list.begin(), watch_list.end(), cl) != watch_list.end();
}

bool napsat::NapSAT::watch_lists_complete()
{
  bool success = true;
  for (Tclause cl = 0; cl < _clauses.size(); cl++) {
    TSclause clause = _clauses[cl];
    if (clause.size < 2 || !clause.watched || clause.deleted)
      continue;
    for (unsigned i = 0; i < 2; i++) {
      Tlit lit = clause.lits[i];
      if (!is_watched(lit, cl)) {
        success = false;
        cerr << error << "Invariant violation: Watch lists complete: clause " << cl << " is not in the watch list of its watched literal " << lit_to_string(lit) << "\n";
        cout << error << "Invariant violation: ";
        print_clause(cl);
        cout << " is not in the watch list of its watched literal ";
        print_lit(lit);
        cout << endl;
      }
    }
  }
  return success;
}

bool napsat::NapSAT::watch_lists_minimal()
{
  bool success = true;
  for (Tlit lit = 0; lit < _watch_lists.size(); lit++) {
    for (Tclause cl : _watch_lists[lit]) {
      TSclause clause = _clauses[cl];
      if (clause.size < 2) {
        success = false;
        cerr << error << "Invariant violation: Watch lists minimal: clause " << cl << " is in the watch list of literal " << lit << " but it is too small\n";
        cout << error << "Invariant violation: ";
        print_clause(cl);
        cout << " is in the watch list of literal ";
        print_lit(lit);
        cout << " but it is too small" << endl;
      }
      if (clause.deleted) {
        success = false;
        cerr << error << "Invariant violation: Watch lists minimal: clause " << cl << " is in the watch list of literal " << lit << " but it is a deleted clause\n";
        cout << error << "Invariant violation: ";
        print_clause(cl);
        cout << " is in the watch list of literal ";
        print_lit(lit);
        cout << " but it is a deleted clause" << endl;
      }
      if (!clause.watched) {
        success = false;
        cerr << error << "Invariant violation: Watch lists minimal: clause " << cl << " is in the watch list of literal " << lit << " but it is not a watched clause\n";
        cout << error << "Invariant violation: ";
        print_clause(cl);
        cout << " is in the watch list of literal ";
        print_lit(lit);
        cout << " but it is not a watched clause" << endl;
      }

      if (lit != clause.lits[0] && lit != clause.lits[1]) {
        success = false;
        cerr << error << "Invariant violation: Watch lists minimal: clause " << cl << " is in the watch list of literal " << lit << " but it is not a watched literal\n";
        cout << error << "Invariant violation: ";
        print_clause(cl);
        cout << " is in the watch list of literal ";
        print_lit(lit);
        cout << " but it is not a watched literal" << endl;
      }
    }
  }

  // Check that that are not multiple copies of the same clause in the watch lists
  std::unordered_set<Tclause> seen_clauses;
  for (Tlit lit = 0; lit < _watch_lists.size(); lit++) {
    seen_clauses.clear();
    for (Tclause cl : _watch_lists[lit]) {
      if (seen_clauses.find(cl) != seen_clauses.end()) {
        success = false;
        cerr << error << "Invariant violation: Watch lists minimal: clause " << cl << " is in the watch list of literal " << lit << " multiple times\n";
        cout << error << "Invariant violation: ";
        print_clause(cl);
        cout << " is in the watch list of literal ";
        print_lit(lit);
        cout << " multiple times" << endl;
        break;
      }
      seen_clauses.insert(cl);
    }
  }
  if (!success) {
    print_trail();
    print_watch_lists();
  }

  return success;
}
