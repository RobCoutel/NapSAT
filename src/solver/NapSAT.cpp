/*
 * This file is part of the source code of the software program
 * NapSAT. It is protected by applicable copyright laws.
 *
 * This source code is protected by the terms of the MIT License.
 */
/**
 * @file src/solver/NapSAT.cpp
 * @author Robin Coutelier
 *
 * @brief This file is part of the NapSAT solver. It implements the CDCL algorithm and .
 */
#include "NapSAT.hpp"

#include "custom-assert.hpp"

#include <iostream>
#include <cstring>
#include <functional>

using namespace napsat;
using namespace std;

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
  ASSERT(reason == CLAUSE_UNDEF || clause_unit(reason));

  _trail.push_back(lit);

  Tvar var = lit_to_var(lit);
  TSvar& svar = _vars[var];
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
    ASSERT(clause_implying(reason));
    ASSERT(size < 2 || lit_is_max_literal(lits[1], lits + 2, size - 2));

    if (clause_size(reason) == 1) {
      svar.level = LEVEL_ROOT;
    } else {
      if (_options.graph_backtracking) {
        // In graph backtracking we do not have any information about the levels
        // we need to compute it by going through the clause
        svar.level = lit_level(lits[1]);
        for (unsigned i = 2; i < size; i++) {
          svar.level = std::max(svar.level, lit_level(lits[i]));
        }

        // compute the chunks and cross-chunks of the variable
        ASSERT(svar.chunks.empty());
        for (unsigned i = 1; i < size; i++) {
          svar.chunks |= lit_chunks(lits[i]);
        }
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

void NapSAT::var_unassign(Tvar var)
{
  ASSERT(!var_undef(var));

  TSvar& v = _vars[var];
  NOTIFY_OBSERVER(unassignment, literal(var, v.state));
  if (v.missed_lower_implication != CLAUSE_UNDEF) {
    NOTIFY_OBSERVER(remove_lower_implication, var);
    v.missed_lower_implication = CLAUSE_UNDEF;
  }
  if (!_variable_heap.contains(var))
    _variable_heap.insert(var, v.activity);

  if (_options.graph_backtracking) {
    if (v.reason == CLAUSE_UNDEF) {
      ASSERT(v.chunks.count() == 1);
      for (Tchunk ck = 0; ck < _n_allocated_chunks; ck++) {
        TSchunk& chunk = _chunks[ck];
        if (chunk.decision == var) {
          _free_chunks.push_back(ck);
          chunk.decision = LIT_UNDEF;
          chunk.missed_implication.clear();
          break;
        }
      }
    }
    v.chunks.clear();
    v.cross_chunks.clear();
  }
  v.state = VAR_UNDEF;
  v.reason = CLAUSE_UNDEF;
  v.level = LEVEL_UNDEF;
  v.propagated = false;
}

void napsat::NapSAT::reimply_literal(Tlit c2, Tclause reason)
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
  ASSERT(clause_implying(reason));
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

  if (_options.graph_backtracking) {
    lit_cross_chunks(c1) |= lit_chunks(c2);

    if (!_options.lazy_chunk_merging) {
      return;
    }

    // Lazy chunk merging
    if (lit_decision(c2) && lit_lazy_reason(c2) == CLAUSE_UNDEF) {
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
    return;
  }

  ASSERT(_options.lazy_strong_chronological_backtracking);
  if (lit_lazy_level(c2) <= lit_level(c1)) {
    return;
  }
  lit_lazy_reason(c2) = reason;
  NOTIFY_OBSERVER(missed_lower_implication, lit_to_var(c2), reason);
}

void napsat::NapSAT::prove_root_literal_removal(Tlit* lits, unsigned size)
{
  ASSERT(_proof);
  // we assume that a resolution chain is already started
  unsigned count = 0;

  for (unsigned i = 0; i < size; i++) {
    ASSERT(lit_false(lits[i]));
    if (lit_level(lits[i]) != LEVEL_ROOT)
      continue;
    if (!lit_marked(lits[i])) {
      count++;
    }
    lit_mark(lits[i]);
  }

  unsigned i = _trail.size() - 1;
  while (count != 0) {
    while (!lit_marked(_trail[i]))
      i--;
    Tlit lit = _trail[i];
    ASSERT(lit_level(lit) == LEVEL_ROOT);
    ASSERT(lit_reason(lit) != CLAUSE_UNDEF);
    Tclause reason = lit_reason(lit);
    ASSERT(reason != CLAUSE_UNDEF);
    _proof->link_resolution(lit_neg(lit), reason);

    for (unsigned j = 1; j < clause_size(reason); j++) {
      Tlit lit = clause_lits(reason)[j];
      ASSERT(lit_false(lit));
      if (lit_marked(lit))
        continue;
      lit_mark(lit);
      count++;
    }
    count--;
    lit_unmark(lit);
  }
}

void NapSAT::restart()
{
  NOTIFY_STAT(_n_restart);
  if (_options.graph_backtracking) {
    unsigned tmp = _n_propagated_lits;
    backtrack(LEVEL_ROOT);
    _n_propagated_lits = min(tmp, _n_propagated_lits);
    size_t i = _trail.size();
    while (i > 0) {
      i--;
      Tlit lit = _trail[i];
      _vars[lit_to_var(lit)].propagated = false;
      if (i < _n_propagated_lits) {
        NOTIFY_OBSERVER(remove_propagation, lit);
      }
    }
    _n_propagated_lits = 0;
  } else {
    backtrack(LEVEL_ROOT);
  }
}

double napsat::NapSAT::default_cost(Tlit lit) {
  if (lit_synced(lit)) {
    return _options.sync_weight;
  }
  return 1;
}

static inline void print_bt_option(const options &options) {
  string bt = "non-chronological";
  if (options.chronological_backtracking)
    bt = "chronological";
  else if (options.weak_chronological_backtracking)
    bt = "weak-chronological";
  else if (options.restoring_strong_chronological_backtracking)
    bt = "restoring-strong-chronological";
  else if (options.lazy_strong_chronological_backtracking)
    bt = "lazy-strong-chronological";
  else if (options.graph_backtracking)
    bt = "graph";

#ifndef NDEBUG
  bt += " (debug)";
#else
  bt += " (release)";
#endif
  LOG_INFO("Using backtracking strategy: " + bt);
}

/*****************************************************************************/
/*                            Public interface                               */
/*****************************************************************************/
napsat::NapSAT::NapSAT(unsigned n_var, unsigned n_clauses, napsat::options& options) :
  _options(options)
{
  print_bt_option(_options);
#if USE_STATISTICS
  if (options.print_stats || options.print_live_stats) {
    _statistics = new napsat::statistics(options);
    const std::string cat_time = "1. Runtime";
    const std::string cat_core = "2. Core statistics";
    const std::string cat_aux = "3. Auxiliary statistics";

    stat.runtime = _statistics->add_stat("Solve time", cat_time, statistics::COUNT);

    stat.decision = _statistics->add_stat("Decisions", cat_core);
    stat.conflict = _statistics->add_stat("Conflicts", cat_core);
    stat.propagation = _statistics->add_stat("Propagation", cat_core);
    stat.implication = _statistics->add_stat("Implication", cat_core);
    stat.unassignment= _statistics->add_stat("Unassignment", cat_core);
    stat.remove_propagation = _statistics->add_stat("Remove propagation", cat_core);
    stat.remove_lower_implication = _statistics->add_stat("Remove lower implication", cat_aux);
    stat.remove_literal = _statistics->add_stat("Remove literal", cat_core);
    stat.block = _statistics->add_stat("Block", cat_aux);
    stat.check_invariants = _statistics->add_stat("Check invariants", cat_aux);
    stat.missed_lower_implication = _statistics->add_stat("Missed lower implication", cat_core);
    stat.backtracking_started = _statistics->add_stat("Backtracking started", cat_core);
    stat.update_level = _statistics->add_stat("Update level", cat_aux);
    stat.update_reason = _statistics->add_stat("Update reason", cat_aux);
    stat.new_clause = _statistics->add_stat("Add clause", cat_core);
    stat.new_variable = _statistics->add_stat("Add variable", cat_core);
    stat.delete_clause = _statistics->add_stat("Delete clause", cat_core);
    stat.marker = _statistics->add_stat("Marker", cat_aux);
    stat.watch = _statistics->add_stat("Watch", cat_aux);
    stat.unwatch = _statistics->add_stat("Unwatch", cat_aux);
    stat.done = _statistics->add_stat("Solve calls", cat_core);
    stat.lock_assumption = _statistics->add_stat("Lock assumption", cat_core);
    stat.unlock_assumption = _statistics->add_stat("Unlock assumption", cat_core);

    stat._n_purged_clauses = _statistics->add_stat("Purging clauses", cat_aux);
    stat._n_binary_clause_simplified = _statistics->add_stat("Binary clause simplified", cat_aux);
    stat._n_binary_clause_added = _statistics->add_stat("Binary clause added", cat_aux);
    stat._n_clause_learned = _statistics->add_stat("Learned clause", cat_aux);
    stat._n_unit_clause_simplified = _statistics->add_stat("Unit clause simplified", cat_aux);
    stat._n_clause_deleted = _statistics->add_stat("Clause deleted", cat_aux);
    stat._n_clause_set_simplified = _statistics->add_stat("Clause set simplified", cat_aux);
    stat._n_allocated_chunks = _statistics->add_stat("Allocated Chunk", cat_aux);
    stat._n_cross_implication_decisions = _statistics->add_stat("Cross implication for decision", cat_aux);
    stat._n_lazy_reimplication_used = _statistics->add_stat("Lazy reimplication used", cat_aux);
    stat._n_propagation_replayed = _statistics->add_stat("Replayed Propagation", cat_aux);
    stat._n_skipped_propagation = _statistics->add_stat("Skipped Propagation", cat_aux);
    stat._n_sync = _statistics->add_stat("Sync", cat_core);
    stat._n_restart = _statistics->add_stat("Restart", cat_aux);
    stat._n_fw_subsumption_in_set = _statistics->add_stat("Forward subsumption in set", cat_aux);
    stat._n_fw_subsumption = _statistics->add_stat("Forward subsumption", cat_aux);
    stat._n_bw_subsumption = _statistics->add_stat("Backward subsumption", cat_aux);
    stat._n_backtrack_limit_reached = _statistics->add_stat("Backtrack limit reached", cat_aux);
    stat._n_conflict_repair = _statistics->add_stat("Conflict repair", cat_aux);
    stat._n_failed_learning = _statistics->add_stat("Failed learning", cat_aux);
    stat._n_backtrack_forced_chunks = _statistics->add_stat("Backtrack forced chunks", cat_aux);
    stat._n_backtrack_better_chunks = _statistics->add_stat("Backtrack better chunks", cat_aux);
    stat._a_learned_clause_size = _statistics->add_stat("Avg learned clause size", cat_aux, statistics::AVERAGE);
  }
#else
  if (options.print_stats)
    LOG_WARNING("The option --print-stats is not available in this build");
#endif

  // We have to create the observer before allocating the variables. Otherwise, the notifications will not be sent
#if USE_OBSERVER
  if (options.interactive || options.observing || options.check_invariants) {
    _observer = new napsat::gui::observer(options);
    // make a functional object that will parse the command
    if (options.interactive) {
      std::function<bool(const std::string&)> command_parser = [this](const std::string& command) {
        return this->parse_command(command);
      };
      _observer->set_command_parser(command_parser);
    }
    if (options.interactive || options.observing || options.check_invariants)
    NOTIFY_OBSERVER(marker, "Start");
#if USE_STATISTICS
    _observer->set_statistics(_statistics);
#endif
  }
#else
  if (options.interactive || options.observing || options.check_invariants) {
    LOG_WARNING("Observer not available in this build");
    if (options.interactive)
      LOG_WARNING("The option --interactive is not available in this build");
    if (options.observing)
      LOG_WARNING("The option --observing is not available in this build");
    if (options.check_invariants)
      LOG_WARNING("The option --check-invariants is not available in this build");
  }
#endif

  _vars.resize(1);
  _trail = vector<Tlit>();
  _trail.reserve(n_var);
  _watches.resize(2 * n_var + 2);

  _clauses = vector<TSclause>();
  _clauses.reserve(n_clauses);
  _activities.reserve(n_clauses);

  // TODO replace this with an std::vector
  _lit_buffer = new Tlit[2*n_var+1];
  _lit_buffer_size = 0;
  var_allocate(n_var + 1);

  if (options.build_proof)
    _proof = new napsat::proof::resolution_proof();

  if (options.record_dependencies)
    _dependency_tracker = new napsat::proof::dependency_tracker();

  if (_options.graph_backtracking) {
    allocate_chunks(4032);
  }
}

Tvar napsat::NapSAT::new_variable()
{
  Tvar var = _vars.size();
  var_allocate(_vars.size() + 1);
  return var;
}

NapSAT::~NapSAT()
{
#if USE_STATISTICS
  if (_options.print_stats) {
    if (_statistics) {
      LOG_INFO("Final statistics:");
      get_statistics()->print_statistics(_options.print_live_stats);
    }
#endif
  }
  for (unsigned i = 0; i < _clauses.size(); i++)
    delete[] _clauses[i].lits;
#if USE_OBSERVER
  if (_observer)
    delete _observer;
#endif
  if (_proof)
    delete _proof;
  if (_dependency_tracker)
    delete _dependency_tracker;
  delete[] _lit_buffer;
}


bool napsat::NapSAT::is_interactive() const
{
  return _options.interactive;
}

napsat::statistics* napsat::NapSAT::get_statistics() const
{
#if USE_STATISTICS
  return _statistics;
#else
  return nullptr;
#endif
}

bool napsat::NapSAT::is_observing() const
{
#if USE_OBSERVER
  return _observer != nullptr;
#else
  return false;
#endif
}

bool napsat::NapSAT::has_statistics() const
{
#if USE_STATISTICS
  return _statistics != nullptr;
#else
  return false;
#endif
}

napsat::gui::observer* napsat::NapSAT::get_observer() const
{
#if USE_OBSERVER
  return _observer;
#else
  return nullptr;
#endif
}

bool NapSAT::propagate()
{
  ASSERT(_conflicts.empty());
  // ASSERT(watch_lists_complete());
  // ASSERT(watch_lists_minimal());
  if (_status != UNKNOWN)
    return false;
  while (_n_propagated_lits < _trail.size()) {
    Tlit lit = _trail[_n_propagated_lits];
#ifdef NDEBUG
    if (lit_propagated(lit)) {
      _n_propagated_lits++;
      NOTIFY_STAT(_n_skipped_propagation);
      NOTIFY_OBSERVER(propagation, lit);
      continue;
    }
#endif
    if (!lit_propagated(lit)) {
      lit_cross_chunks(lit).clear();
    }
    lit_cross_chunks(lit) |= lit_chunks(lit);

    propagate_binary_clauses(lit);
    if (_conflicts.empty() || _options.exhaustive_conflict_repair || _options.partial_conflict_repair) {
      propagate_lit(lit);
    }

    if (_conflicts.empty() || _options.exhaustive_conflict_repair || _options.partial_conflict_repair) {
      _vars[lit_to_var(lit)].propagated = true;
      _n_propagated_lits++;
      NOTIFY_OBSERVER(propagation, lit);
    }

    ASSERT(_conflicts.empty() || !_options.partial_conflict_repair || _n_propagated_lits < _trail.size() || _n_propagated_lits == _trail.size());

    bool stop_propagation = false;
    if (_options.partial_conflict_repair && !_conflicts.empty()) {
      // if all literals in the first conflict are propagated, we can stop
      const Tclause& first_conflict = _conflicts.front();
      const Tlit* lits = clause_lits(first_conflict);
      unsigned size = clause_size(first_conflict);
      stop_propagation = true;
      for (unsigned i = 0; i < size; i++) {
        if (!lit_propagated(lits[i])) {
          stop_propagation = false;
          break;
        }
      }
    }

    if (!_conflicts.empty()
    && (!_options.exhaustive_conflict_repair || _n_propagated_lits == _trail.size())
    && (!_options.partial_conflict_repair || stop_propagation)) {
      _conflict_count++;
      if (_options.conflict_limit >= 0 && _conflict_count > _options.conflict_limit) {
        LOG_INFO("Conflict limit reached: " + std::to_string(_options.conflict_limit));
        return false;
      }
      repair_conflicts();

      if (_status == UNSAT) {
        return false;
      }
      if (_options.delete_clauses && _n_learned_clauses >= _next_clause_elimination){
        simplify_clause_set();
      }
      if (_options.restarts && _luby_counter.increment()) {
        restart();
      }
    }
  }

  if (_trail.size() == _vars.size() - 1) {
    _status = SAT;
    return false;
  }
  return true;
}

status NapSAT::solve()
{
  // cout << "######## SOLVE ########" << endl;
  auto start_time = std::chrono::high_resolution_clock::now();
  if (_status != UNKNOWN) {
    ASSERT(_status != SAT || _trail.size() == _vars.size() - 1);
    return _status;
  }
  while (true) {
    if (!_conflicts.empty()) {
      repair_conflicts();
      if (_status == UNSAT) {
        auto end_time = std::chrono::high_resolution_clock::now();
        long long duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
        NOTIFY_STAT_N(runtime, duration);
        get_statistics()->print_statistics(true);
        return _status;
      }
    }
    NOTIFY_OBSERVER(check_invariants);
    if (!propagate()) {
      if (_status == UNSAT) {
        if (_options.interactive && _n_assumptions > 0) {
          _observer->notify(new napsat::gui::checkpoint());
          if (_status == UNKNOWN) {
            continue;
          }
        }
        break;
      }
      NOTIFY_OBSERVER(done, _status == SAT);
    }
    ASSERT(_n_propagated_lits == _trail.size());
    NOTIFY_OBSERVER(check_invariants);
    if (_n_root_lvl_lits >= _purge_threshold
    && ((!_options.weak_chronological_backtracking && !_options.restoring_strong_chronological_backtracking && !_options.graph_backtracking)
       || solver_level() == LEVEL_ROOT)) {
      // in WCB and RSCB, missed lower implications can be a problem when purging clauses.
      // this is the same trick as in CaDiCaL, but we might be able to do better
      purge_clauses();
      _purge_threshold = _n_root_lvl_lits + _purge_inc;
      if (_status == UNSAT) {
        if (_options.interactive && _n_assumptions > 0) {
          _observer->notify(new napsat::gui::checkpoint());
          if (_status == UNKNOWN) {
            continue;
          }
        }
        auto end_time = std::chrono::high_resolution_clock::now();
        long long duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
        NOTIFY_STAT_N(runtime, duration);
        get_statistics()->print_statistics(true);
        return _status;
      }
      // in chronological backtracking, the purge might have implied some literals
      // therefore we cannot take a decision before we propagate
      continue;
    }
    NOTIFY_OBSERVER(check_invariants);
#if USE_OBSERVER
    if (_options.interactive)
      _observer->notify(new napsat::gui::checkpoint());
    else
#endif
      decide();
    ASSERT(_status != UNSAT);
    if (_status == SAT)
      break;
  }
  if (_status == SAT)
    NOTIFY_OBSERVER(check_invariants);
  NOTIFY_OBSERVER(done, _status == SAT);
  auto end_time = std::chrono::high_resolution_clock::now();
  long long duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
  NOTIFY_STAT_N(runtime, duration);
  get_statistics()->print_statistics(true);
  return _status;
}

status napsat::NapSAT::solve(unsigned conflict_limit)
{
  double old_conflict_limit = _options.conflict_limit;
  _options.conflict_limit = conflict_limit;
  _conflict_count = 0;
  status result = solve();
  _options.conflict_limit = old_conflict_limit;
  return result;
}

status NapSAT::get_status() const
{
  return _status;
}

bool NapSAT::decide()
{
  ASSERT(!_variable_heap.contains(0));
  while (!_variable_heap.empty() && !var_undef(_variable_heap.top()))
    _variable_heap.pop();
  if (_variable_heap.empty()) {
    _status = SAT;
    return false;
  }
  Tvar var = _variable_heap.top();

  ASSERT(var_constrained(var));
  Tlit lit = literal(var, _vars[var].synced);
  imply_literal(lit, CLAUSE_UNDEF);
  return true;
}

bool napsat::NapSAT::decide(Tlit lit)
{
  ASSERT(lit_undef(lit));
  imply_literal(lit, CLAUSE_UNDEF);
  return true;
}

void NapSAT::start_clause()
{
  ASSERT(!_writing_clause);
  _writing_clause = true;
  _lit_buffer_size = 0;
}

void NapSAT::add_literal(Tlit lit)
{
  ASSERT(_writing_clause);
  Tvar var = lit_to_var(lit);
  var_allocate(var);
  ASSERT(_lit_buffer_size < _vars.size());
  _lit_buffer[_lit_buffer_size++] = lit;
}

napsat::Tclause NapSAT::finalize_clause()
{
  ASSERT(_writing_clause);
  Tclause cl = internal_add_clause(_lit_buffer, _lit_buffer_size, false, true);
  _writing_clause = false;
  _lit_buffer_size = 0;
  return cl;
}

napsat::Tclause napsat::NapSAT::add_clause(const Tlit* lits, unsigned size)
{
  Tvar max_var = 0;
  for (unsigned i = 0; i < size; i++)
    if (lit_to_var(lits[i]) > max_var)
      max_var = lit_to_var(lits[i]);
  var_allocate(max_var);
  Tclause cl = internal_add_clause(lits, size, false, true);
  return cl;
}

const Tlit* napsat::NapSAT::get_clause(Tclause cl) const
{
  assert(cl < _clauses.size());
  return _clauses[cl].lits;
}

unsigned napsat::NapSAT::get_clause_size(Tclause cl) const
{
  assert(cl < _clauses.size());
  return _clauses[cl].size;
}

void NapSAT::hint(Tlit lit)
{
  ASSERT(lit_to_var(lit) < _vars.size());
  ASSERT(!_writing_clause);
  ASSERT(lit_undef(lit));
  imply_literal(lit, CLAUSE_LAZY);
}

void NapSAT::hint(Tlit lit, unsigned int level)
{
  ASSERT(lit_to_var(lit) < _vars.size());
  ASSERT(!_writing_clause);
  ASSERT(lit_undef(lit));
  ASSERT(level <= solver_level() + 1);
  hint(lit);
  lit_level(lit) = level;
}

void NapSAT::synchronize()
{
  ASSERT(all_of(_trail.begin(), _trail.begin() + _sync_validity_index, [this](Tlit l){ return !lit_undef(l); }));

  for (size_t i = _sync_validity_index; i < _trail.size(); i++) {
    Tlit lit = _trail[i];
    Tvar var = lit_to_var(lit);
    ASSERT(!var_undef(var));
    if (var_synced(var)) {
      continue;
    }
    var_sync(var);
  }
  _sync_validity_index = _trail.size();
}

unsigned NapSAT::sync_validity_limit()
{
  return _sync_validity_index;
}

void NapSAT::set_markup(void (*markup_function)(void))
{
  ASSERT(markup_function);
}

const std::vector<Tlit>& NapSAT::trail() const
{
  return _trail;
}

void napsat::NapSAT::print_proof()
{
  ASSERT(_proof);
  ASSERT(_status == UNSAT);
  _proof->print_proof();
}

bool napsat::NapSAT::check_proof()
{
  ASSERT(_proof);
  ASSERT(_status == UNSAT);
  return _proof->check_proof();
}
