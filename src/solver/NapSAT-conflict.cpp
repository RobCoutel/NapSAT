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
    if (var_lazy_reason(_chunks[*i].decision)) {
      processed.set(*i, true);
      diff.set(*i, false);
    }
  }

  if (diff.empty()) {
    combinations.push_back(current);
    return;
  }

  for (auto i = diff.cbegin(); i != diff.cend(); ++i) {
    Tclause missed_implication = var_lazy_reason(_chunks[*i].decision);
    ASSERT(missed_implication != CLAUSE_UNDEF);


    bitset merged_chunks = clause_chunks(missed_implication);
    for (auto j = merged_chunks.cbegin(); j != merged_chunks.cend(); ++j) {
      bitset next_process = processed;
      bitset next_current = current;
      next_current.set(*j, true);
      if (var_lazy_reason(_chunks[*j].decision)) {
        next_process.set(*j, true);
      }
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
  size_t non_learning = 0;
  for (size_t i = 0; i < possibilities.size(); i++) {
    bool learning_found = false;
    for (Tclause conflict : _conflicts) {
      if (!conflict_is_UIP_cut(conflict, possibilities[i])) {
        learning_found = true;
        break;
      }
    }
    if (!learning_found) {
      non_learning++;
      bitset tmp = possibilities[i];
      size_t idx = possibilities.size() - non_learning;
      possibilities[i] = possibilities[idx];
      possibilities[idx] = tmp;
      i--;
    }
  }
  return non_learning;
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

  double lowest_weight = weights.back().total_weight;
  double lowest_no_learning = lowest_weight;

  for (Tweight& w : weights) {
    if (w.finished) {
      if (w.maybe_learning && w.total_weight < lowest_weight) {
        lowest_no_learning = w.total_weight;
      } else if (!w.maybe_learning && w.total_weight < lowest_no_learning) {
        lowest_no_learning = w.total_weight;
      }
    }

    // we go right to left
    size_t end = _decision_index[w.lowest_level - 1];
    while (--w.give_up_point >= end) {
      size_t i = w.give_up_point;
      Tlit lit = _trail[i];
      if (lit_chunks(lit).has_intersection(w.chunks)) {
        w.total_weight += _backtrack_cost_estimator(lit);

        if (w.maybe_learning && w.total_weight > lowest_weight) {
          // stop early, we know that this is not the best
          break;
        } else if (!w.maybe_learning && w.total_weight > lowest_no_learning) {
          // stop early, we know that this is not the best
          break;
        }
      }
    }
  }
  // sort again, such that the best candidate is first
  std::sort(weights.begin(), weights.end(),
            [](const Tweight& a, const Tweight& b) { return a < b; });
}

bool napsat::NapSAT::conflict_is_UIP_cut(Tclause conflict, bitset& chunks)
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
  ASSERT(found);
  return found;
}

