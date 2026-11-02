#include "local_time.h"
void pacific_time(const datetime *d, unsigned *h, unsigned *m) {
  // America/Los_Angeles, current US rules (since 2007). RTC always holds UTC.
  unsigned march = 8 + (7 - date_weekday(d->year, 3, 8)) % 7;
  unsigned november = 1 + (7 - date_weekday(d->year, 11, 1)) % 7;
  bool dst = (d->month > 3 && d->month < 11) ||
             (d->month == 3 &&
              (d->day > march || (d->day == march && d->hour >= 10))) ||
             (d->month == 11 &&
              (d->day < november || (d->day == november && d->hour < 9)));
  *h = (d->hour + 24 - (dst ? 7 : 8)) % 24;
  *m = d->minute;
}
void date_format(const datetime *d, char b[21]) {
  const unsigned v[] = {d->year / 100, d->year % 100, d->month, d->day,
                        d->hour,       d->minute,     d->second};
  const unsigned pos[] = {0, 2, 5, 8, 11, 14, 17};
  for (unsigned i = 0; i < 7; i++) {
    b[pos[i]] = '0' + v[i] / 10;
    b[pos[i] + 1] = '0' + v[i] % 10;
  }
  b[4] = '-';
  b[7] = '-';
  b[10] = 'T';
  b[13] = ':';
  b[16] = ':';
  b[19] = 'Z';
  b[20] = 0;
}
