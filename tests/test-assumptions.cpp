/*
 * This file is part of the source code of the software program
 * NapSAT. It is protected by applicable copyright laws.
 *
 * This source code is protected by the terms of the MIT License.
 */
/**
 * @file test-assumptions.cpp
 * @author Robin Coutelier
 *
 * @brief This is a test suite for assumption handling in NapSAT
 */
#include "../include/SAT-config.hpp"
#include "../src/solver/NapSAT.hpp"

#include <catch2/catch_all.hpp>
#include <iostream>

using namespace std;
using namespace napsat;

TEST_CASE( "[NapSAT-assumption] Unit Test : Add from empty" ) {
  vector<string> args;
  args.push_back("-gb");
  Options* opts = new Options(args);

  NapSAT solver(10, 10, opts);
  Tlit l1(1, true);
  Tlit l2(2, false);

  REQUIRE(solver.assume(l1));
  REQUIRE(solver.assume(l2));
  REQUIRE(solver.assume(l1));
  solver.forget_assumption(Tlit(1, true));
  REQUIRE(solver.assume(~l1));
}

TEST_CASE( "[NapSAT-assumption] Unit Test : Contradicting assumptions" ) {
  vector<string> args;
  args.push_back("-gb");
  Options* opts = new Options(args);

  NapSAT solver(10, 10, opts);
  REQUIRE(solver.assume(Tlit(1, true)) == true);
  REQUIRE(solver.assume(Tlit(2, false)) == true);
  REQUIRE(solver.assume(Tlit(1, false)) == false);
  REQUIRE(solver.forget_assumption(Tlit(1, true)) == true);
}

TEST_CASE( "[NapSAT-assumption] Unit Test : Conflict after assumption unlock" ) {

  vector<string> args;
  args.push_back("-gb");
  Options* opts = new Options(args);

  NapSAT solver(10, 10, opts);
  REQUIRE(solver.assume(Tlit(1, true)) == true);
  REQUIRE(solver.assume(Tlit(2, false)) == true);
  REQUIRE(solver.forget_assumption(Tlit(1, true)) == true);
  REQUIRE(solver.assume(Tlit(1, false)) == true);
}

TEST_CASE( "[NapSAT-assumption] Unit Test : True implied assumption" ) {

  /**
    trail: 4 - 0
    1:  1 --> reason: undef       (γ = [0], η = [0])
    1:  2 --> reason: 0: 2 -1     (γ = [0], η = [0])
    1:  3 --> reason: 1: 3 -1 -2  (γ = [0], η = [0])
    1:  4 --> reason: 2: 4 -1     (γ = [0], η = [0])

    trail: 4 - 0 (assumptions: 1)
    1:  1 --> reason: undef                        (γ = [0], η = [0])
    2:   2🔒 --> reason: undef / (lazy) 0: 2🔒 -1  (γ = [1], η = [0])
    2:   3 --> reason: 1: 3 -1 -2🔒                (γ = [0, 1], η = [0])
    1:  4 --> reason: 2: 4 -1                      (γ = [0], η = [0])

    trail: 4 - 0 (assumptions: 2)
    1:  1 --> reason: undef                         (γ = [0], η = [0])
    2:   2🔒 --> reason: undef / (lazy) 0: 2🔒 -1   (γ = [1], η = [0])
    2:   3 --> reason: 1: 3 -1 -2🔒                 (γ = [0, 1], η = [0])
    3:    4🔒 --> reason: undef / (lazy) 2: 4🔒 -1  (γ = [2], η = [0])

    trail: 4 - 0 (assumptions: 2)
    1:  1 --> reason: undef                         (γ = [0], η = [0])
    2:   2🔒 --> reason: undef / (lazy) 0: 2🔒 -1   (γ = [1], η = [0])
    2:   3 --> reason: 1: 3 -1 -2🔒                 (γ = [0, 1], η = [0])
    3:    4🔒 --> reason: undef / (lazy) 2: 4🔒 -1  (γ = [2], η = [0])
    conflict: 3: -3 -1 -2🔒 -4🔒  ([0, 1, 2])

    trail: 3 - 0 (assumptions: 2)
    1:  2🔒 --> reason: undef            (γ = [1], η = [1, 2])
    2:   4🔒 --> reason: undef           (γ = [2], η = [2])
    2:   -1 --> reason: 4: -1 -2🔒 -4🔒  (γ = [1, 2], η = [1, 2])
   */
  vector<string> args;
  args.push_back("-gb");
  Options* opts = new Options(args);

  Tlit l1(1, true);
  Tlit l2(2, true);
  Tlit l3(3, true);
  Tlit l4(4, true);

  NapSAT solver(10, 10, opts);
  solver.decide(l1);
  solver.start_clause();
    solver.add_literal(~l1);
    solver.add_literal(l2);
  solver.finalize_clause();

  solver.start_clause();
    solver.add_literal(~l1);
    solver.add_literal(~l2);
    solver.add_literal(l3);
  solver.finalize_clause();

  solver.propagate();

  solver.start_clause();
    solver.add_literal(~l1);
    solver.add_literal(l4);
  solver.finalize_clause();

  solver.propagate();

  REQUIRE(solver.assume(l2));
  REQUIRE(solver.assume(l4));

  solver.start_clause();
    solver.add_literal(~l1);
    solver.add_literal(~l2);
    solver.add_literal(~l3);
    solver.add_literal(~l4);
  solver.finalize_clause();
  REQUIRE(solver.propagate());
  REQUIRE(!solver.assume(l1));
}
