#include "../src/observer/SAT-notification.hpp"
#include "../src/observer/SAT-observer.hpp"

#include <catch2/catch.hpp>

#include <vector>
#include <functional>
#include <string>
#include <iostream>
#include <iomanip>

using namespace napsat;
using namespace napsat::gui;
using namespace std;

TEST_CASE( "[Notification] Unit Test : New Variable Forward" )
{
  string arg = "-c";
  char* arg_c = &arg[0];
  options opt(&arg_c, 1);
  observer obs(opt);
  obs.notify(new new_variable(1));
  obs.notify(new new_variable(2));
  obs.notify(new new_variable(3));
  REQUIRE( obs.var_value(0) == VAR_ERROR );
  REQUIRE( obs.var_value(1) == VAR_UNDEF );
  REQUIRE( obs.var_value(2) == VAR_UNDEF );
  REQUIRE( obs.var_value(3) == VAR_UNDEF );

  obs.notify(new delete_variable(1));
  REQUIRE( obs.var_value(0) == VAR_ERROR );
  REQUIRE( obs.var_value(1) == VAR_ERROR );
  REQUIRE( obs.var_value(2) == VAR_UNDEF );
  REQUIRE( obs.var_value(3) == VAR_UNDEF );
}

TEST_CASE( "[Notification] Unit Test : New Variable Backward" )
{
  string arg = "-c";
  char* arg_c = &arg[0];
  options opt(&arg_c, 1);
  observer obs(opt);
  obs.notify(new new_variable(1));
  obs.notify(new new_variable(2));
  obs.notify(new new_variable(3));
  obs.notify(new delete_variable(1));
  obs.notify(new delete_variable(2));
  obs.notify(new delete_variable(3));
  REQUIRE( obs.var_value(0) == VAR_ERROR );
  REQUIRE( obs.var_value(1) == VAR_ERROR );
  REQUIRE( obs.var_value(2) == VAR_ERROR );
  REQUIRE( obs.var_value(3) == VAR_ERROR );

  obs.back();
  REQUIRE( obs.var_value(0) == VAR_ERROR );
  REQUIRE( obs.var_value(1) == VAR_ERROR );
  REQUIRE( obs.var_value(2) == VAR_ERROR );
  REQUIRE( obs.var_value(3) == VAR_UNDEF );
  obs.back();
  REQUIRE( obs.var_value(0) == VAR_ERROR );
  REQUIRE( obs.var_value(1) == VAR_ERROR );
  REQUIRE( obs.var_value(2) == VAR_UNDEF );
  REQUIRE( obs.var_value(3) == VAR_UNDEF );
  obs.back();
  REQUIRE( obs.var_value(0) == VAR_ERROR );
  REQUIRE( obs.var_value(1) == VAR_UNDEF );
  REQUIRE( obs.var_value(2) == VAR_UNDEF );
  REQUIRE( obs.var_value(3) == VAR_UNDEF );
  obs.back();
  REQUIRE( obs.var_value(0) == VAR_ERROR );
  REQUIRE( obs.var_value(1) == VAR_UNDEF );
  REQUIRE( obs.var_value(2) == VAR_UNDEF );
  REQUIRE( obs.var_value(3) == VAR_ERROR );
  obs.back();
  REQUIRE( obs.var_value(0) == VAR_ERROR );
  REQUIRE( obs.var_value(1) == VAR_UNDEF );
  REQUIRE( obs.var_value(2) == VAR_ERROR );
  REQUIRE( obs.var_value(3) == VAR_ERROR );
  obs.back();
  REQUIRE( obs.var_value(0) == VAR_ERROR );
  REQUIRE( obs.var_value(1) == VAR_ERROR );
  REQUIRE( obs.var_value(2) == VAR_ERROR );
  REQUIRE( obs.var_value(3) == VAR_ERROR );
}

TEST_CASE( "[Notification] Unit Test : New Clause" )
{
  string arg = "-c";
  char* arg_c = &arg[0];
  options opt(&arg_c, 1);
  observer obs(opt);
  obs.notify(new new_variable(1));
  obs.notify(new new_variable(2));
  obs.notify(new new_variable(3));
  obs.notify(new new_clause(0, { literal(1, 1), literal(2, 1), literal(3, 1) }, false, true));
  auto clauses = obs.get_clauses();
  REQUIRE( clauses.size() == 1 );
  assert(clauses.size() > 0);
  REQUIRE( clauses[0].first == 0 );
  const vector<Tlit> literals = *clauses[0].second;
  REQUIRE( literals.size() == 3 );
  assert(literals.size() >= 3);
  REQUIRE( literals[0] == literal(1, 1) );
  REQUIRE( literals[1] == literal(2, 1) );
  REQUIRE( literals[2] == literal(3, 1) );
}
