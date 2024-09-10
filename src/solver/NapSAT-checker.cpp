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
#include "../utils/printer.hpp"

#include <unordered_set>

using namespace std;
using namespace napsat;

bool napsat::NapSAT::trail_variable_consistency()
{
  bool success = true;
  for (Tlit lit : _trail) {
    if (!lit_true(lit)) {
      success = false;
      LOG_ERROR("Invariant violation: Trail variable consistency: literal " << lit_to_string(lit) << " is in the trail but not true");
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
        LOG_ERROR("Invariant violation: variable " << var << " is assigned " << var_true(var) << " but its literal " << literal(var, var_true(var)) << " is in the trail");
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
        LOG_ERROR("Invariant violation: " << clause_to_string(cl) << " is not in the watch list of its watched literal " << lit_to_string(lit));
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
        LOG_ERROR("Invariant violation: " << clause_to_string(cl) << " is in the watch list of literal " << lit_to_string(lit) << " but it is too small");
      }
      if (clause.deleted) {
        success = false;
        LOG_ERROR("Invariant violation: Watch lists minimal: clause " << cl << " is in the watch list of literal " << lit_to_string(lit) << " but it is a deleted clause");
        LOG_ERROR("Invariant violation: " << clause_to_string(cl) << " is in the watch list of literal " << lit_to_string(lit) << " but it is a deleted clause");
      }
      if (!clause.watched) {
        success = false;
        LOG_ERROR("Invariant violation: " << clause_to_string(cl) << " is in the watch list of literal " << lit_to_string(lit) << " but it is not a watched clause");
      }

      if (lit != clause.lits[0] && lit != clause.lits[1]) {
        success = false;
        LOG_ERROR("Invariant violation: " << clause_to_string(cl) << " is in the watch list of literal " << lit_to_string(lit) << " but it is not a watched literal");
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
        LOG_ERROR("Invariant violation: " << clause_to_string(cl) << " is in the watch list of literal " << lit << " multiple times");

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
