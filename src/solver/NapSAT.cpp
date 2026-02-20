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

double napsat::NapSAT::literal_cost(Tlit lit) const {
  if (_backtrack_cost_estimator) {
    return _backtrack_cost_estimator(lit);
  }
  return 1;
}

static inline void print_bt_option(const options &options) {
#ifndef NDEBUG
    LOG_INFO("Running NapSAT (debug)");
#else
    LOG_INFO("Running NapSAT (release)");
#endif
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
  if (options.exhaustive_conflict_repair)
    LOG_INFO(" - with exhaustive conflict repair");
  if (options.partial_conflict_repair)
    LOG_INFO(" - with partial conflict repair");

  if (options.graph_backtracking) {
    if (options.lazy_chunk_merging)
      LOG_INFO(" - with lazy chunk merging");
    else if (options.eager_chunk_merging)
      LOG_INFO(" - with eager chunk merging");
    if (options.backtrack_smallest_chunk)
      LOG_INFO(" - with backtrack smallest chunk");
    else if (options.backtrack_first_chunk)
      LOG_INFO(" - with backtrack first chunk");
    if (options.use_max_approximate_cost_estimation)
      LOG_INFO(" - with max approximate cost estimation");
    else if (options.use_sum_approximate_cost_estimation)
      LOG_INFO(" - with sum approximate cost estimation");
    LOG_INFO(" - with backtrack possibilities limit: " + std::to_string(options.backtrack_possibilities_limit));
  }
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
    const std::string cat_alg  = "3. Algorithmic";
    const std::string cat_inp  = "4. Inprocessing";
    const std::string cat_aux  = "5. Auxiliary statistics";

    stat.solve_time = _statistics->add_stat("Solve time", cat_time, statistics::TIME);
    stat.repair_time = _statistics->add_stat("Repair time", cat_time, statistics::TIME);
    stat.cost_estimation_time = _statistics->add_stat("Cost estimation time", cat_time, statistics::TIME);
    stat.backtrack_possibilities_time = _statistics->add_stat("Backtrack possibilities time", cat_time, statistics::TIME);
    stat.subsumption_time = _statistics->add_stat("Subsumption time", cat_time, statistics::TIME);
    stat.conflict_analysis_time = _statistics->add_stat("Conflict analysis time", cat_time, statistics::TIME);
    stat.conflict_fixing_time = _statistics->add_stat("Conflict fixing time", cat_time, statistics::TIME);
    stat.backtrack_time = _statistics->add_stat("Backtrack time", cat_time, statistics::TIME);
    stat.reimply_time = _statistics->add_stat("Reimplication time", cat_time, statistics::TIME);

    stat.done = _statistics->add_stat("Solve calls", cat_core);
    stat.new_variable = _statistics->add_stat("Add variable", cat_core);
    stat.new_clause = _statistics->add_stat("Add clause", cat_core);
    stat.decision = _statistics->add_stat("Decisions", cat_core);
    stat.conflict = _statistics->add_stat("Conflicts", cat_core);
    stat._n_conflict_repair = _statistics->add_stat("Conflict repair", cat_core);
    stat.implication = _statistics->add_stat("Implication", cat_core);
    stat.propagation = _statistics->add_stat("Propagation", cat_core);
    stat.unassignment= _statistics->add_stat("Unassignment", cat_core);
    stat.check_invariants = _statistics->add_stat("Check invariants", cat_aux);
    stat.missed_lower_implication = _statistics->add_stat("Missed lower implication", cat_core);
    stat._n_sync = _statistics->add_stat("Sync", cat_core);
    stat._n_restart = _statistics->add_stat("Restart", cat_core);
    stat.lock_assumption = _statistics->add_stat("Lock assumption", cat_core);
    stat.unlock_assumption = _statistics->add_stat("Unlock assumption", cat_core);

    stat.remove_literal = _statistics->add_stat("Remove literal", cat_inp);
    stat.marker = _statistics->add_stat("Marker", cat_aux);
    stat.watch = _statistics->add_stat("Watch", cat_alg);
    stat.unwatch = _statistics->add_stat("Unwatch", cat_alg);
    stat.block = _statistics->add_stat("Block", cat_alg);
    stat._n_propagation_replayed = _statistics->add_stat("Replayed Propagation", cat_alg);
    stat._n_skipped_propagation = _statistics->add_stat("Skipped Propagation", cat_alg);
    stat._n_fw_subsumption = _statistics->add_stat("Forward subsumption", cat_alg);
    stat._n_bw_subsumption = _statistics->add_stat("Backward subsumption", cat_alg);
    stat._n_fw_subsumption_in_set = _statistics->add_stat("Forward subsumption in set", cat_alg);
    stat._n_backtrack_forced_chunks = _statistics->add_stat("Backtrack forced chunks", cat_alg);
    stat._n_backtrack_better_chunks = _statistics->add_stat("Backtrack better chunks", cat_alg);
    stat._n_backtrack_limit_reached = _statistics->add_stat("Backtrack limit reached", cat_alg);
    stat._n_sync_cost = _statistics->add_stat("Sync Cost", cat_alg);
    stat._n_failed_learning = _statistics->add_stat("Failed learning", cat_alg);

    stat._n_purged_clauses = _statistics->add_stat("Purging clauses", cat_inp);
    stat.delete_clause = _statistics->add_stat("Delete clause", cat_inp);
    stat._n_redundant_clause = _statistics->add_stat("Clause deleted", cat_inp);

    stat.backtracking_started = _statistics->add_stat("Backtracking started", cat_aux);
    stat.update_level = _statistics->add_stat("Update level", cat_aux);
    stat.update_reason = _statistics->add_stat("Update reason", cat_aux);
    stat.remove_propagation = _statistics->add_stat("Remove propagation", cat_aux);
    stat.remove_lower_implication = _statistics->add_stat("Remove lower implication", cat_aux);
    stat._n_binary_clause_simplified = _statistics->add_stat("Binary clause simplified", cat_aux);
    stat._n_binary_clause_added = _statistics->add_stat("Binary clause added", cat_aux);
    stat._n_clause_learned = _statistics->add_stat("Learned clause", cat_aux);
    stat._n_unit_clause_simplified = _statistics->add_stat("Unit clause simplified", cat_aux);
    stat._n_clause_set_simplified = _statistics->add_stat("Clause set simplified", cat_aux);
    stat._n_allocated_chunks = _statistics->add_stat("Allocated Chunk", cat_aux);
    stat._n_cross_implication_decisions = _statistics->add_stat("Cross implication for decision", cat_aux);
    stat._n_lazy_reimplication_used = _statistics->add_stat("Lazy reimplication used", cat_aux);
    stat._a_learned_clause_size = _statistics->add_stat("Avg learned clause size", cat_aux, statistics::AVERAGE);
    stat._a_bt_choices = _statistics->add_stat("Avg number of backtrack choices", cat_aux, statistics::AVERAGE);
    stat._a_prefix_size = _statistics->add_stat("Avg prefix size", cat_aux, statistics::AVERAGE);
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
    _observer->set_save_state_function([this]() { this->save_state(); });
    if (options.interactive || options.observing || options.check_invariants)
#if USE_STATISTICS
    _observer->set_statistics(_statistics);
#endif
    NOTIFY_OBSERVER(marker, "Start");
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
  if (!_conflicts.empty()) {
    repair_conflicts();
    if (_status == UNSAT) {
      return false;
    }
  }
  ASSERT(_conflicts.empty());
  // ASSERT(check_watch_lists_complete());
  // ASSERT(check_watch_lists_minimal());
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
  auto start_time = std::chrono::high_resolution_clock::now();
  if (_status != UNKNOWN) {
    ASSERT(_status != SAT || _trail.size() == _vars.size() - 1);
    NOTIFY_OBSERVER(done, _status == SAT);
    return _status;
  }
  while (true) {
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
    }
    ASSERT(_n_propagated_lits == _trail.size());
    NOTIFY_OBSERVER(check_invariants);
    if (_n_root_lvl_lits >= _purge_threshold && solver_level() == LEVEL_ROOT) {
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
        long long duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
        NOTIFY_STAT_N(solve_time, duration);
        if (_options.print_live_stats)
          get_statistics()->print_statistics(true);
        NOTIFY_OBSERVER(done, _status == SAT);
        return _status;
      }
      // in chronological backtracking, the purge might have implied some literals
      // therefore we cannot take a decision before we propagate
      continue;
    }
    NOTIFY_OBSERVER(check_invariants);
    synchronize();
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
  auto end_time = std::chrono::high_resolution_clock::now();
  long long duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
  NOTIFY_STAT_N(solve_time, duration);
  if (_options.print_live_stats)
    get_statistics()->print_statistics(true);
  NOTIFY_OBSERVER(done, _status == SAT);
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
  // chose a random polarity
  // bool value = (rand() % 2) == 0;
  // Tlit lit = literal(var, value);
  // Tlit lit = literal(var, false);
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
    NOTIFY_STAT_N(_n_sync_cost, (unsigned) literal_cost(lit));
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
