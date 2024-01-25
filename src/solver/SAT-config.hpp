#pragma once

#ifndef NDEBUG
#define LOG_LEVEL 0
#define DEBUG_CLAUSE 0
#define STATS 0
#define OBSERVER_ENABLED 1
#else
#define LOG_LEVEL 0
#define DEBUG_CLAUSE 0
#define STATS 0
#define OBSERVER_ENABLED 0
#endif

#define OBSERVED_ASSERTS 1


// It is not recommended to enable this option, as it will slow down the solver significantly.
#define NOTIFY_WATCH_CHANGES 1
#define SAT_BLOCKERS 1
#define SAT_WEAK_BLOCKERS 0
#define SAT_GREEDY_STOP 1
