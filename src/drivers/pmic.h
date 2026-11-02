#pragma once
#include <stdbool.h>
#include <stdint.h>
bool pmic_panel_reset(void);
bool pmic_led_off(void);
bool pmic_unused_rails_off(void);

#define PMIC_DIAGNOSTIC_COUNT 10u
// Ordered service register snapshot; failed reads are UINT32_MAX.
void pmic_diagnostics(uint32_t values[PMIC_DIAGNOSTIC_COUNT]);
