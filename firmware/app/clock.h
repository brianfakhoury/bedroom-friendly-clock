#pragma once
#include <stdbool.h>
#include <stdint.h>
// The entire presentation state. Time itself belongs to the external RTC.
typedef struct {
  bool visible;
  unsigned brightness, rotation, fade_from, level;
  uint64_t shown_at;
  uint32_t activations, prepare_us;
} clock_state;
bool clock_activate(clock_state *clock);
// Returns true exactly when a visible interval ends.
bool clock_update(clock_state *clock);
// Absolute SYSTIMER deadline; zero means no scheduled clock work.
uint64_t clock_deadline(const clock_state *clock);
