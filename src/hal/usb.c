#include "hal.h"
#include "registers.h"
void usb_puts(const char *s) {
  while (*s) {
    // A full uncommitted packet cannot drain, even after the host reconnects.
    if (!(REG(USB_FIFO_CONFIG) & 2))
      REG(USB_FIFO_CONFIG) = 1;
    unsigned timeout = 20000;
    while (!(REG(USB_FIFO_CONFIG) & 2) && --timeout)
      delay_us(1);
    if (!timeout) {
      REG(USB_FIFO_CONFIG) = 1;
      return;
    }
    REG(USB_FIFO) = (uint8_t)*s++;
  }
  REG(USB_FIFO_CONFIG) = 1;
}
void usb_hex(uint32_t v) {
  char b[10];
  for (int i = 0; i < 8; i++)
    b[i] = "0123456789ABCDEF"[(v >> (28 - i * 4)) & 15];
  b[8] = ' ';
  b[9] = 0;
  usb_puts(b);
}
int usb_get(void) {
  return (REG(USB_FIFO_CONFIG) & 4) ? (int)(REG(USB_FIFO) & 255) : -1;
}
