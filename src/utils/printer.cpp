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
#include <string>
#include <vector>
#include <algorithm>
#include <cassert>

using namespace std;

const char ESC_LOCK_START = "🔒"[0];
const char ESC_LOCK_END = "🔒"[4];
namespace napsat
{

unsigned string_length_escaped(string const str)
{
  unsigned n_escaped = 0;
  bool escaping = false;
  for (size_t i = 0; i < str.length(); i++) {
    char c = str[i];
    escaping |= c == ESC_CHAR;
    n_escaped += escaping;
    escaping &= c != ESC_END;

    if (c == ESC_LOCK_START && str.substr(i, 4) == "🔒") {
      n_escaped += 2; // the lock emoji is 4 bytes in UTF-8 but we want to count it as 1 character
    }


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
  if (n == 0) return "0";
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

string pretty_float(double f, unsigned n)
{
  string s = pretty_integer((long long)f);
  if (n)
    s += ".";
  while (n--) {
    f *= 10;
    s += to_string((long long)f % 10);
  }
  return s;
}

string pretty_time(chrono::microseconds time)
{
  string str = "";
  const long long ms = time.count() / 1000;
  const long long hours = ms / 3600000;
  const long long minutes = (ms % 3600000) / 60000;
  const long long seconds = (ms % 60000) / 1000;
  const long long microseconds = ms % 1000;
  if (hours > 0)
    str += to_string(hours) + "h ";
  if (minutes > 0)
    str += to_string(minutes) + "m ";
  if (seconds > 0)
    str += to_string(seconds) + "s ";
  str += to_string(microseconds) + "ms";
  return str;
}

std::string justify_string(const std::string& str, unsigned width, char fill, const std::string& prefix)
{
  assert(width > prefix.length());
  width -= prefix.length();
  string justified_str = "";
  // separate the string into words
  vector<string> words;
  string word = "";
  for (char c : str) {
    if (c == ' ' || c == '\n') {
      if (c == '\n') {
        word += c;
      }
      if (!word.empty()) {
        words.push_back(word);
        word = "";
      }
    } else {
      word += c;
    }
  }
  if (!word.empty()) {
    words.push_back(word);
  }

  // reverse the words to process them in reverse order
  std::reverse(words.begin(), words.end());

  // check how many words can fit in the given width
  while (!words.empty()) {
    unsigned line_length = 0;
    vector<string> line_words;
    bool eol = false;
    do  {
      line_length += words.back().length() + (line_words.empty() ? 0 : 1);
      line_words.push_back(words.back());
      words.pop_back();
      string& last_word = line_words.back();
      eol = last_word.back() == '\n' || words.empty();
    } while (!eol && line_length + words.back().length() < width);

    unsigned extra_spaces = width - line_length;

    // if this is the last line, or there is a line break, don't justify it
    if (words.empty() || eol || line_words.size() < extra_spaces) {
      extra_spaces = 0;
    }

    if (extra_spaces > 0) {
      // we might have some extra spaces to fill. Fill the line
      // 1. If the word ends with a punctuation mark, we don't add extra spaces after it.
      // 2. Pick the longest words first to add extra spaces after them.
      std::vector<unsigned> line_words_sorted;
      for (unsigned i = 0; i < line_words.size() - 1; i++) {
        // we cannot add a space after the last word, so we skip it
        line_words_sorted.push_back(i);
      }
      std::sort(line_words_sorted.begin(), line_words_sorted.end(), [&line_words](unsigned a, unsigned b) {
        bool a_punct = ispunct(line_words[a].back());
        bool b_punct = ispunct(line_words[b].back());
        if (a_punct && !b_punct) return false;
        if (!a_punct && b_punct) return true;
        return line_words[a].length() > line_words[b].length();
      });

      for (unsigned w_idx : line_words_sorted) {
        if (extra_spaces == 0)
          break;
        string& w = line_words[w_idx];
        w += fill;
        extra_spaces--;
      }
    }

    justified_str += prefix;
    for (size_t i = 0; i < line_words.size(); i++) {
      if (i)
        justified_str += " ";
      justified_str += line_words[i];
    }
    if (!eol)
      justified_str += "\n";
  }
  return justified_str;
}

}
