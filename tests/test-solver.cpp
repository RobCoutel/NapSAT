/*
 * This file is part of the source code of the software program
 * NapSAT. It is protected by applicable copyright laws.
 *
 * This source code is protected by the terms of the MIT License.
 */
/**
 * @file tests/test-solver.cpp
 * @author Robin Coutelier
 * @brief This file is part of the NapSAT solver. It contains the integration tests for the SAT solver.
 */
#include "SAT-API.hpp"
#include "SAT-types.hpp"
#include "SAT-config.hpp"
#include "SAT-options.hpp"

#define CATCH_CONFIG_MAIN
#include <catch2/catch_all.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

using namespace std;
using namespace napsat;

namespace {
  // NapSAT-tests may be invoked either from the repository root
  // (build/NapSAT-tests) or from the build directory (./NapSAT-tests).
  // env::invariant_configuration_folder defaults to "../invariant-configurations/",
  // which is only correct in the latter case. Detect the former and correct it,
  // mirroring the dirty trick already used for CNF filenames in setup() below.
  struct invariant_configuration_path_fixup {
    invariant_configuration_path_fixup() {
      if (filesystem::exists("invariant-configurations"))
        env::set_invariant_configuration_folder("invariant-configurations/");
    }
  } invariant_configuration_path_fixup_instance;
}

// Backtracking strategies exercised by every integration test below.
static const vector<string> backtracking_options = {
  "",
  "-cb",
  "-lscb",
  "-gb",
  "-gb -lcm",
  "-gb -ecm",
};

static NapSAT* setup(const char* filename, const string& extra_options = "") {
  // check if the file exists.
  // if it does not exist, try to remove the first 3 characters of the filename
  // and try again.
  // This is a dirty trick to be able to run the tests either from the root or the build directory.
  ifstream file(filename);
  if (!file.good()) {
    filename += 3;
  }
  vector<string> args;
  args.push_back("--suppress-info");
  args.push_back("-o");
  args.push_back("{--check-only}");
  args.push_back("--check-proof");
  istringstream iss(extra_options);
  string token;
  while (iss >> token)
    args.push_back(token);
  args = env::extract_environment_variables(args);
  Options* options = new Options(args);
  NapSAT* solver = create_solver(0, 0, options);
  REQUIRE(solver != nullptr);

  parse_dimacs(solver, filename);
  return solver;
}

static void teardown(NapSAT* solver) {
  delete_solver(solver);
}

TEST_CASE( "[SAT-Integration] Integration Test : Satisfiable" ) {
  string opt = GENERATE(from_range(backtracking_options));
  SECTION ("Empty") {
    NapSAT* solver = setup("../tests/cnf/sat-empty.cnf", opt);
    REQUIRE(solve(solver) == status::SAT);
    teardown(solver);
  }

}

TEST_CASE( "[SAT-Integration] Integration Test : Unsatisfiable" ) {
  string opt = GENERATE(from_range(backtracking_options));
  SECTION("Unsatisfiable 1") {
    NapSAT* solver = setup("../tests/cnf/unsat-01.cnf", opt);
    REQUIRE(solve(solver) == status::UNSAT);
    teardown(solver);
  }
  SECTION("Unsatisfiable 2") {
    NapSAT* solver = setup("../tests/cnf/unsat-02.cnf", opt);
    REQUIRE(solve(solver) == status::UNSAT);
    teardown(solver);
  }
  SECTION("Unsatisfiable 3") {
    NapSAT* solver = setup("../tests/cnf/unsat-03.cnf", opt);
    REQUIRE(solve(solver) == status::UNSAT);
    teardown(solver);
  }
  SECTION("Unsatisfiable 4") {
    NapSAT* solver = setup("../tests/cnf/unsat-04.cnf", opt);
    REQUIRE(solve(solver) == status::UNSAT);
    teardown(solver);
  }
  SECTION("Unsatisfiable 5") {
    NapSAT* solver = setup("../tests/cnf/unsat-05.cnf", opt);
    REQUIRE(solve(solver) == status::UNSAT);
    teardown(solver);
  }
  SECTION("Unsatisfiable 6") {
    NapSAT* solver = setup("../tests/cnf/unsat-06.cnf", opt);
    REQUIRE(solve(solver) == status::UNSAT);
    teardown(solver);
  }
  SECTION("Duplicate literals 1") {
    NapSAT* solver = setup("../tests/cnf/duplicate-literals-1.cnf", opt);
    REQUIRE(solve(solver) == status::SAT);
    teardown(solver);
  }
  SECTION("Duplicate literals 2") {
    NapSAT* solver = setup("../tests/cnf/duplicate-literals-2.cnf", opt);
    REQUIRE(solve(solver) == status::SAT);
    teardown(solver);
  }
}

TEST_CASE( "[SAT-Integration] Integration Test : Decompress" ) {
  string opt = GENERATE(from_range(backtracking_options));
  SECTION ("Decompress 1") {
    NapSAT* solver = setup("../tests/cnf/test-compress-01.cnf.xz", opt);
    REQUIRE(solve(solver) == status::UNSAT);
    teardown(solver);
  }
  SECTION ("Decompress 2") {
    NapSAT* solver = setup("../tests/cnf/sat-01.cnf.xz", opt);
    REQUIRE(solve(solver) == status::SAT);
    teardown(solver);
  }
}
