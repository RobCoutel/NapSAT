/*
 * This file is part of the source code of the software program
 * NapSAT. It is protected by applicable copyright laws.
 *
 * This source code is protected by the terms of the MIT License.
 */
/**
 * @file src/utils/printer.hpp
 * @author Robin Coutelier
 * @brief This file is part of the NapSAT solver. It defines functions for string manipulation and pretty printing.
 */
#pragma once
#include "SAT-options.hpp"

#include <iostream>
#include <string>
#include <chrono>

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
 * @param n The integer to convert.
 * @return A string representation of the integer, with commas every three digits.
 */
std::string pretty_integer(long long n);

/**
 * @brief Returns a string representation of a time in milliseconds.
 * @param time The time to convert.
 * @return A string representation of the time.
 */
std::string pretty_time(std::chrono::milliseconds time);

const std::string ERROR_HEAD = "\033[1;31mERROR: \033[0m";
const std::string WARNING_HEAD = "\033[0;33mWARNING: \033[0m";
const std::string INFO_HEAD = "\033[34mINFO: \033[0m";

#define LOG_ERROR(msg) std::cerr << ERROR_HEAD << msg << std::endl
#define LOG_WARNING(msg) if(!napsat::env::get_suppress_warning()) std::cout << WARNING_HEAD << msg << std::endl
#define LOG_INFO(msg)    if(!napsat::env::get_suppress_info()) std::cout << INFO_HEAD << msg << std::endl
