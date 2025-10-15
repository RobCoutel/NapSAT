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
      ASSERT(_chunks.size() == solver_level() + _free_chunks.size());
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

  v.synced = 0;
}

void napsat::NapSAT::reimply_literal(Tlit lit, Tclause reason)
{
  TSclause& clause = _clauses[reason];
  unsigned size = clause.size;

  ASSERT(lit_true(lit));
  ASSERT(_options.lazy_strong_chronological_backtracking || _options.graph_backtracking);
  ASSERT(reason != CLAUSE_UNDEF && reason != CLAUSE_LAZY);
  ASSERT(lit == clause.lits[0]);
  ASSERT(clause_implying(reason));
  ASSERT(size < 2 || lit_is_max_literal(clause.lits[1], clause.lits + 2, size - 2));

  Tlevel reimplication_level = size == 1 ? 0 : lit_level(clause.lits[1]);
  if (lit_level(lit) <= reimplication_level)
    return;
  if (lit_lazy_reason(lit) != CLAUSE_UNDEF
   && lit_level(clause_lits(lit_lazy_reason(lit))[1]) <= reimplication_level)
    return;
  lit_lazy_reason(lit) = reason;
  NOTIFY_OBSERVER(missed_lower_implication, lit_to_var(lit), reason);
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

/*****************************************************************************/
/*                            Public interface                               */
/*****************************************************************************/
napsat::NapSAT::NapSAT(unsigned n_var, unsigned n_clauses, napsat::options& options) :
  _options(options)
{
#if USE_STATISTICS
  if (options.print_stats || options.print_live_stats) {
    _statistics = new napsat::statistics(options);
    const std::string cat_core = "Core statistics";
    const std::string cat_aux = "Auxiliary statistics";

    stat.decision = _statistics->add_stat("Decisions", cat_core);
    stat.conflict = _statistics->add_stat("Conflicts", cat_core);
    stat.propagation = _statistics->add_stat("Propagation", cat_core);
    stat.implication = _statistics->add_stat("Implication", cat_core);
    stat.unassignment = _statistics->add_stat("Unassignment", cat_core);
    stat.remove_propagation = _statistics->add_stat("Remove propagation", cat_core);
    stat.remove_lower_implication = _statistics->add_stat("Remove lower implication", cat_core);
    stat.remove_literal = _statistics->add_stat("Remove literal", cat_core);
    stat.block = _statistics->add_stat("Block", cat_core);
    stat.check_invariants = _statistics->add_stat("Check invariants", cat_core);
    stat.missed_lower_implication = _statistics->add_stat("Missed lower implication", cat_core);
    stat.backtracking_started = _statistics->add_stat("Backtracking started", cat_core);
    stat.update_level = _statistics->add_stat("Update level", cat_core);
    stat.new_clause = _statistics->add_stat("Add clause", cat_core);
    stat.new_variable = _statistics->add_stat("Add variable", cat_core);
    stat.delete_clause = _statistics->add_stat("Delete clause", cat_core);
    stat.marker = _statistics->add_stat("Marker", cat_core);
    stat.watch = _statistics->add_stat("Watch", cat_core);
    stat.unwatch = _statistics->add_stat("Unwatch", cat_core);
    stat.done = _statistics->add_stat("Done", cat_core);

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
    stat._n_sync = _statistics->add_stat("Sync", cat_aux);
    stat._n_restart = _statistics->add_stat("Restart", cat_aux);
    stat._n_fw_subsumption_in_set = _statistics->add_stat("Forward subsumption in set", cat_aux);
    stat._n_fw_subsumption = _statistics->add_stat("Forward subsumption", cat_aux);
    stat._n_bw_subsumption = _statistics->add_stat("Backward subsumption", cat_aux);
    stat._n_backtrack_limit_reached = _statistics->add_stat("Backtrack limit reached", cat_aux);
    stat._n_conflict_repair = _statistics->add_stat("Conflict repair", cat_aux);
    stat._n_failed_learning = _statistics->add_stat("Failed learning", cat_aux);
    stat._n_backtrack_forced_chunks = _statistics->add_stat("Backtrack forced chunks", cat_aux);
    stat._n_backtrack_better_chunks = _statistics->add_stat("Backtrack better chunks", cat_aux);
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
  else
    _proof = nullptr;

  if (_options.graph_backtracking) {
    allocate_chunks(4032);
  }
  // _backtrack_cost_estimator = default_cost;
}

NapSAT::~NapSAT()
{
  for (unsigned i = 0; i < _clauses.size(); i++)
    delete[] _clauses[i].lits;
#if USE_OBSERVER
  if (_observer)
    delete _observer;
#endif
  if (_proof)
    delete _proof;
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
  // ASSERT(watch_lists_complete());
  // ASSERT(watch_lists_minimal());
  if (_status != UNDEF)
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
    if (!lit_propagated(lit)) {
      lit_cross_chunks(lit).clear();
    }
#else
    if (!lit_propagated(lit)) {
      lit_cross_chunks(lit).clear();
    }
#endif
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
  LOG_INFO("Using backtracking strategy: " + bt);
}

status NapSAT::solve()
{
  if (_status != UNDEF)
    return _status;
  print_bt_option(_options);
  while (true) {
    NOTIFY_OBSERVER(check_invariants);
    if (!propagate()) {
      if (_status == UNSAT || !_options.interactive)
        break;
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
      if (_status == UNSAT)
        return _status;
      // in chronological backtracking, the purge might have implied some literals
      // therefore we cannot take a decision before we propagate
      continue;
    }
    NOTIFY_OBSERVER(check_invariants);
    synchronize();
#if USE_OBSERVER
    if (_observer && _options.interactive)
      _observer->notify(new napsat::gui::checkpoint());
    else
#endif
      decide();
    ASSERT(_status != UNSAT);
    if (_status == SAT)
      break;
  }
  synchronize();
  if (_status == SAT)
    NOTIFY_OBSERVER(check_invariants);
  NOTIFY_OBSERVER(done, _status == SAT);
  return _status;
}

status NapSAT::get_status()
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
  Tlit lit = literal(var, _vars[var].phase_cache);
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
#ifndef NDEBUG
  for (size_t i = 0; i < _sync_validity_index; i++) { ASSERT(lit_synced(_trail[i])); }
#endif
  for (size_t i = _sync_validity_index; i < _trail.size(); i++) {
    Tlit lit = _trail[i];
    Tvar var = lit_to_var(lit);
    TSvar& v = _vars[var];
    if (!v.synced) {
      NOTIFY_STAT(_n_sync);
      v.synced = 1;
    }
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
