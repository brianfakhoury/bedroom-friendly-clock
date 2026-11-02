#include "board.h"
#include "hal.h"
// Open-drain software I2C, including bounded clock stretching. Only used on
// demand.
static void sda(bool high) {
  REG(high ? GPIO_ENABLE_CLEAR : GPIO_ENABLE_SET) = BIT(I2C_SDA);
}
static bool scl(bool high) {
  REG(high ? GPIO_ENABLE_CLEAR : GPIO_ENABLE_SET) = BIT(I2C_SCL);
  for (unsigned n = 0; high && !gpio_get(I2C_SCL); n++) {
    if (n == 1000)
      return false;
    delay_us(1);
  }
  delay_us(3);
  return true;
}
static bool start(void) {
  sda(true);
  if (!scl(true))
    return false;
  sda(false);
  delay_us(3);
  scl(false);
  return true;
}
static void stop(void) {
  sda(false);
  scl(true);
  sda(true);
  delay_us(3);
}
static bool put(uint8_t b) {
  for (unsigned i = 0; i < 8; i++) {
    sda(b & 128);
    if (!scl(true))
      return false;
    scl(false);
    b <<= 1;
  }
  sda(true);
  if (!scl(true))
    return false;
  bool ack = !gpio_get(8);
  scl(false);
  return ack;
}
static bool get(uint8_t *b, bool last) {
  *b = 0;
  sda(true);
  for (unsigned i = 0; i < 8; i++) {
    if (!scl(true))
      return false;
    *b = (*b << 1) | gpio_get(8);
    scl(false);
  }
  sda(last);
  if (!scl(true))
    return false;
  scl(false);
  sda(true);
  return true;
}
void i2c_init(void) {
  gpio_init(I2C_SCL, false, true);
  gpio_init(I2C_SDA, false, true);
  gpio_set(I2C_SCL, false);
  gpio_set(I2C_SDA, false);
  for (unsigned i = 0; i < 9; i++) {
    scl(false);
    scl(true);
  }
  stop();
}
bool i2c_read(uint8_t a, uint8_t r, uint8_t *b, unsigned n) {
  bool ok = start() && put(a << 1) && put(r) && start() && put((a << 1) | 1);
  for (unsigned i = 0; ok && i < n; i++)
    ok = get(b + i, i == n - 1);
  stop();
  return ok;
}
bool i2c_write(uint8_t a, uint8_t r, const uint8_t *b, unsigned n) {
  bool ok = start() && put(a << 1) && put(r);
  for (unsigned i = 0; ok && i < n; i++)
    ok = put(b[i]);
  stop();
  return ok;
}
bool i2c_read16(uint8_t a, uint16_t r, uint8_t *b, unsigned n) {
  bool ok = start() && put(a << 1) && put(r >> 8) && put(r & 255) && start() &&
            put((a << 1) | 1);
  for (unsigned i = 0; ok && i < n; i++)
    ok = get(b + i, i == n - 1);
  stop();
  return ok;
}
