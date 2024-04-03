#pragma once

#include "SAT-config.hpp"

#include <iostream>

namespace sat
{
  class statistics
  {
    unsigned long long conflicts = 0;
    unsigned long long propagations = 0;
    unsigned long long decisions = 0;
    unsigned restarts = 0;
    unsigned long long external_clauses = 0;
    unsigned long long learned_clauses = 0;
    unsigned long long purged_clauses = 0;
    unsigned long long deleted_clauses = 0;

    unsigned _n_backtracks = 0;
    double average_backtrack_difference = 0;

  public:
    inline void inc_conflicts() { conflicts++; }
    inline void inc_propagations() { propagations++; }
    inline void inc_decisions() { decisions++; }
    inline void inc_restarts() { restarts++; }
    inline void inc_external_clauses() { external_clauses++; }
    inline void inc_learned_clauses() { learned_clauses++; }
    inline void inc_purged_clauses() { purged_clauses++; }
    inline void inc_deleted_clauses() { deleted_clauses++; }

    inline void add_backtrack_difference(double difference)
    {
      _n_backtracks++;
      average_backtrack_difference += (difference - average_backtrack_difference) / _n_backtracks;
    }

    inline std::string format_long(long long number)
    {
      std::string str = std::to_string(number);
      int i = str.size() - 3;
      while (i > 0) {
        str.insert(i, ",");
        i -= 3;
      }
      return str;
    }

    void print()
    {
      std::cout << "c conflicts:    " << format_long(conflicts) << "\n";
      std::cout << "c propagations: " << format_long(propagations) << "\n";
      std::cout << "c decisions:    " << format_long(decisions) << "\n";
      std::cout << "c restarts:     " << format_long(restarts) << "\n";
      std::cout << "c external clauses: " << format_long(external_clauses) << "\n";
      std::cout << "c learned clauses:  " << format_long(learned_clauses) << "\n";
      std::cout << "c purged clauses:   " << format_long(purged_clauses) << "\n";
      std::cout << "c deleted clauses:  " << format_long(deleted_clauses) << "\n";
      std::cout << "c average backtrack difference: " << average_backtrack_difference << "\n";
    }
  };
}
