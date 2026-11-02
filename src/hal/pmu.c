/* SPDX-FileCopyrightText: 2019-2025 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 * Adapted from Espressif ESP-IDF v5.4.2 pmu_init/param/sleep and analog-I2C
 * register routines (copyright 2019-2025 Espressif Systems).
 * One cold-boot policy: retained LP SRAM, HP TOP/CPU/AON off, LP GPIO wake.
 * No SDK runtime, radio stack, heap, LP program, or retention DMA.
 */
#include "hal.h"
#include "pmu_registers.h"
#define COLD __attribute__((section(".init.pmu"), noinline))
#define PMU_HW (*(volatile pmu_dev_t *)0x600b0000u)
enum { ACTIVE, MODEM, SLEEP };

// Internal analog bus, unrelated to the board's external software I2C.
static COLD void regulator_bit(unsigned reg, unsigned bit, bool set) {
  REG(0x600af018u) |= BIT(2); // Analog I2C master clock.
  REG(0x600af81cu) = 0x00fffbffu; // Select DIG_REG read path.
  uint32_t ctrl = 0x600af800u + ((REG(0x600af820u) & BIT(12)) ? 0 : 4);
  while (REG(ctrl) & BIT(25)) {}
  REG(ctrl) = 0x6d | (reg << 8);
  while (REG(ctrl) & BIT(25)) {}
  unsigned value = (REG(ctrl) >> 16) & 255;
  value = set ? value | BIT(bit) : value & ~BIT(bit);
  REG(ctrl) = 0x6d | (reg << 8) | (value << 16) | BIT(24);
  while (REG(ctrl) & BIT(25)) {}
}

static COLD unsigned active_bias(unsigned fuse, unsigned fallback) {
  return fuse ? (fuse > 29 ? 31 : fuse + 2) : fallback;
}

static COLD void hp_states(unsigned bias) {
  // Retention backup entries are configured but disabled, matching the
  // manufacturer's initialization. No SRAM save/restore engine is started.
  const uint32_t backup[] = {0x010100a0, 0x00100010, 0x12800200};
  for (unsigned mode = ACTIVE; mode <= SLEEP; ++mode) {
    volatile pmu_hp_hw_regmap_t *hp = &PMU_HW.hp_sys[mode];
    bool sleep = mode == SLEEP;
    hp->dig_power.val = sleep ? 0xe8200000u : (mode == MODEM ? BIT(29) : 0);
    hp->icg_func = mode == ACTIVE ? ~0u : (mode == MODEM ? BIT(20) : 0);
    hp->icg_apb = mode == ACTIVE ? ~0u : (mode == MODEM ? BIT(9) : 0);
    hp->icg_modem.code = mode == ACTIVE ? 2 : (mode == MODEM ? 1 : 0);
    hp->clk_power.val = sleep ? BIT(26) | BIT(27) : BIT(28) | BIT(29) | BIT(30);
    hp->xtal.xpd_xtal = !sleep;
    hp->syscntl.uart_wakeup_en = mode != ACTIVE;
    hp->syscntl.lp_pad_hold_all = 0;
    hp->syscntl.hp_pad_hold_all = 0;
    hp->syscntl.dig_pad_slp_sel = sleep;
    hp->syscntl.dig_pause_wdt = mode != ACTIVE;
    hp->syscntl.dig_cpu_stall = mode != ACTIVE;
    hp->sysclk.dig_sysclk_nodiv = 0;
    hp->sysclk.icg_sysclk_en = !sleep;
    hp->sysclk.sysclk_slp_sel = mode != ACTIVE;
    hp->sysclk.icg_slp_sel = mode != ACTIVE;
    hp->sysclk.dig_sysclk_sel = mode == MODEM ? 1 : 0;
    hp->bias.xpd_bias = mode == ACTIVE;
    hp->bias.dbg_atten = 0;
    hp->bias.pd_cur = sleep;
    hp->bias.bias_sleep = sleep;
    hp->regulator0.slp_mem_xpd = 0;
    hp->regulator0.slp_logic_xpd = 0;
    hp->regulator0.xpd = !sleep;
    hp->regulator0.slp_mem_dbias = 0;
    hp->regulator0.slp_logic_dbias = 0;
    hp->regulator0.dbias = sleep ? 0 : bias;
    hp->regulator1.drv_b = 0;
    hp->backup.val = backup[mode];
    hp->backup_clk = ~0u;
    PMU_HW.imm.modem_icg.update_dig_icg_modem_en = 1;
    PMU_HW.imm.sleep_sysclk.update_dig_icg_switch = 1;
    PMU_HW.wakeup.cntl3.sleep_prt_sel = 2;
  }
}

