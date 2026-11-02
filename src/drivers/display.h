#pragma once
#include "hal.h"
bool display_init(void);
void display_rect(unsigned x, unsigned y, unsigned w, unsigned h,
                  uint16_t color);
void display_on(void);
void display_off(void);
void display_brightness(unsigned value);
