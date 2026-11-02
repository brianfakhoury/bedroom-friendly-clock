#pragma once
#include "hal.h"
typedef struct {
  unsigned year, month, day, hour, minute, second;
} datetime;
bool date_valid(const datetime *d);
bool rtc_read(datetime *d);
bool rtc_set(const datetime *d);
unsigned date_weekday(unsigned year, unsigned month, unsigned day);
