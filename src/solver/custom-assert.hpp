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

#ifndef NDEBUG
#if OBSERVED_ASSERTS && USE_OBSERVER
#define ASSERT(cond) \
  do {                                                              \
    if (_observer) {                                                \
      if (!(cond))  {                                               \
        NOTIFY_OBSERVER(marker, "Assertion failed: " #cond);  \
        assert(cond);                                               \
      }                                                             \
    } else {                                                        \
      assert(cond);                                                 \
    }                                                               \
  } while(0)

#define ASSERT_MSG(cond, msg) \
  do {                                                              \
    if (!(cond))  {                                                 \
      LOG_ERROR( "MESSAGE: " << msg );                              \
      NOTIFY_OBSERVER(marker, "Assertion failed: " #cond);  \
      assert(cond);                                                 \
    }                                                               \
  } while(0)
#else
#define ASSERT(cond) assert(cond);
#define ASSERT_MSG(cond, msg) assert(cond);
#endif
#else
#define ASSERT(cond)           ((void)0)
#define ASSERT_MSG(cond, msg)  ((void)0)
#endif
