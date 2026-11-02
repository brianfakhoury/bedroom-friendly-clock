#include "board.h"
#include "clock.h"
#include "hal.h"
#include "service.h"
#include "standby.h"
#include "touch.h"
#include "watchdog.h"

static clock_state clock = {.brightness = 128};
static bool maintenance;

static void activate(void) {
  bool valid = clock_activate(&clock);
  // USB can back-pressure while reconnecting after sleep. Keep it off the
  // normal display path; maintenance retains the host test protocol.
  if (maintenance)
    usb_puts(valid ? "ON\n" : "ON RTC_INVALID\n");
}

// Keep USB work outside the normal wake-to-display path.
static void poll_maintenance(void) {
  if (!gpio_get(BUTTON_BOOT) && !gpio_get(BUTTON_KEY))
    maintenance = true;
  if (service_poll(&clock))
    activate();
  if (service_take_sleep_request()) {
    maintenance = false;
    delay_us(20000); // Allow the final USB reply to leave before shutdown.
  }
}

static void handle_input(unsigned events, bool *touch_down,
                         uint64_t *last_button) {
  if (events & EVENT_TOUCH) {
    bool down;
    if (touch_read(&down)) {
      if (down && (!*touch_down || !clock.visible))
        activate();
      *touch_down = down;
    }
  }
  if ((events & EVENT_BUTTON) && ticks() - *last_button > 200 * TICKS_PER_MS) {
    *last_button = ticks();
    activate();
  }
}

static void run(void) {
  // A wake contact has already activated the display. Consume it once.
  bool touch_down = clock.visible;
  uint64_t last_button = 0;
  for (;;) {
    watchdog_feed();
    poll_maintenance();
    if (clock_update(&clock) && maintenance)
      usb_puts("OFF\n");
    if (!clock.visible && !maintenance) {
      // Successful sleep restarts at wake_main. A return means a touch
      // arrived during the pin handoff and must be handled here instead.
      standby_enter();
      activate();
      touch_down = true;
      continue;
    }
    handle_input(wait_until(clock_deadline(&clock)), &touch_down, &last_button);
  }
}

// Called by the ROM wake stub, not by the interrupted standby call.
__attribute__((used, externally_visible)) void wake_main(void) {
  standby_resume();
  board_resume();
  events_init();
  activate();
  run();
}

int main(void) {
  board_init();
  maintenance = !gpio_get(BUTTON_KEY);
  usb_puts("READY BEDSIDE CLOCK 0.10.2\n");
  run();
}
