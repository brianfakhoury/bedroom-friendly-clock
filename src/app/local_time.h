#pragma once
#include "rtc.h"
void pacific_time(const datetime *utc, unsigned *hour, unsigned *minute);
void date_format(const datetime *d, char out[21]);
