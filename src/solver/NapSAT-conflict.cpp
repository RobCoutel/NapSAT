/*
 * This file is part of the source code of the software program
 * NapSAT. It is protected by applicable copyright laws.
 *
 * This source code is protected by the terms of the MIT License.
 */
/**
 * @file src/solver/NapSAT-conflict.cpp
 * @author Robin Coutelier
 *
 * @brief This file is part of the NapSAT solver. It implements the conflict analysis
 * and backtracking procedures.
 */
#include "NapSAT.hpp"

#include "custom-assert.hpp"
#include "../exporter/implication_graph_exporter.hpp"

#include <cstring>
#include <iostream>
#include <queue>

using namespace std;

namespace napsat
{

void NapSAT::purge_conflict_buffer()
{
  ASSERT(!_options.graph_backtracking);
  size_t write_index = 0;
  for (size_t read_index = 0; read_index < _conflicts.size(); read_index++) {
    Tclause conflict = _conflicts[read_index];
    if (check_clause_falsified(conflict)) {
      // still conflicting, keep it
      _conflicts[write_index++] = conflict;
      continue;
    }
  }
  _conflicts.resize(write_index);
}

void NapSAT::subsumption_filter(vector<bitset>& possibilities)
{
  if (possibilities.size() <= 1)
    return;
  for (size_t i = 0; i < possibilities.size() - 1; i++) {
    for (size_t j = i + 1; j < possibilities.size(); j++) {
      if (possibilities[i] >= possibilities[j]) {
        // j subsumes i, remove i
        possibilities[i] = possibilities.back();
        possibilities.pop_back();
        i--;
        break;
      }
      else if (possibilities[j] >= possibilities[i]) {
        // i subsumes j, remove j
        possibilities[j] = possibilities.back();
        possibilities.pop_back();
        j--;
      }
    }
  }
}

static bool subsumed(const vector<bitset>& a, const bitset& b)
{
  for (const bitset& ba : a) {
    if (ba <= b) {
      return true;
    }
  }
  return false;
}

void NapSAT::compute_lazy_merge_chunk_combination(vector<bitset>& combinations,
                                                  const bitset& mergeable_chunks,
                                                  bitset current,
                                                  bitset processed) const
{
  // check for subsumption
  if (subsumed(combinations, current)) {
    return;
  }

  // Set of chunks that have to be inspected
  bitset diff = current - processed;
  diff &= mergeable_chunks;

  if (diff.empty()) {
    combinations.push_back(current);
    return;
  }

  for (auto i = diff.cbegin(); i != diff.cend(); ++i) {
    Tchunk c = *i;
    bitset next_process = processed;
    next_process.set(c, true);
    bitset merged_chunks = _chunks[c].missed_implication;
    if (_n_assumptions > 0) {
      // the assumptions cannot be backtracked, and are removed from the possibilities
      merged_chunks -= _locked_chunks;
    }

    for (auto j = merged_chunks.cbegin(); j != merged_chunks.cend(); ++j) {
      Tchunk c2 = *j;
      if (current.get(c2)) {
        // we already have this chunk, any further combination will be subsumed
        continue;
      }
      bitset next_current = current;
      next_current.set(c2, true);
      compute_lazy_merge_chunk_combination(combinations, mergeable_chunks, next_current, next_process);
      if (combinations.size() > _options.backtrack_possibilities_limit) {
        return;
      }
    }
  }
}

void NapSAT::enhance_backtrack_possibilities_with_lazy_merging(const bitset& combined_chunks,
                                                               vector<bitset>& possibilities) const
{
  // check the chunks among the combined chunks that can be merged
  bitset mergeable_chunks(_n_allocated_chunks);
  for (Tchunk c = 0; c < _n_allocated_chunks; c++) {
    if (!_chunks[c].is_reimplied) {
      continue;
    }
    mergeable_chunks.set(c, true);
  }

  size_t original_size = possibilities.size();
  while(original_size > 0) {
    bitset current = possibilities[original_size - 1];
    if (possibilities.size() > _options.backtrack_possibilities_limit
     || !current.has_intersection(mergeable_chunks)) {
      // no mergeable chunk, nothing to do
      original_size--;
      continue;
    }
    possibilities[original_size - 1] = possibilities.back();
    possibilities.pop_back();
    original_size--;
    compute_lazy_merge_chunk_combination(possibilities, mergeable_chunks, current, bitset(_n_allocated_chunks));
    // remove the original possibility
  }
}

typedef pair<unsigned, bitset> prefix_t;
struct compare_prefixes {
  bool operator()(const prefix_t& a, const prefix_t& b) const {
    return a.first > b.first;
  }
};

void NapSAT::compute_backtrack_possibilities(vector<bitset>& possibilities) const
{
  ASSERT(all_of(_conflicts_chunks.begin(), _conflicts_chunks.end(),
      [](const bitset& cl_chunks){ return !cl_chunks.empty(); }
    ));
  ASSERT(possibilities.empty());
  priority_queue<prefix_t, vector<prefix_t>, compare_prefixes> prefixes;

  bitset remaining(_n_allocated_chunks);
  prefixes.push(make_pair(_conflicts_chunks.size(), bitset(_n_allocated_chunks)));
  while (!prefixes.empty()) {
    // check if the prefix is subsumed by an existing possibility
    if (possibilities.size() > _options.backtrack_possibilities_limit) {
      NOTIFY_STAT(_n_backtrack_limit_reached);
      break;
    }

    prefix_t p = prefixes.top();
    bitset& prefix = p.second;
    prefixes.pop();

    // by virtue of the priority queue, we know that future prefixes cannot subsum older ones.
    // But it can still be the case that an older prefix subsumes a newer one
    if (any_of(possibilities.begin(), possibilities.end(),
        [prefix](const bitset& p){ return p <= prefix; }
      )) {
      // this prefix is subsumed by an existing possibility, skip it
      continue;
    }
    if (p.first == prefix.count()) {
      // all conflicts are solved, we can stop
      possibilities.push_back(prefix);
      continue;
    }

    // Calculate which chunks can still be required
    remaining.clear();
    for (const bitset& cl_chunks : _conflicts_chunks)
      if (!prefix.has_intersection(cl_chunks))
        remaining |= cl_chunks;
    ASSERT (!remaining.empty());

    // expand the prefix
    Tchunk min_chunk = prefix.empty() ? CHUNK_UNDEF : *prefix.cbegin();

    for (auto it = remaining.cbegin(); it != remaining.cend(); ++it) {
      if (*it > min_chunk) {
        // Only add prefixes that expand to the left
        // The ones on the left should be expanded elsewhere.
        // This prevents duplicates and makes the search more efficient, but is more complex to implement.
        break;
      }
      bitset next = prefix;
      next.set(*it, true);

      unsigned conflicts_left = 0;

      for (const bitset& cl_chunks : _conflicts_chunks)
        if (!next.has_intersection(cl_chunks))
          conflicts_left++;
      prefixes.push(make_pair(conflicts_left + next.count(), next));
    }
  }

  NOTIFY_STAT_N(_a_prefix_size, prefixes.size());
  remaining.clear();
  for (const bitset& cl_chunks : _conflicts_chunks) {
    remaining |= cl_chunks;
  }
  enhance_backtrack_possibilities_with_lazy_merging(remaining, possibilities);

  NOTIFY_STAT_N(_a_bt_choices, possibilities.size());
}

void NapSAT::fix_conflicts_and_learned_in_order(const vector<pair<Tclause, vector<Tlit>>>& learned)
{
  auto conflicts = vector<Tclause>(_conflicts);
  _conflicts.clear();
  // check which clauses are subsumed by others. We do not want to add them to the set.
  // however, to make the proofs correct, we need to add them and then delete them. Otherwise the proof will not be correct.

  vector<bool> added(learned.size(), false);
  vector<bool> subsumed(learned.size(), false);
  vector<bool> examined_conflicts(conflicts.size(), false);
  compute_subsumed_clauses(learned, subsumed);

  // we have to imply literals from lowest to highest level
  // otherwise rscb will fail to recover the missed lower implications
  // we cannot simply sort the clauses, because the level of the clause depends on its literals, which might change as we add new clauses

  while(true) {
    size_t lowest_id = learned.size() + conflicts.size();
    Tlevel lowest_level = LEVEL_UNDEF;
    bool existing_clause_is_better = false;

    for (size_t i = 0; i < learned.size(); i++) {
      if (subsumed[i] || added[i]) {
        continue;
      }

      auto& id_clause = learned[i];
      const vector<Tlit>& clause = id_clause.second;
      Tlevel level = LEVEL_ROOT;
      for (Tlit lit : clause) {
        if (lit_undef(lit))
          continue;
        level = max(level, lit_level(lit));
        if (level > lowest_level) {
          break;
        }
      }
      if (level < lowest_level) {
        lowest_level = level;
        lowest_id = i;
      }
    }

    // check among the existing clauses
    for (size_t i = 0; i < conflicts.size(); i++) {
      if (examined_conflicts[i]) {
        continue;
      }
      Tclause conflict = conflicts[i];
      Tlevel level = LEVEL_ROOT;
      const Tlit* lits = clause_lits(conflict);
      unsigned n_undef = 0;
      for (unsigned j = 0; j < clause_size(conflict); j++) {
        if (lit_undef(lits[j])) {
          if (n_undef++ > 1) {
            // we still need to fix the clause so we need to have a lower less than LEVEL_UNDEF
            level = LEVEL_UNDEF - 1;
          }
          continue;
        }
        if (lit_true(lits[j])) {
          level = LEVEL_UNDEF - 1;
        }
        level = max(level, lit_level(lits[j]));
        if (level > lowest_level) {
          break;
        }
      }
      if (level < lowest_level) {
        existing_clause_is_better = true;
        lowest_level = level;
        lowest_id = i;
      }
    }

    if (lowest_id >= learned.size() + conflicts.size()) {
      break;
    }

    if (existing_clause_is_better) {
      examined_conflicts[lowest_id] = true;

      Tclause conflict = conflicts[lowest_id];
      Tlit* lits = clause_lits(conflict);
      if (clause_size(conflict) >= 2) {
        fix_watched_literals(conflict);
        if (_options.graph_backtracking
         && lit_true(lits[0]) && lit_false(lits[1])) {
          lit_cross_chunks(lits[1]) |= lit_chunks(lits[0]);
        }
      }
      if (lit_undef(lits[0])
       && (clause_size(conflict) == 1 || lit_false(lits[1]))) {
        imply_literal(lits[0], conflict);
      }
      if (lit_false(lits[0])) {
        _conflicts.push_back(conflict);
      }
    } else {
      added[lowest_id] = true;
      auto & id_clause = learned[lowest_id];
      Tclause id = id_clause.first;
      const vector<Tlit>& clause = id_clause.second;

      internal_add_clause(clause.data(), clause.size(), true, false, id);
    }
  }

  // fix the proof by adding the subsumed clauses and then deleting them
  for (size_t i = 0; i < learned.size(); i++) {
    if (!subsumed[i])
      continue;
    auto& id_clause = learned[i];
    Tclause id = id_clause.first;
    const vector<Tlit>& clause = id_clause.second;
    add_and_delete_clause(id, clause);
  }
  if (!_conflicts.empty()) {
    repair_conflicts();
  }
}

Tclause NapSAT::clause_subsumed_in_formula(const Tlit* lits, size_t size) const
{
  ASSERT(all_of(lits, lits + size, [this](Tlit l){ return lit_marked(l); }));

  // go through the watch lists of all literals and check if the clause is there
  // since clauses are watched by two literals, we can skip the last one
  Tclause found = CLAUSE_UNDEF;
  for (unsigned i = 0; i < size - 1 && found == CLAUSE_UNDEF; i++) {
    Tlit lit = lits[i];
    // check the binary clauses
    for (const TSwatch& watch : _binary_watches[lit]) {
      Tlit b = watch.block;
      if (lit_false(b) && lit_marked(b)) {
        found = watch.cl;
        break;
      }
    }
    if (found != CLAUSE_UNDEF) {
      break;
    }

    const auto& watch_list = _watches[lit];
    for (const TSwatch& watch : watch_list) {
      Tclause cl = watch.cl;
      Tlit b = watch.block;
      if (!lit_false(b) && !lit_marked(b)) {
        continue;
      }
      const Tlit* cls = clause_lits(cl);
      bool all_found = true;
      for (unsigned j = 0; j < _clauses[cl].size && all_found; j++) {
        all_found = lit_false(cls[j]) && lit_marked(cls[j]);
      }
      if (all_found) {
        found = cl;
        break;
      }
    }
  }

  return found;
}

bool NapSAT::learned_clause_is_redundant()
{
  ASSERT(all_of(_lit_buffer, _lit_buffer + _lit_buffer_size, [this](Tlit l){
    return lit_marked(l); }));

  if (_options.graph_backtracking) {
    Tclause redundant = clause_subsumed_in_formula(_lit_buffer, _lit_buffer_size);
    if (redundant != CLAUSE_UNDEF) {
      NOTIFY_STAT(_n_fw_subsumption_in_set);
      return true;
    }
  } else {
    for (size_t k = 0; k < _conflicts.size(); k++) {
      Tlit* lits = clause_lits(_conflicts[k]);
      bool all_marked = true;
      for (unsigned j = 0; j < clause_size(_conflicts[k]); j++) {
        if (!lit_marked(lits[j])) {
          all_marked = false;
          break;
        }
      }
      if (all_marked) {
        return true;
      }
    }
  }

  return false;
}

bool NapSAT::root_level_conflict()
{
  // check if there is a conflict at level 0
  for (Tclause conflict : _conflicts) {
    if (clause_level(conflict) == LEVEL_ROOT) {
      // the conflict cannot be repaired
      _status = status::UNSAT;
      if (_proof) {
        _proof->start_resolution_chain();
        _proof->link_resolution(LIT_UNDEF, conflict);
        prove_root_literal_removal(clause_lits(conflict), clause_size(conflict));
        _proof->finalize_resolution(_clauses.size(), nullptr, 0);
      }
      return true;
    }
  }
  return false;
}

void NapSAT::calculate_bitset_weights(vector<Tweight>& weights)
{
  double lowest_weight = numeric_limits<double>::max();

  for (Tweight& w : weights) {
    // we go right to left to estimate the cost of backtracking to this level
    size_t end = decision_lit_ptr(w.lowest_level) - _trail.data();
    while (w.give_up_point > end && w.total_weight < lowest_weight) {
      ASSERT(w.give_up_point <= _trail.size());
      size_t i = --w.give_up_point;
      Tlit lit = _trail[i];
      if (!lit_synced(lit)) {
        continue;
      }
      if (lit_chunks(lit).has_intersection(w.chunks)) {
        w.total_weight += literal_cost(lit);
      }
    }

    w.finished = (w.give_up_point == end);
    if (w.finished && w.total_weight < lowest_weight) {
      lowest_weight = w.total_weight;
    }
  }

  // sort again, such that the best candidate is first
  sort(weights.begin(), weights.end(),
            [](const Tweight& a, const Tweight& b) { return b < a; });
}

void NapSAT::calculate_bitset_weights_approx(vector<Tweight>& weights)
{
  if (weights.empty() || weights[0].finished) {
    sort(weights.begin(), weights.end(),
            [](const Tweight& a, const Tweight& b) { return b < a; });
    return;
  }

  vector<double> chunk_weights(_n_allocated_chunks, 0.0);
  vector<double> decision_weights(_n_allocated_chunks, 0.0);
  bitset counted_chunks(_n_allocated_chunks);
  Tlevel lowest_level = LEVEL_UNDEF;
  for (Tweight& w : weights) {
    size_t end = decision_lit_ptr(w.lowest_level) - _trail.data();
    // this is a bit dirty, but like this we can reuse the same interface as above.
    // The function will not do anything on the second call.
    if (w.give_up_point == end)
      continue;
    w.give_up_point = end;
    w.finished = true;
    counted_chunks |= w.chunks;

    lowest_level = min(lowest_level, w.lowest_level);

    if (_options.use_vsids_approximate_cost_estimation) {
      // use VSIDS activity as a proxy for literal cost
      for (auto it = w.chunks.cbegin(); it != w.chunks.cend(); ++it) {
        size_t chunk_id = *it;
        Tvar decision = _chunks[chunk_id].decision;
        chunk_weights[chunk_id] += var_activity(decision);
      }
    }
  }

  if (_options.use_vsids_approximate_cost_estimation) {
    sort(weights.begin(), weights.end(),
            [](const Tweight& a, const Tweight& b) { return b < a; });
    return;
  }

  // calculate the weight of each chunk
  size_t start_point = _decision_index[lowest_level-1];
  bitset lit_chunks_set(_n_allocated_chunks);
  const bitset* last_chunks = &lit_chunks_set;
  size_t count = 0;
  size_t counted_chunk_size = counted_chunks.count();

  for (size_t i = start_point; i < _trail.size(); i++) {
    Tlit lit = _trail[i];
    if (!lit_synced(lit)) {
      ASSERT(!var_synced(lit.var()));
      continue;
    }
    // if (!lit_chunks(lit).has_intersection(counted_chunks)) {
    //   continue;
    // }
    // no need to clear because we know that lit_chunks_set is always a subset of counted_chunks
    if (*last_chunks != lit_chunks(lit)) {
      lit_chunks_set = lit_chunks(lit);
      lit_chunks_set &= counted_chunks;
      count = lit_chunks_set.count();
    } else {
      last_chunks = &lit_chunks(lit);
    }
    // if the literal belongs to all, or none of the counted chunks, we can skip it because it will not change the order of the weights
    if (count == 0 || count == counted_chunk_size)
      continue;

    double lit_cost = literal_cost(lit);

    // we do not check for cend() because it is slower
    auto it = lit_chunks_set.cbegin();
    for (size_t j = 0; j < count; ++j, ++it) {
      ASSERT(it != lit_chunks_set.cend());
      size_t chunk_id = *it;
      chunk_weights[chunk_id] += lit_cost;
      if (lit_decision(lit)) {
        decision_weights[chunk_id] += lit_cost;
      }
    }
  }

  // now put the estimated weights together
  for (Tweight& w : weights) {
    double max_cost = 0.0;
    for (auto it = w.chunks.cbegin(); it != w.chunks.cend(); ++it) {
      size_t chunk_id = *it;
      if (_options.use_sum_approximate_cost_estimation) {
        w.total_weight += chunk_weights[chunk_id];
        continue;
      }
      Tvar decision = _chunks[chunk_id].decision;
      double decision_cost = 0.0;
      if (var_synced(decision)) {
        decision_cost = decision_weights[chunk_id];
      }
      ASSERT(chunk_weights[chunk_id] >= decision_cost,
                 "Chunk weight " + to_string(chunk_weights[chunk_id]) +
                 " is less than decision cost " + to_string(decision_cost) +
                 " for chunk " + to_string(chunk_id));
      max_cost = max(max_cost, chunk_weights[chunk_id] - decision_cost);
      w.total_weight += decision_cost;
    }
    w.total_weight += max_cost;
  }

  // sort again, such that the best candidate is first
  sort(weights.begin(), weights.end(),
            [](const Tweight& a, const Tweight& b) { return b < a; });
}

double NapSAT::calculate_weight(const bitset& chunks)
{
  double total_weight = 0.0;
  for (size_t i = 0; i < _trail.size(); i++) {
    Tlit lit = _trail[i];
    if (!lit_synced(lit)) {
      continue;
    }
    if (lit_chunks(lit).has_intersection(chunks)) {
      total_weight += literal_cost(lit);
    }
  }
  return total_weight;
}

bool NapSAT::conflict_is_UIP_cut(Tclause conflict, const bitset& chunks)
{
  ASSERT(_options.graph_backtracking);
  ASSERT(conflict != CLAUSE_UNDEF);
  ASSERT(_clauses[conflict].size > 0);
  ASSERT(!chunks.empty());

  bool found = false;
  const Tlit* lits = clause_lits(conflict);
  for (unsigned i = 0; i < clause_size(conflict); i++) {
    if (chunks.has_intersection(lit_chunks(lits[i]))) {
      if (found)
        return false;
      found = true;
    }
  }
  ASSERT(found, "The conflict " + clause_to_string(conflict) + " does not contain any chunk from " + chunks.to_string());
  return found;
}

bool NapSAT::conflict_is_UIP_cut(Tclause conflict, Tlevel bt)
{
  ASSERT(!_options.graph_backtracking);
  ASSERT(conflict != CLAUSE_UNDEF);
  ASSERT(_clauses[conflict].size > 0);

  bool found = false;
  const Tlit* lits = clause_lits(conflict);
  for (unsigned i = 0; i < clause_size(conflict); i++) {
    Tlevel lvl = lit_level(lits[i]);

    if (lvl >= bt) {
      if (found)
        return false;
      found = true;
    }
  }
  return found;
}

bool NapSAT::lit_is_required_in_learned_clause(Tlit lit)
{
  if (lit_decision(lit))
    return true;

  ASSERT(lit_reason(lit) < _clauses.size());
  TSclause& clause = _clauses[lit_reason(lit)];
  ASSERT(!clause.deleted);

  bool found_missing = false;

  for (unsigned i = 1; i < clause.size && !found_missing; i++)
    if (!lit_marked(clause.lits[i]))
      found_missing = true;

  if (found_missing) {
    // check if the lazy reason contains any marked literal
    Tclause lazy_reason = lit_lazy_reason(lit);
    if (lazy_reason != CLAUSE_UNDEF) {
      const Tlit* lazy_lits = clause_lits(lazy_reason);
      for (unsigned i = 0; i < clause_size(lazy_reason); i++)
        if (lit_marked(lazy_lits[i]))
          return true;
    }
  }

  return found_missing;
}

bool NapSAT::lit_analyzed(Tlit lit, Tlevel level)
{
  ASSERT(!_options.graph_backtracking);
  return lit_level(lit) == level;
}

bool NapSAT::lit_analyzed(Tlit lit, const bitset& chunks)
{
  ASSERT(_options.graph_backtracking);
  return lit_chunks(lit).has_intersection(chunks);
}

template <typename T>
bool NapSAT::mark_relevant_literals(Tlit lit, T level, unsigned& count) {
  Tclause reason = lit_reason(lit);
  bool reset_head = false;
  if (lit_lazy_reason(lit) != CLAUSE_UNDEF) {
    reason = lit_lazy_reason(lit);
    // if we use the lazy reason, we need to ensure that we will look at all the literals in the clause
    // since the missed lower implication does not satisfy the trail invariant, we need to push the reading head to the back
    // note that this may lead to duplicate literals in the learned clause, but this will be cleaned up in the "internal_add_clause" function
    reset_head = true;
  }

  ASSERT(reason != CLAUSE_UNDEF);
  if (_proof)
    _proof->link_resolution(~lit, reason);
  if (_dependency_tracker)
    _dependency_tracker->track_dependency(reason);
  const Tlit* reason_lits = clause_lits(reason);
  for (unsigned j = 1; j < clause_size(reason); j++) {
    Tlit reason_lit = reason_lits[j];
    if (lit_marked(reason_lit))
      continue;
    lit_mark(reason_lit);
    if (lit_analyzed(reason_lit, level))
      count++;
  }
  count--;
  lit_unmark(lit);
  return reset_head;
}

template <typename T>
void NapSAT::analyze_conflict_impl(T level) {
  ASSERT(_lit_buffer_size > 0);
  ASSERT(all_of(_trail.begin(), _trail.end(), [this](Tlit l){ return !lit_marked(l); }));
  NOTIFY(message, "Analyzing conflict " + clause_to_string(_lit_buffer, _lit_buffer_size) + " at level " + level.to_string(), 8);
  unsigned count = 0;
  for (unsigned i = 0; i < _lit_buffer_size; i++) {
    Tlit lit = _lit_buffer[i];
    ASSERT(lit_false(lit));
    if (!lit_marked(lit) && lit_analyzed(lit, level)) {
      count++;
    }
    lit_mark(lit);
  }
  ASSERT(count > 0);

  // We need to clear the literal buffer now.
  // The information is held in the "marked" markers
  _lit_buffer_size = 0;
  unsigned i = _trail.size();

  while (count > 1) {
    ASSERT(i > 0);
    Tlit lit = _trail[--i];
    if (!lit_marked(lit))
      continue;

    if (!lit_analyzed(lit, level))
      continue;
    bump_var_activity(lit.var());
    if (count == 1) {
      // this is the UIP. We want to put it at the front of the buffer
      _lit_buffer[_lit_buffer_size++] = ~lit;
      lit_unmark(lit);
      break;
    }
    // mark the literals of the reason
    if (lit_reason(lit) == CLAUSE_UNDEF && lit_lazy_reason(lit) == CLAUSE_UNDEF) {
      // this is a decision literal, we cannot go further
      // we are guaranteed that there is a merge later, that will reset the index i. We will need this literal again
      continue;
    }
    if (mark_relevant_literals(lit, level, count)) {
      i = _trail.size();
    }
  }
  ASSERT(count == 1);


  Tlit uip;
  // make space to push the UIP literal at the front of the buffer
  _lit_buffer_size = 1;
  // collect all the marked literals
  for (size_t i = 0; i < _trail.size(); i++) {
    Tlit lit = _trail[i];
    if (!lit_marked(lit))
      continue;
    lit_unmark(lit);

    if (lit_analyzed(lit, level)) {
      ASSERT(uip == LIT_UNDEF, "There should be only one UIP literal");
      uip = lit;
      continue;
    }

    bump_var_activity(lit.var());
    if(lit_is_required_in_learned_clause(lit)) {
      _lit_buffer[_lit_buffer_size++] = ~lit;
    } else {
      if (_proof)
        _proof->link_resolution(~lit, lit_reason(lit));
      if (_dependency_tracker)
        _dependency_tracker->track_dependency(lit_reason(lit));
    }
  }
  ASSERT(uip != LIT_UNDEF, "There should be a UIP literal");
  _lit_buffer[0] = ~uip;

  // clean up the literals at level 0
  if (_proof)
    prove_root_literal_removal(_lit_buffer, _lit_buffer_size);
  if (_dependency_tracker) {
    for (unsigned i = 0; i < _lit_buffer_size; i++) {
      Tlit lit = _lit_buffer[i];
      if (lit_level(lit) == LEVEL_ROOT) {
        _dependency_tracker->track_dependency(lit_reason(lit));
      }
    }
  }

  unsigned k = 0;
  for (unsigned i = 0; i < _lit_buffer_size; i++) {
    Tlit lit = _lit_buffer[i];
    if (lit_level(lit) == LEVEL_ROOT) {
      // we do not want to add the root literals to the learned clause
      continue;
    }
    _lit_buffer[k++] = lit;
  }
  _lit_buffer_size = k;
}

void NapSAT::analyze_conflict(Tlevel l) { analyze_conflict_impl(l); };
void NapSAT::analyze_conflict(const bitset& b) { analyze_conflict_impl(b); };


Tlevel NapSAT::compute_repair_level()
{
  Tlevel min_level = LEVEL_UNDEF;
  for (Tclause conflict : _conflicts) {
    const Tlit* lits = clause_lits(conflict);
    ASSERT(lit_is_max_literal(lits[0], lits + 1, clause_size(conflict) - 1));
    min_level = min(min_level, lit_level(lits[0]));
  }
  return min_level;
}

void NapSAT::fix_watched_literals(Tclause conflict)
{
  // In chronological backtracking, it might be the case that the second highest literal is not at the second position.
  Tlit* lits = clause_lits(conflict);
  // We need to ensure that it becomes the second watched literal
  Tlit* end = lits + _clauses[conflict].size;

  unsigned highest_heuristic = 0;
  Tlit* highest_lit = nullptr;
  unsigned second_highest_heuristic = 0;
  Tlit* second_highest_lit = nullptr;
  unsigned max_heuristic = max_utility_heuristic();

  for (Tlit* i = lits; i < end; i++) {
    unsigned h = utility_heuristic(*i);
    if (h > highest_heuristic) {
      second_highest_heuristic = highest_heuristic;
      second_highest_lit = highest_lit;
      highest_heuristic = h;
      highest_lit = i;
    } else if (h > second_highest_heuristic) {
      second_highest_heuristic = h;
      second_highest_lit = i;
    }
    if (second_highest_heuristic == max_heuristic) {
      // we cannot do better
      break;
    }
  }
  ASSERT(highest_lit != nullptr);
  ASSERT(second_highest_lit != nullptr,
             "Conflict clause " + clause_to_string(conflict) + " has only one literal?");
  ASSERT(highest_lit != second_highest_lit);

  if (second_highest_lit == lits) {
    // swap the two first literals
    Tlit tmp = lits[0];
    lits[0] = lits[1];
    lits[1] = tmp;
    second_highest_lit = lits + 1;
    if (highest_lit == lits + 1) {
      highest_lit = lits;
    }
  }

  if (highest_lit == lits + 1) {
    // swap the two first literals
    Tlit tmp = lits[0];
    lits[0] = lits[1];
    lits[1] = tmp;
    highest_lit = lits;
    ASSERT(second_highest_lit != lits);
  } else if (highest_lit > lits + 1) {
    stop_watch(lits[0], conflict);
    Tlit tmp = lits[0];
    lits[0] = *highest_lit;
    *highest_lit = tmp;
    watch_lit(lits[0], conflict);
  }

  ASSERT(second_highest_lit != lits);
  ASSERT(second_highest_lit != highest_lit);

  if (second_highest_lit > lits + 1) {
    stop_watch(lits[1], conflict);
    Tlit tmp = lits[1];
    lits[1] = *second_highest_lit;
    *second_highest_lit = tmp;
    watch_lit(lits[1], conflict);
  }
}

void NapSAT::repair_conflicts()
{
  synchronize();
  NOTIFY_STAT(_n_conflict_repair);
  auto start = chrono::high_resolution_clock::now();
  /**
   * Precondition:
   * - The conflict clause C is conflicting with the current partial assignment π
   *    C, π ⊧ ⊥
   * - The conflict clause has more than one literal
   *    |C| > 1
   * - The first literal in the conflict clause is the highest level literal
   *    δ(c₁) = δ(C)
   */
  ASSERT(!_conflicts.empty());
  ASSERT(_conflicts.size() == 1
      || any_of(_conflicts.begin(), _conflicts.end(), [this](Tclause c){
    return _clauses[c].external;
  }));

  // in general, a conflict may appear twice in the list.
  // clean up duplicates

  sort(_conflicts.begin(), _conflicts.end());
  _conflicts.erase(unique(_conflicts.begin(), _conflicts.end()), _conflicts.end());

  if (_status == status::SAT)
    _status = status::UNKNOWN;

  for (Tclause conflict : _conflicts) {
    bump_clause_activity(conflict);
  }

  if (root_level_conflict())
    return;

  /** FIND REPAIR POSITIONS **/
  if (_options.graph_backtracking) {
    graph_repair();
  } else {
    level_repair();
  }
  if (_status != status::UNSAT)
    _conflicts.clear();
  _var_activity_increment /= _options.var_activity_decay;

  auto end = chrono::high_resolution_clock::now();
  long long duration = chrono::duration_cast<chrono::microseconds>(end - start).count();
  // if the duration is above 0.5 sec, we save the state
  NOTIFY_STAT_N(repair_time, duration);
}

bool NapSAT::conflict_can_generate_learned_clause(Tclause conflict, const bitset& bt)
{

  /**
   * Conflict subset ck_a U ck_b U ck_c
   * bt {ck_b,ck_c, ck_e}
   * ck_b and ck_d imply ck_c
   *
   * virtually, after merge, Conflict subset ck_a U ck_b U ck_d
   */
  bitset conflict_chunks;
  if (_conflicts.size() > clause_size(conflict)) {
    // recomputing is cheaper than checking the existing chunks for all the conflicts
    conflict_chunks = clause_chunks(conflict);
  } else {
    for (unsigned i = 0; i < _conflicts.size(); i++) {
      if (_conflicts[i] == conflict) {
        conflict_chunks = _conflicts_chunks[i];
        break;
      }
    }
  }
  ASSERT(!conflict_chunks.empty());

  conflict_chunks &= bt;
  unsigned chunk_count = conflict_chunks.count();
  if (!_options.lazy_chunk_merging) {
    return chunk_count == 1;
  }

  bool changed = true;
  while (changed && chunk_count > 1) {
    changed = false;
    for (auto it = conflict_chunks.cbegin(); it != conflict_chunks.cend(); ++it) {
      Tchunk chunk = *it;
      const bitset& reimplied_chunks = _chunks[chunk].missed_implication;
      if (conflict_chunks.has_intersection(reimplied_chunks)) {
        conflict_chunks |= reimplied_chunks;
        conflict_chunks.set(chunk, false);

        conflict_chunks &= bt;
        chunk_count = conflict_chunks.count();
        changed = true;
        break;
      }
    }
  }
  return chunk_count == 1;
}

bool NapSAT::conflict_can_generate_learned_clause(Tclause conflict, Tlevel level)
{
  if (!_options.chronological_backtracking) {
    return true;
  }

  const Tlit* lits = clause_lits(conflict);
  for (unsigned i = 0; i < clause_size(conflict); i++) {
    if (lit_level(lits[i]) > level) {
      return false;
    }
  }
  return true;
}

bool NapSAT::implication_active_after_backtrack(Tclause reason, Tlevel level)
{
  for (unsigned i = 1; i < clause_size(reason); i++) {
    if (lit_level(clause_lits(reason)[i]) > level
     && lit_lazy_level(clause_lits(reason)[i]) > level) {
      return false;
    }
  }
  return true;
}

bool NapSAT::implication_active_after_backtrack(Tclause reason, const bitset& chunks)
{
  Tlit* lits = clause_lits(reason);
  for (unsigned i = 1; i < clause_size(reason); i++) {
    if (chunks.has_intersection(lit_chunks(lits[i]))) {
      return false;
    }
  }
  return true;
}

void NapSAT::add_and_delete_clause(Tclause cl, const vector<Tlit>& clause)
{
  Tclause new_id = internal_add_clause(clause.data(), clause.size(), true, false, cl);
    if (new_id == CLAUSE_UNDEF) {
      // the clause was not added because it was satisfied at level 0
      return;
    }
    // the clause should be at the back of the watch lists
    // we need to remove it from the watch lists
    Tlit* lits = clause_lits(new_id);
    ASSERT(clause.size() > 1);
    if (clause.size() == 2) {
      ASSERT(_binary_watches[lits[0]].back().cl == new_id);
      _binary_watches[lits[0]].pop_back();
      ASSERT(_binary_watches[lits[1]].back().cl == new_id);
      _binary_watches[lits[1]].pop_back();
    } else {
      ASSERT(_watches[lits[0]].back().cl == new_id);
      _watches[lits[0]].pop_back();
      ASSERT(_watches[lits[1]].back().cl == new_id);
      _watches[lits[1]].pop_back();
    }
    ASSERT(!is_protected(new_id));
    delete_clause(new_id);
}

void NapSAT::compute_subsumed_clauses(const vector<pair<Tclause, vector<Tlit>>>& clauses, vector<bool>& subsumed)
{
  ASSERT(clauses.size() == subsumed.size());
  // do a backward subsumption check on the learned clauses
  for (size_t i = 0; i < clauses.size(); i++) {
    if (subsumed[i])
      continue;
    // check if the clause i is subsumed by any later clause
    for (Tlit lit : clauses[i].second) { lit_mark(lit); }

    for (size_t j = i + 1; j < clauses.size(); j++) {
      if (subsumed[j])
        continue;

      bool all_found = true;
      for (Tlit lit : clauses[j].second) {
        if (!lit_marked(lit)) {
          all_found = false;
          break;
        }
      }
      if (all_found) {
        NOTIFY_STAT(_n_bw_subsumption);
        subsumed[i] = true;
        break;
      }
    }
    for (Tlit lit : clauses[i].second) { lit_unmark(lit); }
  }
}

const bitset& NapSAT::update_bt_after_analysis_of_reimplication(const bitset& chunks)
{
  // check whether the chunks still intersect
  return chunks;
}

Tlevel NapSAT::update_bt_after_analysis_of_reimplication(Tlevel level)
{
  Tlevel new_level = LEVEL_ROOT;
  for (unsigned j = 0; j < _lit_buffer_size; j++) {
    new_level = max(new_level, lit_level(_lit_buffer[j]));
  }
  ASSERT(new_level < level, "The new backtrack level " + to_string(new_level) + " is not lower than the current level " + to_string(level));
  return new_level;
}

template<typename T>
void NapSAT::try_and_learn_impl(T bt, vector<pair<Tclause, vector<Tlit>>>& learned_clauses)
{
  for (Tclause conflict : _conflicts) {
    if (!conflict_can_generate_learned_clause(conflict, bt)) {
      continue;
    }
    if (conflict_is_UIP_cut(conflict, bt)) {
      continue;
    }

    // we try to learn a clause
    ASSERT(_lit_buffer_size == 0);
    Tlit* lits = clause_lits(conflict);
    for (unsigned j = 0; j < clause_size(conflict); j++) {
      Tlit lit = lits[j];
      ASSERT(lit_false(lit),
                 "Conflict clause " + clause_to_string(conflict) + " is not conflicting?");
      _lit_buffer[_lit_buffer_size++] = lit;
    }
    if (_proof) {
      _proof->start_resolution_chain();
      _proof->link_resolution(LIT_UNDEF, conflict);
    }
    if (_dependency_tracker)
      _dependency_tracker->start_tracking();

    T bt_save = bt;
    do {
      analyze_conflict(bt);

      // check if the uip is a missed implication
      Tlit uip = _lit_buffer[0];
      Tclause lazy_reason = lit_lazy_reason(uip);
      if (lazy_reason == CLAUSE_UNDEF) {
        break;
      }
      // check if the missed implication is at a lower level
      if (!implication_active_after_backtrack(lazy_reason, bt)) {
        break;
      }

      // we need to continue the analysis with the missed implication
      // fill in the literal buffer
      if (_proof)
        _proof->link_resolution(uip, lazy_reason);
      if (_dependency_tracker)
        _dependency_tracker->track_dependency(lazy_reason);

      const Tlit* reason_lits = clause_lits(lazy_reason);
      for (unsigned j = 1; j < clause_size(lazy_reason); j++) {
        _lit_buffer[_lit_buffer_size++] = reason_lits[j];
      }
      if (_lit_buffer_size == 1) {
        _lit_buffer_size = 0;
        break;
      }
      // remove the uip from the learned clause
      _lit_buffer[0] = _lit_buffer[--_lit_buffer_size];

      bt = update_bt_after_analysis_of_reimplication(bt);
    } while (true);
    bt = bt_save;

    // check if the clause is new

    for (unsigned j = 0; j < _lit_buffer_size; j++) { lit_mark(_lit_buffer[j]); }
    bool redundant = learned_clause_is_redundant();
    if (redundant) {
      NOTIFY_STAT(_n_fw_subsumption_in_set);
    }
    // check if the clause is already learned
    if (!redundant) {
      for (const pair<Tclause, vector<Tlit>>& id_clause : learned_clauses) {
        const vector<Tlit>& clause = id_clause.second;
        if (clause.size() != _lit_buffer_size)
          continue;
        bool all_found = true;
        for (unsigned j = 0; j < clause.size() && all_found; j++) { all_found &= lit_marked(clause[j]); }
        if (all_found) {
          NOTIFY_STAT(_n_fw_subsumption);

          redundant = true;
          break;
        }
      }
    }
    for (unsigned j = 0; j < _lit_buffer_size; j++) { lit_unmark(_lit_buffer[j]); }
    if (redundant) {
      if (_proof)
        _proof->cancel_resolution_chain();
      if (_dependency_tracker)
        _dependency_tracker->cancel_tracking();
      _lit_buffer_size = 0;
      continue;
    }

    // add the clause
    vector<Tlit> learned_clause(_lit_buffer, _lit_buffer + _lit_buffer_size);
    // sort the clause using the utility heuristic
    sort(learned_clause.begin(), learned_clause.end(),
              [this](Tlit a, Tlit b) { return utility_heuristic(a) > utility_heuristic(b); });
    Tclause id = next_clause_id(_lit_buffer_size);
    learned_clauses.push_back({id, learned_clause});
    if (_proof)
      _proof->finalize_resolution(id, learned_clause.data(), learned_clause.size());
    if (_dependency_tracker)
      _dependency_tracker->finalize_tracking(id);

    if (_lit_buffer_size == 0) {
      _status = status::UNSAT;
      return;
    }
    _lit_buffer_size = 0;
  }
}

void NapSAT::try_and_learn(const bitset& chunks, vector<pair<Tclause, vector<Tlit>>>& learned_clauses) {
  try_and_learn_impl(chunks, learned_clauses);
}
void NapSAT::try_and_learn(Tlevel level, vector<pair<Tclause, vector<Tlit>>>& learned_clauses) {
  try_and_learn_impl(level, learned_clauses);
}

void NapSAT::graph_repair()
{
  static vector<bitset> possibilities;
  possibilities.clear();
  _conflicts_chunks.clear();
  _conflicts_chunks.reserve(_conflicts.size());
  for (Tclause conflict : _conflicts) {
    _conflicts_chunks.push_back(clause_chunks(conflict));
    if (_n_assumptions > 0) {
      _conflicts_chunks.back() -= _locked_chunks;
      if (_conflicts_chunks.back().empty()) { // conflict cannot be solved
        _status = status::UNSAT;
        return;
      }
    }
  }

  compute_backtrack_possibilities(possibilities);

  if (possibilities.empty()) {
    // we cannot repair the conflicts
    _status = status::UNSAT;
    return;
  }

  // Calculate the highest chunk involved in conflicts
  static vector<Tweight> weights;
  weights.clear();
  weights.reserve(possibilities.size());

  Tlevel highest_level = LEVEL_ROOT;

  setup_weights(possibilities, highest_level, weights);

  static vector<Tclause> implying_conflicts;
  static vector<pair<Tclause, vector<Tlit>>> learned_clauses;
  implying_conflicts.clear();
  learned_clauses.clear();

  Tweight best;
  Tweight analyzed;

  if (_options.chunk_level_penalty != 0.0) {
    // add an extra cost on the lowest levels
    for (Tweight& w : weights) {
      ASSERT(w.total_weight == 0);
      ASSERT(w.lowest_level > 0);
      w.total_weight = _options.chunk_level_penalty * (_trail.size() - _decision_index[w.lowest_level - 1]);
    }
  }

  bool filtered = false;

  do {
    ASSERT(learned_clauses.empty());
    implying_conflicts.clear();

    if (!best.chunks.empty() && !filtered) {
      // filter out weights that cannot learn and are not at the highest level
      for (size_t i = 0; i < weights.size(); i++) {
        if (!weights[i].maybe_learning && weights[i].highest_level < highest_level) {
          weights[i] = weights.back();
          weights.pop_back();
          i--;
        }
      }
      filtered = true;
      // we want to do this only once
    }
    ASSERT(!weights.empty());

    bool approximate = _options.use_max_approximate_cost_estimation;
    approximate     |= _options.use_sum_approximate_cost_estimation;
    approximate     |= _options.use_vsids_approximate_cost_estimation;

    const auto start = chrono::high_resolution_clock::now();

    if (approximate) {
      calculate_bitset_weights_approx(weights);
    } else {
      calculate_bitset_weights(weights);
    }

    const auto end = chrono::high_resolution_clock::now();
    const auto duration = chrono::duration_cast<chrono::microseconds>(end - start).count();
    NOTIFY_STAT_N(cost_estimation_time, duration);

    // the top element is the best one because it was sorted by the calculate_bitset_weights function
    ASSERT(!weights.empty());
    analyzed = weights.back();
    ASSERT(analyzed.finished);
    weights.pop_back();

#ifndef NDEBUG
    double penalty = _options.chunk_level_penalty * (_trail.size() - _decision_index[analyzed.lowest_level - 1]);
#endif
    ASSERT(approximate || analyzed.total_weight - penalty <= calculate_weight(analyzed.chunks) + 1e-6);
    ASSERT(approximate || analyzed.total_weight - penalty >= calculate_weight(analyzed.chunks) - 1e-6);
    ASSERT(!_options.use_max_approximate_cost_estimation
      || analyzed.total_weight - penalty <= calculate_weight(analyzed.chunks) + 1e-6,
               "Analyzed is " + analyzed.chunks.to_string() + " with weight " + to_string(analyzed.total_weight) + " and penalty " + to_string(penalty) +
               " but calculated weight is " + to_string(calculate_weight(analyzed.chunks)));
    ASSERT(!_options.use_sum_approximate_cost_estimation
      || analyzed.total_weight - penalty >= calculate_weight(analyzed.chunks) - 1e-6);

    if(best.chunks.empty()) {
      best.total_weight = analyzed.total_weight;
      best.chunks = analyzed.chunks;
    }
    ASSERT(best.total_weight <= analyzed.total_weight,
               "Best is " + best.chunks.to_string() + " with weight " + to_string(best.total_weight) +
                " but analyzed is " + analyzed.chunks.to_string() + " with weight " + to_string(analyzed.total_weight));

    if (analyzed.maybe_learning) {
      try_and_learn(analyzed.chunks, learned_clauses);

      if(_status == status::UNSAT) {
        return;
      }

      // if we manage to learn something, we stop
      if (!learned_clauses.empty()) {
        break;
      }

      NOTIFY_STAT(_n_failed_learning);
    }

    if (_just_learned_from_user || analyzed.highest_level == highest_level) {
      // this is the lightest and highest -> we're taking it
      // we are at the highest level, we can stop there
      break;
    }

  } while (!weights.empty());

  ASSERT(best.total_weight <= analyzed.total_weight);
  if (_options.backtrack_learned || learned_clauses.empty()) {
    backtrack(analyzed.chunks);
    // add assertion to make sure we are indeed at the highest level
    if (!_options.backtrack_learned) {
      NOTIFY_STAT(_n_backtrack_forced_chunks);
    }
  } else {
    // we did learn, so we backtrack whatever we want (i.e. the smallest chunk)
    backtrack(best.chunks);
    if (best.chunks != analyzed.chunks) {
      NOTIFY_STAT(_n_backtrack_better_chunks);
    }
  }

  fix_conflicts_and_learned_in_order(learned_clauses);
  _just_learned_from_user = false;
}

void NapSAT::setup_weights(vector<bitset>& possibilities, Tlevel& highest_level, vector<NapSAT::Tweight>& weights)
{
  for (size_t i = 0; i < possibilities.size(); i++) {
    Tweight w;
    w.chunks = possibilities[i];
    w.maybe_learning = false;
    for (Tclause conflict : _conflicts) {
      if (conflict_can_generate_learned_clause(conflict, w.chunks)
        && !conflict_is_UIP_cut(conflict, w.chunks)) {
        w.maybe_learning = true;
        break;
      }
    }

    w.give_up_point = _trail.size();

    for (auto it = w.chunks.cbegin(); it != w.chunks.cend(); ++it) {
      Tlevel cl = chunk_level(*it);
      w.highest_level = max(w.highest_level, cl);
      w.lowest_level = min(w.lowest_level, cl);
    }
    highest_level = max(highest_level, w.highest_level);
    weights.push_back(w);
  }
  // sort the weights by highest lowest level
  sort(weights.begin(), weights.end(),
            [](const Tweight& a, const Tweight& b) { return a.lowest_level > b.lowest_level; });
}

void NapSAT::level_repair()
{
  // chronological backtracking
  Tlevel repair_level = compute_repair_level();

  vector<pair<Tclause, vector<Tlit>>> learned_clauses;

  try_and_learn(repair_level, learned_clauses);

  if(_status == status::UNSAT) {
    return;
  }

  Tlevel bt = LEVEL_UNDEF;
  for (Tclause conflict : _conflicts) {
    bt = min(choose_backtracked_level(clause_lits(conflict), clause_size(conflict)), bt);
  }
  for (pair<Tclause, vector<Tlit>>& id_clause : learned_clauses) {
    vector<Tlit>& clause = id_clause.second;
    bt = min(choose_backtracked_level(clause.data(), clause.size()), bt);
  }

  backtrack(bt);

  fix_conflicts_and_learned_in_order(learned_clauses);
}

}