static COLD void lp_states(unsigned active, unsigned sleep, unsigned attenuation) {
  for (unsigned mode = 0; mode < 2; ++mode) {
    volatile pmu_lp_hw_regmap_t *lp = &PMU_HW.lp_sys[mode];
    lp->regulator0.slp_xpd = 0;
    lp->regulator0.xpd = 1;
    lp->regulator0.slp_dbias = 0;
    lp->regulator0.dbias = mode ? sleep : active;
    lp->regulator1.drv_b = 0;
    // Retain LP GPIO and RC_FAST for latched touch-edge detection.
    lp->dig_power.val = mode ? BIT(30) : 0;
    lp->clk_power.val = BIT(30);
  }
  PMU_HW.lp_sys[1].xtal.xpd_xtal = 0;
  PMU_HW.lp_sys[1].bias.xpd_bias = 0;
  PMU_HW.lp_sys[1].bias.dbg_atten = attenuation;
  PMU_HW.lp_sys[1].bias.pd_cur = 1;
  PMU_HW.lp_sys[1].bias.bias_sleep = 1;
}

// Same nominal RC periods used by the qualified reference probe, Q19 us.
// These govern transition delays only; wall time comes from the PCF85063.
static COLD unsigned slow_cycles(unsigned us) { return ((uint64_t)us << 19) / 3495253; }
static COLD unsigned fast_cycles(unsigned us) { return ((uint64_t)us << 19) / 29959; }

COLD void standby_init(void) {
  watchdogs_disable();
  timer_init();
  PMU_HW.lp_sys[0].clk_power.xpd_fosc = 1;
  delay_us(50);
  REG(0x600b0400u) &= ~BIT(2); // LP fast clock: RC_FAST, independent of XTAL.
  delay_us(3);

  PMU_HW.hp_ext.rf_pwc.perif_i2c_rstb = 1;
  PMU_HW.hp_ext.rf_pwc.xpd_perif_i2c = 1;
  regulator_bit(5, 7, true);   // Hand LP regulator control to PMU.
  regulator_bit(7, 7, true);   // Hand HP regulator control to PMU.
  regulator_bit(13, 2, false); // Release software LP regulator force-on.
  regulator_bit(13, 3, false); // Release software HP regulator force-on.

  unsigned version = REG(0x600b0850u);
  bool calibrated = ((version >> 30) * 100 + ((version >> 27) & 7)) >= 3;
  unsigned fuse = REG(0x600b084cu);
  hp_states(calibrated ? active_bias(fuse & 31, 25) : 25);
  lp_states(calibrated ? active_bias((fuse >> 5) & 31, 26) : 26,
            calibrated ? (((fuse >> 19) & 15) + 23) & 31 : 23,
            calibrated ? ((fuse >> 16) & 7) + 8 : 12);

  // Release force overrides for TOP, AON, CPU and radio; board policy then
  // explicitly disables the unused radio. Leave VDD_SPI force defaults alone.
  const unsigned domains[] = {0, 1, 2, 4};
  for (unsigned i = 0; i < 4; ++i) PMU_HW.power.hp_pd[domains[i]].val &= ~63u;
  PMU_HW.power.mem_cntl.force_hp_mem_no_iso = 0;
  PMU_HW.power.lp_peri.val &= ~63u;

  PMU_HW.wakeup.cntl3.hp_min_slp_val = slow_cycles(450);
  PMU_HW.wakeup.cntl3.lp_min_slp_val = slow_cycles(450);
  PMU_HW.wakeup.cntl7.ana_wait_target = fast_cycles(154);
  PMU_HW.wakeup.cntl5.lp_ana_wait_target = slow_cycles(500);
  PMU_HW.power.wait_timer0.wait_timer = fast_cycles(2);
  PMU_HW.power.wait_timer0.powerup_timer = fast_cycles(2);
  PMU_HW.power.wait_timer1.wait_timer = fast_cycles(2);
  PMU_HW.power.wait_timer1.powerup_timer = fast_cycles(2);
  PMU_HW.power.clk_wait.wait_xtal_stable = fast_cycles(250);
  PMU_HW.power.clk_wait.wait_pll_stable = fast_cycles(50);
  // No modem wake source is enabled, so its scheduling target is unused.
  PMU_HW.wakeup.cntl5.modem_wait_target = 0;
}
