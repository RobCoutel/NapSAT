#include "NapSAT.hpp"

#include "custom-assert.hpp"

#include <cstring>
#include <iostream>

using namespace std;
using namespace napsat;

void napsat::NapSAT::backtracked_chunks_subsumption(std::vector<bitset>& possibilities)
{
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

void napsat::NapSAT::compute_lazy_merge_chunk_combination(vector<bitset>& combinations,
                                                          const bitset& current,
                                                          bitset processed)
{
  bitset diff = current - processed;

  for (auto i = diff.cbegin(); i != diff.cend(); ++i) {
    if (var_lazy_reason(_chunks[*i].decision) == CLAUSE_UNDEF) {
      processed.set(*i, true);
    }
  }
  diff = current - processed;

  if (diff.empty()) {
    combinations.push_back(current);
    return;
  }

  for (auto i = diff.cbegin(); i != diff.cend(); ++i) {
    Tclause missed_implication = var_lazy_reason(_chunks[*i].decision);
    ASSERT(missed_implication != CLAUSE_UNDEF);
    bitset next_process = processed;
    next_process.set(*i, true);
    bitset merged_chunks = clause_chunks(missed_implication);
    for (auto j = merged_chunks.cbegin(); j != merged_chunks.cend(); ++j) {
      bitset next_current = current;
      next_current.set(*j, true);
      compute_lazy_merge_chunk_combination(combinations, next_current, next_process);
    }
  }
}

void napsat::NapSAT::enhance_backtrack_possibilities_with_lazy_merging(std::vector<bitset>& possibilities)
{
  size_t original_size = possibilities.size();
  while(original_size > 0) {
    compute_lazy_merge_chunk_combination(possibilities, possibilities[original_size - 1], bitset(_n_allocated_chunks));
    // remove the original possibility
    possibilities[original_size - 1] = possibilities.back();
    possibilities.pop_back();
    original_size--;
  }

  backtracked_chunks_subsumption(possibilities);
}

size_t napsat::NapSAT::split_learning_possibilities(std::vector<bitset>& possibilities)
{
  size_t maybe_learning = 0;
  size_t non_learning = 0;
  while (maybe_learning + non_learning < possibilities.size()) {
    bool learning_found = false;
    for (Tclause conflict : _conflicts) {
      if (!conflict_is_UIP_cut(conflict, possibilities[maybe_learning])) {
        learning_found = true;
        break;
      }
    }
    if (learning_found) {
      maybe_learning++;
      continue;
    }
    non_learning++;
    bitset tmp = possibilities[maybe_learning];
    size_t idx = possibilities.size() - non_learning;
    possibilities[maybe_learning] = possibilities[idx];
    possibilities[idx] = tmp;
  }
  return maybe_learning;
}

void napsat::NapSAT::fix_conflicts_and_learned_in_order(const std::vector<Tclause>& conflicts, const std::vector<std::pair<Tclause, std::vector<Tlit>>>& learned)
{
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
        level = std::max(level, lit_level(lit));
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
          n_undef++;
          if (n_undef > 1) {
            // we still need to fix the clause so we need to have a lower less than LEVEL_UNDEF
            level = LEVEL_UNDEF - 1;
          }
          continue;
        }
        if (lit_true(lits[j])) {
          level = LEVEL_UNDEF - 1;
        }
        level = std::max(level, lit_level(lits[j]));
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
      fix_watched_literals(conflict);
      Tlit* lits = clause_lits(conflict);
      if (_options.graph_backtracking
       && lit_true(lits[0]) && lit_false(lits[1])) {
        lit_cross_chunks(lits[1]) |= lit_chunks(lits[0]);
      }
      if (lit_undef(lits[0]) && lit_false(lits[1])) {
        imply_literal(lits[0], conflict);
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
}

bool napsat::NapSAT::learned_clause_is_redundant()
{
  for (unsigned j = 0; j < _next_literal_index; j++) { lit_mark(_literal_buffer[j]); }

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
      for (unsigned j = 0; j < _next_literal_index; j++) { lit_unmark(_literal_buffer[j]); }
      return true;
    }
  }
  for (unsigned j = 0; j < _next_literal_index; j++) { lit_unmark(_literal_buffer[j]); }
  return false;
}

