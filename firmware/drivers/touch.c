#include "touch.h"
#include "board.h"
// CST9217, 16-bit register addresses. Report layout follows Waveshare's driver.
bool touch_read(bool *down) {
  uint8_t data[10];
  *down = false;
  if (!i2c_read16(0x5a, 0xd000, data, sizeof(data)) || data[6] != 0xab)
    return false;
  *down = (data[5] & 0x7f) != 0 && (data[0] & 15) == 6;
  return true;
}
bool touch_init(void) {
  gpio_init(TOUCH_IRQ, false, true);
  gpio_init(TOUCH_RESET, true, false);
  gpio_set(TOUCH_RESET, false);
  delay_us(200000);
  gpio_set(TOUCH_RESET, true);
  delay_us(200000);
  uint8_t b[10];
  return i2c_read16(0x5a, 0xd000, b, sizeof(b));
}
