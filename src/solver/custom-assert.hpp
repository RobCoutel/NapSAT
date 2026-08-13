/*
 * This file is part of the source code of the software program
 * NapSAT. It is protected by applicable copyright laws.
 *
 * This source code is protected by the terms of the MIT License.
 */
/**
 * @file src/solver/custom-assert.hpp
 * @author Robin Coutelier
 *
 * @brief This file is part of the NapSAT solver. It defines custom assertions to send notifications to the observer.
 */
#pragma once

#include "../utils/printer.hpp"

#include <cassert>

// Source - https://stackoverflow.com/a/55420185
// Posted by John Bollinger, modified by community. See post 'Timeline' for change history
// Retrieved 2026-06-23, License - CC BY-SA 4.0
#define is_empty(...) ( sizeof((int[]){__VA_ARGS__})/sizeof(int) == 0)


#ifndef NDEBUG
#if OBSERVED_ASSERTS && USE_OBSERVER
#define ASSERT(cond, ...)                                                   \
  do {                                                                      \
    if (!(cond))  {                                                         \
      LOG_ERROR("Assertion failed: " #cond);                                \
      __VA_OPT__(LOG_ERROR("MESSAGE: " << __VA_ARGS__));                    \
      NOTIFY(message, ("Assertion failed: " #cond));    \
      assert(cond);                                                         \
    }                                                                       \
  } while(0)

#else
// Variadic like the observed variant above: call sites pass an optional
// message after the condition, which this variant simply ignores.
#define ASSERT(cond, ...) assert(cond)
#endif
#else
#define ASSERT(cond, ...) ((void)0)
#endif
