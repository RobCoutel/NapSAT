#pragma once

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
