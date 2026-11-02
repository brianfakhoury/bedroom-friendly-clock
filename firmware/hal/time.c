#include "board.h"
#include "hal.h"
#include "watchdog.h"
void timer_init(void) {
  REG(PCR_SYSTIMER_CONFIG) = 1;
  REG(PCR_SYSTIMER_CLOCK) = BIT(22);
  REG(SYSTIMER_CONFIG) = BIT(30) | BIT(31);
}
uint64_t ticks(void) {
  REG(SYSTIMER_UPDATE0) = BIT(30);
  while (!(REG(SYSTIMER_UPDATE0) & BIT(29))) {
  }
  return ((uint64_t)REG(SYSTIMER_VALUE0_HIGH) << 32) | REG(SYSTIMER_VALUE0_LOW);
}
void delay_us(uint32_t us) {
  uint64_t end = ticks() + (uint64_t)us * 16;
  while (ticks() < end) {
  }
}
void events_init(void) {
  // All handlers run synchronously after WFI; global machine interrupts stay
  // masked.
  // Permit hardware to gate the CPU clock during WFI; this is not PMU light
  // sleep.
  REG(PCR_CPU_WAITI) &= ~BIT(3);
  REG(PLIC_ENABLE) = 0;
  REG(PLIC_TYPE) = 0;
  REG(PLIC_PRIORITY1) = 1;
  REG(PLIC_THRESHOLD) = 0;
  REG(IRQ_MAP_GPIO) = 1;
  REG(IRQ_MAP_USB) = 1;
  REG(IRQ_MAP_SYSTIMER0) = 1;
  REG(USB_IRQ_ENABLE) = BIT(2); // USB received packet only
  REG(GPIO_PIN0 + BUTTON_BOOT * 4) = BIT(13) | (2u << 7); // BOOT falling edge
  REG(GPIO_PIN0 + BUTTON_KEY * 4) = BIT(13) | (2u << 7);  // KEY falling edge
  REG(GPIO_PIN0 + TOUCH_IRQ * 4) = BIT(13) | (2u << 7); // CST9217 report ready
  REG(GPIO_STATUS_CLEAR) = BIT(TOUCH_IRQ) | BUTTON_MASK;
  REG(SYSTIMER_IRQ_ENABLE) = 1;
  REG(PLIC_ENABLE) = BIT(1);
  __asm__ volatile("csrw mie, %0" ::"r"(0xffffffffu));
}
unsigned wait_until(uint64_t deadline) {
  REG(SYSTIMER_CONFIG) &= ~BIT(24);
  REG(SYSTIMER_IRQ_CLEAR) = 1;
  if (deadline) {
    if (ticks() >= deadline)
      return EVENT_TIMER;
    uint64_t target = deadline;
    REG(SYSTIMER_TARGET0_CONFIG) = 0;
    REG(SYSTIMER_TARGET0_HIGH) = target >> 32;
    REG(SYSTIMER_TARGET0_LOW) = (uint32_t)target;
    REG(SYSTIMER_TARGET0_LOAD) = 1;
    REG(SYSTIMER_CONFIG) |= BIT(24);
  }
  // A partially drained USB FIFO may not generate another packet interrupt.
  // GPIO/USB IRQ status stays latched across the final check and WFI.
  bool ready = (REG(USB_FIFO_CONFIG) & 4) || (deadline && ticks() >= deadline);
  if (!ready) {
    watchdog_disarm();
    __asm__ volatile("fence iorw, iorw; wfi" ::: "memory");
    watchdog_arm();
  }
  unsigned event = (REG(USB_FIFO_CONFIG) & 4) ? EVENT_USB : 0;
  if (deadline && ticks() >= deadline)
    event |= EVENT_TIMER;
  if (REG(GPIO_STATUS) & BUTTON_MASK) {
    REG(GPIO_STATUS_CLEAR) = BUTTON_MASK;
    event |= EVENT_BUTTON;
  }
  if (REG(GPIO_STATUS) & BIT(TOUCH_IRQ)) {
    REG(GPIO_STATUS_CLEAR) = BIT(TOUCH_IRQ);
    event |= EVENT_TOUCH;
  }
  if (REG(USB_IRQ_RAW) & BIT(2)) {
    REG(USB_IRQ_CLEAR) = BIT(2);
    event |= EVENT_USB;
  }
  if (REG(SYSTIMER_IRQ_RAW) & 1) {
    REG(SYSTIMER_IRQ_CLEAR) = 1;
    event |= EVENT_TIMER;
  }
  REG(SYSTIMER_CONFIG) &= ~BIT(24);
  return event;
}
