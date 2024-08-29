#include "SAT-API.hpp"
#include "SAT-types.hpp"
#include "SAT-config.hpp"

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

using namespace std;
using namespace napsat;

static NapSAT* setup(const char* filename) {
  // check if the file exists.
  // if it does not exist, try to remove the first 3 characters of the filename
  // and try again.
  // This is a dirty trick to be able to run the tests either from the root or the build directory.
  ifstream file(filename);
  if (!file.good()) {
    filename += 3;
  }
  options options(nullptr, 0);
  NapSAT* solver = create_solver(0, 0, options);
  REQUIRE(solver != nullptr);

  parse_dimacs(solver, filename);
  return solver;
}

static void teardown(NapSAT* solver) {
  delete_solver(solver);
}

TEST_CASE( "[SAT-Integration] Integration Test : Satisfiable" ) {
  SECTION ("Empty") {
    NapSAT* solver = setup("../tests/cnf/sat-empty.cnf");
    REQUIRE(solve(solver) == SAT);
    teardown(solver);
  }

}

TEST_CASE( "[SAT-Integration] Integration Test : Unsatisfiable" ) {
  SECTION("Unsatisfiable 1") {
    NapSAT* solver = setup("../tests/cnf/unsat-01.cnf");
    REQUIRE(solve(solver) == UNSAT);
    teardown(solver);
  }
  SECTION("Unsatisfiable 2") {
    NapSAT* solver = setup("../tests/cnf/unsat-02.cnf");
    REQUIRE(solve(solver) == UNSAT);
    teardown(solver);
  }
  SECTION("Unsatisfiable 3") {
    NapSAT* solver = setup("../tests/cnf/unsat-03.cnf");
    REQUIRE(solve(solver) == UNSAT);
    teardown(solver);
  }
  SECTION("Unsatisfiable 4") {
    NapSAT* solver = setup("../tests/cnf/unsat-04.cnf");
    REQUIRE(solve(solver) == UNSAT);
    teardown(solver);
  }
  SECTION("Unsatisfiable 5") {
    NapSAT* solver = setup("../tests/cnf/unsat-05.cnf");
    REQUIRE(solve(solver) == UNSAT);
    teardown(solver);
  }
  SECTION("Unsatisfiable 6") {
    NapSAT* solver = setup("../tests/cnf/unsat-06.cnf");
    REQUIRE(solve(solver) == UNSAT);
    teardown(solver);
  }
}

TEST_CASE( "[SAT-Integration] Integration Test : Decompress" ) {
  SECTION ("Decompress 1") {
    NapSAT* solver = setup("../tests/cnf/test-compress-01.cnf.xz");
    REQUIRE(solve(solver) == UNSAT);
    teardown(solver);
  }
  SECTION ("Decompress 2") {
    NapSAT* solver = setup("../tests/cnf/sat-01.cnf.xz");
    REQUIRE(solve(solver) == SAT);
    teardown(solver);
  }
}
