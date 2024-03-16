#pragma once
/**
 * This file defines the environment of the SAT solver.
 * It is used to get global information about the problem.
*/

#include <string>

namespace sat::env
{
  std::string get_input_file();

  std::string get_problem_name();

  std::string get_man_page_directory();

  std::string get_invariant_configuration_folder();

  void set_input_file(std::string file);

  void set_problem_name(std::string name);

  void set_man_page_directory(std::string dir);

  void set_invariant_configuration_folder(std::string dir);
} // namespace sat