bool napsat::NapSAT::root_level_conflict()
{
  // check if there is a conflict at level 0
  for (Tclause conflict : _conflicts) {
    if (clause_level(conflict) == LEVEL_ROOT) {
      // the conflict cannot be repaired
      _status = UNSAT;
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

size_t napsat::NapSAT::compute_backtrack_possibilities(const std::vector<Tclause>& conflicts,
                                                       std::vector<bitset>& possibilities)
{
  ASSERT(possibilities.empty());
  vector<bitset> conflict_chunks;

  for (Tclause conflict : conflicts) {
    bitset cl_chunks = clause_chunks(conflict);
    conflict_chunks.push_back(cl_chunks);
  }

  vector<bitset> prefixes;
  prefixes.push_back(bitset(_n_allocated_chunks));

  while (!prefixes.empty()) {
    bitset prefix = prefixes.back();
    prefixes.pop_back();
    bitset remaining(_n_allocated_chunks);

    // Calculate which chunks can still be required
    for (const bitset& cl_chunks : conflict_chunks) {
      if (prefix.has_intersection(cl_chunks))
        continue;
      remaining |= cl_chunks;
    }

    if (remaining.empty()) {
      possibilities.push_back(prefix);
      continue;
    }
    // expand the prefix
    for (auto it = remaining.cbegin(); it != remaining.cend(); ++it) {
      bitset next = prefix;
      next.set(*it, true);
      prefixes.push_back(next);
    }
  }

  backtracked_chunks_subsumption(possibilities);

  enhance_backtrack_possibilities_with_lazy_merging(possibilities);

  return split_learning_possibilities(possibilities);
}

void napsat::NapSAT::calculate_bitset_weights(std::vector<Tweight>& weights)
{
  // sort the weights by highest lowest level
  std::sort(weights.begin(), weights.end(),
            [](const Tweight& a, const Tweight& b) { return a < b; });

  double lowest_learning = std::numeric_limits<double>::max();
  double lowest_no_learning = lowest_learning;

  for (Tweight& w : weights) {
    // we go right to left
    size_t end = decision_lit_ptr(w.highest_level) - _trail.data();
    while (w.give_up_point > end) {
      ASSERT(w.give_up_point <= _trail.size());
      size_t i = --w.give_up_point;
      Tlit lit = _trail[i];
      if (lit_chunks(lit).has_intersection(w.chunks)) {
        w.total_weight += _backtrack_cost_estimator(lit);

        if (w.maybe_learning && w.total_weight > lowest_learning) {
          // stop early, we know that this is not the best
          break;
        } else if (!w.maybe_learning && w.total_weight > lowest_no_learning) {
          // stop early, we know that this is not the best
          break;
        }
      }
    }
    w.finished = (w.give_up_point == end);
    if (w.finished) {
      if (w.maybe_learning && w.total_weight < lowest_learning) {
        lowest_learning = w.total_weight;
      } else if (!w.maybe_learning && w.total_weight < lowest_no_learning) {
        lowest_no_learning = w.total_weight;
      }
    }
  }
  // sort again, such that the best candidate is first
  std::sort(weights.begin(), weights.end(),
            [](const Tweight& a, const Tweight& b) { return a < b; });
}

bool napsat::NapSAT::conflict_is_UIP_cut(Tclause conflict, const bitset& chunks)
{
  bool found = false;
  const Tlit* lits = clause_lits(conflict);
  for (unsigned i = 0; i < clause_size(conflict); i++) {
    if (chunks.has_intersection(lit_chunks(lits[i]))) {
      if (found)
        return false;
      found = true;
    }
  }
  ASSERT_MSG(found, "The conflict " + clause_to_string(conflict) + " does not contain any chunk from " + chunks.to_string());
  return found;
}

bool napsat::NapSAT::conflict_is_UIP_cut(Tclause conflict, Tlevel level)
{
  ASSERT(!_options.graph_backtracking);
  ASSERT(conflict != CLAUSE_UNDEF);
  ASSERT(_clauses[conflict].size > 0);
  const Tlit* lits = clause_lits(conflict);
  Tlevel high_lvl = lit_level(lits[0]);
  unsigned count = 1;
  for (unsigned i = 1; i < clause_size(conflict); i++) {
    Tlevel lvl = lit_level(lits[i]);
    if (lvl == high_lvl)
      count++;
    if (lvl > high_lvl) {
      high_lvl = lvl;
      count = 1;
    }
  }
  return count == 1;
}

bool napsat::NapSAT::lit_is_required_in_learned_clause(Tlit lit)
{
  if (lit_decision(lit))
    return true;
  ASSERT(lit_reason(lit) < _clauses.size());
  TSclause& clause = _clauses[lit_reason(lit)];
  ASSERT(!clause.deleted);
  for (unsigned i = 1; i < clause.size; i++)
    if (!lit_marked(clause.lits[i]))
      return true;
  return false;
}

bool napsat::NapSAT::lit_analyzed(Tlit lit, Tlevel level)
{
  ASSERT(!_options.graph_backtracking);
  return lit_level(lit) == level;
}

bool napsat::NapSAT::lit_analyzed(Tlit lit, const bitset& chunks)
{
  ASSERT(_options.graph_backtracking);
  return _options.graph_backtracking && lit_chunks(lit).has_intersection(chunks);
}

template <typename T>
void NapSAT::analyze_conflict_impl(T level) {
#ifndef NDEBUG
  // check that all variables are unmarked
  for (unsigned i = 0; i < _vars.size(); i++) {
    ASSERT(!var_marked(Tvar(i)));
  }
#endif

  unsigned count = 0;

  for (unsigned i = 0; i < _next_literal_index; i++) {
    Tlit lit = _literal_buffer[i];
    ASSERT(lit_false(lit));
    if (!lit_marked(lit) && lit_analyzed(lit, level)) {
      // it could be that the literal is duplicated in the buffer
      // in this case, we do not want to count it twice
      count++;
    }
    lit_mark(lit);
  }

  // We need to clear the literal buffer now.
  // The information is held in the "marked" markers
  _next_literal_index = 0;
  ASSERT(count > 0);
  unsigned i = _trail.size();

  while (i > 0) {
    Tlit lit = _trail[--i];
    if (!lit_marked(lit))
      continue;
    if (!lit_analyzed(lit, level))
      continue;
    lit_unmark(lit);
    bump_var_activity(lit_to_var(lit));
    // We already have the FUIP. No need to check the reason
    // We just need to finish collecting the literals that are marked.
    if (count == 1) {
      // this is the UIP
      _literal_buffer[_next_literal_index++] = lit_neg(lit);
      break;
    }
    // mark the literals of the reason
    Tclause reason = lit_reason(lit);
    if (lit_lazy_reason(lit) != CLAUSE_UNDEF) {
      reason = lit_lazy_reason(lit);
      // if we use the lazy reason, we need to ensure that we will look at all the literals in the clause
      // since the missed lower implication does not satisfy the trail invariant, we need to push the reading head to the back
      // note that this may lead to duplicate literals in the learned clause, but this will be cleaned up in the "internal_add_clause" function
      i = _trail.size();
    }
    ASSERT(reason != CLAUSE_UNDEF);
    if (_proof)
      _proof->link_resolution(lit_neg(lit), reason);

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
  }
  ASSERT(count == 1);


  // collect all the marked literals
  for (size_t i = 0; i < _trail.size(); i++) {
    Tlit lit = _trail[i];
    if (!lit_marked(lit))
      continue;
    lit_unmark(lit);
    bump_var_activity(lit_to_var(lit));
    if(lit_is_required_in_learned_clause(lit)) {
      _literal_buffer[_next_literal_index++] = lit_neg(lit);
    } else {
      if (_proof) {
        _proof->link_resolution(lit_neg(lit), lit_reason(lit));
      }
    }
  }

  // clean up the literals at level 0
  if (_proof) {
    prove_root_literal_removal(_literal_buffer, _next_literal_index);
  }

  unsigned k = 0;
  for (unsigned i = 0; i < _next_literal_index; i++) {
    Tlit lit = _literal_buffer[i];
    if (lit_level(lit) == LEVEL_ROOT) {
      // we do not want to add the root literals to the learned clause
      continue;
    }
    _literal_buffer[k++] = lit;
  }
  _next_literal_index = k;
}

void NapSAT::analyze_conflict(Tlevel l) { analyze_conflict_impl(l); };
void NapSAT::analyze_conflict(const bitset& b) { analyze_conflict_impl(b); };


Tlevel napsat::NapSAT::compute_repair_level()
{
  Tlevel min_level = LEVEL_UNDEF;
  for (Tclause conflict : _conflicts) {
    const Tlit* lits = clause_lits(conflict);
    ASSERT(lit_is_max_literal(lits[0], lits + 1, clause_size(conflict) - 1));
    min_level = std::min(min_level, lit_level(lits[0]));
  }
  return min_level;
}

void napsat::NapSAT::fix_watched_literals(Tclause conflict)
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
  ASSERT(second_highest_lit != nullptr);
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

void napsat::NapSAT::repair_unary_clause_conflict(Tclause conflict)
{
  ASSERT(conflict != CLAUSE_UNDEF);
  ASSERT(_clauses[conflict].external);
  ASSERT(clause_size(conflict) == 1);
  Tlit lit = clause_lits(conflict)[0];
  if (lit_level(lit) == LEVEL_ROOT) {
    // the clause is empty under the current assignment
    _status = UNSAT;
    return;
  }
  if (_options.graph_backtracking) {
    // TODO use chunk weights here
    bitset undone_chunks = lit_chunks(lit);
    backtrack(undone_chunks);
    ASSERT(lit_undef(lit));
  } else {
    Tlevel backtrack_level = choose_backtracked_level(clause_lits(conflict), 1);
    backtrack(backtrack_level);
  }
  imply_literal(lit, conflict);
}

void NapSAT::repair_conflicts()
{
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
  ASSERT(_options.exhaustive_conflict_search || _conflicts.size() == 1);

  // in general, a conflict may appear twice in the list.
  // clean up duplicates
  std::sort(_conflicts.begin(), _conflicts.end());
  _conflicts.erase(std::unique(_conflicts.begin(), _conflicts.end()), _conflicts.end());

  if (_status == SAT)
    _status = UNDEF;

  for (Tclause conflict : _conflicts) {
    NOTIFY_OBSERVER(_observer, new napsat::gui::conflict(conflict));
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

  _conflicts.clear();
  _var_activity_increment /= _options.var_activity_decay;
}

bool napsat::NapSAT::propagating_after_analysis(Tclause conflict, const bitset& chunks)
{
  bitset conflict_chunks = clause_chunks(conflict);
  conflict_chunks &= chunks;
  if (!_options.lazy_chunk_merging) {
    return conflict_chunks.count() == 1;
  }
  unsigned chunk_count = conflict_chunks.count();
  for (auto it = conflict_chunks.cbegin(); it != conflict_chunks.cend(); ++it) {
    Tchunk chunk = *it;
    const bitset& reimplied_chunks = _chunks[chunk].missed_implication;
    if (conflict_chunks.has_intersection(reimplied_chunks)) {
      chunk_count--;
    }
  }
  return chunk_count == 1;
}

bool napsat::NapSAT::propagating_after_analysis(Tclause conflict, Tlevel level)
{
  if (!_options.chronological_backtracking) {
    return true;
  }

  const Tlit* lits = clause_lits(conflict);
  for (unsigned i = 1; i < clause_size(conflict); i++) {
    if (lit_level(lits[i]) > level) {
      return false;
    }
  }
  return true;
}

bool napsat::NapSAT::implication_active_after_backtrack(Tclause conflict, Tlevel level)
{
  return lit_level(clause_lits(conflict)[1]) <= level;
}

bool napsat::NapSAT::implication_active_after_backtrack(Tclause conflict, const bitset& chunks)
{
  Tlit* lits = clause_lits(conflict);
  for (unsigned i = 1; i < clause_size(conflict); i++) {
    if (chunks.has_intersection(lit_chunks(lits[i]))) {
      return false;
    }
  }
  return false;
}

void napsat::NapSAT::add_and_delete_clause(Tclause cl, const std::vector<Tlit>& clause)
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

void napsat::NapSAT::compute_subsumed_clauses(const vector<pair<Tclause, vector<Tlit>>>& clauses, std::vector<bool>& subsumed)
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
        subsumed[i] = true;
        break;
      }
    }
    for (Tlit lit : clauses[i].second) { lit_unmark(lit); }
  }
}

