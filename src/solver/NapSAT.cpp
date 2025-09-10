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
 * @brief This file is part of the NapSAT solver. It implements the core functions of the
 * solver such as the CDCL loop, BCP, conflict analysis, and backtracking.
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
  ASSERT(svar.synced != 0);
  if (svar.synced == 1)
    svar.synced = 3;

  if (reason == CLAUSE_UNDEF) {
    // Decision
    _decision_index.push_back(_trail.size() - 1);
    svar.level = solver_level();
    NOTIFY_OBSERVER(_observer, new napsat::gui::decision(lit));
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
    NOTIFY_OBSERVER(_observer, new napsat::gui::implication(lit, reason, svar.level));
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
  NOTIFY_OBSERVER(_observer, new napsat::gui::unassignment(literal(var, v.state)));
  if (v.missed_lower_implication != CLAUSE_UNDEF) {
    NOTIFY_OBSERVER(_observer, new napsat::gui::remove_lower_implication(var));
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

  // for the logic, look at the comment in NapSAT.hpp
  switch (v.synced)
  {
  case 0:
  case 2:
    v.synced = 2;
    break;
  case 3:
    v.synced = 1;
    break;
  default:
    ASSERT(false);
  }
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
  NOTIFY_OBSERVER(_observer, new napsat::gui::missed_lower_implication(lit_to_var(lit), reason));
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
  NOTIFY_OBSERVER(_observer, new napsat::gui::stat("Restart"));
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
        NOTIFY_OBSERVER(_observer, new napsat::gui::remove_propagation(lit));
      }
    }
    _n_propagated_lits = 0;
  } else {
    backtrack(LEVEL_ROOT);
  }
}

static unsigned estimate_backtrack_cost(Tlit lits) {
  return 1;
}

/*****************************************************************************/
/*                            Public interface                               */
/*****************************************************************************/

napsat::NapSAT::NapSAT(unsigned n_var, unsigned n_clauses, napsat::options& options) :
  _options(options)
{
  // We have to create the observer before allocating the variables. Otherwise the notifications will not be sent
#if USE_OBSERVER
  if (options.interactive || options.observing || options.check_invariants || options.print_stats || options.print_live_stats) {
    _observer = new napsat::gui::observer(options);
    // make a functional object that will parse the command
    if (options.interactive) {
      std::function<bool(const std::string&)> command_parser = [this](const std::string& command) {
        return this->parse_command(command);
        };
      _observer->set_command_parser(command_parser);
    }
  }
  else
    _observer = nullptr;
#else
  if (options.interactive || options.observing || options.check_invariants || options.print_stats) {
    LOG_WARNING("Observer not available in this build");
    if (options.interactive)
      LOG_WARNING("The option --interactive is not available in this build");
    if (options.observing)
      LOG_WARNING("The option --observing is not available in this build");
    if (options.check_invariants)
      LOG_WARNING("The option --check-invariants is not available in this build");
    if (options.print_stats)
      LOG_WARNING("The option --print-stats is not available in this build");
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
  _literal_buffer = new Tlit[2*n_var+1];
  _next_literal_index = 0;
  var_allocate(n_var + 1);

  if (options.build_proof)
    _proof = new napsat::proof::resolution_proof();
  else
    _proof = nullptr;

  if (_options.graph_backtracking) {
    allocate_chunks(4032);
  }

  _backtrack_cost_estimator = estimate_backtrack_cost;
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
  delete[] _literal_buffer;
}


bool napsat::NapSAT::is_interactive() const
{
  return _options.interactive;
}

bool napsat::NapSAT::is_observing() const
{
#if USE_OBSERVER
  return _observer != nullptr;
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
      NOTIFY_OBSERVER(_observer, new napsat::gui::stat("Skipped Propagation"));
      NOTIFY_OBSERVER(_observer, new napsat::gui::propagation(lit));
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
    if (_options.exhaustive_conflict_search || _conflicts.empty()) {
      propagate_lit(lit);
    }
    if (_options.exhaustive_conflict_search || _conflicts.empty()) {
      _vars[lit_to_var(lit)].propagated = true;
      _n_propagated_lits++;
      NOTIFY_OBSERVER(_observer, new napsat::gui::propagation(lit));
    }
    if ((!_options.exhaustive_conflict_search || _n_propagated_lits == _trail.size()) && !_conflicts.empty()) {
      repair_conflicts();
      if (_status == UNSAT)
        return false;
      if (!_options.no_restart && _luby_counter.increment()) {
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
    NOTIFY_OBSERVER(_observer, new napsat::gui::check_invariants());
    if (!propagate()) {
      if (_status == UNSAT || !_options.interactive)
        break;
      NOTIFY_OBSERVER(_observer, new napsat::gui::done(_status == SAT));
    }
    ASSERT(_n_propagated_lits == _trail.size());
    NOTIFY_OBSERVER(_observer, new napsat::gui::check_invariants());
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
    NOTIFY_OBSERVER(_observer, new napsat::gui::check_invariants());
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
    NOTIFY_OBSERVER(_observer, new napsat::gui::check_invariants());
  NOTIFY_OBSERVER(_observer, new napsat::gui::done(_status == SAT));
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
  _next_literal_index = 0;
}

void NapSAT::add_literal(Tlit lit)
{
  ASSERT(_writing_clause);
  Tvar var = lit_to_var(lit);
  var_allocate(var);
  ASSERT(_next_literal_index < _vars.size());
  _literal_buffer[_next_literal_index++] = lit;
}

napsat::Tclause NapSAT::finalize_clause()
{
  ASSERT(_writing_clause);
  Tclause cl = internal_add_clause(_literal_buffer, _next_literal_index, false, true);
  _writing_clause = false;
  _next_literal_index = 0;
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
  if(!_options.print_stats && !_options.print_live_stats)
    return;
  for (Tvar var = 1; var < _vars.size(); var++) {
    TSvar& v = _vars[var];
    switch (v.synced)
    {
    case 0:
    case 1:
      // do nothing
      break;
    case 2:
      NOTIFY_OBSERVER(_observer, new napsat::gui::stat("Sync unassign"));
      if (v.state == VAR_UNDEF)
        v.synced = 1;
      else {
        v.synced = 0;
        NOTIFY_OBSERVER(_observer, new napsat::gui::stat("Sync assign"));
      }
      break;
    case 3:
      ASSERT (v.state != VAR_UNDEF);
      NOTIFY_OBSERVER(_observer, new napsat::gui::stat("Sync assign"));
      v.synced = 0;
    default:
      break;
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
