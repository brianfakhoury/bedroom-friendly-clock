#include "rtc.h"
#include "hal.h"
static unsigned mdays(unsigned y, unsigned m) {
  static const uint8_t days[] = {31, 28, 31, 30, 31, 30,
                                 31, 31, 30, 31, 30, 31};
  return days[m - 1] + (m == 2 && y % 4 == 0);
}
bool date_valid(const datetime *d) {
  return d->year >= 2020 && d->year <= 2099 && d->month >= 1 &&
         d->month <= 12 && d->day >= 1 && d->day <= mdays(d->year, d->month) &&
         d->hour < 24 && d->minute < 60 && d->second < 60;
}
static uint8_t bcd(unsigned v) { return (v / 10) * 16 + v % 10; }
static unsigned dec(uint8_t v) { return (v >> 4) * 10 + (v & 15); }
static bool bcd_ok(uint8_t v) { return (v & 15) < 10 && (v >> 4) < 10; }
bool rtc_read(datetime *d) {
  uint8_t ctrl, b[7];
  if (!i2c_read(0x51, 0, &ctrl, 1) || (ctrl & 0x22) ||
      !i2c_read(0x51, 4, b, 7) || (b[0] & 128))
    return false;
  b[0] &= 127;
  b[1] &= 127;
  b[2] &= 63;
  b[3] &= 63;
  b[5] &= 31;
  for (unsigned i = 0; i < 7; i++)
    if (i != 4 && !bcd_ok(b[i]))
      return false;
  *d = (datetime){2000 + dec(b[6]), dec(b[5]), dec(b[3]),
                  dec(b[2]),        dec(b[1]), dec(b[0])};
  return date_valid(d);
}
unsigned date_weekday(unsigned y, unsigned m, unsigned d) {
  // Gregorian calendar, Sunday=0; valid for the supported 2020–2099 interval.
  static const unsigned t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  if (m < 3)
    y--;
  return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
}
bool rtc_set(const datetime *d) {
  if (!date_valid(d))
    return false;
  uint8_t ctrl;
  if (!i2c_read(0x51, 0, &ctrl, 1))
    return false;
  ctrl = (ctrl & ~0x22) | 0x20;
  if (!i2c_write(0x51, 0, &ctrl, 1))
    return false;
  uint8_t b[] = {bcd(d->second),
                 bcd(d->minute),
                 bcd(d->hour),
                 bcd(d->day),
                 date_weekday(d->year, d->month, d->day),
                 bcd(d->month),
                 bcd(d->year - 2000)};
  bool ok = i2c_write(0x51, 4, b, 7);
  ctrl &= ~0x20;
  bool running = i2c_write(0x51, 0, &ctrl, 1);
  return ok && running;
}
