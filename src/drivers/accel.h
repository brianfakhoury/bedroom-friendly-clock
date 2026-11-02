#pragma once
#include <stdbool.h>
#include <stdint.h>
bool accel_init(void);
bool accel_read(int16_t xyz[3]);

#define ACCEL_DIAGNOSTIC_COUNT 2u
// Ordered service register snapshot; failed reads are UINT32_MAX.
void accel_diagnostics(uint32_t values[ACCEL_DIAGNOSTIC_COUNT]);
