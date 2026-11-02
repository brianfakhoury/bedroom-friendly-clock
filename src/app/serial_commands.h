#pragma once
#include "clock.h"
// Process bounded host input; return true for an activation request.
bool serial_commands_poll(clock_state *clock);

bool serial_commands_take_sleep_request(void);
