/*
 * This file is part of the source code of the software program
 * NapSAT. It is protected by applicable copyright laws.
 *
 * This source code is protected by the terms of the MIT License.
 */
/**
 * @file src/solver/NapSAT-assumptions.cpp
 * @author Robin Coutelier
 *
 * @brief This file is part of the NapSAT solver. It implements the assumption
 * handling procedures.
 */
#include "NapSAT.hpp"

using namespace napsat;
using namespace std;

bool NapSAT::assume(Tlit assumption)
{
  ASSERT_MSG(_options.graph_backtracking, "Only implemented for graph backtracking.");
  // In CB and NCB, assumptions are added as decisions at the bottom of the trail.
  bool success = true;
  if (_options.graph_backtracking) {
    if (lit_true(assumption)) {
      add_assumption_GB_true(assumption);
    } else if (lit_false(assumption)) {
      success = add_assumption_GB_false(assumption);
    } else {
      add_assumption_GB_undef(assumption);
    }
  }
  return success;
}

bool NapSAT::add_assumption(const std::vector<Tlit>& assumptions)
{
  return false;
}

bool NapSAT::forget_assumption(Tlit assumption)
{
  ASSERT_MSG(_options.graph_backtracking, "Only implemented for graph backtracking.");
  if (_options.graph_backtracking) {
    if (!lit_locked(assumption)) {
      // the assumption was not added
      return false;
    }
    ASSERT(lit_decision(assumption));
    // unlock the chunk of the literal
    ASSERT(lit_chunks(assumption).count() == 1);
    _locked_chunks = _locked_chunks - lit_chunks(assumption);
    lit_unlock(assumption);
  }
  if (_status == UNSAT) {
    _status = UNKNOWN;
    repair_conflicts();
  }
  return true;
}

void NapSAT::forget_assumption()
{
  ASSERT_MSG(_options.graph_backtracking, "Only implemented for graph backtracking.");
  if (_options.graph_backtracking) {
    // unlock all locked chunks
    for (auto it = _locked_chunks.cbegin(); it != _locked_chunks.cend(); ++it) {
      Tvar var = _chunks[*it].decision;
      Tlit lit = literal(var, lit_pol(var));
      ASSERT(lit_decision(lit));
      lit_unlock(lit);
    }
    _locked_chunks.clear();
  }
  if (_status == UNSAT) {
    _status = UNKNOWN;
    repair_conflicts();
  }
}

std::vector<Tlit> napsat::NapSAT::failed_assumptions() const
{
  ASSERT(_status == UNSAT);
  ASSERT_MSG(_options.graph_backtracking, "Only implemented for graph backtracking.");
  // look at each conflict clause, and find the ones that are failed by locked
  // chunks only. Return the corresponding literals.
  std::vector<Tlit> failed;
  bitset conflict_chunks(_n_allocated_chunks);
  for (const Tclause conflict : _conflicts) {
    const Tlit* lits = clause_lits(conflict);
    const unsigned size = clause_size(conflict);
    conflict_chunks.clear();
    for (unsigned i = 0; i < size; i++) {
      conflict_chunks |= lit_chunks(lits[i]);
    }
    if (conflict_chunks <= _locked_chunks) {
      // all chunks in the conflict are locked, the assumption failed
      for (unsigned i = 0; i < size; i++) {
        Tlit lit = lits[i];
        if (lit_decision(lit) && lit_locked(lit)) {
          failed.push_back(lit);
        }
      }
      return failed;
    }
  }
  ASSERT(false);
  return failed;
}

bool NapSAT::add_assumption_N_CB(Tlit lit)
{
  ASSERT_MSG(_options.graph_backtracking, "Only implemented for graph backtracking.");
  return false;
}

