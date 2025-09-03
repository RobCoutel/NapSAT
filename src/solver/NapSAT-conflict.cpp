#include "NapSAT.hpp"

#include "custom-assert.hpp"

#include <cstring>
#include <iostream>

using namespace std;
using namespace napsat;

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
  // Computes the UIP at a given level
  // We assume that the level is a top level of the clause. That is, the level is is not reachable from a literal l at a higher level in the clause.
  ASSERT(level <= solver_level());

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
      ASSERT(lit_analyzed(lit, level));
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

  // ensure that the UIP is the first literal in the buffer
  // note that in NCB, this is automatically the case. The loop will fail at the first iteration
  for (unsigned i = 0; i < _next_literal_index; i++) {
    Tlit lit = _literal_buffer[i];
    ASSERT(lit_false(lit));
    if (lit_level(lit) == level) {
      // this is the UIP
      _literal_buffer[i] = _literal_buffer[0];
      _literal_buffer[0] = lit;
      break;
    }
  }
}

void NapSAT::analyze_conflict(Tlevel l) { analyze_conflict_impl(l); };
void NapSAT::analyze_conflict(const bitset& b) { analyze_conflict_impl(b); };


Tlevel napsat::NapSAT::compute_repair_level()
{
  Tlevel min_level = LEVEL_UNDEF;
  for (Tclause conflict : _conflicts) {
    const Tlit* lits = clause_lits(conflict);
    ASSERT(max_literal(lits[0], lits + 1, clause_size(conflict) - 1));
    min_level = std::min(min_level, lit_level(lits[0]));
  }
  return min_level;
}

bitset napsat::NapSAT::compute_repair_chunks()
{
  vector<pair<Tclause, bitset>> set_of_chunks;
  bitset to_return(_n_allocated_chunks);

  // gather all the possible chunk combinations
  for (Tclause conflict : _conflicts) {
    bitset chunks = clause_chunks(conflict);

    // check that there is not already an entry that covers chunks
    bool subsumed = false;
    for (const auto& [other_conflict, other_chunks] : set_of_chunks) {
      if (other_chunks >= chunks) {
        subsumed = true;
        break;
      }
    }
    if (!subsumed) {
      set_of_chunks.push_back({conflict, chunks});
    }
  }

  // generate all combinations of chunks with lazy merging
  vector<vector<bitset>> chunk_combinations;
  for (const auto& [conflict, chunks] : set_of_chunks) {
    vector<bitset> combination;
    compute_chunk_combination(conflict, combination, bitset(_n_allocated_chunks));
    chunk_combinations.push_back(combination);
  }

  // check if there are any chunks with no option (must be backtracked)
  // if yes, remove all other chunks that can use it for backtracking, since it will definitely be part of the restore set
  // we can also remove it here, since we log it in the to_return bitset
  for (unsigned i = 0; i < chunk_combinations.size(); i++) {
    if (chunk_combinations[i].size() == 1) {
      bitset& single_chunk = chunk_combinations[i][0];
      to_return |= single_chunk;
      for (unsigned j = 0; j < chunk_combinations.size(); j++) {
        if (find(chunk_combinations[j].begin(), chunk_combinations[j].end(), single_chunk) != chunk_combinations[j].end()) {
          chunk_combinations.erase(chunk_combinations.begin() + j);
          if (j < i) {
            i--;
          }
        }
      }
    }
  }

  // flatten and remove duplicates
  vector<pair<bitset, unsigned>> chunks_count;
  for (const auto& combination : chunk_combinations) {
    for (const auto& chunk : combination) {
      auto it = std::find_if(chunks_count.begin(), chunks_count.end(),
                             [&chunk](const auto& pair) { return pair.first == chunk; });
      if (it != chunks_count.end()) {
        it->second++;
      } else {
        chunks_count.emplace_back(chunk, 1);
      }
    }
  }

  // compute the lower level in the chunk set
  vector<Tlevel> chunks_level(chunks_count.size());
  for (size_t i = 0; i < chunks_count.size(); i++) {
    Tlevel level = LEVEL_ROOT;
    for (auto j = chunks_count[i].first.cbegin(); j != chunks_count[i].first.cend(); ++j) {
      level = std::max(level, var_level(_chunks[*j].decision));
    }
    chunks_level[i] = level;
  }

  // compute the weights of the chunks
  vector<double> weights;
  for (size_t i = 0; i < chunks_count.size(); i++) {
    bitset& chunks = chunks_count[i].first;
    Tlevel level = chunks_level[i];
    double weight = 0.0;
    size_t start_position = _decision_index[level - 1];
    for (size_t j = start_position; j < _trail.size(); j++) {
      weight += _backtrack_cost_estimator(_trail[j]);
    }
    weights.push_back(weight);
  }

  // compute the minimal coverage greedily
  // this is in general an NP-Hard problem, but we approximate by removing the least weighted chunks that occur in a lot of conflicts
  double max_weight = *max_element(weights.begin(), weights.end());

  while (!chunks_count.empty()) {
    bitset min_chunk = chunks_count[0].first;
    // the advantage is the gain compared to the worst possible choice (count) * (max_weight - weight)
    double max_advantage = weights[0];
    for (size_t i = 1; i < chunks_count.size(); i++) {
      double advantage = chunks_count[i].second * (max_weight - weights[i]);
      if (advantage > max_advantage) {
        max_advantage = advantage;
        min_chunk = chunks_count[i].first;
      }
    }

    to_return |= min_chunk;
    // remove all chunks from chunk_combination and recalculate chunks_count
    chunks_count.clear();
    for (unsigned i = 0; i < chunk_combinations.size(); i++) {
      const auto& combination = chunk_combinations[i];
      if (find(combination.begin(), combination.end(), min_chunk) != combination.end()) {
        chunk_combinations.erase(chunk_combinations.begin() + i);
        i--;
        continue;
      }
      for (const auto& chunk : combination) {
        auto it = std::find_if(chunks_count.begin(), chunks_count.end(),
                               [&chunk](const auto& pair) { return pair.first == chunk; });
        if (it != chunks_count.end()) {
          it->second++;
        } else {
          chunks_count.emplace_back(chunk, 1);
        }
      }
    }
  }

  return to_return;
}

