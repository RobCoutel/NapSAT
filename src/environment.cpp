#include "environment.hpp"

#include <string>

static std::string input_file;
static std::string problem_name;
static std::string man_page_directory;
static std::string invariant_configuration_folder;

std::string sat::env::get_input_file() {
  return input_file;
}

std::string sat::env::get_problem_name() {
  return problem_name;
}

std::string sat::env::get_man_page_directory() {
  return man_page_directory;
}

std::string sat::env::get_invariant_configuration_folder() {
  return invariant_configuration_folder;
}

void sat::env::set_input_file(std::string file) {
  input_file = file;
}

void sat::env::set_problem_name(std::string name) {
  problem_name = name;
}

void sat::env::set_man_page_directory(std::string dir) {
  man_page_directory = dir;
}

void sat::env::set_invariant_configuration_folder(std::string dir) {
  invariant_configuration_folder = dir;
}
