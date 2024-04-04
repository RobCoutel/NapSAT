#include "SAT-API.hpp"

#include "solver/NapSAT.hpp"

#include <iostream>

sat::NapSAT* sat::create_solver(unsigned n_var, unsigned n_clauses, options& opt)
{
  return new sat::NapSAT(n_var, n_clauses, opt);
}

void sat::delete_solver(NapSAT* solver)
{
  assert(solver != nullptr);
  delete solver;
}

void sat::parse_dimacs(NapSAT* solver, const char* filename)
{
  assert(solver != nullptr);
  solver->parse_dimacs(filename);
}

bool sat::propagate(NapSAT* solver)
{
  assert(solver != nullptr);
  return solver->propagate();
}

bool sat::decide(NapSAT* solver)
{
  assert(solver != nullptr);
  return solver->decide();
}

bool sat::decide(NapSAT* solver, Tlit lit)
{
  assert(solver != nullptr);
  return solver->decide(lit);
}

sat::status sat::solve(NapSAT* solver)
{
  assert(solver != nullptr);
  return solver->solve();
}

sat::status sat::get_status(NapSAT* solver)
{
  return solver->get_status();
}

void sat::start_new_clause(NapSAT* solver)
{
  assert(solver != nullptr);
  solver->start_clause();
}

void sat::push_literal(NapSAT* solver, Tlit lit)
{
  assert(solver != nullptr);
  solver->add_literal(lit);
}

sat::Tclause sat::finalize_clause(NapSAT* solver)
{
  assert(solver != nullptr);
  return solver->finalize_clause();
}

sat::Tclause sat::add_clause(NapSAT* solver, const Tlit* lits, unsigned n_lits)
{
  assert(solver != nullptr);
  return solver->add_clause(lits, n_lits);
}

const std::vector<sat::Tlit>& sat::get_partial_assignment(NapSAT* solver)
{
  assert(solver != nullptr);
  return solver->trail();
}

void sat::print_statistics(NapSAT* solver)
{
  assert(solver != nullptr);
  sat::gui::observer* obs = solver->get_observer();
  if (obs == nullptr) {
    std::cout << "No statistic collected. Use -stat in the options to collect them." << std::endl;
    return;
  }
  std::cout << obs->get_statistics();
}
