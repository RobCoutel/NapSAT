/**
 * @file include/SAT-config.hpp
 * @author Robin Coutelier
 *
 * @brief This file is part of the NapSAT solver. It contains the configuration of the SAT
 * solver.
 */
#pragma once

#define VERSION "1.0.0"

#define OBSERVED_ASSERTS 1

// It is not recommended to enable this option for difficult problems, as it will slow down the solver significantly.
#define NOTIFY_WATCH_CHANGES 1
#define SAT_BLOCKERS 1
#define SAT_WEAK_BLOCKERS 0
#define SAT_GREEDY_STOP 1
