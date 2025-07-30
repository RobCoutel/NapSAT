/**
 * @file include/SAT-config.hpp
 * @author Robin Coutelier
 *
 * @brief This file is part of the NapSAT solver. It contains the configuration of the SAT
 * solver.
 */
#pragma once

#define VERSION "1.0.0"

// If this option is enabled, if the observer is enabled, the solver will notify the observer of the assertion failures
// This will interrupt the solver and display the assertion failure and the current state of the observer.
#define OBSERVED_ASSERTS 1

// If this option is enabled, the solver will check the consistency of the clauses
#define USE_OBSERVER 1

// It is not recommended to enable this option for difficult problems, as it will slow down the solver significantly.
// If the observer is not enabled, this option has no effect.
#define NOTIFY_WATCH_CHANGES 0
