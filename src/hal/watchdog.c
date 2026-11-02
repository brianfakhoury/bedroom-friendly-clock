#include "watchdog.h"
#include "registers.h"
void watchdogs_disable(void) {
  // ROM flash boot enables TIMG0 watchdog; RAM loading does not always do so.
  for (unsigned i = 0; i < 2; i++) {
    uint32_t base = TIMG0_BASE + i * 0x1000;
    REG(base + 0x64) = 0x50d83aa1;
    REG(base + 0x48) = 0;
    REG(base + 0x64) = 0;
  }
  REG(LP_WDT_PROTECT) = 0x50d83aa1;
  REG(LP_WDT_CONFIG) = 0;
  REG(LP_WDT_PROTECT) = 0;
  REG(LP_SWD_PROTECT) = 0x50d83aa1;
  REG(LP_SWD_CONFIG) |= BIT(30);
  REG(LP_SWD_PROTECT) = 0;
}

void watchdog_arm(void) {
  // LP watchdog uses the slow clock independently of CPU execution.
  // Nominal ~8 s at 150 kHz; RC tolerance makes this a recovery bound, not a
  // clock.
  unsigned shift =
      1 + ((REG(0x600b0834u) >> 16) & 3); // eFuse WDT_DELAY_SEL, read only.
  REG(LP_WDT_PROTECT) = 0x50d83aa1;
  REG(LP_WDT_CONFIG) = 0;
  REG(LP_WDT_CONFIG + 4) = 1200000u >> shift;
  REG(LP_WDT_CONFIG) =
      BIT(31) | (4u << 28) | (7u << 13) | (7u << 16) | BIT(8) | 20u;
  REG(LP_WDT_CONFIG + 0x14) = BIT(31);
  REG(LP_WDT_PROTECT) = 0;
}
void watchdog_disarm(void) {
  REG(LP_WDT_PROTECT) = 0x50d83aa1;
  REG(LP_WDT_CONFIG) = 0;
  REG(LP_WDT_PROTECT) = 0;
}
void watchdog_feed(void) {
  REG(LP_WDT_PROTECT) = 0x50d83aa1;
  REG(LP_WDT_CONFIG + 0x14) = BIT(31);
  REG(LP_WDT_PROTECT) = 0;
}
