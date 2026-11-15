// Host-sim stub for hardware/sync.h -- single-threaded simulation, the
// barrier is a compiler fence (interleavings are modeled by call order).
#pragma once
static inline void __dmb(void) { __asm volatile("" ::: "memory"); }
