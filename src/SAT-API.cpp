#include "SAT-API.hpp"

#include "solver/NapSAT.hpp"

#include <iostream>

napsat::NapSAT* napsat::create_solver(unsigned n_var, unsigned n_clauses, options& opt)
{
  return new napsat::NapSAT(n_var, n_clauses, opt);
}

void napsat::delete_solver(NapSAT* solver)
{
  assert(solver != nullptr);
  delete solver;
}

bool napsat::parse_dimacs(NapSAT* solver, const char* filename)
{
  assert(solver != nullptr);
  return solver->parse_dimacs(filename);
}

bool napsat::propagate(NapSAT* solver)
{
  assert(solver != nullptr);
  return solver->propagate();
}

bool napsat::decide(NapSAT* solver)
{
  assert(solver != nullptr);
  return solver->decide();
}

bool napsat::decide(NapSAT* solver, Tlit lit)
{
  assert(solver != nullptr);
  return solver->decide(lit);
}

napsat::status napsat::solve(NapSAT* solver)
{
  assert(solver != nullptr);
  return solver->solve();
}

napsat::status napsat::get_status(NapSAT* solver)
{
  return solver->get_status();
}

void napsat::start_new_clause(NapSAT* solver)
{
  assert(solver != nullptr);
  solver->start_clause();
}

void napsat::push_literal(NapSAT* solver, Tlit lit)
{
  assert(solver != nullptr);
  solver->add_literal(lit);
}

napsat::Tclause napsat::finalize_clause(NapSAT* solver)
{
  assert(solver != nullptr);
  return solver->finalize_clause();
}

napsat::Tclause napsat::add_clause(NapSAT* solver, const Tlit* lits, unsigned n_lits)
{
  assert(solver != nullptr);
  return solver->add_clause(lits, n_lits);
}

const std::vector<napsat::Tlit>& napsat::get_partial_assignment(NapSAT* solver)
{
  assert(solver != nullptr);
  return solver->trail();
}

bool napsat::is_decided(NapSAT* solver, Tlit lit)
{
  assert(solver != nullptr);
  return solver->is_decided(lit);
}

void napsat::print_statistics(NapSAT* solver)
{
  assert(solver != nullptr);
  assert(USE_OBSERVER);
#if USE_OBSERVER
  napsat::gui::observer* obs = solver->get_observer();
  if (obs == nullptr) {
    std::cout << "No statistic collected. Use -stat in the options to collect them." << std::endl;
    return;
  }
  std::cout << obs->get_statistics();
#endif
}

void napsat::print_proof(NapSAT* solver)
{
  assert(solver != nullptr);
  solver->print_proof();
}

bool napsat::check_proof(NapSAT* solver)
{
  assert(solver != nullptr);
  return solver->check_proof();
}
