#pragma once

namespace sat
{
  enum status
  {
    // All variables are assigned and no conflict was found.
    SAT,
    // The clause set is unsatisfiable.
    UNSAT,
    // The solver does not know if the clause set is satisfiable or not.
    UNDEF
  };
  /**
   * @brief Type to denote a variable.
   * @details The variable 0 is not used. The first variable is 1.
   */
  typedef unsigned Tvar;

  /**
   * @brief Type to denote a propositional literal. The first bit is the polarity of the literal, the other bits are the variable.
   * @details The polarity is 0 for negative literals and 1 for positive literals.
   * @details The literals 0 and 1 are not used. The first literal is 2 for variable 1 and polarity 0.
   */
  typedef unsigned Tlit;

  /**
   * @brief Type to denote the value of a variable.
   * @details The value can be SAT_VAR_TRUE, SAT_VAR_FALSE or SAT_VAR_UNDEF.
   */
  typedef unsigned Tval;

  /**
   * @brief Type to denote a decision level.
   * @details The decision level 0 is the root level. The first decision level is 1.
   * @details A decision level of SAT_LEVEL_UNDEF = 0xFFFFFFFF means that the variable is unassigned.
   */
  typedef unsigned Tlevel;

  /**
   * @brief Type to denote the ID a clause.
   * @details The first clause has the ID 0.
   */
  typedef unsigned Tclause;

  const Tlit SAT_LIT_UNDEF = 0;

  const Tval SAT_VAR_TRUE = 0;
  const Tval SAT_VAR_FALSE = 1;
  const Tval SAT_VAR_UNDEF = 2;

  const Tlevel SAT_LEVEL_ROOT = 0;
  const Tlevel SAT_LEVEL_UNDEF = 0xFFFFFFFF;

  const Tclause SAT_CLAUSE_UNDEF = 0xFFFFFFFF;
  const Tclause SAT_CLAUSE_LAZY = 0xFFFFFFFE;

  /*************************************************************************/
  /*                         Operation on literals                         */
  /*************************************************************************/
  /**
   * @brief Returns a literal corresponding to the given variable and polarity.
   */
  inline Tlit literal(Tvar var, unsigned pol) { return var << 1 | pol; }

  /**
   * @brief Returns the variable of the given literal.
   * @param lit literal to evaluate.
   * @return variable of the literal.
   */
  inline Tvar lit_to_var(Tlit lit) { return lit >> 1; }

  /**
   * @brief Returns the negation of the given literal.
   * @param lit literal to evaluate.
   * @return negation of the literal.
   */
  inline Tlit lit_neg(Tlit lit) { return lit ^ 1; }

  /**
   * @brief Returns the polarity of the given literal.
   * @param lit literal to evaluate.
   * @return polarity of the literal (0 for negative literals, 1 for positive literals)
   */
  inline unsigned lit_pol(Tlit lit) { return lit & 1; }

}