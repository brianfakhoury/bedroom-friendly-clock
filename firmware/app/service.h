#pragma once
#include "clock.h"
// Process bounded host input; return true for an activation request.
bool service_poll(clock_state *clock);

bool service_take_sleep_request(void);
