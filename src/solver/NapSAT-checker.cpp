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

bool napsat::NapSAT::check_clause_unit(Tclause cl) const
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

bool napsat::NapSAT::check_clause_implying(Tclause cl) const
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

bool napsat::NapSAT::check_clause_satisfied(Tclause cl) const
{
  const TSclause& c = _clauses[cl];
  for (size_t i = 0; i < c.size; i++)
    if (lit_true(c.lits[i]))
      return true;
  return false;
}

bool napsat::NapSAT::check_clause_falsified(Tclause cl) const
{
  const TSclause& c = _clauses[cl];
  for (size_t i = 0; i < c.size; i++)
    if (!lit_false(c.lits[i]))
      return false;
  return true;
}

bool napsat::NapSAT::check_lit_needs_fixing(Tlit lit) const
{
  lit = ~lit;
  for (const TSwatch& bw : _binary_watches[lit]) {
    if (!lit_true(bw.block))
      return true;
  }
  for (const TSwatch& w: _watches[lit]) {
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
      if (lit_level(lit) < lit_level(lits[i])) {
        return false;
      }
    }
  }
  return true;
}

bool napsat::NapSAT::check_trail_variable_consistency() const
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
        LOG_ERROR("Invariant violation: variable " << var << " is assigned " << var_true(var) << " but its literal " << Tlit(var, var_true(var)) << " is in the trail");
      }
    }
  }
  return success;
}

bool napsat::NapSAT::check_decision_index_consistency() const
{
  bool success = true;
  for (Tlevel i = 0; i < _decision_index.size(); i++) {
    if (_decision_index[i] >= _trail.size()) {
      success = false;
      LOG_ERROR("Invariant violation: Decision index consistency: at decision level " << i << ", index " << _decision_index[i] << " is out of bounds (trail size: " << _trail.size() << ")");
      continue;
    }
    Tlit lit = _trail[_decision_index[i]];
    if (!lit_decision(lit)) {
      success = false;
      LOG_ERROR("Invariant violation: Decision index consistency: at decision level " << i << ", index " << _decision_index[i] << " points to literal " << lit_to_string(lit) << " which is not a decision literal");
    }
    if (lit_level(lit) != i + 1) {
      success = false;
      LOG_ERROR("Invariant violation: Decision index consistency: at decision level " << (i + 1) << ", index " << _decision_index[i] << " points to literal " << lit_to_string(lit) << " which is at level " << lit_level(lit));
    }
  }
  if (!success) {
    // print the _decision_index for debugging
    for (Tlevel i = 0; i < _decision_index.size(); i++) {
      cout << "_decision_index[" << i << "] = " << _decision_index[i] << endl;
    }
  }
  return success;
}

bool napsat::NapSAT::check_is_watched(Tlit lit, Tclause cl) const
{
  ASSERT(cl != CLAUSE_UNDEF);
  ASSERT(lit != LIT_UNDEF, "check_is_watched(" << lit_to_string(lit) << ", " << clause_to_string(cl) << ")");
  if (_clauses[cl].size == 2) {
    // check the binary clause list
    for (const TSwatch &w : _binary_watches[lit])
      if (w.cl == cl)
        return true;
    return false;
  }
  const vector<TSwatch>& watch_list = _watches[lit];
  for (const TSwatch &w : watch_list) {
    if (w.cl == cl) {
      return true;
    }
  }
  return false;
}

bool napsat::NapSAT::check_watch_lists_complete() const
{
  bool success = true;
  for (Tclause cl = 0; cl < _clauses.size(); cl++) {
    TSclause clause = _clauses[cl];
    if (clause.size < 2 || !clause.watched || clause.deleted)
      continue;
    Tlit lit = clause.lits[0];
    if (!check_is_watched(lit, cl)) {
      success = false;
      LOG_ERROR("Invariant violation: " << clause_to_string(cl) << " is not in the watch list of its watched literal " << lit_to_string(lit));
    }
    lit = clause.lits[1];
    if (!check_is_watched(lit, cl)) {
      success = false;
      LOG_ERROR("Invariant violation: " << clause_to_string(cl) << " is not in the watch list of its watched literal " << lit_to_string(lit));
    }
  }
  return success;
}

bool napsat::NapSAT::check_watch_lists_minimal() const
{
  bool success = true;
  for (Tlit lit = 0; lit < _watches.size(); lit.value++) {
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
  for (Tlit lit = 0; lit < _watches.size(); lit.value++) {
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

void napsat::NapSAT::load_invariant_configuration(sentinel::Options& s_options)
{
  string filename = napsat::env::get_invariant_configuration_folder();
  if (_options.lazy_strong_chronological_backtracking)
    filename += "lazy-strong-chronological-backtracking";
  else if (_options.chronological_backtracking)
    filename += "chronological-backtracking";
  else if (_options.graph_backtracking)
    filename += "graph-backtracking";
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
          return  !(lit_false(n_c1) && lit_propagated(n_c1))
                || (lit_true(n_c2) && lit_lazy_level(n_c2) <= lit_level(n_c1))
                || (lit_true(n_blocker) && lit_lazy_level(n_blocker) <= lit_level(n_c1));
        },
        "¬c₁ ∈ τ ⇒ [c₂ ∈ π ∧ [δ(c₂) ≤ δ(c₁) ∨ δ(λ(c₂) \\ {c₂}) ≤ δ(c₁)] ∨ [b ∈ π ∧ δ(b) ≤ δ(c₁)]")
  },
  {"blocked_graph_backtrack_compatible_strong_watched_literals",
      new sentinel::WatchInvariant("Graph backtrack compatible strong watched literals (with blocker)",
                                   [this](sentinel::Tlit c1,
                                          sentinel::Tlit c2,
                                          sentinel::Tlit blocker,
                                          std::string& err_msg) {
          Tlit n_c1 = Tlit(c1.value);
          Tlit n_c2 = Tlit(c2.value);
          Tlit n_blocker = Tlit(blocker.value);
          return  !(lit_false(n_c1) && lit_propagated(n_c1))
                || (lit_true(n_c2) && lit_chunks(n_c2) <= lit_cross_chunks(n_c1))
                || (lit_true(n_blocker) && lit_chunks(n_blocker) <= lit_cross_chunks(n_c1));
        },
        "¬c₁ ∈ τ ⇒ [(c₂ ∈ π ∧ γ(c₂) ⊆ η(c₁)) ∨ (b ∈ π ∧ γ(b) ⊆ η(c₁))]")}
  });
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
