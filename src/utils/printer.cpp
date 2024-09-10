/**
 * @file utils/printer.cpp
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