bool napsat::NapSAT::conflict_is_UIP_cut(Tclause conflict)
{
  ASSERT(!_options.graph_backtracking);
  ASSERT(conflict != CLAUSE_UNDEF);
  ASSERT(_clauses[conflict].size > 0);
  const Tlit* lits = clause_lits(conflict);
  Tlevel high_lvl = lit_level(lits[0]);
  unsigned count = 1;
  for (unsigned i = 1; i < clause_size(conflict); i++) {
    Tlevel lvl = lit_level(lits[i]);
    if (lvl == high_lvl) {
      count++;
      if (count > 1)
        return false;
    } else if (lvl > high_lvl) {
      high_lvl = lvl;
      count = 1;
    }
  }
  return true;
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

  if (_options.graph_backtracking) {
    // If could be that the undefined literal is not the first one
    // We need to find the undefined literal and put it at the first position
    Tlit* undef_lit = nullptr;
    for (Tlit* i = lits; i < end; i++) {
      if (lit_undef(*i)) {
        undef_lit = i;
        break;
      }
    }
    ASSERT(undef_lit != nullptr);
    ASSERT(lit_undef(*undef_lit));
    if (undef_lit == lits + 1) {
      // just swap the first two literals
      Tlit tmp = lits[0];
      lits[0] = lits[1];
      lits[1] = tmp;
    } else if (undef_lit > lits + 1) {
      // the undefined literal is somewhere else in the clause. We need to change the watched literals
      stop_watch(lits[0], conflict);
      Tlit tmp = lits[0];
      lits[0] = *undef_lit;
      *undef_lit = tmp;
      watch_lit(lits[0], conflict);
    }

    // Now, we bring a literal that is at the top of the chunk lattice to the second position
    // This is similar to the highest level in chronological backtracking, but we use the chunk instead
    Tlit* high_lit = lits + 1;
    unsigned chunk_count = lit_chunks(*high_lit).count();
    for (Tlit* i = lits + 2; i < end; i++) {
      unsigned curr_chunk_count = lit_chunks(*i).count();
      if (curr_chunk_count > chunk_count) {
        chunk_count = curr_chunk_count;
        high_lit = i;
      }
    }

    if (high_lit > lits + 1) {
      stop_watch(lits[1], conflict);
      Tlit tmp = lits[1];
      lits[1] = *high_lit;
      *high_lit = tmp;
      watch_lit(lits[1], conflict);
    }

  } else { // graph backtracking
    // Now, we bring a literal that is at the top of the chunk lattice to the second position
    // This is similar to the highest level in chronological backtracking, but we use the chunk instead
    Tlit* high_lit = lits + 1;
    Tlevel high_lvl = lit_level(*high_lit);
    for (Tlit* i = lits + 2; i < end; i++) {
      if (lit_level(*i) > high_lvl) {
        high_lvl = lit_level(*i);
        high_lit = i;
      }
    }

    if (high_lit > lits + 1) {
      stop_watch(lits[1], conflict);
      Tlit tmp = lits[1];
      lits[1] = *high_lit;
      *high_lit = tmp;
      watch_lit(lits[1], conflict);
    }
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
  ASSERT(!_options.exhaustive_conflict_search || _conflicts.size() == 1);

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
    // chronological backtracking
    Tlevel repair_level = compute_repair_level();

    for (Tclause conflict : _conflicts) {
      if (lit_level(clause_lits(conflict)[0]) > repair_level) {
        continue;
      }

      ASSERT(lit_level(clause_lits(conflict)[0]) == repair_level);
      vector<Tclause> implying_conflicts;
      vector<vector<Tlit>> learned_clauses;
      if (conflict_is_UIP_cut(conflict)) {
        implying_conflicts.push_back(conflict);
        continue;
      }

      // we try to learn a clause
      // fill in the literal buffer
      ASSERT(_next_literal_index == 0);
      Tlit* lits = clause_lits(conflict);
      for (unsigned j = 0; j < clause_size(conflict); j++) {
        Tlit lit = lits[j];
        ASSERT(lit_false(lit));
        _literal_buffer[_next_literal_index++] = lit;
      }

      do {
        analyze_conflict(repair_level);

        Tlit uip = _literal_buffer[0];
        Tclause lazy_reason = lit_lazy_reason(uip);
        if (lazy_reason == CLAUSE_UNDEF) {
          // we found the learned clause
          if (_proof) {
            _proof->finalize_resolution(_clauses.size() + learned_clauses.size(),
              _literal_buffer, _next_literal_index);
          }
          learned_clauses.push_back(vector<Tlit>(_literal_buffer, _literal_buffer + _next_literal_index));
          break;
        }
        Tlit* lazy_lits = clause_lits(lazy_reason);
        for (unsigned j = 0; j < clause_size(lazy_reason); j++) {
          Tlit lit = lazy_lits[j];
          if (lit == lit_neg(uip))
            continue;
          _literal_buffer[_next_literal_index++] = lit;
        }
        // remove the UIP from the learned clause
        _literal_buffer[0] = _literal_buffer[--_next_literal_index];
        if (_proof) {
          _proof->link_resolution(lit_neg(uip), lazy_reason);
        }
      } while(true);
    }

  }

  _var_activity_increment /= _options.var_activity_decay;
}

