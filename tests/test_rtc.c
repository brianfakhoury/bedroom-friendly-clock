#include "rtc.h"
#include "hal.h"
#include "local_time.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint8_t regs[32];
bool i2c_read(uint8_t a, uint8_t r, uint8_t *b, unsigned n) {
  assert(a == 0x51);
  memcpy(b, regs + r, n);
  return true;
}
bool i2c_write(uint8_t a, uint8_t r, const uint8_t *b, unsigned n) {
  assert(a == 0x51);
  memcpy(regs + r, b, n);
  return true;
}
static void local(unsigned mon, unsigned day, unsigned hour, unsigned min,
                  unsigned expect) {
  datetime d = {2026, mon, day, hour, min, 0};
  unsigned h, m;
  pacific_time(&d, &h, &m);
  assert(h == expect && m == min);
}
int main(void) {
  datetime d = {2026, 11, 1, 16, 0, 0}, got;
  assert(rtc_set(&d) && rtc_read(&got));
  assert(got.year == 2026 && got.month == 11 && got.day == 1 && got.hour == 16);
  regs[4] |= 128;
  assert(!rtc_read(&got));
  regs[4] &= 127;
  regs[0] |= 32;
  assert(!rtc_read(&got));
  regs[0] &= ~32;
  regs[5] = 0x6a;
  assert(!rtc_read(&got));
  d = (datetime){2026, 2, 29, 0, 0, 0};
  assert(!date_valid(&d));
  d.year = 2028;
  assert(date_valid(&d));
  d.second = 60;
  assert(!date_valid(&d));
  d.second = 0;
  d.month = 0;
  assert(!date_valid(&d));
  local(3, 8, 9, 59, 1);
  local(3, 8, 10, 0, 3);
  local(11, 1, 8, 59, 1);
  local(11, 1, 9, 0, 1);
  local(11, 1, 10, 0, 2);
  local(1, 1, 0, 0, 16);
  local(7, 1, 0, 0, 17);
  char text[21];
  d = (datetime){2026, 11, 1, 16, 5, 9};
  date_format(&d, text);
  assert(strcmp(text, "2026-11-01T16:05:09Z") == 0);
  puts("PASS: RTC BCD, oscillator-stop rejection, calendar validation, Pacific "
       "DST boundaries, UTC formatting");
}
