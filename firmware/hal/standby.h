#pragma once
#include <stdint.h>

// Initialize the PMU once at cold boot, before board radio shutdown.
void standby_init(void);

// Powers down HP TOP/AON/CPU; wake restarts at wake_entry with a fresh stack.
// Returns only when touch was already pending during the pin handoff.
void standby_enter(void);

// Restore MCU execution state. Actual reset-wake also needs board_resume().
void standby_resume(void);
uint32_t standby_cause(void);
