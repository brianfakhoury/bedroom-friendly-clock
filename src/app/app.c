#include "app.h"
#include "board.h"
#include "clock.h"
#include "hal.h"
#include "serial_commands.h"
#include "standby.h"
#include "touch.h"
#include "watchdog.h"

#define DEFAULT_BRIGHTNESS    128u
#define BUTTON_DEBOUNCE_MS    200u

static clock_state clock = {.brightness = DEFAULT_BRIGHTNESS};

static void activate(void) {
  bool valid = clock_activate(&clock);
  // USB can back-pressure while reconnecting after sleep. Keep it off the
  // normal display path; latched operation permits host diagnostics.
  if (clock.held)
    usb_puts(valid ? "ON\n" : "ON RTC_INVALID\n");
}

// Keep USB work outside the normal wake-to-display path.
static void poll_usb(void) {
  if (serial_commands_poll(&clock))
    activate();
  if (serial_commands_take_sleep_request())
    clock_hold(&clock, false);
}

static void handle_input(unsigned events, bool *touch_down,
                         uint64_t *last_button) {
  bool down;
  if ((events & EVENT_TOUCH) && touch_read(&down)) {
    if (down && (!*touch_down || !clock.visible))
      activate();
    *touch_down = down;
  }

  if (!(events & EVENT_BUTTON))
    return;
  if (ticks() - *last_button <= BUTTON_DEBOUNCE_MS * TICKS_PER_MS)
    return;

  *last_button = ticks();
  if (events & EVENT_KEY)
    clock_hold(&clock, !clock.held);
  else
    activate();
}

static _Noreturn void run(void) {
  // The contact that activated the display is already consumed.
  bool touch_down = clock.visible;
  uint64_t last_button = 0;
  for (;;) {
    watchdog_feed();
    poll_usb();
    clock_update(&clock);
    if (!clock.visible) {
      clock_hold(&clock, false);
      // Successful sleep restarts at wake_main. A return means a touch
      // arrived during the pin handoff and must be handled here instead.
      standby_enter();
      activate();
      touch_down = true;
      continue;
    }
    unsigned events = wait_until(clock_deadline(&clock));
    handle_input(events, &touch_down, &last_button);
  }
}

// Cold startup reads KEY once; its release must not toggle the hold.
_Noreturn void app_start(void) {
  clock.held = !gpio_get(BUTTON_KEY);
  if (clock.held)
    activate();
  usb_puts("READY BEDSIDE CLOCK 0.11.1\n");
  run();
}

_Noreturn void app_resume(void) {
  activate();
  run();
}
