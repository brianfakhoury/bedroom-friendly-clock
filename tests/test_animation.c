#include "animation.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
  for (unsigned target = 1; target <= 255; target++) {
    unsigned previous = 0;
    for (unsigned ms = 0; ms <= FADE_IN_MS; ms++) {
      unsigned level = animation_level(ms, 0, target);
      assert(level >= previous && level <= target);
      previous = level;
    }
    assert(previous == target);
    for (unsigned ms = FADE_IN_MS; ms <= DISPLAY_DURATION_MS; ms++) {
      unsigned level = animation_level(ms, 0, target);
      if (ms < DISPLAY_DURATION_MS - FADE_OUT_MS)
        assert(level == target);
      assert(level <= previous);
      previous = level;
    }
    assert(previous == 0);
    assert(animation_level(DISPLAY_DURATION_MS + 1, 0, target) == 0);
  }
  // A tap during fade-out starts from the existing level without a jump.
  for (unsigned from = 0; from <= 255; from++) {
    assert(animation_level(0, from, 255) == from);
    assert(animation_level(FADE_IN_MS, from, 255) == 255);
  }
  assert(animation_next_ms(0) == 10);
  assert(animation_next_ms(FADE_IN_MS) == 2470);
  assert(animation_next_ms(DISPLAY_DURATION_MS - FADE_OUT_MS) == 10);
  assert(animation_next_ms(DISPLAY_DURATION_MS) == 0);
  puts("PASS: fade endpoints, monotonicity, hold, retrigger, event timing");
}