const bitset& napsat::NapSAT::update_bt_after_analysis(const bitset& chunks)
{
  return chunks;
}

Tlevel napsat::NapSAT::update_bt_after_analysis(Tlevel level)
{
  Tlevel new_level = LEVEL_ROOT;
  for (unsigned j = 0; j < _next_literal_index; j++) {
    new_level = std::max(new_level, lit_level(_literal_buffer[j]));
  }
  ASSERT(new_level < level);
  return new_level;
}

template<typename T>
void napsat::NapSAT::try_and_learn_impl(T bt, vector<pair<Tclause, vector<Tlit>>>& learned_clauses)
{
  vector<Tclause> implying_conflicts;

  for (Tclause conflict : _conflicts) {
    if (!propagating_after_analysis(conflict, bt)) {
      continue;
    }
    if (conflict_is_UIP_cut(conflict, bt)) {
      implying_conflicts.push_back(conflict);
      continue;
    }

    // we try to learn a clause
    ASSERT(_next_literal_index == 0);
    Tlit* lits = clause_lits(conflict);
    for (unsigned j = 0; j < clause_size(conflict); j++) {
      Tlit lit = lits[j];
      ASSERT(lit_false(lit));
      _literal_buffer[_next_literal_index++] = lit;
    }
    if (_proof) {
      _proof->start_resolution_chain();
      _proof->link_resolution(LIT_UNDEF, conflict);
    }

    T bt_save = bt;
    do {
      analyze_conflict(bt);

      // check if the uip is a missed implication
      Tlit uip = _literal_buffer[0];
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
      if (_proof) {
        _proof->link_resolution(lit_neg(uip), lazy_reason);
      }
      const Tlit* reason_lits = clause_lits(lazy_reason);
      for (unsigned j = 1; j < clause_size(lazy_reason); j++) {
        _literal_buffer[_next_literal_index++] = reason_lits[j];
      }
      // remove the uip from the learned clause
      _literal_buffer[0] = _literal_buffer[--_next_literal_index];

      bt = update_bt_after_analysis(bt);
    } while (true);
    bt = bt_save;

    // check if the clause is new
    if (learned_clause_is_redundant()) {
      if (_proof) {
        _proof->cancel_resolution_chain();
      }
      _next_literal_index = 0;
      continue;
    }
    // check if the clause is already learned
    bool already_learned = false;
    for (unsigned j = 0; j < _next_literal_index; j++) { lit_mark(_literal_buffer[j]); }

    for (const pair<Tclause, vector<Tlit>>& id_clause : learned_clauses) {
      const vector<Tlit>& clause = id_clause.second;
      if (clause.size() != _next_literal_index)
        continue;
      bool all_found = true;
      for (unsigned j = 0; j < clause.size() && all_found; j++) { all_found &= lit_marked(clause[j]); }
      if (all_found) {
        already_learned = true;
        break;
      }
    }
    for (unsigned j = 0; j < _next_literal_index; j++) { lit_unmark(_literal_buffer[j]); }
    if (already_learned) {
      if (_proof) {
        _proof->cancel_resolution_chain();
      }
      _next_literal_index = 0;
      continue;
    }

    // add the clause
    vector<Tlit> learned_clause(_literal_buffer, _literal_buffer + _next_literal_index);
    Tclause id = next_clause_id(_next_literal_index);
    learned_clauses.push_back({id, learned_clause});
    _next_literal_index = 0;
    if (_proof) {
      _proof->finalize_resolution(id, learned_clause.data(), learned_clause.size());
    }
  }
}

