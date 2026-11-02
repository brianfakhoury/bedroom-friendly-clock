#include "clock.h"
#include "accel.h"
#include "animation.h"
#include "digits.h"
#include "display.h"
#include "hal.h"
#include "local_time.h"
#include "orientation.h"

static bool prepare_frame(clock_state *clock, bool activation) {
  uint64_t start = ticks();
  unsigned rotation = clock->rotation;
  int16_t xyz[3];
  if (accel_read(xyz))
    rotation = orientation_choose(xyz, rotation);
  bool rotated = rotation != clock->rotation;
  if (rotated) {
    display_off();
    // Erase every old segment at its old location before changing orientation.
    digits_draw(0, 0, true, clock->rotation, 0);
    clock->rotation = rotation;
  }
  datetime utc;
  unsigned hour = 0, minute = 0;
  bool valid = rtc_read(&utc);
  if (valid) {
    pacific_time(&utc, &hour, &minute);
    hour = hour % 12;
    if (!hour)
      hour = 12;
  }
  if (activation) {
    clock->fade_from = clock->visible && !rotated ? clock->level : 0;
    clock->level = clock->fade_from;
  }
  display_brightness(clock->level);
  digits_draw(hour, minute, valid, rotation, DIGITS_RED);
  display_on();
  clock->prepare_us = (uint32_t)((ticks() - start) / TICKS_PER_US);
  clock->visible = true;
  clock->refreshed_at = ticks();
  return valid;
}

bool clock_activate(clock_state *clock) {
  bool valid = prepare_frame(clock, true);
  clock->activations++;
  clock->shown_at = ticks();
  return valid;
}

void clock_hold(clock_state *clock, bool held) {
  if (clock->held && !held && clock->visible) {
    uint64_t fade_start = (DISPLAY_DURATION_MS - FADE_OUT_MS) * TICKS_PER_MS;
    // An expired hold gets only the fade-out, not a new display interval.
    if (ticks() - clock->shown_at > fade_start)
      clock->shown_at = ticks() - fade_start;
  }
  clock->held = held;
}

bool clock_update(clock_state *clock) {
  if (!clock->visible)
    return false;
  unsigned elapsed = (unsigned)((ticks() - clock->shown_at) / TICKS_PER_MS);
  // Refresh pixels without moving the original tap's animation deadline.
  if (clock->held && ticks() - clock->refreshed_at >= 1000 * TICKS_PER_MS)
    prepare_frame(clock, false);
  unsigned level = animation_level(
      clock->held && elapsed > FADE_IN_MS ? FADE_IN_MS : elapsed,
      clock->fade_from, clock->brightness);
  if (level != clock->level) {
    display_brightness(level);
    clock->level = level;
  }
  if (clock->held || elapsed < DISPLAY_DURATION_MS)
    return false;
  display_off();
  clock->visible = false;
  return true;
}

uint64_t clock_deadline(const clock_state *clock) {
  if (!clock->visible)
    return 0;
  unsigned elapsed = (unsigned)((ticks() - clock->shown_at) / TICKS_PER_MS);
  if (clock->held && elapsed >= FADE_IN_MS)
    return clock->refreshed_at + 1000 * TICKS_PER_MS;
  // Anchor each deadline to activation, so other work cannot extend the hold.
  return clock->shown_at +
         (uint64_t)(elapsed + animation_next_ms(elapsed)) * TICKS_PER_MS;
}
