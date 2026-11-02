#pragma once
#include <stdbool.h>
#include <stdint.h>
bool accel_init(void);
bool accel_read(int16_t xyz[3]);