void NapSAT::add_assumption_GB_true(Tlit lit)
{
  ASSERT_MSG(_options.graph_backtracking, "Only implemented for graph backtracking.");
  ASSERT(lit_true(lit));
  ASSERT(_options.graph_backtracking);
  // check if the literal is already a decision
  if (lit_decision(lit)) {
    // just lock the chunk of the literal
    ASSERT(lit_chunks(lit).count() == 1);
    _locked_chunks |= lit_chunks(lit);
    if (!lit_locked(lit)) {
      lit_lock(lit);
    }
    return;
  }

  if (lit_level(lit) == LEVEL_ROOT) {
    // the literal is at root level, just lock its chunk
    // nothing needs to be done
    ASSERT(lit_chunks(lit).count() == 0);
    return;
  }

  // search the literal in the trail
  Tlevel level = lit_level(lit);
  ASSERT(level != LEVEL_UNDEF);
  // first literal at level after the decision (we do not want to count the decision to increment the level)
  size_t pos = _decision_index[level-1] + 1;
  for (; pos < _trail.size(); pos++) {
    Tlit l = _trail[pos];
    if (l == lit) {
      break;
    }
    level += lit_decision(l) ? 1 : 0;
  }
  ASSERT(pos < _trail.size());
  cout << "Adding assumption " << lit << " as decision at level " << level << " position " << pos << endl;

  /**
   * To fix the trail, we need to
   * 1. make the literal a decision at the right level
   *    i.e., counting the number of decisions before it in the trail (stored in `level`)
   * 2. lock its chunk
   * 3. update the chunks of all literals whose chunks are superset of the chunk of the assumed literal (before the change)
   * 4. update the levels of all literals above `level` in the trail (because we added a decision)
   */
  bitset lit_chunk = lit_chunks(lit);
  _decision_index.resize(_decision_index.size() + 1);
  _decision_index[level] = pos;

  lit_lazy_reason(lit) = lit_reason(lit);
  NOTIFY_OBSERVER(missed_lower_implication, lit_to_var(lit), lit_reason(lit));
  lit_reason(lit) = CLAUSE_UNDEF;
  NOTIFY_OBSERVER(update_reason, lit, CLAUSE_UNDEF);
  lit_level(lit) = level+1;
  NOTIFY_OBSERVER(update_level, lit, level+1);

  lit_chunks(lit).clear();
  unsigned ck = _free_chunks.back();
  _free_chunks.pop_back();

  lit_chunks(lit).set(ck, true);
  _chunks[ck].decision = lit;
  _chunks[ck].missed_implication = lit_chunk;
  _locked_chunks.set(ck, true);

  lit_lock(lit);

  // update the chunks and levels of all literals above `pos` in the trail
  for (size_t i = pos + 1; i < _trail.size(); i++) {
    Tlit l = _trail[i];
    // decisions should be updated in the index
    // we do not need to compute -1 because we are going to increment the level right after
    if (lit_decision(l)) {
      _decision_index[lit_level(l)] = i;
    }
    // update level
    if (lit_level(l) >= level) {
      lit_level(l)++;
      NOTIFY_OBSERVER(update_level, l, lit_level(l));
    }
    // update chunks if necessary
    if (lit_chunk >= lit_chunks(l)) {
      Tlevel recomputed_level = LEVEL_ROOT;
      lit_chunks(l).clear();
      const Tclause reason = lit_reason(l);
      ASSERT(reason != CLAUSE_UNDEF);
      const Tlit* lits = clause_lits(reason);
      const unsigned size = clause_size(reason);
      for (unsigned j = 1; j < size; j++) {
        Tlit rl = lits[j];
        lit_chunks(l) |= lit_chunks(rl);
        recomputed_level = std::max(recomputed_level, lit_level(rl));
      }
      if (lit_level(l) != recomputed_level) {
        NOTIFY_OBSERVER(update_level, l, recomputed_level);
      }
      lit_level(l) = recomputed_level;
      NOTIFY_OBSERVER(update_level, l, recomputed_level);
    }
  }
}

bool NapSAT::add_assumption_GB_false(Tlit lit)
{
  ASSERT_MSG(_options.graph_backtracking, "Only implemented for graph backtracking.");
  ASSERT(lit_false(lit));
  ASSERT(_options.graph_backtracking);

  vector<Tlit> lits;
  lits.push_back(lit);
  return add_assumption_GB_false(lits);
}

bool NapSAT::add_assumption_GB_false(std::vector<Tlit>& lits)
{
  ASSERT_MSG(_options.graph_backtracking, "Only implemented for graph backtracking.");
  _status = UNKNOWN;
  ASSERT(!lits.empty());
  ASSERT(_options.graph_backtracking);
  ASSERT(all_of(lits.begin(), lits.end(),
    [this](Tlit l){ return lit_false(l); }
  ));

  // find the cheapest set of chunks to backtrack to in order to free all literals in L
  vector<bitset> conflict_chunks;
  for (Tlit l : lits) {
    conflict_chunks.push_back(lit_chunks(l));
  }
  vector<bitset> possibilities;
  compute_backtrack_possibilities(conflict_chunks, possibilities);
  cout << "Possibilities: " << endl;
  for (const bitset& b : possibilities) {
    cout << "  " << b.to_string() << endl;
  }
  if (possibilities.empty()) {
    return false;
  }

  // choose the cheapest possibility
  // here we do not care about termination, so we do not need to learn a new clause
  // we just pick to the cheapest possibility
  vector<Tweight> weights;
  weights.reserve(possibilities.size());

  Tlevel highest_level = LEVEL_ROOT;

  setup_weights(possibilities, highest_level, weights);
  calculate_bitset_weights(weights);
  const Tweight& best = weights.back();

  cout << "Removing chunks " << best.chunks.to_string() << " to add assumptions with weight " << best.total_weight << endl;
  backtrack(best.chunks);

  for (Tlit l : lits) {
    add_assumption_GB_undef(l);
  }
  return true;
}

void NapSAT::add_assumption_GB_undef(Tlit lit)
{
  ASSERT_MSG(_options.graph_backtracking, "Only implemented for graph backtracking.");
  ASSERT(lit_undef(lit));
  ASSERT(_options.graph_backtracking);
  // convert the literal into a decision at the current level
  imply_literal(lit, CLAUSE_UNDEF);
  // lock the chunk of the literal
  ASSERT(lit_chunks(lit).count() == 1);
  _locked_chunks |= lit_chunks(lit);
  lit_lock(lit);
}

void NapSAT::remove_assumption_N_CB(Tlit lit)
{
  ASSERT_MSG(_options.graph_backtracking, "Only implemented for graph backtracking.");
}

void NapSAT::remove_assumption_GB(Tlit lit)
{
  ASSERT_MSG(_options.graph_backtracking, "Only implemented for graph backtracking.");
}
