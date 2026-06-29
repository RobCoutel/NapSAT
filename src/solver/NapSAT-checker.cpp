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

#include <fstream>

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
        if (lit.var() == var) {
          found = true;
          break;
        }
      }
      if (!found) {
        success = false;
        LOG_ERROR("Invariant violation: variable " << var.to_string() << " is assigned " << var_true(var) << " but its literal " << Tlit(var, var_true(var)).to_string() << " is in the trail");
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
  for (Tlit lit = 0; lit < _watch_lists.size(); lit.value++) {
    for (Tclause cl : _watch_lists[lit]) {
      TSclause clause = _clauses[cl];
      if (clause.size < 2) {
        success = false;
        LOG_ERROR("Invariant violation: " << clause_to_string(cl) << " is in the watch list of literal " << lit_to_string(lit) << " but it is too small");
      }
      if (clause.deleted) {
        success = false;
        LOG_ERROR("Invariant violation: Watch lists minimal: clause " << cl.to_string() << " is in the watch list of literal " << lit_to_string(lit) << " but it is a deleted clause");
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
  std::set<Tclause> seen_clauses;
  for (Tlit lit = 0; lit < _watch_lists.size(); lit.value++) {
    seen_clauses.clear();
    for (Tclause cl : _watch_lists[lit]) {
      if (seen_clauses.find(cl) != seen_clauses.end()) {
        success = false;
        LOG_ERROR("Invariant violation: " << clause_to_string(cl) << " is in the watch list of literal " << lit.to_string() << " multiple times");

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

void napsat::NapSAT::load_invariant_configuration(sentinel::SentinelOptions& s_options)
{
  string filename = napsat::env::get_invariant_configuration_folder();
  if (_options.lazy_strong_chronological_backtracking)
    filename += "lazy-strong-chronological-backtracking";
  else if (_options.restoring_strong_chronological_backtracking)
    filename += "restoring-strong-chronological-backtracking";
  else if (_options.weak_chronological_backtracking)
    filename += "weak-chronological-backtracking";
  // else if (_options.graph_backtracking)
  //   filename += "graph-backtracking";
  else
    filename += "non-chronological-backtracking";
  filename += ".conf";
  ifstream file(filename);
  if (!file.is_open()) {
    LOG_ERROR("The invariant configuration could not be loaded from file: " + filename);
    return;
  }
  unordered_map<string, bool*> invariants({
    {"trail_sanity", &s_options.check_no_conflicts},
    {"trail_monotonicity", &s_options.check_trail_monotonicity},
    {"implied_levels", &s_options.check_implied_levels},
    {"no_missed_implications", &s_options.check_no_missed_implications},
    {"topological_order", &s_options.check_topological_order},
    {"assignment_coherence", &s_options.check_assignment_coherence},
#if NOTIFY_WATCH_CHANGES
    {"weak_watched_literals", &s_options.check_weak_watched_literals},
    {"strong_watched_literals", &s_options.check_strong_watched_literals},
#endif
  });

#if NOTIFY_WATCH_CHANGES
  unordered_map<string, sentinel::WatchInvariant*> custom_invariants({
    {"blocked_backtrack_compatible_weak_watched_literals",
      new sentinel::WatchInvariant("Weak watched literal (with blocker)",
                                   [this](sentinel::Tlit c1,
                                          sentinel::Tlit c2,
                                          sentinel::Tlit blocker,
                                          std::string& err_msg) {
          Tlit n_c1 = Tlit(c1.value);
          Tlit n_c2 = Tlit(c2.value);
          Tlit n_blocker = Tlit(blocker.value);
          bool success =
                   !(lit_false(n_c1) && lit_propagated(n_c1))
                || !(lit_false(n_c2) && lit_propagated(n_c2))
                || (lit_true(n_blocker) && lit_level(n_blocker) <= lit_level(n_c1));
          if (!success) {
            err_msg += lit_to_string(~n_c1) + " ∈ τ ⇒ [" + lit_to_string(~n_c2) + " ∉ τ ∨ (" + lit_to_string(n_blocker) + " ∈ π ∧ δ(" + lit_to_string(n_blocker) + ") ≤ δ(" + lit_to_string(n_c1) + "))]\n";
            err_msg += (lit_false(n_c1) && lit_propagated(n_c1) ? "true" : "false");
            err_msg += " ⇒ ";
            err_msg += (lit_false(n_c2) && lit_propagated(n_c2) ? "true" : "false");
            err_msg += " ∨ (";
            err_msg += (lit_true(n_blocker) ? "true" : "false");
            err_msg += " ∧ ";
            err_msg += (lit_level(n_blocker) <= lit_level(n_c1) ? "true" : "false");
            err_msg += ")\n";
          }
          return success;
        },
        "¬c₁ ∈ τ ⇒ [(¬c₂ ∉ τ) ∨ (b ∈ π ∧ δ(b) ≤ δ(c₁))]")},
    {"blocked_backtrack_compatible_strong_watched_literals",
      new sentinel::WatchInvariant("Backtrack compatible strong watched literals (with blocker)",
                                   [this](sentinel::Tlit c1,
                                          sentinel::Tlit c2,
                                          sentinel::Tlit blocker,
                                          std::string& err_msg) {
          Tlit n_c1 = Tlit(c1.value);
          Tlit n_c2 = Tlit(c2.value);
          Tlit n_blocker = Tlit(blocker.value);
          return  !(lit_false(n_c1) && lit_propagated(n_c1))
                || (lit_true(n_c2) && lit_level(n_c2) <= lit_level(n_c1))
                || (lit_true(n_blocker) && lit_level(n_blocker) <= lit_level(n_c1));
        },
        "¬c₁ ∈ τ ⇒ [(c₂ ∈ π ∧ δ(c₂) ≤ δ(c₁)) ∨ (b ∈ π ∧ δ(b) ≤ δ(c₁))]")},
    {"blocked_lazy_backtrack_compatible_strong_watched_literals",
      new sentinel::WatchInvariant("Lazy backtrack compatible strong watched literals (with blocker)",
                                   [this](sentinel::Tlit c1,
                                          sentinel::Tlit c2,
                                          sentinel::Tlit blocker,
                                          std::string& err_msg) {
          Tlit n_c1 = Tlit(c1.value);
          Tlit n_c2 = Tlit(c2.value);
          Tlit n_blocker = Tlit(blocker.value);
          return  !(lit_false(n_c1) || !lit_propagated(n_c1))
                || (lit_true(n_c2) && lit_lazy_level(n_c2) <= lit_level(n_c1))
                || (lit_true(n_blocker) && lit_lazy_level(n_blocker) <= lit_level(n_c1));
        },
        "¬c₁ ∈ τ ⇒ [c₂ ∈ π ∧ [δ(c₂) ≤ δ(c₁) ∨ δ(λ(c₂) \\ {c₂}) ≤ δ(c₁)] ∨ [b ∈ π ∧ δ(b) ≤ δ(c₁)]")
  }});
#endif

  // reset all invariants to false
  for (auto &invariant : invariants)
    *(invariant.second) = false;


  // read the file
  string line;
  while (getline(file, line)) {
    if (invariants.find(line) != invariants.end())
      *(invariants[line]) = true;
    else if (custom_invariants.find(line) != custom_invariants.end())
      _watch_invariants.push_back(custom_invariants[line]);
    else
      LOG_INFO("Unknown invariant: " + line + "\nWatched literal invariants are not supported in this build. Check the SAT-config.hpp file to enable them.");
  }
  file.close();
}
