#include "SAT-API.hpp"
#include "SAT-types.hpp"
#include "SAT-config.hpp"

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

using namespace std;
using namespace napsat;

static NapSAT* setup(const char* filename) {
  options options(nullptr, 0);
  NapSAT* solver = create_solver(0, 0, options);
  REQUIRE(solver != nullptr);

  parse_dimacs(solver, filename);
  return solver;
}

TEST_CASE( "[SAT-Integration] Integration Test : Satisfiable 1" ) {
  NapSAT* solver = setup("../tests/cnf/sat-empty.cnf");
  REQUIRE(solve(solver) == SAT);
}

TEST_CASE( "[SAT-Integration] Integration Test : Unsatisfiable 1" ) {
  NapSAT* solver = setup("../tests/cnf/unsat-01.cnf");
  REQUIRE(solve(solver) == UNSAT);
}

TEST_CASE( "[SAT-Integration] Integration Test : Unsatisfiable 2" ) {
  NapSAT* solver = setup("../tests/cnf/unsat-02.cnf");
  REQUIRE(solve(solver) == UNSAT);
}

TEST_CASE( "[SAT-Integration] Integration Test : Unsatisfiable 3" ) {
  NapSAT* solver = setup("../tests/cnf/unsat-03.cnf");
  REQUIRE(solve(solver) == UNSAT);
}

TEST_CASE( "[SAT-Integration] Integration Test : Unsatisfiable 4" ) {
  NapSAT* solver = setup("../tests/cnf/unsat-04.cnf");
  REQUIRE(solve(solver) == UNSAT);
}

TEST_CASE( "[SAT-Integration] Integration Test : Unsatisfiable 5" ) {
  NapSAT* solver = setup("../tests/cnf/unsat-05.cnf");
  REQUIRE(solve(solver) == UNSAT);
}

TEST_CASE( "[SAT-Integration] Integration Test : Unsatisfiable 6" ) {
  NapSAT* solver = setup("../tests/cnf/unsat-06.cnf");
  REQUIRE(solve(solver) == UNSAT);
}

TEST_CASE( "[SAT-Integration] Integration Test : Decompress 1" ) {
  NapSAT* solver = setup("../tests/cnf/sat-01.cnf.xz");
  REQUIRE(solve(solver) == SAT);
}
