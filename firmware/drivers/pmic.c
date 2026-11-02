#include "pmic.h"
// AXP2101 on the 2.16 board. ALDO3 is wired to LCD_RESET, not VCI.
bool pmic_panel_reset(void) {
  uint8_t v;
  if (!i2c_read(0x34, 0x94, &v, 1))
    return false;
  v = (v & 0xe0) | 28; // 500mV + 28*100mV = 3.3V.
  if (!i2c_write(0x34, 0x94, &v, 1) || !i2c_read(0x34, 0x90, &v, 1))
    return false;
  v |= 4;
  if (!i2c_write(0x34, 0x90, &v, 1))
    return false;
  delay_us(100000);
  v &= ~4;
  if (!i2c_write(0x34, 0x90, &v, 1))
    return false;
  delay_us(100000);
  v |= 4;
  if (!i2c_write(0x34, 0x90, &v, 1))
    return false;
  delay_us(100000);
  return true;
}
bool pmic_led_off(void) {
  uint8_t v, actual;
  // Manual charging-indicator control: off. Charging settings are untouched.
  if (!i2c_read(0x34, 0x69, &v, 1))
    return false;
  v = (v & 0xc8) | 0x05;
  return i2c_write(0x34, 0x69, &v, 1) &&
         i2c_read(0x34, 0x69, &actual, 1) && (actual & 0x37) == 0x05;
}

bool pmic_unused_rails_off(void) {
  // Schematic: DC1 supplies the board; ALDO1 audio, ALDO2 amplifier enable,
  // ALDO3 panel reset. Other adjustable outputs have no required load.
  // Preserve RTC backup, charging, voltage settings, and DC1 mode bits.
  const uint8_t regs[] = {0x80, 0x90, 0x91};
  const uint8_t masks[] = {0x1e, 0xfa, 0x01};
  // Disabled outputs discharge internally, holding the amplifier CTRL low.
  // This does not enable battery discharge; it discharges switched-off outputs.
  uint8_t common;
  bool good = i2c_read(0x34, 0x10, &common, 1);
  if (good) {
    common |= 0x20;
    good = i2c_write(0x34, 0x10, &common, 1);
  }
  for (unsigned i = 0; i < sizeof(regs); i++) {
    uint8_t value;
    if (!i2c_read(0x34, regs[i], &value, 1)) {
      good = false;
      continue;
    }
    value &= ~masks[i];
    if (!i2c_write(0x34, regs[i], &value, 1))
      good = false;
  }
  return good;
}
