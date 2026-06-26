/**
 * @file include/SAT-types.hpp
 * @author Robin Coutelier
 *
 * @brief This file is part of the NapSAT solver. It defines the basic types used in the SAT
 * solver, and some basic operations on these types.
 */
#pragma once

#include <string>
#include <ostream>
#include <cassert>
#include <functional>
#include <cstdint>

namespace napsat
{
  enum class status {
    // All variables are assigned and no conflict was found.
    SAT,
    // The clause set is unsatisfiable.
    UNSAT,
    // The solver does not know if the clause set is satisfiable or not.
    UNDEF,
    // An error occurred.
    ERROR,
    // The solver was stopped by the user.
    INTERRUPTED,
    // The solver timed out.
    TIMEOUT
  };

  /**
   * @brief Type to denote the value of a variable.
   * @details The value can be VAR_TRUE, VAR_FALSE or VAR_UNDEF.
   */
  enum class Tval : std::uint8_t {
    VAR_FALSE = 0,
    VAR_TRUE  = 1,
    VAR_UNDEF = 2,
    VAR_ERROR = 3
  };
  // define the bitwise operators for Tval
  inline std::uint8_t operator^(std::uint8_t a, Tval b) {
    return a ^ static_cast<std::uint8_t>(b);
  }
  inline std::uint8_t operator>>(Tval a, std::uint8_t b) {
    return static_cast<std::uint8_t>(a) >> b;
  }



  /**
   * @brief Type to denote a variable.
   * @details The variable 0 is not used. The first variable is 1.
   */
  typedef struct Tvar {
    unsigned int value;

    Tvar() : value(0) {}
    Tvar(unsigned value) : value(value) {}
    inline bool operator==(const Tvar& other) const { return value == other.value; }
    inline bool operator!=(const Tvar& other) const { return value != other.value; }
    inline bool operator< (const Tvar& other) const { return value < other.value;  }
    inline bool operator> (const Tvar& other) const { return value > other.value;  }
    inline bool operator<=(const Tvar& other) const { return value <= other.value; }
    inline bool operator>=(const Tvar& other) const { return value >= other.value; }
    inline Tvar operator++()    { value++; return *this; }
    inline Tvar operator++(int) { Tvar tmp = *this; value++; return tmp; }
    inline Tvar operator--()    { value--; return *this; }
    inline Tvar operator--(int) { Tvar tmp = *this; value--; return tmp; }
    inline Tvar operator+(const Tvar& other) const { return Tvar(value + other.value); }
    inline Tvar operator-(const Tvar& other) const { return Tvar(value - other.value); }
    inline Tvar operator+=(const Tvar& other) { value += other.value; return *this; }
    inline Tvar operator-=(const Tvar& other) { value -= other.value; return *this; }
    inline Tvar operator+(unsigned int n) const { return Tvar(value + n); }
    inline Tvar operator-(unsigned int n) const { return Tvar(value - n); }
    inline Tvar operator+=(unsigned int n) { value += n; return *this; }
    inline Tvar operator-=(unsigned int n) { value -= n; return *this; }

    inline std::string to_string() const { return "v" + std::to_string(value); }
    inline std::ostream& operator<<(std::ostream& os) const { os << to_string(); return os; }
  } Tvar;

  /**
   * @brief Type to denote a propositional literal. The first bit is the polarity of the literal, the other bits are the variable.
   * @details The polarity is 0 for negative literals and 1 for positive literals.
   * @details The literals 0 and 1 are not used. The first literal is 2 for variable 1 and polarity 0.
   */
  typedef struct Tlit {
    unsigned int value;

    Tlit() : value(0) {}
    Tlit(unsigned value) : value(value) {}
    Tlit(Tvar var, unsigned pol) : value(var.value << 1 | pol) { assert(pol == 0 || pol == 1); }
    inline bool operator==(const Tlit& other) const { return value == other.value; }
    inline bool operator!=(const Tlit& other) const { return value != other.value; }
    inline bool operator<(const Tlit& other) const { return value < other.value; }
    inline Tlit operator~() const { return Tlit(value ^ 1); }
    inline Tvar var() const { return Tvar(value >> 1); }
    inline bool pol() const { return value & 1; }
    inline std::string to_string() const { return (pol() ? "" : "~") + std::to_string(var().value); }
    inline std::ostream& operator<<(std::ostream& os) const { os << to_string(); return os; }

    inline Tlit operator^(Tlit a) const { return Tlit(value ^ a.value); }
    inline Tlit operator^=(Tlit a) { value ^= a.value; return *this; }

    inline bool satisfied(Tval val) const { return !(pol() ^ val); }
    inline bool falsified(Tval val) const { return !(pol() ^ val ^ 1); }
    inline bool undefined(Tval val) const { return val >> 1; }
  } Tlit;

