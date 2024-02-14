#include "../src/observer/SAT-notification.hpp"
#include "../src/observer/SAT-observer.hpp"

#include <vector>
#include <functional>
#include <string>
#include <iostream>
#include <iomanip>

using namespace sat;
using namespace sat::gui;
using namespace std;

static bool test_new_variables_forward()
{
  bool success = true;
  options opt(nullptr, 0);
  observer obs(opt);
  obs.notify(new new_variable(1));
  obs.notify(new new_variable(2));
  obs.notify(new new_variable(3));
  success &= obs.var_value(0) == VAR_ERROR;
  success &= obs.var_value(1) == VAR_UNDEF;
  success &= obs.var_value(2) == VAR_UNDEF;
  success &= obs.var_value(3) == VAR_UNDEF;

  obs.notify(new delete_variable(1));
  success &= obs.var_value(0) == VAR_ERROR;
  success &= obs.var_value(1) == VAR_ERROR;
  success &= obs.var_value(2) == VAR_UNDEF;
  success &= obs.var_value(3) == VAR_UNDEF;
  return success;
}

static bool test_new_variables_backward()
{
  bool success = true;
  options opt(nullptr, 0);
  observer obs(opt);
  obs.notify(new new_variable(1));
  obs.notify(new new_variable(2));
  obs.notify(new new_variable(3));
  obs.notify(new delete_variable(1));
  obs.notify(new delete_variable(2));
  obs.notify(new delete_variable(3));
  success &= obs.var_value(0) == VAR_ERROR;
  success &= obs.var_value(1) == VAR_ERROR;
  success &= obs.var_value(2) == VAR_ERROR;
  success &= obs.var_value(3) == VAR_ERROR;

  obs.back();
  success &= obs.var_value(0) == VAR_ERROR;
  success &= obs.var_value(1) == VAR_ERROR;
  success &= obs.var_value(2) == VAR_ERROR;
  success &= obs.var_value(3) == VAR_UNDEF;
  obs.back();
  success &= obs.var_value(0) == VAR_ERROR;
  success &= obs.var_value(1) == VAR_ERROR;
  success &= obs.var_value(2) == VAR_UNDEF;
  success &= obs.var_value(3) == VAR_UNDEF;
  obs.back();
  success &= obs.var_value(0) == VAR_ERROR;
  success &= obs.var_value(1) == VAR_UNDEF;
  success &= obs.var_value(2) == VAR_UNDEF;
  success &= obs.var_value(3) == VAR_UNDEF;
  obs.back();
  success &= obs.var_value(0) == VAR_ERROR;
  success &= obs.var_value(1) == VAR_UNDEF;
  success &= obs.var_value(2) == VAR_UNDEF;
  success &= obs.var_value(3) == VAR_ERROR;
  obs.back();
  success &= obs.var_value(0) == VAR_ERROR;
  success &= obs.var_value(1) == VAR_UNDEF;
  success &= obs.var_value(2) == VAR_ERROR;
  success &= obs.var_value(3) == VAR_ERROR;
  obs.back();
  success &= obs.var_value(0) == VAR_ERROR;
  success &= obs.var_value(1) == VAR_ERROR;
  success &= obs.var_value(2) == VAR_ERROR;
  success &= obs.var_value(3) == VAR_ERROR;

  return success;

}

static bool test_new_clause()
{
  bool success = true;
  options opt(nullptr, 0);
  observer obs(opt);
  obs.notify(new new_variable(1));
  obs.notify(new new_variable(2));
  obs.notify(new new_variable(3));
  obs.notify(new new_clause(0, { literal(1, 1), literal(2, 1), literal(3, 1) }, false, true));
  auto clauses = obs.get_clauses();
  success &= clauses.size() == 1;
  assert(clauses.size() > 0);
  success &= clauses[0].first == 0;
  const vector<Tlit> literals = *clauses[0].second;
  success &= literals.size() == 3;
  assert(literals.size() >= 3);
  success &= literals[0] == literal(1, 1);
  success &= literals[1] == literal(2, 1);
  success &= literals[2] == literal(3, 1);

  return success;
}

int main()
{
  vector<pair<string, function<bool()>>> tests = {
    {"new_variables_forward", test_new_variables_forward},
    {"new_variables_backward", test_new_variables_backward},
    {"new_clause", test_new_clause}
  };

  for (auto test : tests) {
    cout << "Test " << left << setw(30) << test.first << " : ";
    if (test.second())
      cout << "OK" << endl;
    else
      cout << "FAILED" << endl;
  }
}