void napsat::NapSAT::graph_repair()
{
  vector<bitset> possibilities;
  size_t non_learning = compute_backtrack_possibilities(_conflicts, possibilities);
  size_t maybe_learning = possibilities.size() - non_learning;
  ASSERT(!possibilities.empty());

  // Calculate the highest chunk involved in conflicts
  vector<Tweight> weights;
  weights.reserve(possibilities.size());

  Tlevel highest_level = LEVEL_ROOT;

  for (size_t i = 0; i < possibilities.size(); i++) {
    Tweight w;
    w.chunks = possibilities[i];
    w.maybe_learning = i <= maybe_learning;
    w.give_up_point = _trail.size();

    for (auto it = w.chunks.cbegin(); it != w.chunks.cend(); ++it) {
      Tlevel cl = chunk_level(*it);
      w.highest_level = std::max(w.highest_level, cl);
      w.lowest_level = std::min(w.lowest_level, cl);
    }
    highest_level = std::max(highest_level, w.highest_level);
    weights.push_back(w);
  }

  // filter all the weights that cannot learn anything and are not at the highest level
  // they will never be picked
  for (size_t i = maybe_learning; i < weights.size(); i++) {
    if (weights[i].highest_level < highest_level && !weights[i].maybe_learning) {
      weights[i] = weights.back();
      weights.pop_back();
      i--;
    }
  }

  do {
    if (maybe_learning == 0) {
      // we know that no conflict will lead to learning
      // therefore, we need to backtrack the highest chunk in the trail
      // filter out all possibilities that do not include the highest chunk
      Tweight* i = weights.data();
      Tweight* j = i + weights.size();
      Tweight* end = i + weights.size();
      while (i < end) {
        if (i->highest_level < highest_level) {
          i++;
          continue;
        }
        *j++ = *i++;
      }
      weights.resize(j - weights.data());
    }

    calculate_bitset_weights(weights);

    // the top element is the best one
    ASSERT(!weights.empty());
    Tweight& best = weights[0];

    bitset& undone_chunks = best.chunks;
    bool* active_after_bt = new bool[_conflicts.size()];
    for (Tclause conflict : _conflicts) {
      bitset cl_chunks = clause_chunks(conflict);
      cl_chunks &= undone_chunks;
      // check if among the remaining chunks, some are lazily merged
      unsigned chunk_count = cl_chunks.count();
      for (auto it = cl_chunks.cbegin(); it != cl_chunks.cend(); ++it) {
        Tchunk chunk = *it;
        const bitset& reimplied_chunks = _chunks[chunk].missed_implication;
        if (cl_chunks.has_intersection(reimplied_chunks)) {
          chunk_count--;
        }
      }
      active_after_bt[conflict] = chunk_count > 1;
    }

    vector<Tclause> implying_conflicts;
    vector<vector<Tlit>> learned_clauses;

    if (best.maybe_learning) {
      // attempt conflict analysis. Then, check if the learned clause is new
      for (size_t i = 0; i < _conflicts.size(); i++) {
        if (!active_after_bt[_conflicts[i]])
          continue;

        if (conflict_is_UIP_cut(_conflicts[i], undone_chunks))
          continue;

        // we try to learn a clause
        // fill in the literal buffer
        ASSERT(_next_literal_index == 0);
        Tlit* lits = clause_lits(_conflicts[i]);
        for (unsigned j = 0; j < clause_size(_conflicts[i]); j++) {
          Tlit lit = lits[j];
          ASSERT(lit_false(lit));
          _literal_buffer[_next_literal_index++] = lit;
        }
        if (_proof) {
          _proof->start_resolution_chain();
          _proof->link_resolution(LIT_UNDEF, _conflicts[i]);
        }
        analyze_conflict(undone_chunks);

        // check if the clause is new
        if (learned_clause_is_redundant()) {
          implying_conflicts.push_back(_conflicts[i]);
          if (_proof) {
            _proof->cancel_resolution_chain();
          }
        }
        else {
          vector<Tlit> learned_clause(_literal_buffer, _literal_buffer + _next_literal_index);
          if (_proof) {
            _proof->finalize_resolution(_clauses.size() + learned_clauses.size(),
              learned_clause.data(), learned_clause.size());
          }
          learned_clauses.push_back(learned_clause);
        }
      }

      if (!learned_clauses.empty()) {
        // we found a learned clause
        // we need to backtrack and add the clause
        backtrack(undone_chunks);
        ASSERT(!implying_conflicts.empty() || !_conflicts.empty());
        for (Tclause conflict : implying_conflicts) {
          fix_watched_literals(conflict);
          imply_literal(clause_lits(conflict)[0], conflict);
        }
        for (const vector<Tlit>& lits : learned_clauses) {
          internal_add_clause(lits.data(), lits.size(), true, false);
        }
        delete[] active_after_bt;
        break;
      }
    }

    if (!best.highest_level) {
      // remove the possibility
      weights.erase(weights.begin());
      maybe_learning--;
      delete[] active_after_bt;
      continue;
    }

    // we backtrack without learning, but at the highest level
    backtrack(best.highest_level);
    for (size_t i = 0; i < _conflicts.size(); i++) {
      if (!active_after_bt[_conflicts[i]])
        continue;
      fix_watched_literals(_conflicts[i]);
      Tlit* lits = clause_lits(_conflicts[i]);
      ASSERT(lit_undef(lits[0]));
      if (lit_false(lits[1])) {
        imply_literal(lits[0], _conflicts[i]);
      }
    }
    break;
  } while (true);
}
