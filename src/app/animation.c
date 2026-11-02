#include "animation.h"
// Cubic smoothstep, fixed point; no floating point, heap, or animation
// framework.
static unsigned ease(unsigned elapsed, unsigned duration) {
  if (elapsed >= duration)
    return 1000;
  unsigned x = elapsed * 1000 / duration;
  return x * x * (3000 - 2 * x) / 1000000;
}
uint8_t animation_level(unsigned ms, unsigned from, unsigned target) {
  if (ms >= DISPLAY_DURATION_MS)
    return 0;
  if (ms < FADE_IN_MS) {
    unsigned f = ease(ms, FADE_IN_MS);
    return (from * (1000 - f) + target * f) / 1000;
  }
  if (ms < DISPLAY_DURATION_MS - FADE_OUT_MS)
    return target;
  return target *
         (1000 - ease(ms - (DISPLAY_DURATION_MS - FADE_OUT_MS), FADE_OUT_MS)) /
         1000;
}
unsigned animation_next_ms(unsigned ms) {
  if (ms >= DISPLAY_DURATION_MS)
    return 0;
  if (ms < FADE_IN_MS || ms >= DISPLAY_DURATION_MS - FADE_OUT_MS)
    return 10;
  return DISPLAY_DURATION_MS - FADE_OUT_MS - ms;
}
