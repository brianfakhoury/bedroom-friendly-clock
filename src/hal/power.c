#include "power.h"
#include "hal.h"
#include "registers.h"
void power_read(uint32_t values[POWER_REGISTER_COUNT]) {
  const uint32_t addresses[] = {
      PMU_RADIO_POWER,     MODEM_CLOCKS, MODEM_CLOCKS_FORCE, MODEM_CLOCKS1,
      MODEM_CLOCKS1_FORCE, MODEM_RESET,  MODEM_LP_CLOCKS,    PMU_ACTIVE_POWER,
  };
  for (unsigned i = 0; i < POWER_REGISTER_COUNT; i++)
    values[i] = REG(addresses[i]);
}

void radios_disable(void) {
  // Wi-Fi, BLE, 802.15.4 and their shared RF front end are never used.
  // Assert reset before stopping clocks, then isolate and power down the
  // domain.
  const uint32_t reset = BIT(8) | BIT(10) | BIT(14) | BIT(15) | BIT(16) |
                         BIT(17) | BIT(18) | BIT(24) | BIT(30);
  REG(MODEM_RESET) |= reset;
  REG(MODEM_CLOCKS) &= BIT(21); // Preserve the data-dump mux, disable enables.
  REG(MODEM_CLOCKS_FORCE) = 0;
  REG(MODEM_CLOCKS1) = 0;
  REG(MODEM_CLOCKS1_FORCE) = 0;
  // Keep the analog I2C controller setting: it also serves non-radio analog
  // logic.
  REG(MODEM_LP_CLOCKS) &= ~(BIT(0) | BIT(1) | BIT(3));
  uint32_t power = REG(PMU_RADIO_POWER);
  power &= ~(BIT(2) | BIT(3) |
             BIT(4));       // No forced power-up/reset bypass/isolation bypass.
  power |= BIT(0) | BIT(1); // Reset and isolate.
  REG(PMU_RADIO_POWER) = power;
  delay_us(2);
  REG(PMU_RADIO_POWER) = power | BIT(5); // Force power-down.
}

// APB clock enables for blocks absent from this firmware. GPIO, USB, timers,
// flash/cache and clock/reset infrastructure are deliberately not in this list.
static const uint32_t unused_clocks[] = {
    0x60096000u, // UART0
    0x6009600cu, // UART1
    0x60096020u, // Hardware I2C (the clock uses software I2C)
    0x60096028u, // UHCI
    0x6009602cu, // RMT
    0x60096034u, // LEDC
    0x6009605cu, // TWAI0
    0x60096064u, // TWAI1
    0x6009606cu, // I2S
    0x60096098u, // ETM
    0x600960a4u, // Parallel IO
    0x600960bcu, // GDMA
    0x600960c8u, // AES
    0x600960ccu, // SHA
    0x600960d0u, // RSA
    0x600960d8u, // ECC
    0x6009603cu, // Timer group 0 (watchdogs disabled before gating)
    0x60096048u, // Timer group 1
    0x60096094u, // Pulse counter
    0x6009609cu, // Motor PWM
    0x600960b0u, // SDIO slave
    0x600960c0u, // SPI2 (panel uses software QSPI)
    0x600960e0u, // Digital signature
};
static const uint32_t unused_function_clocks[] = {
    0x60096004u, 0x60096010u,              // UART0/1
    0x60096024u, 0x60096030u, 0x60096038u, // I2C, RMT, LEDC
    0x60096040u, 0x60096044u, 0x6009604cu,
    0x60096050u,              // Timer groups/timer WDTs
    0x60096060u, 0x60096068u, // TWAI0/1
    0x60096070u, 0x60096078u, // I2S TX/RX
    0x60096084u, 0x60096088u, // SAR ADC, temperature sensor
    0x600960a0u, 0x600960c4u, // Motor PWM, SPI2
};
void unused_peripheral_clocks_disable(void) {
  for (unsigned i = 0;
       i < sizeof(unused_function_clocks) / sizeof(unused_function_clocks[0]);
       i++)
    REG(unused_function_clocks[i]) &= ~BIT(22);
  REG(0x600960a8u) &= ~BIT(18);           // Parallel RX functional clock.
  REG(0x600960acu) &= ~BIT(18);           // Parallel TX functional clock.
  REG(0x60096080u) &= ~(BIT(0) | BIT(2)); // ADC functional/register interfaces.

  for (unsigned i = 0; i < sizeof(unused_clocks) / sizeof(unused_clocks[0]);
       i++)
    REG(unused_clocks[i]) &= ~BIT(0);
}
uint32_t unused_clocks_enabled(void) {
  uint32_t enabled = 0;
  for (unsigned i = 0; i < sizeof(unused_clocks) / sizeof(unused_clocks[0]);
       i++)
    if (REG(unused_clocks[i]) & BIT(0))
      enabled |= BIT(i);
  return enabled;
}

uint32_t unused_function_clocks_enabled(void) {
  uint32_t enabled = 0;
  for (unsigned i = 0;
       i < sizeof(unused_function_clocks) / sizeof(unused_function_clocks[0]);
       i++)
    if (REG(unused_function_clocks[i]) & BIT(22))
      enabled |= BIT(i);
  if (REG(0x600960a8u) & BIT(18))
    enabled |= BIT(24);
  if (REG(0x600960acu) & BIT(18))
    enabled |= BIT(25);
  if (REG(0x60096080u) & (BIT(0) | BIT(2)))
    enabled |= BIT(26);
  return enabled;
}
