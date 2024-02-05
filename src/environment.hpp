#pragma once
/**
 * This file defines the environment of the SAT solver.
 * It is used to get global information about the problem.
*/

#include <string>

namespace sat::env
{
  static std::string input_file;

  static std::string problem_name;

  static std::string man_page_directory;

} // namespace sat
