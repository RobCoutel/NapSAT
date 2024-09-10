#pragma once
#include "SAT-options.hpp"

#include <iostream>
#include <string>

const char ESC_CHAR = '\033'; // the decimal code for escape character is 27
const char ESC_END = 'm';

/**
 * @brief Returns the length of a string without the escape characters.
 */
unsigned string_length_escaped(std::string const str);

/**
 * @brief Adds spaces to the left of the number to make it have as many digits as the maximum number of digits in the given range.
 * @param n The number to pad.
 * @param max_int The maximum number in the range.
 * @pre max_int > 0
 * @return A string of spaces such that any n in [0, max_int] will have the same number of digits.
 */
std::string pad(unsigned n, unsigned max_int);


/**
 * @brief Returns a pretty string representation of an integer.
 */
std::string pretty_integer(long long n);

const std::string ERROR_HEAD = "\033[1;31mERROR: \033[0m";
const std::string WARNING_HEAD = "\033[0;33mWARNING: \033[0m";
const std::string INFO_HEAD = "\033[34mINFO: \033[0m";

#define LOG_ERROR(msg) std::cerr << ERROR_HEAD << msg << std::endl
#define LOG_WARNING(msg) if(!napsat::env::get_suppress_warning()) std::cout << WARNING_HEAD << msg << std::endl
#define LOG_INFO(msg)    if(!napsat::env::get_suppress_info()) std::cout << INFO_HEAD << msg << std::endl
