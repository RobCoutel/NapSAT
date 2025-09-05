/*
 * This file is part of the source code of the software program
 * NapSAT. It is protected by applicable copyright laws.
 *
 * This source code is protected by the terms of the MIT License.
 */
/**
 * @file src/solver/NapSAT-checker.cpp
 * @author Robin Coutelier
 *
 * @brief This file is part of the NapSAT solver. It implements some invariant checkers
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

bool napsat::NapSAT::clause_unit(Tclause cl) const
{
  const TSclause& c = _clauses[cl];
  ASSERT(c.size > 0);
  if (!lit_undef(c.lits[0]))
    return false;
  for (size_t i = 1; i < c.size; i++)
    if (!lit_false(c.lits[i]))
      return false;
  return true;
}

bool napsat::NapSAT::clause_implying(Tclause cl) const
{
  const TSclause& c = _clauses[cl];
  ASSERT(c.size > 0);
  if (!lit_true(c.lits[0]))
    return false;
  for (size_t i = 1; i < c.size; i++)
    if (!lit_false(c.lits[i]))
      return false;
  return true;
}

bool napsat::NapSAT::clause_satisfied(Tclause cl) const
{
  const TSclause& c = _clauses[cl];
  for (size_t i = 0; i < c.size; i++)
    if (lit_true(c.lits[i]))
      return true;
  return false;
}

bool napsat::NapSAT::clause_falsified(Tclause cl) const
{
  const TSclause& c = _clauses[cl];
  for (size_t i = 0; i < c.size; i++)
    if (!lit_false(c.lits[i]))
      return false;
  return true;
}

bool napsat::NapSAT::lit_needs_fixing(Tlit lit) const
{
  lit = lit_neg(lit);
  for (const TSwatch& bw : _binary_watch[lit]) {
    if (!lit_true(bw.block))
      return true;
  }
  for (const TSwatch& w: _watches[lit]) {
    cout << "Checking watch " << clause_to_string(w.cl) << " blocked by " << lit_to_string(w.block) << endl;
    if (lit_true(w.block))
      continue;
    Tlit c1 = clause_lits(w.cl)[0];
    Tlit c2 = clause_lits(w.cl)[1];
    if (lit_true(c1) || lit_true(c2))
      continue;
    if (lit_false(c1) || lit_false(c2))
      return true;
  }
  return false;
}


bool napsat::NapSAT::lit_is_max_literal(Tlit lit, const Tlit* lits, size_t size) const
{
  if (_options.graph_backtracking) {
    const bitset& chunks = lit_chunks(lit);
    for (size_t i = 0; i < size; i++) {
      const bitset& chunks_i = lit_chunks(lits[i]);
      if (chunks < chunks_i)
        return false;
    }
  } else {
    for (size_t i = 0; i < size; i++) {
      if (lit_level(lit) < lit_level(lits[i]))
        return false;
    }
  }
  return true;
}

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
    for (TSwatch &w : _binary_watch[lit])
      if (w.cl == cl)
        return true;
    return false;
  }
  vector<TSwatch>& watch_list = _watches[lit];
  for (TSwatch &w : watch_list) {
    if (w.cl == cl) {
      return true;
    }
  }
  return false;
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
  for (Tlit lit = 0; lit < _watches.size(); lit++) {
    for (TSwatch w : _watches[lit]) {
      Tclause cl = w.cl;
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
  for (Tlit lit = 0; lit < _watches.size(); lit++) {
    seen_clauses.clear();
    for (TSwatch w : _watches[lit]) {
      Tclause cl = w.cl;
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
