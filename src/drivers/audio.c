#include "audio.h"
#include "hal.h"
// Power-down register sequences from Espressif's ES8311 suspend and ES7210
// stop drivers bundled with Waveshare's 2.16 Audio_Test example.
// Retain the shared audio supply: safe unpowered operation on the active I2C
// bus has not been established. Software shutdown avoids that back-feed risk.
static bool program(uint8_t address, const uint8_t (*sequence)[2], unsigned n) {
  bool good = true;
  for (unsigned i = 0; i < n; i++)
    if (!i2c_write(address, sequence[i][0], &sequence[i][1], 1))
      good = false;
  return good;
}
bool audio_disable(void) {
  const uint8_t codec[][2] = {
      {0x32, 0},    {0x17, 0}, {0x0e, 0xff}, {0x12, 2},    {0x14, 0},
      {0x0d, 0xfa}, {0x15, 0}, {0x02, 0x10}, {0x00, 0},    {0x00, 0x1f},
      {0x01, 0x30}, {0x01, 0}, {0x45, 0},    {0x0d, 0xfc}, {0x02, 0},
  };
  const uint8_t adc[][2] = {
      {0x47, 0xff}, {0x48, 0xff}, {0x49, 0xff}, {0x4a, 0xff}, {0x4b, 0xff},
      {0x4c, 0xff}, {0x40, 0xc0}, {0x01, 0x7f}, {0x06, 7},
  };
  bool codec_off = program(0x18, codec, sizeof(codec) / sizeof(codec[0]));
  bool adc_off = program(0x40, adc, sizeof(adc) / sizeof(adc[0]));
  // No I2S clock or data transitions, even if boot pin states change later.
  const unsigned outputs[] = {19, 20, 22, 23};
  for (unsigned i = 0; i < sizeof(outputs) / sizeof(outputs[0]); i++) {
    gpio_set(outputs[i], false);
    gpio_init(outputs[i], true, false);
  }
  return codec_off && adc_off;
}

void audio_diagnostics(uint32_t values[AUDIO_DIAGNOSTIC_COUNT]) {
  const uint8_t addresses[] = {0x18, 0x18, 0x40, 0x40};
  const uint8_t regs[] = {0x0d, 0x01, 0x06, 0x01};
  for (unsigned i = 0; i < AUDIO_DIAGNOSTIC_COUNT; i++) {
    uint8_t value;
    values[i] = i2c_read(addresses[i], regs[i], &value, 1) ? value : UINT32_MAX;
  }
}