void napsat::NapSAT::filter_deactivated_conflicts(bitset& undone_chunks)
{
  for (size_t i = 0; i < _conflicts.size(); i++) {
    Tclause conflict = _conflicts[i];
    bitset chunks = clause_chunks(conflict);
    chunks &= undone_chunks;

    ASSERT(!chunks.empty());
    // check if among the remaining chunks, some are lazily merged
    unsigned chunk_count = chunks.count();
    for (auto it = chunks.cbegin(); it != chunks.cend(); ++it) {
      Tchunk chunk = *it;
      const bitset& reimplied_chunks = _chunks[chunk].missed_implication;
      if (chunks.has_intersection(reimplied_chunks)) {
        chunk_count--;
      }
    }

    ASSERT(chunk_count > 0);
    if (chunk_count > 1) {
      // remove the conflict
      _conflicts[i--] = _conflicts.back();
      _conflicts.pop_back();
    }
  }
  ASSERT(!_conflicts.empty());
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
  ASSERT(_clauses[conflict].size == 1);
  Tlevel backtrack_level = LEVEL_ROOT;
  Tlit lit = clause_lits(conflict)[0];
  if (_options.graph_backtracking) {
    bitset undone_chunks = choose_analyzed_chunk(conflict);
    if (undone_chunks.empty()) {
      // The literal does not belong to any chunk, therefore, it does not depend on a decision and the conflict cannot be repaired
      _status = UNSAT;
      return;
    }
    bitset chunks(_n_allocated_chunks);
    if (_clauses[conflict].size > 1) {
      const Tlit* lits = clause_lits(conflict);
      lit_cross_chunks(lits[0]) |= undone_chunks; // ensure that the cross chunks are set
      lit_cross_chunks(lits[1]) |= undone_chunks; // ensure that the cross chunks are set
    }
    backtrack(undone_chunks);
    ASSERT(lit_undef(lit));
  } else {
    if (_options.chronological_backtracking)
      backtrack_level = lit_level(lit) - 1;
    backtrack(backtrack_level);
    // In strong chronological backtracking, the literal might have been implied again during reimplication
    // Therefore, we might need to trigger another conflict analysis
    ASSERT(_options.lazy_strong_chronological_backtracking || lit_undef(lit));
    if (!lit_undef(lit)) {
      // the problem is unsat
      // The literal could have been propagated by the reimplication
      _status = UNSAT;
      return;
    }
  }
  imply_literal(lit, conflict);
}

