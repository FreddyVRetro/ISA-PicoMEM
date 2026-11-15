// Host-sim stub for pico/time.h -- the simulated clock is owned by the test
// driver (g_sim_now), so scenarios control time explicitly.
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern uint64_t g_sim_now;
static inline uint64_t time_us_64(void) { return g_sim_now; }
static inline uint32_t time_us_32(void) { return (uint32_t)g_sim_now; }

#ifdef __cplusplus
}
#endif
