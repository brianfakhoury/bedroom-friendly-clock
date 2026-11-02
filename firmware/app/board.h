#pragma once
// Waveshare ESP32-C6-AMOLED-2.16 only. CS/IRQ differ from some vendor examples.
enum {
  PANEL_CLOCK = 0,
  PANEL_DATA0 = 1,
  PANEL_DATA_LAST = 4,
  PANEL_CS = 15,
  TOUCH_IRQ = 5,
  SD_CS = 6,
  I2C_SCL = 7,
  I2C_SDA = 8,
  BUTTON_BOOT = 9,
  BUTTON_KEY = 10,
  TOUCH_RESET = 11,
};
#define BUTTON_MASK ((1u << BUTTON_BOOT) | (1u << BUTTON_KEY))
void board_init(void);
void board_resume(void);
