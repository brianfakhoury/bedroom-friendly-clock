#include "clock.h"
#include "hal.h"
#include "rtc.h"
#include <assert.h>
#include <stdio.h>
static uint64_t now;
static bool valid_rtc = true, valid_accel = true;
static int16_t acceleration[3] = {0, 16384, 0};
static unsigned panel_level, on_count, off_count, erase_count;
static bool drawn_valid;
static unsigned drawn_rotation, drawn_hour, local_hour = 8;
uint64_t ticks(void) { return now; }
bool accel_read(int16_t xyz[3]) {
  for (unsigned i = 0; i < 3; i++)
    xyz[i] = acceleration[i];
  return valid_accel;
}
bool rtc_read(datetime *d) {
  *d = (datetime){2026, 11, 1, 16, 0, 0};
  return valid_rtc;
}
void pacific_time(const datetime *d, unsigned *h, unsigned *m) {
  (void)d;
  *h = local_hour;
  *m = 0;
}
void display_on(void) { on_count++; }
void display_off(void) { off_count++; }
void display_brightness(unsigned value) { panel_level = value; }
void digits_draw(unsigned h, unsigned m, bool valid, unsigned rotation,
                 uint16_t ink) {
  (void)h;
  (void)m;
  if (!ink)
    erase_count++;
  else {
    drawn_hour = h;
    drawn_valid = valid;
    drawn_rotation = rotation;
  }
}
static void at(unsigned ms) { now = (uint64_t)ms * TICKS_PER_MS; }
int main(void) {
  clock_state c = {.brightness = 255};
  assert(clock_deadline(&c) == 0 && !clock_update(&c));
  assert(clock_activate(&c));
  assert(c.visible && on_count == 1 && panel_level == 0 && drawn_valid);
  at(180);
  assert(!clock_update(&c) && panel_level == 255);
  assert(clock_deadline(&c) == 2650 * TICKS_PER_MS);
  at(1000);
  assert(clock_deadline(&c) == 2650 * TICKS_PER_MS);
  at(2800);
  assert(!clock_update(&c) && panel_level > 0 && panel_level < 255);
  unsigned level = panel_level;
  assert(clock_activate(&c) && panel_level == level); // Retrigger without jump.
  at(2980);
  assert(!clock_update(&c) && panel_level == 255);
  at(5800);
  assert(clock_update(&c) && !c.visible && panel_level == 0);
  assert(off_count == 1 && clock_deadline(&c) == 0);
  assert(!clock_update(&c) && off_count == 1);
  acceleration[0] = 16384;
  acceleration[1] = 0;
  assert(clock_activate(&c) && drawn_rotation == 3 && erase_count == 1);
  assert(panel_level == 0);
  valid_accel = false;
  valid_rtc = false;
  assert(!clock_activate(&c));
  assert(!drawn_valid && drawn_rotation == 3 && erase_count == 1);
  valid_rtc = true;
  const unsigned hours[] = {0, 1, 11, 12, 13, 23};
  const unsigned expected[] = {12, 1, 11, 12, 1, 11};
  for (unsigned i = 0; i < sizeof(hours) / sizeof(hours[0]); i++) {
    local_hour = hours[i];
    assert(clock_activate(&c) && drawn_hour == expected[i]);
  }
  c.held = true;
  at(10000);
  clock_activate(&c);
  at(10180);
  assert(!clock_update(&c) && panel_level == 255);
  assert(clock_deadline(&c) == 11000 * TICKS_PER_MS);
  local_hour = 4;
  at(14000);
  assert(!clock_update(&c) && c.visible && drawn_hour == 4);
  assert(panel_level == 255);
  c.held = false;
  clock_activate(&c);
  at(17000);
  assert(clock_update(&c) && !c.visible);
  puts("PASS: lifecycle, absolute hold deadline, retrigger, rotation cleanup, "
       "sensor failures, 12-hour display boundaries");
}
