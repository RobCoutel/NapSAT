/**
 * @file src/solver/custom-assert.hpp
 * @author Robin Coutelier
 *
 * @brief This file is part of the SAT solver NapSAT. It defines custom assertions to send notifications to the observer.
 */
#pragma once

#include <cassert>

#ifndef NDEBUG
#if OBSERVED_ASSERTS
#define ASSERT(cond)                                                \
  if (_observer) {                                                  \
    if (!(cond))  {                                                 \
      NOTIFY_OBSERVER(_observer, new napsat::gui::marker("Assertion failed: " #cond));  \
      assert(cond);                                                 \
    }                                                               \
  } else {                                                          \
    assert(cond);                                                   \
  }

#define ASSERT_MSG(cond, msg)                                       \
  if (_observer) {                                                  \
    if (!(cond))  {                                                 \
      std::cout << msg << std::endl;                                          \
      NOTIFY_OBSERVER(_observer, new napsat::gui::marker("Assertion failed: " #cond));  \
      assert(cond);                                                 \
    }                                                               \
  } else {                                                          \
    assert(cond);                                                   \
  }
#else
#define ASSERT(cond) assert(cond);
#define ASSERT_MSG(cond, msg) assert(cond && msg);
#endif
#else
#define ASSERT(cond)
#define ASSERT_MSG(cond, msg)
#endif
