/**
 * @file modulariT-SAT-checker.hpp
 * @brief Invariant checker for modulariT-SAT solver.
 * @details This file contains the invariant checker for the modulariT-SAT solver.
 *
 * @author Robin Coutelier
 */
#include "modulariT-SAT.hpp"
#include "custom-assert.hpp"

#include <unordered_set>

using namespace std;
using namespace sat;

static const string error = "\033[1;31m" + string("Error: ") + "\033[0m";

bool sat::modulariT_SAT::trail_consistency()
{
  bool success = true;
  if (_status == UNSAT)
    return true;
  if (_status == SAT && _trail.size() != _vars.size() - 1) {
    success = false;
    cerr << error << "Invariant violation: Trail consistency: the trail is not complete yet the solver claimed SAT\n";
    cout << error << "Invariant violation: the trail is not complete yet the solver claimed SAT" << endl;
  }
  for (unsigned index : _decision_index) {
    if (index >= _trail.size()) {
      success = false;
      cerr << error << "Invariant violation: Trail consistency: The decision index is out of range\n";
      cout << error << "Invariant violation: The decision index is out of range" << endl;
    }
    if (lit_reason(_trail[index]) != CLAUSE_UNDEF
      && lit_reason(_trail[index]) != CLAUSE_LAZY) {
      success = false;
      cerr << error << "Invariant violation: Trail consistency: The decision index is not a decision\n";
      cout << error << "Invariant violation: The decision index is not a decision" << endl;
    }
  }
  for (Tclause cl = 0; cl < _clauses.size(); cl++) {
    if (_clauses[cl].deleted)
      continue;
    if (!weak_non_falsified_clause(cl)) {
      success = false;
      cerr << error << "Invariant violation: Trail consistency: clause " << cl << " is falsified\n";
      cout << "clause " << cl << ": ";
      print_clause(cl);
      cout << " is falsified" << endl;
    }
  }
  if (!success) {
    cerr << error << "Invariant violation: Trail consistency: some clauses are falsified\n";
  }
  return success && trail_variable_consistency();
}

bool sat::modulariT_SAT::trail_variable_consistency()
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

bool sat::modulariT_SAT::trail_monotonicity()
{
  return true;
}

bool sat::modulariT_SAT::trail_decision_levels()
{
  return true;
}

bool sat::modulariT_SAT::trail_topological_sort()
{
  return true;
}

bool sat::modulariT_SAT::trail_unit_propagation()
{
  return true;
}

bool sat::modulariT_SAT::bcp_safety()
{
  return true;
}

bool sat::modulariT_SAT::is_watched(Tlit lit, Tclause cl)
{
  if (_clauses[cl].size == 2) {
    // check the binary clause list
    for (pair bin : _binary_clauses[lit])
      if (bin.second == cl)
        return true;
    return false;
  }
  Tclause watched = _watch_lists[lit];
  unsigned count = 0;
  while (watched != CLAUSE_UNDEF) {
    if (watched == cl)
      return true;
    if (count++ > _clauses.size()) {
      cerr << error << "Invariant violation: Watch lists no cycle: cycle detected in the watch list of literal " << lit_to_string(lit) << "\n";
      return false;
    }
    TSclause clause = _clauses[watched];
    ASSERT_MSG(lit == clause.lits[0] || lit == clause.lits[1],
      "Invariant violation: Watch lists correct: clause " + clause_to_string(cl) + " is not in the watch list of its watched literal " + lit_to_string(lit) + "\n");
    watched = clause.first_watched;
    if (lit == clause.lits[1])
      watched = clause.second_watched;
  }
  return false;
}

bool sat::modulariT_SAT::watch_lists_complete()
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

