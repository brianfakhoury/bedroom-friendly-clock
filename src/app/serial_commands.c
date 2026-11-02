#include "serial_commands.h"
#include "accel.h"
#include "audio.h"
#include "hal.h"
#include "local_time.h"
#include "pmic.h"
#include "power.h"
#include "rtc.h"
static char line[48];
static unsigned used;
static bool overflow;
static bool equal(const char *a, const char *b) {
  while (*a && *a == *b) {
    a++;
    b++;
  }
  return *a == *b;
}
static void status(const clock_state *clock) {
  datetime d;
  char text[21];
  usb_puts("CLOCK UTC ");
  if (rtc_read(&d)) {
    date_format(&d, text);
    usb_puts(text);
  } else
    usb_puts("INVALID");
  usb_puts(clock->visible ? " DISPLAY ON" : " DISPLAY OFF");
  usb_puts(" WAKE_HEX ");
  usb_hex(clock->activations);
  usb_puts(" DRAW_US_HEX ");
  usb_hex(clock->prepare_us);
  usb_puts(" ROTATION_HEX ");
  usb_hex(clock->rotation * 90);
  usb_puts("\n");
}
static bool parse_time(const char *p, datetime *d) {
  for (unsigned i = 0; i < 14; i++)
    if (p[i] < '0' || p[i] > '9')
      return false;
  if (p[14])
    return false;
  unsigned v[7];
  for (unsigned i = 0; i < 7; i++)
    v[i] = (p[i * 2] - '0') * 10 + p[i * 2 + 1] - '0';
  *d = (datetime){v[0] * 100 + v[1], v[2], v[3], v[4], v[5], v[6]};
  return date_valid(d);
}
static bool command(clock_state *clock) {
  if (equal(line, "POWER")) {
    uint32_t values[POWER_REGISTER_COUNT];
    power_read(values);
    usb_puts("POWER_HEX ");
    for (unsigned i = 0; i < POWER_REGISTER_COUNT; i++)
      usb_hex(values[i]);
    usb_puts("\nUNUSED_CLOCKS_HEX ");
    usb_hex(unused_clocks_enabled());
    usb_puts("\nUNUSED_FUNCTION_CLOCKS_HEX ");
    usb_hex(unused_function_clocks_enabled());
    usb_puts("\nPMIC_HEX ");
    uint32_t pmic[PMIC_DIAGNOSTIC_COUNT];
    pmic_diagnostics(pmic);
    for (unsigned i = 0; i < PMIC_DIAGNOSTIC_COUNT; i++)
      usb_hex(pmic[i]);
    usb_puts("\nAUDIO_HEX ");
    uint32_t audio[AUDIO_DIAGNOSTIC_COUNT];
    audio_diagnostics(audio);
    for (unsigned i = 0; i < AUDIO_DIAGNOSTIC_COUNT; i++)
      usb_hex(audio[i]);
    usb_puts("\nIMU_HEX ");
    uint32_t accel[ACCEL_DIAGNOSTIC_COUNT];
    accel_diagnostics(accel);
    for (unsigned i = 0; i < ACCEL_DIAGNOSTIC_COUNT; i++)
      usb_hex(accel[i]);
    usb_puts("\n");
  } else if (equal(line, "ACCEL")) {
    int16_t xyz[3];
    if (accel_read(xyz)) {
      usb_puts("ACCEL_HEX ");
      for (unsigned i = 0; i < 3; i++)
        usb_hex((uint32_t)(int32_t)xyz[i]);
      usb_puts("\n");
    } else
      usb_puts("ERR ACCEL\n");
  } else if (equal(line, "STATUS"))
    status(clock);
  else if (used >= 5 && line[0] == 'D' && line[1] == 'I' && line[2] == 'M' &&
           line[3] == ' ') {
    unsigned v = 0;
    bool good = true;
    for (unsigned i = 4; i < used; i++) {
      if (line[i] < '0' || line[i] > '9' || v > 255) {
        good = false;
        break;
      }
      v = v * 10 + (line[i] - '0');
    }
    if (good && v >= 1 && v <= 255) {
      clock->brightness = v;
      usb_puts("OK DIM\n");
    } else
      usb_puts("ERR DIM\n");
  } else if (equal(line, "DIAG")) {
    // Read-only service diagnostics. No panel pin toggles or undocumented
    // writes.
    status(clock);
  } else if (equal(line, "SHOW"))
    return true;
  else if (used >= 5 && line[0] == 'T' && line[1] == 'I' && line[2] == 'M' &&
           line[3] == 'E' && line[4] == ' ') {
    datetime d;
    if (parse_time(line + 5, &d) && rtc_set(&d)) {
      usb_puts("OK TIME\n");
      status(clock);
    } else
      usb_puts("ERR TIME\n");
  } else
    usb_puts("ERR COMMAND\n");
  return false;
}

bool serial_commands_poll(clock_state *clock) {
  // Bound each turn so a busy host cannot starve fade deadlines or touch input.
  for (unsigned budget = 0; budget < 64; budget++) {
    int c = usb_get();
    if (c < 0)
      break;
    if (c == '\r')
      continue;
    if (c == '\n') {
      line[used] = 0;
      bool activate = false;
      if (overflow)
        usb_puts("ERR LENGTH\n");
      else if (used)
        activate = command(clock);
      used = 0;
      overflow = false;
      return activate;
    } else if (used < sizeof(line) - 1)
      line[used++] = (char)c;
    else
      overflow = true;
  }
  return false;
}
