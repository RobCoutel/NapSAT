/*
 * This file is part of the source code of the software program
 * NapSAT. It is protected by applicable copyright laws.
 *
 * This source code is protected by the terms of the MIT License.
 */
/**
 * @file src/SAT-API.cpp
 * @author Robin Coutelier
 * @brief This file is part of the NapSAT solver. It implements the API for the SAT solver.
 */
#include "SAT-API.hpp"

#include "solver/NapSAT.hpp"

#include <iostream>

namespace napsat {

NapSAT* create_solver(unsigned n_var, unsigned n_clauses, options& opt)
{
  return new NapSAT(n_var, n_clauses, opt);
}

Tvar new_variable(NapSAT* solver)
{
  assert(solver != nullptr);
  return solver->new_variable();
}

void delete_solver(NapSAT* solver)
{
  assert(solver != nullptr);
  delete solver;
}

bool parse_dimacs(NapSAT* solver, const char* filename)
{
  assert(solver != nullptr);
  return solver->parse_dimacs(filename);
}

bool propagate(NapSAT* solver)
{
  assert(solver != nullptr);
  return solver->propagate();
}

bool decide(NapSAT* solver)
{
  assert(solver != nullptr);
  return solver->decide();
}

bool decide(NapSAT* solver, Tlit lit)
{
  assert(solver != nullptr);
  return solver->decide(lit);
}

bool add_assumption(NapSAT* solver, Tlit assumption)
{
  assert(solver != nullptr);
  return solver->assume(assumption);
}

bool add_assumption(NapSAT* solver, const std::vector<Tlit>& assumptions)
{
  assert(solver != nullptr);
  return solver->add_assumption(assumptions);
}

bool forget_assumption(NapSAT* solver, Tlit assumption)
{
  assert(solver != nullptr);
  return solver->forget_assumption(assumption);
}

void forget_assumption(NapSAT* solver)
{
  assert(solver != nullptr);
  solver->forget_assumption();
}

status solve(NapSAT* solver)
{
  assert(solver != nullptr);
  return solver->solve();
}

status solve_limited(NapSAT* solver, unsigned conflict_limit)
{
  assert(solver != nullptr);
  status s = solver->solve(conflict_limit);
  // solver->print_trail();
  return s;
}

status get_status(NapSAT* solver)
{
  return solver->get_status();
}

void start_new_clause(NapSAT* solver)
{
  assert(solver != nullptr);
  solver->start_clause();
}

void push_literal(NapSAT* solver, Tlit lit)
{
  assert(solver != nullptr);
  solver->add_literal(lit);
}

Tclause finalize_clause(NapSAT* solver)
{
  assert(solver != nullptr);
  return solver->finalize_clause();
}

Tclause add_clause(NapSAT* solver, const Tlit* lits, unsigned n_lits)
{
  assert(solver != nullptr);
  return solver->add_clause(lits, n_lits);
}

const std::vector<Tlit>& get_partial_assignment(const NapSAT* solver)
{
  assert(solver != nullptr);
  return solver->trail();
}

bool is_decided(const NapSAT* solver, Tlit lit)
{
  assert(solver != nullptr);
  return solver->is_decided(lit);
}

bool is_root_level(const NapSAT* solver, Tvar var)
{
  assert(solver != nullptr);
  Tlevel lvl =  solver->var_level(var);
  return lvl == LEVEL_ROOT;
}

void suggest_polarity(NapSAT* solver, Tlit lit, bool polarity)
{
  assert(solver != nullptr);
  solver->suggest_polarity(lit, polarity);
}

void set_weight_function(NapSAT* solver, std::function<double(Tlit)> weight_function)
{
  assert(solver != nullptr);
  solver->set_weight_function(weight_function);
}

void print_statistics(const NapSAT* solver)
{
#if USE_STATISTICS
  assert(solver != nullptr);
  assert(USE_STATISTICS);
  statistics* stat = solver->get_statistics();
  if (stat == nullptr) {
    std::cout << "No statistic collected. Use -stat in the options to collect them." << std::endl;
    return;
  }
  std::cout << stat->get_statistics();
#endif
}

void print_proof(NapSAT* solver)
{
  assert(solver != nullptr);
  solver->print_proof();
}

bool check_proof(NapSAT* solver)
{
  assert(solver != nullptr);
  return solver->check_proof();
}

std::vector<Tlit> unsat_core(const NapSAT* solver)
{
  assert(solver != nullptr);
  assert(solver->get_status() == napsat::status::UNSAT);
  return solver->unsat_core();
}

std::vector<Tclause> clause_unsat_core(NapSAT* solver)
{
  assert(solver != nullptr);
  assert(solver->get_status() == napsat::status::UNSAT);
  return solver->clause_unsat_core();
}

Tval get_variable_value(const NapSAT* solver, Tvar var)
{
  assert(solver != nullptr);
  return solver->var_value(var);
}

unsigned variables_count(const NapSAT* solver)
{
  assert(solver != nullptr);
  return solver->var_count();
}

}
