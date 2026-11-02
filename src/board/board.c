#include "board.h"
#include "accel.h"
#include "audio.h"
#include "display.h"
#include "hal.h"
#include "pins.h"
#include "pmic.h"
#include "power.h"
#include "registers.h"
#include "standby.h"
#include "touch.h"
#include "watchdog.h"

__attribute__((section(".init.board"), noinline))
void board_init(void) {
  standby_init();
  watchdog_arm();
  radios_disable();
  unused_peripheral_clocks_disable();
  i2c_init();
  gpio_init(BUTTON_BOOT, false, true);
  gpio_init(BUTTON_KEY, false, true);
  // Deselect the card before enabling the output, avoiding a low pulse.
  gpio_set(SD_CS, true);
  gpio_init(SD_CS, true, true);
  if (!pmic_led_off())
    usb_puts("ERR LED OFF\n");
  if (!pmic_unused_rails_off())
    usb_puts("ERR RAILS\n");
  if (!audio_disable())
    usb_puts("ERR AUDIO OFF\n");
  if (!display_init())
    usb_puts("ERR PANEL INIT\n");
  if (!touch_init())
    usb_puts("ERR TOUCH INIT\n");
  if (!accel_init())
    usb_puts("ERR ACCEL INIT\n");
  events_init();
}

void board_resume(void) {
  // TOP-off wake resets GPIO routing/enables.
  // Restore buses and safe output levels without resetting external devices.
  const unsigned outputs[] = {
      0,     1,           2,        3,
      4,                                // Panel clock and four data lines: low.
      SD_CS, TOUCH_RESET, PANEL_CS,     // Deselect devices; keep touch out of
                                        // reset.
      19,    20,          22,       23, // Audio clock/data: low.
  };
  for (unsigned i = 0; i < sizeof(outputs) / sizeof(outputs[0]); i++) {
    unsigned p = outputs[i];
    gpio_set(p, p == SD_CS || p == TOUCH_RESET || p == PANEL_CS);
    gpio_init(p, true, p == SD_CS);
  }
  gpio_init(BUTTON_BOOT, false, true);
  gpio_init(BUTTON_KEY, false, true);
  gpio_init(TOUCH_IRQ, false, true);
  // Restore output routing before releasing the retained pad levels.
  REG(0x600b102cu) = 0; // Release this board's per-pin LP_AON holds.
  i2c_init();
}

bool board_key_pressed(void) { return !gpio_get(BUTTON_KEY); }