  /**
   * @brief Type to denote a decision level.
   * @details The decision level 0 is the root level. The first decision level is 1.
   * @details A decision level of LEVEL_UNDEF = 0xFFFFFFFF means that the variable is unassigned.
   */
  typedef struct Tlevel {
    unsigned int value;

    Tlevel(unsigned value) : value(value) {}
    Tlevel(const Tlevel& other) : value(other.value) {}
    Tlevel() : value(0xFFFFFFFF) {}
    inline bool operator==(const Tlevel& other) const { return value == other.value; }
    inline bool operator!=(const Tlevel& other) const { return value != other.value; }
    inline bool operator< (const Tlevel& other) const { return value < other.value; }
    inline bool operator> (const Tlevel& other) const { return value > other.value; }
    inline bool operator<=(const Tlevel& other) const { return value <= other.value; }
    inline bool operator>=(const Tlevel& other) const { return value >= other.value; }
    inline void operator++() { value++; }
    inline void operator++(int) { value++; }
    inline void operator--() { value--; }
    inline void operator--(int) { value--; }
    inline Tlevel operator+(const Tlevel& other) const { return Tlevel(value + other.value); }
    inline Tlevel operator-(const Tlevel& other) const { return Tlevel(value - other.value); }
    inline Tlevel operator+=(const Tlevel& other) { value += other.value; return *this; }
    inline Tlevel operator-=(const Tlevel& other) { value -= other.value; return *this; }

    inline Tlevel operator+(unsigned int n) const { return Tlevel(value + n); }
    inline Tlevel operator-(unsigned int n) const { return Tlevel(value - n); }
    inline Tlevel operator+=(unsigned int n) { value += n; return *this; }
    inline Tlevel operator-=(unsigned int n) { value -= n; return *this; }
    inline std::ostream& operator<<(std::ostream& os) const { os << to_string(); return os; }

    inline std::string to_string() const {
      if (value == 0xFFFFFFFF) return "∞";
      if (value == 0xFFFFFFFE) return "ERROR";
      return std::to_string(value);
    }
  } Tlevel;

  /**
   * @brief Type to denote the ID a clause.
   * @details The first clause has the ID 0.
   */
  typedef struct Tclause {
    unsigned int value;

    Tclause(unsigned value) : value(value) {}
    Tclause() : value(0xFFFFFFFF) {}
    inline bool operator==(const Tclause& other) const { return value == other.value; }
    inline bool operator!=(const Tclause& other) const { return value != other.value; }
    inline bool operator<(const Tclause& other) const { return value < other.value; }
    inline bool operator>(const Tclause& other) const { return value > other.value; }
    inline bool operator<=(const Tclause& other) const { return value <= other.value; }
    inline bool operator>=(const Tclause& other) const { return value >= other.value; }
    inline Tclause operator++() { value++; return *this; }
    inline Tclause operator++(int) { Tclause tmp = *this; value++; return tmp; }
    inline Tclause operator--() { value--; return *this; }
    inline Tclause operator--(int) { Tclause tmp = *this; value--; return tmp; }
    inline Tclause operator+(unsigned int n) const { return Tclause(value + n); }
    inline Tclause operator-(unsigned int n) const { return Tclause(value - n); }
    inline Tclause operator+=(unsigned int n) { value += n; return *this; }
    inline Tclause operator-=(unsigned int n) { value -= n; return *this; }
    inline Tclause operator+(const Tclause& other) const { return Tclause(value + other.value); }
    inline Tclause operator-(const Tclause& other) const { return Tclause(value - other.value); }
    inline Tclause operator+=(const Tclause& other) { value += other.value; return *this; }
    inline Tclause operator-=(const Tclause& other) { value -= other.value; return *this; }

    inline std::string to_string() const {
      if (value == 0xFFFFFFFF) return "UNDEF";
      if (value == 0xFFFFFFFE) return "LAZY";
      if (value == 0xFFFFFFFD) return "ASSUMPTION";
      return "C" + std::to_string(value);
    }

    inline unsigned long hash() const {
      return std::hash<unsigned int>()(value);
    }

    inline std::ostream& operator<<(std::ostream& os) const { os << "C" << to_string(); return os; }
  } Tclause;

  const Tlit LIT_UNDEF = 0;

  const Tlevel LEVEL_ROOT  = 0;
  const Tlevel LEVEL_UNDEF = 0xFFFFFFFF;
  const Tlevel LEVEL_ERROR = 0xFFFFFFFE;

  const Tclause CLAUSE_UNDEF      = 0xFFFFFFFF;
  const Tclause CLAUSE_LAZY       = 0xFFFFFFFE;
  const Tclause CLAUSE_ASSUMPTION = 0xFFFFFFFD;
  const Tclause CLAUSE_ERROR      = 0xFFFFFFFC;

  template <typename T, typename Index>
  class indexed_vector : public std::vector<T> {
      using base = std::vector<T>;

  public:
      using base::base; // inherit std::vector constructors

      T& operator[](Index idx) {
          return base::operator[](idx.value);
      }

      const T& operator[](Index idx) const {
          return base::operator[](idx.value);
      }

      Index size() const {
          return Index(base::size());
      }

      void resize(Index n) {
          base::resize(n.value);
      }

      void resize(Index n, const T& value) {
          base::resize(n.value, value);
      }

      void reserve(Index n) {
          base::reserve(n.value);
      }
  };

}