bool sat::modulariT_SAT::watch_lists_minimal()
{
  bool success = true;
  for (Tlit lit = 0; lit < _watch_lists.size(); lit++) {
    Tclause cl = _watch_lists[lit];
    unsigned count = 0;
    while (cl != CLAUSE_UNDEF) {
      TSclause clause = _clauses[cl];
      if (count++ > _clauses.size()) {
        success = false;
        cerr << error << "Invariant violation: Watch lists minimal: clause " << cl << " is in the watch list of literal " << lit << " but it is not a watched clause\n";
        cout << error << "Invariant violation: ";
        print_clause(cl);
        cout << " is in the watch list of literal ";
        print_lit(lit);
        cout << " but it is not a watched clause" << endl;
        goto loop_end;
      }
      if (clause.size < 2) {
        success = false;
        cerr << error << "Invariant violation: Watch lists minimal: clause " << cl << " is in the watch list of literal " << lit << " but it is too small\n";
        cout << error << "Invariant violation: ";
        print_clause(cl);
        cout << " is in the watch list of literal ";
        print_lit(lit);
        cout << " but it is too small" << endl;
        goto loop_end;
      }
      if (clause.deleted) {
        success = false;
        cerr << error << "Invariant violation: Watch lists minimal: clause " << cl << " is in the watch list of literal " << lit << " but it is a deleted clause\n";
        cout << error << "Invariant violation: ";
        print_clause(cl);
        cout << " is in the watch list of literal ";
        print_lit(lit);
        cout << " but it is a deleted clause" << endl;
        goto loop_end;
      }
      if (!clause.watched) {
        success = false;
        cerr << error << "Invariant violation: Watch lists minimal: clause " << cl << " is in the watch list of literal " << lit << " but it is not a watched clause\n";
        cout << error << "Invariant violation: ";
        print_clause(cl);
        cout << " is in the watch list of literal ";
        print_lit(lit);
        cout << " but it is not a watched clause" << endl;
        goto loop_end;
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
    loop_end:;
      cl = clause.first_watched;
      if (lit == clause.lits[1])
        cl = clause.second_watched;
    }
  }

  // Check that that are not multiple copies of the same clause in the watch lists
  std::unordered_set<Tclause> seen_clauses;
  for (Tlit lit = 0; lit < _watch_lists.size(); lit++) {
    Tclause cl = _watch_lists[lit];
    seen_clauses.clear();
    while (cl != CLAUSE_UNDEF) {
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
      TSclause clause = _clauses[cl];
      cl = clause.first_watched;
      if (lit == clause.lits[1])
        cl = clause.second_watched;
    }
  }
  if (!success) {
    print_trail();
    print_watch_lists();
  }

  return success;
}

bool sat::modulariT_SAT::strong_watched_literals()
{
  bool success = true;

  for (Tclause cl = 0; cl < _clauses.size(); cl++) {
    TSclause clause = _clauses[cl];
    if (clause.size < 2 || clause.watched == 0)
      continue;
    if (lit_false(clause.lits[0]) && lit_false(clause.lits[1]) && !lit_waiting(clause.lits[0]) && !lit_waiting(clause.lits[1])) {
      success = false;
      cerr << error << "Invariant violation: Strong watched literals: clause " << cl << " has both watched literals falsified and propagated\n";
      cout << error << "Invariant violation: ";
      print_clause(cl);
      cout << " has both watched literals falsified and propagated" << endl;
    }
  }
  return success;
}

bool sat::modulariT_SAT::weak_watched_literals()
{
  ASSERT(SAT_BLOCKERS);
  bool success = true;
  for (Tclause cl = 0; cl < _clauses.size(); cl++) {
    TSclause clause = _clauses[cl];
    if (clause.size < 2 || clause.watched == 0)
      continue;
    if (lit_false(clause.lits[0]) && lit_false(clause.lits[1]) && !lit_waiting(clause.lits[0]) && !lit_waiting(clause.lits[1])) {
      Tlit blocker = lit_blocker(cl);
      if (lit_true(blocker) && lit_level(blocker) <= lit_level(clause.lits[0]))
        continue;
      success = false;
      cerr << error << "Invariant violation: Strong watched literals: clause " << cl << " has both watched literals falsified and propagated\n";
      cout << error << "Invariant violation: ";
      print_clause(cl);
      cout << " has both watched literals falsified and propagated" << endl;
    }
  }
  return success;
}

bool sat::modulariT_SAT::watched_levels(Tclause cl)
{
  bool success = true;
  Tlevel max_level = 0;
  TSclause clause = _clauses[cl];
  for (Tlit* i = clause.lits; i < clause.lits + clause.size; i++)
    if (lit_level(*i) > max_level)
      max_level = lit_level(*i);

  success = max_level == _vars.at(lit_to_var(clause.lits[0])).level || max_level == _vars.at(lit_to_var(clause.lits[1])).level;
  if (!success) {
    cerr << error << "Invariant violation: Watched levels: clause " << cl << " has a watched literal with a level different from the maximum level\n";
    cout << error << "Invariant violation: "
      << "clause " << cl << ": ";
    print_clause(cl);
    cout << " has a watched literal with a level different from the maximum level" << endl;
  }
  return success;
}

bool sat::modulariT_SAT::non_falsified_clause(Tclause clause)
{
  for (Tlit* i = _clauses[clause].lits; i < _clauses[clause].lits + _clauses[clause].size; i++)
    if (!lit_false(*i))
      return true;

  cerr << error << "Invariant violation: Strong non falsified clause: clause " << clause << " is falsified\n";
  cout << error << "Invariant violation: "
    << "clause " << clause << ": ";
  print_clause(clause);
  cout << " is falsified" << endl;
  return false;
}

bool sat::modulariT_SAT::weak_non_falsified_clause(Tclause clause)
{
  for (Tlit* i = _clauses[clause].lits; i < _clauses[clause].lits + _clauses[clause].size; i++)
    if (!lit_false(*i) || lit_waiting(*i))
      return true;

  cerr << error << "Invariant violation: Weak non falsified clause: clause " << clause << " is falsified\n";
  cout << error << "Invariant violation: "
    << "clause " << clause << ": ";
  print_clause(clause);
  cout << " is falsified" << endl;
  return false;
}

bool sat::modulariT_SAT::no_unit_clauses()
{
  if (_options.chronological_backtracking)
    return true;
  bool success = true;
  for (Tclause cl = 0; cl < _clauses.size(); cl++) {
    TSclause clause = _clauses[cl];
    if (clause.deleted)
      continue;
    unsigned n_non_false = 0;
    for (Tlit* i = clause.lits; i < clause.lits + clause.size; i++) {
      if (!lit_false(*i) || lit_waiting(*i))
        n_non_false++;
      if (lit_true(*i))
        n_non_false += 2;
    }
    if (n_non_false == 1) {
      success = false;
      cerr << error << "Invariant violation: Non unit clauses: clause " << cl << " is unit\n";
      cout << error << "Invariant violation: "
        << "clause " << cl << ": ";
      print_clause(cl);
      cout << " is unit" << endl;
    }
  }
  if (!success) {
    cerr << error << "Invariant violation: Non unit clauses: some clauses are unit\n";
    print_trail();
  }
  return success;
}

bool sat::modulariT_SAT::most_relevant_watched_literals(Tclause clause)
{
  if (_clauses[clause].size < 2)
    return true;
  bool success = true;
  Tlit* lits = _clauses[clause].lits;
  unsigned size = _clauses[clause].size;
  ASSERT(size > 1);
  success &= utility_heuristic(lits[0]) >= utility_heuristic(lits[1]);
  if (!success) {
    cerr << error << "Invariant violation: Most relevant watched literals: clause " << clause << " has its first watched literal with a lower utility heuristic than its second watched literal\n";
    cout << "clause " << clause << ": ";
    print_clause(clause);
    cout << " has a watched literal with a higher utility heuristic than the second watched literal" << endl;
  }
  for (unsigned i = 2; i < size; i++)
    success &= utility_heuristic(lits[1]) >= utility_heuristic(lits[i]);

  if (!success) {
    cerr << error << "Property violation: Most relevant watched literals: clause " << clause << " are not the correct watched literals\n";
    cout << error << "Property violation: "
      << "clause: ";
    print_clause(clause);
    cout << " are not the correct watched literals" << endl;
    // print_trail();
  }
  return success;
}

bool sat::modulariT_SAT::clause_soundness(Tclause clause)
{
  bool success = true;
  vector<bool> found(_vars.size(), false);
  for (Tlit* i = _clauses[clause].lits; i < _clauses[clause].lits + _clauses[clause].size; i++) {
    if (found[lit_to_var(*i)]) {
      success = false;
      cerr << error << "Invariant violation: Clause soundness: clause " << clause << " has a duplicated literal\n";
      cout << error << "Invariant violation: "
        << "clause " << clause << ": ";
      print_clause(clause);
      cout << " has a duplicated literal" << endl;
    }
    found[lit_to_var(*i)] = true;
  }
  return success;
}
