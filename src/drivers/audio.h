#pragma once
#include <stdbool.h>
#include <stdint.h>
bool audio_disable(void);

#define AUDIO_DIAGNOSTIC_COUNT 4u
// Ordered service register snapshot; failed reads are UINT32_MAX.
void audio_diagnostics(uint32_t values[AUDIO_DIAGNOSTIC_COUNT]);
