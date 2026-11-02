#pragma once
#include <stdint.h>

#define TICKS_PER_US 16u
#define TICKS_PER_MS 16000u
void timer_init(void);
uint64_t ticks(void);
void delay_us(uint32_t us);
