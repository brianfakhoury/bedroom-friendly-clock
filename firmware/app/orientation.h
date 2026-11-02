#pragma once
#include <stdint.h>
unsigned orientation_choose(const int16_t xyz[3], unsigned previous);
void orientation_rect(unsigned rotation, unsigned *x, unsigned *y, unsigned *w,
                      unsigned *h);
