#include "hal.h"
void gpio_init(unsigned p, bool output, bool pullup) {
  REG(IOMUX_GPIO0 + p * 4) =
      BIT(12) | BIT(9) | (pullup ? BIT(8) : 0) | (2u << 10);
  REG(GPIO_OUT_SELECT0 + p * 4) = 128 | BIT(9);
  REG(output ? GPIO_ENABLE_SET : GPIO_ENABLE_CLEAR) = BIT(p);
}
void gpio_set(unsigned p, bool high) {
  REG(high ? GPIO_OUT_SET : GPIO_OUT_CLEAR) = BIT(p);
}
bool gpio_get(unsigned p) { return (REG(GPIO_INPUT) & BIT(p)) != 0; }
