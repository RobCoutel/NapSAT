
/*
 * This file is part of the source code of the software program
 * NapSAT. It is protected by applicable copyright laws.
 *
 * This source code is protected by the terms of the MIT License.
 */
/**
 * @file src/solver/NapSAT-imply.cpp
 * @author Robin Coutelier
 *
 * @brief This file is part of the NapSAT solver. It implements the CDCL algorithm and .
 */
#include "NapSAT.hpp"

#include "custom-assert.hpp"

using namespace std;

namespace napsat {

void NapSAT::imply_literal(Tlit lit, Tclause reason)
{
  /**
   * Preconditions:
   * - lit ℓ is undefined
   *    ℓ ∉ π
   * - the reason C is either undefined or a propagating clause
   *    C = ■ ∨ [ℓ ∈ C ∧ C \ {ℓ}, π ⊧ ⊥]
   * - the first literal of the clause is ℓ
   *    C ≠ ■ ⇒ C[0] = ℓ
   * STANDARD BACKTRACKING :
   * - the second literal of the clause is at the highest level in C
   *    δ(ℓ) = δ(C \ {ℓ})
   * GRAPH BACKTRACKING :
   * - the second literal of the clause is a top element of the lattice
   * while not strictly necessary, this condition is useful to reduce the
   * number of repropagations.
   */
  ASSERT(lit_undef(lit));
  ASSERT(reason == CLAUSE_UNDEF || check_clause_unit(reason));

  if (_current_order == ORDER_RESET) {
    defragment_order();
  }

  _trail.push_back(lit);

  Tvar var = lit_to_var(lit);
  TSvar& svar = _vars[var];
  svar.order = _current_order++;
  svar.state = lit_pol(lit);
  svar.propagated = false;
  svar.reason = reason;
  svar.phase_cache = lit_pol(lit);

  // for the logic, look at the comment in NapSAT.hpp

  if (reason == CLAUSE_UNDEF) {
    // Decision
    _decision_index.push_back(_trail.size() - 1);
    svar.level = solver_level();
    NOTIFY_OBSERVER(decision, lit);
    if (_options.graph_backtracking) {
      if (_free_chunks.empty()) {
        allocate_chunks(2 * _n_allocated_chunks);
      }
      Tchunk chunk_number = _free_chunks.back();
      ASSERT (_n_allocated_chunks == _chunks.size());
      ASSERT_MSG(chunk_number < _n_allocated_chunks,
        "Chunk number: " + std::to_string(chunk_number) +
        "\nNumber of allocated chunks: " + std::to_string(_n_allocated_chunks));
      _free_chunks.pop_back();
      svar.chunks.set(chunk_number, true);
      _chunks[chunk_number].decision = var;
      ASSERT_MSG(_chunks.size() == solver_level() + _free_chunks.size(),
        "Chunks size: " + std::to_string(_chunks.size()) +
        "\nSolver level: " + std::to_string(solver_level()) +
        "\nFree chunks size: " + std::to_string(_free_chunks.size()));
    }
  }
  else if (reason == CLAUSE_LAZY) {
    // Theory propagation
    ASSERT_MSG(false, "Lazy reason is not implemented yet");
  }
  else {
    // Implied literal
    const Tlit* lits = clause_lits(reason);
    const unsigned size = clause_size(reason);
    ASSERT(lit == lits[0]);
    ASSERT(check_clause_implying(reason));
    ASSERT(size < 2 || lit_is_max_literal(lits[1], lits + 2, size - 2));

    if (clause_size(reason) == 1) {
      svar.level = LEVEL_ROOT;
    } else {
      if (_options.graph_backtracking) {
        // In graph backtracking we do not have any information about the levels
        // we need to compute it by going through the clause
        svar.level = implication_level(reason);
        recompute_chunks(lit);
        if (size > 2) {
          lit_cross_chunks(lits[1]) |= svar.chunks;
        }
      } else {
        svar.level = lit_level(lits[1]);
      }
    }
    NOTIFY_OBSERVER(implication, lit, reason, svar.level);
  }

  if (svar.level == LEVEL_ROOT) {
    _n_root_lvl_lits++;
    if (_proof)
      _proof->root_assign(lit, reason);
  }
  ASSERT(svar.level != LEVEL_UNDEF);
  ASSERT(svar.level <= solver_level());
}

void napsat::NapSAT::reimply_literal_root(Tlit lit, Tclause reason)
{
  ASSERT(check_decision_index_consistency());
  ASSERT(lit_true(lit));
  ASSERT(reason != CLAUSE_UNDEF && reason != CLAUSE_LAZY);
  ASSERT(lit == clause_lits(reason)[0]);
  ASSERT(clause_size(reason) == 1);
  if (lit_level(lit) == LEVEL_ROOT)
    return;

  if (!_options.chronological_backtracking && !_options.graph_backtracking) {
    // Non-chronological backtracking
    Tlevel backtrack_level = LEVEL_ROOT;
    backtrack(backtrack_level);
    imply_literal(lit, reason);
    return;
  }

  // search the literal in the trail
  Tlevel old_level = lit_level(lit);
  ASSERT(old_level != LEVEL_UNDEF);
  // first literal at level after the decision (we do not want to count the decision to increment the level)
  size_t pos = _decision_index[old_level-1];
  for (; pos < _trail.size(); pos++)
    if (_trail[pos] == lit)
      break;
  ASSERT(pos < _trail.size());

  // if the literal is a decision, we will need to update the decision index
  bool squash_level = lit_decision(lit);
  if (squash_level) {
    // we need to free the chunk of the decision
    ASSERT(lit_chunks(lit).count() == 1);
    Tchunk ck = *lit_chunks(lit).cbegin();
    _locked_chunks.set(ck, false);
    _free_chunks.push_back(ck);
    _chunks[ck].decision = VAR_UNDEF;
    _chunks[ck].missed_implication.clear();
    _decision_index.resize(_decision_index.size() - 1);
  }

  // update the level of the literal to root
  lit_level(lit) = LEVEL_ROOT;
  NOTIFY_OBSERVER(update_level, lit, LEVEL_ROOT);
  lit_reason(lit) = reason;
  NOTIFY_OBSERVER(update_reason, lit, reason);

  // this does nothing outside of GB, but it does not hurt either
  bitset old_chunks = lit_chunks(lit);
  lit_chunks(lit).clear();

  // fix the trail
  if (!_options.graph_backtracking) {
    for (size_t i = pos + 1; i < _trail.size(); i++) {
      Tlit l = _trail[i];

      if (squash_level) {
        if (lit_level(l) > old_level) {
          lit_level(l)--;
          NOTIFY_OBSERVER(update_level, l, lit_level(l));
        }
      }

      if (lit_decision(l)) {
        ASSERT_MSG(lit_level(l) >= old_level,
                   "Literal " + lit_to_string(l) + " at position " + std::to_string(i) +
                   " should have level greater than " + std::to_string(old_level) +
                   " but has level " + std::to_string(lit_level(l)));
        if (squash_level) {
          _decision_index[lit_level(l) - 1] = i;
        }
        continue;
      }

      // recompute the level
      Tclause reason = lit_reason(l);
      Tlit* lits = clause_lits(reason);
      unsigned size = clause_size(reason);
      Tlit* r = advanced_level_replacement(lits, size);
      // if the replacement is the same as before, then the watched literals do not change.
      // replacement is the highest level literal in the clause
      ASSERT(lit_is_max_literal(*r, lits + 1, size - 1));
      if (*r != lits[1]) {
        // we have to fix the watches
        std::swap(*r, lits[1]);
        // note that now *r is the old watched literal
        stop_watch(*r, reason);
        watch_lit(lits[1], reason);
      }
      lit_level(l) = lit_level(lits[1]);
    }
  }

  if (_options.graph_backtracking) {
    for (size_t i = pos + 1; i < _trail.size(); i++) {
      Tlit l = _trail[i];

      if (squash_level) {
        if (lit_level(l) > old_level) {
          lit_level(l)--;
          NOTIFY_OBSERVER(update_level, l, lit_level(l));
        }
        lit_cross_chunks(l).set(*old_chunks.cbegin(), false);
      }

      if (lit_decision(l)) {
        ASSERT_MSG(lit_level(l) >= old_level,
                   "Literal " + lit_to_string(l) + " at position " + std::to_string(i) +
                   " should have level greater than " + std::to_string(old_level) +
                   " but has level " + std::to_string(lit_level(l)));
        if (squash_level) {
          _decision_index[lit_level(l) - 1] = i;
        }
        continue;
      }

      if (lit_chunks(l) >= old_chunks) {
        lit_level(lit) = implication_level(lit_reason(lit));
        recompute_chunks(lit);
        if (lit_level(l) != old_level) {
          NOTIFY_OBSERVER(update_level, l, lit_level(l));
        }
      }
    }
  }

  ASSERT(check_decision_index_consistency());
}

void NapSAT::reimply_literal(Tlit c2, Tclause reason)
{
  ASSERT(reason != CLAUSE_UNDEF && reason != CLAUSE_LAZY);
  TSclause& clause = _clauses[reason];
  Tlit* lits = clause.lits;
  Tlit c1 = lits[1];

  if (_options.restoring_strong_chronological_backtracking && !clause.external) {
    // Nothing to do. The restoration will be done during backtracking, where the propagation head will be moved back.
    return;
  }

  // The levels are ok. Nothing to do here.
  if (!_options.graph_backtracking &&
        lit_level(c2) <= lit_level(c1))
      return;

  ASSERT_MSG(!_options.restoring_strong_chronological_backtracking,
              "RSCB reimplication not supported yet. Need to add restore point calculation");

  ASSERT_MSG(_options.lazy_strong_chronological_backtracking || _options.graph_backtracking || clause.external,
            "Clause " + clause_to_string(reason) + " cannot be used for reimplication without LSCB or GB");
  ASSERT(lit_true(c2));
  ASSERT(c2 == lits[0]);
  ASSERT(check_clause_implying(reason));
  ASSERT(clause.size >= 2);

  /**
   * We want to recover some backtrack compatible watch literal invariants
   * NCB
   *  ¬c₁ ∈ (τ ⋅ ℓ) ⇒ c₂ ∈ π ∨ b ∈ π
   *  ∀ij. i < j ⇒ δ(π[i]) ≤ δ(π[j])
   * RSCB
   *  ¬c₁ ∈ (τ ⋅ ℓ) ⇒ [c₂ ∈ π ∧ δ(c₂) ≤ δ(c₁)] ∨ [b ∈ π ∧ δ(b) ≤ δ(c₁)]
   * LSCB
   *  ¬c₁ ∈ (τ ⋅ ℓ) ⇒ [c₂ ∈ π ∧ [δ(c₂) ≤ δ(c₁) ∨ δ(λ(c₂) \ {c₂}) ≤ δ(c₁)]
   *                ∨ [b  ∈ π ∧  δ(b)  ≤ δ(c₁)]
   * GB
   *  ¬c₁ ∈ (τ ⋅ ℓ) ⇒ [c₂ ∈ π ∧ γ(c₂) ⊆ γ(c₁) ∪ η(c₁)]
   *                ∨ [b  ∈ π ∧ γ(b)  ⊆ γ(c₁) ∪ η(c₁)]
   */

  auto start = std::chrono::high_resolution_clock::now();

  if (_options.graph_backtracking) {
    lit_cross_chunks(c1) |= lit_chunks(c2);

    if (!lit_decision(c2)) {
      //nothing to do here
      return;
    }

    if (_options.eager_chunk_merging) {
      ASSERT(lit_lazy_reason(c2) == CLAUSE_UNDEF);
      eager_decision_reimplication(c2, reason);
      auto stop = std::chrono::high_resolution_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
      NOTIFY_STAT_N(reimply_time, duration.count());
      return;
    }

    // Lazy chunk merging
    if (_options.lazy_chunk_merging && lit_lazy_reason(c2) == CLAUSE_UNDEF) {
      // compute the chunk set of the clause, excluding the lits[1]
      bitset chunks(_n_allocated_chunks);
      for (size_t j = 1; j < clause.size; j++) {
        ASSERT(lits[j] != c2);
        chunks |= lit_chunks(lits[j]);
      }

      ASSERT(lit_chunks(c2).count() == 1);
      Tchunk decision_chunk = *lit_chunks(c2).cbegin();
      NOTIFY_STAT(_n_cross_implication_decisions);
      if (!reimplication_cycle(decision_chunk, chunks)) {
        lit_lazy_reason(c2) = reason;
        _chunks[decision_chunk].missed_implication = chunks;
        NOTIFY_OBSERVER(missed_lower_implication, lit_to_var(c2), reason);
      }
    }
    auto stop = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
    NOTIFY_STAT_N(reimply_time, duration.count());
    return;
  }

  ASSERT(lit_is_max_literal(lits[1], lits + 2, clause.size - 2));
  if (!_options.chronological_backtracking && !_options.graph_backtracking) {
    // Non-chronological backtracking
    ASSERT(clause.external);

    // NCB does not support lazy reimplication. We need to backtrack and reimply now.
    Tlevel backtrack_level = lit_level(c1);
    backtrack(backtrack_level);
    imply_literal(c2, reason);
    auto stop = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
    NOTIFY_STAT_N(reimply_time, duration.count());
    return;
  }

  ASSERT(_options.lazy_strong_chronological_backtracking);
  if (lit_lazy_level(c2) <= lit_level(c1)) {
    return;
  }
  lit_lazy_reason(c2) = reason;
  NOTIFY_OBSERVER(missed_lower_implication, lit_to_var(c2), reason);
  auto stop = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
  NOTIFY_STAT_N(reimply_time, duration.count());
}

void NapSAT::eager_decision_reimplication(Tlit decision, Tclause reason)
{
  ASSERT(lit_decision(decision));
  ASSERT(lit_true(decision));
  ASSERT(reason != CLAUSE_UNDEF && reason != CLAUSE_LAZY);
  ASSERT(clause_size(reason) > 0);
  ASSERT(decision == clause_lits(reason)[0]);
  ASSERT(check_clause_implying(reason));

  // check that the chunks don't cycle. That is, check that γ(C) ∩ γ(decision) = ∅. If they do, we cannot do eager reimplication, as we would end up with a cycle of implications.
  bitset reason_chunks = clause_chunks(reason);
  if (reason_chunks.has_intersection(lit_chunks(decision))) {
    return;
  }

  Tlit last_literal = clause_lits(reason)[1];
  size_t last_order = lit_order(last_literal);
  for (unsigned i = 1; i < clause_size(reason); i++) {
    Tlit lit = clause_lits(reason)[i];
    size_t order = lit_order(lit);
    if (order > last_order) {
      last_order = order;
      last_literal = lit;
    }
  }

  Tchunk decision_chunk = *lit_chunks(decision).cbegin();

  // buffer holding the literals that will be moved. That is, the literals that are reimplied and will be shifted in the trail.
  vector<Tlit> to_move;

  // we are going to defragment the trail between the moving decision and the last literal in the reason.
  // this makes it easier to move literals around and maintain a correct order.
  size_t current_order = lit_order(decision);

  size_t decision_position = find_literal_in_trail(decision);
  ASSERT(decision_position < _trail.size());
  size_t last_position = find_literal_in_trail(last_literal);
  ASSERT(last_position < _trail.size());
  cout << "Decision position: " << decision_position << ", last literal position: " << last_position << endl;

  // we mark the literals in the to_move buffer. This makes it easier to check if a literal needs to be moved as well, as we only need to check if one of its reasons is in the to_move buffer.
  // in principle, in GB, we could use chunks to determine whether a literal needs to be moved.
  // However, we need to recompute the chunks and levels anyway, so we have to go through the literals of each reason anyhow.
  lit_mark(decision);

  lit_reason(decision) = reason;
  ASSERT(lit_chunks(decision).count() == 1);
  _free_chunks.push_back(*lit_chunks(decision).cbegin());
  NOTIFY_OBSERVER(update_reason, decision, reason);
  // update the decision level and bitset
  lit_level(decision) = implication_level(reason);
  lit_chunks(decision) = reason_chunks;

#if USE_OBSERVER
  // we need to change the trail completely. Notify the observer.
  if (_observer) {
    for (size_t i = _trail.size(); i-- > decision_position;) {
      NOTIFY_OBSERVER(unassignment, _trail[i]);
    }
  }
#endif

  // update the cross-chunks of all the literals
  for (unsigned i = 0; i < decision_position; i++) {
    Tlit lit = _trail[i];
    if (lit_cross_chunks(lit).get(decision_chunk)) {
      lit_cross_chunks(lit).set(decision_chunk, false);
      lit_cross_chunks(lit) |= lit_chunks(decision);
    }
  }

  // we need to shift all literals depending on the decision and with an order lower than the last literal in the reason.
  // this needs to be stable to maintain the relative order of the literals.
  size_t read = decision_position + 1;
  size_t write = decision_position;
  to_move.push_back(decision);

  for (; read <= last_position; read++) {
    if (_n_propagated_lits == read) {
      _n_propagated_lits = write;
    }
    // cout << "Checking literal " << lit_to_string(_trail[read]) << " at position " << read << endl;
    ASSERT(read - write == to_move.size());

    Tlit lit = _trail[read];
    bool keep = true;
    if (lit_decision(lit)) {
      lit_level(lit)--;
      _decision_index[lit_level(lit) - 1] = write;
    } else {
      if (_options.graph_backtracking) {
        keep = !lit_chunks(lit).get(decision_chunk);
      } else {
        // check if there is a marked literal in the reason of lit with a level greater or equal to the decision level. If there is, then we need to move lit as well.
        Tclause reason = lit_reason(lit);
        ASSERT(reason != CLAUSE_UNDEF);
        Tlit* lits = clause_lits(reason);
        unsigned size = clause_size(reason);
        for (unsigned i = 1; i < size && keep; i++) {
          keep &= !lit_marked(lits[i]);
        }
      }
      lit_level(lit) = implication_level(lit_reason(lit));
      if (_options.graph_backtracking && !keep) {
        recompute_chunks(lit);
      }
    }

    if (lit_cross_chunks(lit).get(decision_chunk)) {
      lit_cross_chunks(lit).set(decision_chunk, false);
      lit_cross_chunks(lit) |= lit_chunks(decision);
    }

    ASSERT(keep || !lit_decision(lit));
    if (keep) {
      _trail[write++] = lit;
      lit_order(lit) = current_order++;
      continue;
    }
    to_move.push_back(lit);
    lit_mark(lit);
  }

  // now we have a hole the perfect size of the literals we need to move. We just need to fill it with the literals in the to_move buffer.
  while(read != write) {
    // cout << "Moving literal " << lit_to_string(to_move.back()) << endl;
    ASSERT(read > write);
    ASSERT(read - write <= to_move.size());
    Tlit lit = to_move[to_move.size() - (read - write)];
    ASSERT(!lit_decision(lit));
    _trail[write++] = lit;
    lit_order(lit) = current_order++;
    lit_unmark(lit);
    // those literals did not follow the topological order. We need to recompute their decision level.
    lit_level(lit) = implication_level(lit_reason(lit));
  }

  // now we need to update the levels of the literals that were moved, as well as the decision index and the chunks if we are doing graph backtracking.
  ASSERT(read == write);
  while(read < _trail.size()) {
    Tlit lit = _trail[read];
    // cout << "Updating literal " << lit_to_string(lit) << " at position " << read << endl;
    if (lit_decision(lit)) {
      lit_level(lit)--;
      _decision_index[lit_level(lit) - 1] = read;
    } else {
      lit_level(lit) = implication_level(lit_reason(lit));
      if (_options.graph_backtracking) {
        recompute_chunks(lit);
      }
    }

    if (lit_cross_chunks(lit).get(decision_chunk)) {
      lit_cross_chunks(lit).set(decision_chunk, false);
      lit_cross_chunks(lit) |= lit_chunks(decision);
    }

    lit_order(lit) = current_order++;
    read++;
  }
  // we have partially defragmented the order
  _current_order = current_order;
  // we removed a decision, we need to update the decision index
  _decision_index.resize(_decision_index.size() - 1);


#if USE_OBSERVER
  // we need to change the trail completely. Notify the observer.
  if (_observer) {
    for (size_t i = decision_position; i < _trail.size(); i++) {
      Tlit lit = _trail[i];
      if (lit_decision(lit)) {
        NOTIFY_OBSERVER(decision, lit);
      } else {
        NOTIFY_OBSERVER(implication, lit, lit_reason(lit), lit_level(lit));
      }
      if (i < _n_propagated_lits) {
        NOTIFY_OBSERVER(propagation, lit);
      }
    }
  }
#endif
  ASSERT(check_decision_index_consistency());
}

bool napsat::NapSAT::reimplication_cycle(Tchunk decision_chunk, const bitset& reimplying_chunks)
{
  bitset closure = reimplying_chunks;
  if (closure.empty()) {
    return false;
  }

  bool changed = true;

  while (changed) {
    if(closure[decision_chunk]) {
      // we found a cycle
      return true;
    }
    changed = false;
    for (auto it = closure.cbegin(); it != closure.cend(); ++it) {
      Tchunk c = *it;
      bitset& m = _chunks[c].missed_implication;
      if (!(m < closure)) {
        closure |= m;
        changed = true;
        break;
      }
    }
  }
  return false;
}

} // namespace napsat
