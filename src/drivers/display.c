#include "display.h"
#include "board.h"
#include "pmic.h"
enum {
  DISPLAY_OFF = 0x28,
  DISPLAY_ON = 0x29,
  SLEEP_OUT = 0x11,
  COLUMN_ADDRESS = 0x2a,
  ROW_ADDRESS = 0x2b,
  MEMORY_WRITE = 0x2c,
  MEMORY_CONTINUE = 0x3c,
  BRIGHTNESS = 0x51,
  SPI_COMMAND = 0x02,
  QSPI_PIXELS = 0x32,
  // Fast pixel writes rely on this board's contiguous clock/data GPIOs.
  BUS_MASK = 0x1f,
  CLOCK_MASK = 0x01,
  DATA0_MASK = 0x02,
};
_Static_assert(PANEL_CLOCK == 0 && PANEL_DATA0 == 1 && PANEL_DATA_LAST == 4,
               "QSPI fast path requires GPIO0 through GPIO4");
static void byte(uint8_t v) {
  for (unsigned i = 0; i < 8; i++) {
    REG(GPIO_OUT_CLEAR) = CLOCK_MASK | DATA0_MASK;
    if (v & 128)
      REG(GPIO_OUT_SET) = DATA0_MASK;
    delay_us(1);
    REG(GPIO_OUT_SET) = CLOCK_MASK;
    delay_us(1);
    v <<= CLOCK_MASK;
  }
  REG(GPIO_OUT_CLEAR) = CLOCK_MASK;
}
static void command(uint8_t c, const uint8_t *d, unsigned n) {
  gpio_set(PANEL_CS, false);
  byte(SPI_COMMAND);
  byte(0);
  byte(c);
  byte(0);
  for (unsigned i = 0; i < n; i++)
    byte(d[i]);
  gpio_set(PANEL_CS, true);
}
static void cmd(uint8_t c) { command(c, 0, 0); }
static void reg(uint8_t c, uint8_t v) { command(c, &v, 1); }
void display_rect(unsigned x, unsigned y, unsigned w, unsigned h,
                  uint16_t color) {
  uint8_t a[] = {x >> 8, x, (x + w - 1) >> 8, x + w - 1};
  command(COLUMN_ADDRESS, a, 4);
  uint8_t b[] = {y >> 8, y, (y + h - 1) >> 8, y + h - 1};
  command(ROW_ADDRESS, b, 4);
  cmd(MEMORY_WRITE);
  gpio_set(PANEL_CS, false);
  byte(QSPI_PIXELS);
  byte(0);
  byte(MEMORY_CONTINUE);
  byte(0);
  for (unsigned i = 0; i < w * h; i++)
    for (int n = 12; n >= 0; n -= 4) {
      REG(GPIO_OUT_CLEAR) = BUS_MASK;
      REG(GPIO_OUT_SET) = ((color >> n) & 15) << 1;
      REG(GPIO_OUT_SET) = CLOCK_MASK;
    }
  REG(GPIO_OUT_CLEAR) = CLOCK_MASK;
  gpio_set(PANEL_CS, true);
}
static void configure(void) {
  reg(0xfe, 0x20);
  reg(0x19, 0x10);
  reg(0x1c, 0xa0);
  reg(0xfe, 0);
  reg(0xc4, 0x80);
  reg(0x3a, 0x55);
  reg(0x36, 0x30);
  reg(0x53, 0x20);
  delay_us(10000);
  reg(0x63, 0xff);
  delay_us(10000);
  reg(BRIGHTNESS, 0);
  reg(0x58, 0);
}
__attribute__((section(".init.board"), noinline))
bool display_init(void) {
  for (unsigned p = 0; p < 5; p++) {
    gpio_init(p, true, false);
    gpio_set(p, false);
  }
  gpio_init(PANEL_CS, true, false);
  gpio_set(PANEL_CS, true);
  if (!pmic_panel_reset()) {
    return false;
  }
  cmd(DISPLAY_OFF);
  cmd(SLEEP_OUT);
  delay_us(600000);
  configure();
  display_rect(0, 0, 480, 480, 0);
  display_off();
  return true;
}
void display_on(void) { cmd(DISPLAY_ON); }
void display_off(void) { cmd(DISPLAY_OFF); }
void display_brightness(unsigned value) { reg(BRIGHTNESS, (uint8_t)value); }
