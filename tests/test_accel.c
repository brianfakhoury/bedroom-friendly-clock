#include "accel.h"
#include "hal.h"
#include <assert.h>
#include <stdio.h>

static uint8_t device;
static unsigned reads;
static bool fail_reads, fail_writes;

bool i2c_read(uint8_t addr, uint8_t reg, uint8_t *out, unsigned n) {
  reads++;
  assert(n == 1);
  if (fail_reads || addr != device)
    return false;
  *out = reg == 0 ? 0x05 : reg;
  return true;
}

bool i2c_write(uint8_t addr, uint8_t reg, const uint8_t *data, unsigned n) {
  (void)reg;
  (void)data;
  assert(n == 1);
  assert(addr == device);
  return !fail_writes;
}

void delay_us(uint32_t us) { (void)us; }

static void unavailable(void) {
  uint32_t values[ACCEL_DIAGNOSTIC_COUNT];
  unsigned before = reads;
  accel_diagnostics(values);
  assert(reads == before);
  assert(values[0] == UINT32_MAX && values[1] == UINT32_MAX);
}

int main(void) {
  unavailable();
  for (device = 0x6a; device <= 0x6b; device++) {
    assert(accel_init());
    uint32_t values[ACCEL_DIAGNOSTIC_COUNT];
    accel_diagnostics(values);
    assert(values[0] == 2 && values[1] == 8);
    fail_reads = true;
    accel_diagnostics(values);
    assert(values[0] == UINT32_MAX && values[1] == UINT32_MAX);
    fail_reads = false;
  }
  device = 0x6a;
  fail_writes = true;
  assert(!accel_init());
  unavailable();
  fail_writes = false;
  device = 0;
  assert(!accel_init());
  unavailable();
  puts("PASS: accelerometer diagnostics follow discovery and handle unavailable devices");
}
