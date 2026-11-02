#pragma once
#include <stdint.h>
#define POWER_REGISTER_COUNT 8
void power_read(uint32_t registers[POWER_REGISTER_COUNT]);
void radios_disable(void);
void unused_peripheral_clocks_disable(void);
uint32_t unused_clocks_enabled(void);
uint32_t unused_function_clocks_enabled(void);