void NapSAT::repair_conflicts()
{
  /**
   * Precondition:
   * - The conflict clause C is conflicting with the current partial assignment π
   *    C, π ⊧ ⊥
   * - The conflict clause is not the empty clause
   *    |C| > 0
   * - The first literal in the conflict clause is the highest level literal
   *    δ(c₁) = δ(C)
  */

  /********** CHECKING PRECONDITIONS **********/
  ASSERT(!_conflicts.empty());
  ASSERT(!_options.exhaustive_conflict_search || _conflicts.size() == 1);
 #ifndef NDEBUG
  for (Tclause conflict : _conflicts) {
    ASSERT(conflict < _clauses.size());
    const Tlit* lits = clause_lits(conflict);
    ASSERT(clause_size(conflict) > 0);
    ASSERT_MSG(_options.chronological_backtracking || _options.graph_backtracking
            || _clauses[conflict].external
    || (lit_level(lits[0]) == solver_level()
     && lit_level(lits[1]) == solver_level()),
      "Conflict: " + clause_to_string(conflict) + "\nDecision level: " + to_string(solver_level()));
    for (unsigned i = 0; i < _clauses[conflict].size; i++) {
      ASSERT(lit_false(lits[i]));
      ASSERT(_options.graph_backtracking || lit_level(lits[i]) <= lit_level(lits[0]));
    }
  }
#endif

  if (_status == SAT)
    _status = UNDEF;

  for (Tclause conflict : _conflicts) {
    NOTIFY_OBSERVER(_observer, new napsat::gui::conflict(conflict));
    bump_clause_activity(conflict);
  }

  /** FIND REPAIR POSITIONS **/



  /********** UNIT CLAUSE **********/
  if (_clauses[conflict].size == 1) {
    repair_unary_clause_conflict(conflict);
    return;
  }


  // Copy the literals of the clause to the literal buffer
  ASSERT(!_writing_clause);
  ASSERT(_next_literal_index == 0);
  _writing_clause = true;
  for (unsigned i = 0; i < _clauses[conflict].size; i++) {
    Tlit lit = lits[i];
    ASSERT(lit_false(lit));
    _literal_buffer[_next_literal_index++] = lit;
  }

  // perform conflict analysis
  if (_proof) {
    _proof->start_resolution_chain();
    _proof->link_resolution(LIT_UNDEF, conflict);
  }

  bitset analyzed_chunks;

  bool identical = false;
  if (_options.graph_backtracking) {
    analyzed_chunks = choose_analyzed_chunk(conflict);
    if (analyzed_chunks.empty()) {
      // The conflict does not have a literal that belongs to a chunk, therefore, it cannot be repaired
      _status = UNSAT;
      if (_proof) {
        prove_root_literal_removal(_literal_buffer, _next_literal_index);
        _proof->finalize_resolution(_clauses.size(), nullptr, 0);
      }
      return;
    }
    if (!(identical = conflict_is_UIP_cut(conflict, analyzed_chunks))) {
      analyze_conflict(LEVEL_ROOT, analyzed_chunks);
      _next_literal_index = cleanup_duplicate_literals(_literal_buffer, _next_literal_index);
    }
  } else if (!(identical = conflict_is_UIP_cut(conflict))) {
    do {
      Tlevel conflict_level = lit_level(_literal_buffer[0]);
      if (conflict_level == LEVEL_ROOT) {
        // The conflict does not have a literal that belongs to a chunk, therefore, it cannot be repaired
        _status = UNSAT;
        if (_proof) {
          prove_root_literal_removal(_literal_buffer, _next_literal_index);
          _proof->finalize_resolution(_clauses.size(), nullptr, 0);
        }
        return;
      }

      analyze_conflict(conflict_level, analyzed_chunks);
      // in lazy strong chronological backtracking, if the UIP is a missed lower implication, we need to recalculate the conflict level
      Tclause lazy_reason = lit_lazy_reason(_literal_buffer[0]);
      if (lazy_reason == CLAUSE_UNDEF) {
        // The UIP is not a missed lower implication, we can stop the analysis
        break;
      }

      // this should not be necessary. The duplicates cannot touch the UIP
      // _next_literal_index = cleanup_duplicate_literals(_literal_buffer, _next_literal_index);

      if (_proof)
        _proof->link_resolution(_literal_buffer[0], lazy_reason);
      // The UIP is a missed lower implication, we need to reanalyze the conflict
      // Replace the UIP by its lazy reason
      ASSERT(lit_level(_literal_buffer[0]) == conflict_level);
      _literal_buffer[0] = _literal_buffer[--_next_literal_index];
      const Tlit* uip_lazy_lits = clause_lits(lazy_reason);
      // Note that this may introduce duplicate literals in the learned clause, but they will be cleaned up when marking the literals
      for (unsigned i = 1; i < clause_size(lazy_reason); i++) {
        _literal_buffer[_next_literal_index++] = uip_lazy_lits[i];
      }
      // find the highest level and bring it to the front
      for (unsigned i = 1; i < _next_literal_index; i++) {
        if (lit_level(_literal_buffer[i]) > lit_level(_literal_buffer[0])) {
          Tlit tmp = _literal_buffer[0];
          _literal_buffer[0] = _literal_buffer[i];
          _literal_buffer[i] = tmp;
        }
      }
    } while (true);
  }

  // Later, if the clause is identical, we do not add it to the clause set.

  if (_options.graph_backtracking) {
    backtrack(analyzed_chunks);
  } else {
    // make sure that the second literal is at the second highest level
    for (unsigned i = 2; i < _next_literal_index; i++) {
      if (lit_level(_literal_buffer[i]) > lit_level(_literal_buffer[1])) {
        Tlit tmp = _literal_buffer[1];
        _literal_buffer[1] = _literal_buffer[i];
        _literal_buffer[i] = tmp;
      }
    }
    ASSERT(_next_literal_index <= 1 || lit_level(_literal_buffer[1]) <= lit_level(_literal_buffer[0]));
    Tlevel backtrack_level = choose_backtracked_level(_literal_buffer, _next_literal_index);
    if (backtrack_level == LEVEL_UNDEF) {
      // The conflict cannot be repaired, therefore, we need to stop the search
      _status = UNSAT;
      /**
       * Note that we will still backtrack to the root level, otherwise we might falsify the invariants on watched literals.
       * indeed, the conflict may have swapped the watched literal for an already propagated literal.
       * for example, let the clause a v b v c watched by a and b not propagated.
       * if -c is propagated at level 2, and -a and -b are at level 1, then the
       * clause will change its watched literal to -c, and the clause will be
       * c v a v b, but c is propagated, hence violating strong watched literals.
       *
       * In general this is okay since we will backtrack c, but not if we interrupt
       * and do not backtrack.
       */
      backtrack(LEVEL_ROOT);
      if (_proof) {
        _proof->finalize_resolution(_clauses.size(), _literal_buffer, _next_literal_index);
      }
      return;
    }

    backtrack(backtrack_level);
    ASSERT(lit_undef(_literal_buffer[0]));
    ASSERT(_next_literal_index == 1 || !lit_undef(_literal_buffer[1]));
  }

  if (identical) {
    if (_proof)
      _proof->cancel_resolution_chain();
    _writing_clause = false;
    _next_literal_index = 0;

    if (clause_falsified(conflict)) {
      repair_conflicts(conflict);
    } else {
      fix_watched_literals(conflict);

      // we need to imply the literal
      imply_literal(clause_lits(conflict)[0], conflict);
    }

  } else {
    NOTIFY_OBSERVER(_observer, new napsat::gui::stat("Learned clause: "));
    Tclause learned = internal_add_clause(_literal_buffer, _next_literal_index, true, false);
    if (_proof)
      _proof->finalize_resolution(learned, _literal_buffer, _next_literal_index);

    _writing_clause = false;
    _next_literal_index = 0;
    // finalizing the clause will also imply the first literal of the clause
  }

  _var_activity_increment /= _options.var_activity_decay;
}
