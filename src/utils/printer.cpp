/*
 * This file is part of the source code of the software program
 * NapSAT. It is protected by applicable copyright laws.
 *
 * This source code is protected by the terms of the MIT License.
 */
/**
 * @file src/utils/printer.cpp
 * @author Robin Coutelier
 * @brief This file is part of the NapSAT solver. It implements functions for string manipulation and pretty printing.
 */
#include "printer.hpp"

#include <iostream>
#include <cassert>

using namespace std;

unsigned string_length_escaped(string const str)
{
  unsigned n_escaped = 0;
  bool escaping = false;
  for (char c : str) {
    escaping |= c == ESC_CHAR;
    n_escaped += escaping;
    escaping &= c != ESC_END;
  }
  return str.length() - n_escaped;
}

static inline unsigned log10(int n)
{
  assert(n > 0);
  unsigned digits = 0;
  while (n > 0) {
    n /= 10;
    digits++;
  }
  return digits;
}

string pad(unsigned n, unsigned max_int)
{
  n = max(n, 1u);
  int max_digits = log10(max_int);
  int digits = log10(n);
  string s = "";
  for (int i = digits; i < max_digits; i++)
    s += " ";
  return s;
}

string pretty_integer(long long n)
{
  string s = "";
  while (n > 0) {
    s = to_string(n % 1000) + "," + s;
    n /= 1000;
    if (s.size() % 4 != 0 && n > 0)
      s = string(4 - s.size() % 4, '0') + s;
  }
  if (s.size() > 0)
    s = s.substr(0, s.size() - 1);
  return s;
}

string pretty_time(chrono::milliseconds time)
{
  string str = "";
  const long long ms = time.count();
  const long long hours = ms / 3600000;
  const long long minutes = (ms % 3600000) / 60000;
  const long long seconds = (ms % 60000) / 1000;
  const long long milliseconds = ms % 1000;
  if (hours > 0)
    str += to_string(hours) + "h ";
  if (minutes > 0)
    str += to_string(minutes) + "m ";
  if (seconds > 0)
    str += to_string(seconds) + "s ";
  str += to_string(milliseconds) + "ms";
  return str;
}