void napsat::NapSAT::try_and_learn(const bitset& chunks, vector<pair<Tclause, vector<Tlit>>>& learned_clauses) {
  // remove conflicts that will not propagate after backtrack
  vector<Tclause> filtered_conflicts;
  for (size_t i = 0; i < _conflicts.size(); i++) {
    if (!propagating_after_analysis(_conflicts[i], chunks)) {
      filtered_conflicts.push_back(_conflicts[i]);
      _conflicts[i--] = _conflicts.back();
      _conflicts.pop_back();
    }
  }

  try_and_learn_impl(chunks, learned_clauses);

  // restore the conflicts
  for (Tclause c : filtered_conflicts) {
    _conflicts.push_back(c);
  }
}
void napsat::NapSAT::try_and_learn(Tlevel level, vector<pair<Tclause, vector<Tlit>>>& learned_clauses) {
  try_and_learn_impl(level, learned_clauses);
}

void napsat::NapSAT::graph_repair()
{
  vector<bitset> possibilities;
  size_t maybe_learning = compute_backtrack_possibilities(_conflicts, possibilities);
  ASSERT(!possibilities.empty());

  // Calculate the highest chunk involved in conflicts
  vector<Tweight> weights;
  weights.reserve(possibilities.size());

  Tlevel highest_level = LEVEL_ROOT;

  for (size_t i = 0; i < possibilities.size(); i++) {
    Tweight w;
    w.chunks = possibilities[i];
    w.maybe_learning = i < maybe_learning;
    w.give_up_point = _trail.size();

    for (auto it = w.chunks.cbegin(); it != w.chunks.cend(); ++it) {
      Tlevel cl = chunk_level(*it);
      w.highest_level = std::max(w.highest_level, cl);
      w.lowest_level = std::min(w.lowest_level, cl);
    }
    highest_level = std::max(highest_level, w.highest_level);
    weights.push_back(w);
  }

  vector<Tclause> implying_conflicts;
  vector<pair<Tclause, vector<Tlit>>> learned_clauses;
  bitset undone_chunks;

  do {
    implying_conflicts.clear();
    learned_clauses.clear();

    // filter out weights that cannot learn and are not at the highest level
    for (size_t i = 0; i < weights.size(); i++) {
      if (weights[i].highest_level < highest_level && !weights[i].maybe_learning) {
        weights[i] = weights.back();
        weights.pop_back();
        i--;
      }
    }

    calculate_bitset_weights(weights);
    ASSERT(maybe_learning <= weights.size());

    // the top element is the best one because it was sorted by the calculate_bitset_weights function
    ASSERT(!weights.empty());
    Tweight& best = weights[0];
    undone_chunks = best.chunks;

    if (best.maybe_learning) {
      try_and_learn(undone_chunks, learned_clauses);

      if (!learned_clauses.empty() || best.highest_level == highest_level) {
        break;
      }
      // learning was not possible
      best.maybe_learning = false;
      maybe_learning--;
    }
    if (maybe_learning == 0) {
      // we have tried all the learning possibilities
      break;
    }
  } while (true);

  backtrack(undone_chunks);

  fix_conflicts_and_learned_in_order(_conflicts, learned_clauses);

}

void napsat::NapSAT::level_repair()
{
  // chronological backtracking
  Tlevel repair_level = compute_repair_level();

  vector<pair<Tclause, vector<Tlit>>> learned_clauses;

  try_and_learn(repair_level, learned_clauses);

  Tlevel bt = LEVEL_UNDEF;
  for (Tclause conflict : _conflicts) {
    bt = min(choose_backtracked_level(clause_lits(conflict), clause_size(conflict)), bt);
  }
  for (pair<Tclause, vector<Tlit>>& id_clause : learned_clauses) {
    vector<Tlit>& clause = id_clause.second;
    bt = min(choose_backtracked_level(clause.data(), clause.size()), bt);
  }

  backtrack(bt);

  fix_conflicts_and_learned_in_order(_conflicts, learned_clauses);
}
