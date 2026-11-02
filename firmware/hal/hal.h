#pragma once
#include "registers.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define REG(a) (*(volatile uint32_t *)(uintptr_t)(a))
#define BIT(n) (1u << (n))
void gpio_init(unsigned pin, bool output, bool pullup);
void gpio_set(unsigned pin, bool high);
bool gpio_get(unsigned pin);
void usb_puts(const char *s);
void usb_hex(uint32_t v);
int usb_get(void);
bool i2c_read(uint8_t addr, uint8_t reg, uint8_t *out, unsigned n);
bool i2c_write(uint8_t addr, uint8_t reg, const uint8_t *data, unsigned n);
void i2c_init(void);
#define TICKS_PER_US 16u
#define TICKS_PER_MS 16000u
void timer_init(void);
uint64_t ticks(void);
void delay_us(uint32_t us);
void events_init(void);
// Zero deadline means wait indefinitely; pending work always wins.
unsigned wait_until(uint64_t deadline);
#define EVENT_BUTTON 1u
#define EVENT_USB 2u
#define EVENT_TIMER 4u
void watchdogs_disable(void);
bool i2c_read16(uint8_t addr, uint16_t reg, uint8_t *out, unsigned n);
#define EVENT_TOUCH 8u
