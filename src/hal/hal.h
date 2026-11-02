#pragma once
#include "timer.h"
#include <stdbool.h>
#include <stdint.h>
void gpio_init(unsigned pin, bool output, bool pullup);
void gpio_set(unsigned pin, bool high);
bool gpio_get(unsigned pin);
void usb_puts(const char *s);
void usb_hex(uint32_t v);
int usb_get(void);
bool i2c_read(uint8_t addr, uint8_t reg, uint8_t *out, unsigned n);
bool i2c_write(uint8_t addr, uint8_t reg, const uint8_t *data, unsigned n);
void i2c_init(void);
void events_init(void);
// Zero deadline means wait indefinitely; pending work always wins.
unsigned wait_until(uint64_t deadline);
#define EVENT_BOOT 1u
#define EVENT_KEY 16u
#define EVENT_BUTTON (EVENT_BOOT | EVENT_KEY)
#define EVENT_USB 2u
#define EVENT_TIMER 4u
bool i2c_read16(uint8_t addr, uint16_t reg, uint8_t *out, unsigned n);
#define EVENT_TOUCH 8u
