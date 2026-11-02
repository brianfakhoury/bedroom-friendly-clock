#include "standby.h"
#include "board.h"
#include "hal.h"
#include "power.h"
#include "standby_regs.h"
#include "watchdog.h"
extern void wake_entry(void);
extern char _crc_end[];

// ESP32-C6 ROM ABI, archived in references/esp-idf/.../esp32c6.rom.ld.
#define RETAINED_RAM_BASE 0x50000000u
static void (*const rom_set_wake_entry)(void (*)(void), unsigned) =
    (void (*)(void (*)(void), unsigned))0x400000a4;
static unsigned (*const rom_get_wake_entry)(void) =
    (unsigned (*)(void))0x400000a8;

uint32_t standby_cause(void) { return REG(PMU_SLP_WAKEUP_STATUS0_REG); }

void standby_resume(void) {
  REG(LP_TIMER_TARGET_HIGH) = 0;
  REG(LP_TIMER_CLEAR) = BIT(31);
  REG(LP_IO_MUX) &= ~BIT(TOUCH_IRQ);
  REG(LP_IO_PIN5) = BIT(3);
  REG(LP_IO_CLEAR) = BIT(TOUCH_IRQ);
  REG(RTC_SLEEP_MODE) &= ~1u;
  watchdogs_disable();
  timer_init();
  // TOP shutdown resets the USB peripheral clock and pad enable.
  REG(0x6009608cu) = 1;
  REG(0x6000f018u) = (REG(0x6000f018u) | BIT(14)) & ~BIT(8);
  watchdog_arm();
  // ROM wake enables UART0 clocks. Gate unused peripherals again.
  // Radio force-off survives sleep; do not reconfigure its powered-down bus.
  unused_peripheral_clocks_disable();
  gpio_init(TOUCH_IRQ, false, true);
}

static void arm_touch_wake(void) {
  REG(LP_TIMER_TARGET_HIGH) = 0;
  REG(LP_TIMER_CLEAR) = BIT(31);
  REG(LPPERI_CLK_EN_REG) |= BIT(27);
  REG(LP_IO_DATE_REG) |= BIT(31);
  REG(LP_IO_OUT_ENABLE_W1TC_REG) = BIT(TOUCH_IRQ);
  REG(LP_IO_PAD5) = BIT(9) | BIT(8) | BIT(4) | BIT(3);
  REG(LP_IO_CLEAR) = BIT(TOUCH_IRQ);
  REG(LP_IO_PIN5) = BIT(10) | (2u << 7) | BIT(3);
  REG(LP_IO_MUX) |= BIT(TOUCH_IRQ);
}

void standby_enter(void) {
  arm_touch_wake();

  // Preserve a pending touch across the HP-to-LP pin handoff.
  if ((REG(GPIO_STATUS) | REG(LP_IO_STATUS)) & BIT(TOUCH_IRQ)) {
    REG(GPIO_STATUS_CLEAR) = BIT(TOUCH_IRQ);
    standby_resume();
    return;
  }
  REG(RTC_SLEEP_MODE) |= 1u;
  REG(LP_AON_SYS_CFG_REG) &= ~BIT(30);
  rom_set_wake_entry(wake_entry, (unsigned)_crc_end - RETAINED_RAM_BASE);
  if (rom_get_wake_entry() != (unsigned)wake_entry)
    for (;;) {
    } // Active watchdog resets invalid retained image.
  REG(PMU_SLP_WAKEUP_CNTL1_REG) = 0;
  REG(PMU_SLP_WAKEUP_CNTL2_REG) = BIT(2);
  REG(PMU_HP_INT_CLR_REG) = BIT(31) | BIT(30);
  REG(PMU_SLP_WAKEUP_CNTL4_REG) = BIT(31);
  REG(0x6000f018u) &= ~BIT(14);
  delay_us(100000);
  // Hold only the board outputs. Global holds also capture USB pads and
  // failed repeated reconnects; touch and I2C remain inputs during standby.
  REG(0x600b102cu) = 0x1fu | BIT(SD_CS) | BIT(TOUCH_RESET) | BIT(PANEL_CS) |
                     BIT(19) | BIT(20) | BIT(22) | BIT(23);
  watchdog_feed();
  __asm__ volatile("fence iorw, iorw" ::: "memory");
  REG(PMU_SLP_WAKEUP_CNTL0_REG) = BIT(31);
  for (;;) {
  } // Wake restarts HP through wake_entry, never returns here.
}
