#include "digits.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint16_t pixels[480][480], expected[480][480];
static unsigned pixels_written;
void display_rect(unsigned x, unsigned y, unsigned w, unsigned h,
                  uint16_t color) {
  assert(x + w <= 480 && y + h <= 480);
  pixels_written += w * h;
  for (unsigned row = y; row < y + h; row++)
    for (unsigned col = x; col < x + w; col++)
      pixels[row][col] = color;
}
int main(void) {
  for (unsigned rotation = 0; rotation < 4; rotation++) {
    digits_draw(0, 0, true, rotation, 0);
    memset(pixels, 0, sizeof(pixels));
    digits_draw(9, 59, true, rotation, DIGITS_RED);
    memcpy(expected, pixels, sizeof(pixels));
    digits_draw(10, 0, true, rotation, DIGITS_RED);
    digits_draw(0, 0, false, rotation, DIGITS_RED);
    digits_draw(9, 59, true, rotation, DIGITS_RED);
    assert(memcmp(pixels, expected, sizeof(pixels)) == 0);
    digits_draw(0, 0, true, rotation, 0);
    for (unsigned y = 0; y < 480; y++)
      for (unsigned x = 0; x < 480; x++)
        assert(!pixels[y][x]);
  }
  digits_draw(8, 58, true, 0, DIGITS_RED);
  unsigned left = 480, right = 0, top = 480, bottom = 0;
  for (unsigned y = 0; y < 480; y++)
    for (unsigned x = 0; x < 480; x++)
      if (pixels[y][x]) {
        if (x < left) left = x;
        if (x > right) right = x;
        if (y < top) top = y;
        if (y > bottom) bottom = y;
      }
  assert(right - left + 1 == 335 && bottom - top + 1 == 210);
  assert(left == 72 && top == 135);
  // Compare incremental transitions with a cleared-panel rendering, including
  // size boundaries, invalid time and ink changes, in all four orientations.
  digits_draw(0, 0, true, 0, 0);
  const unsigned hours[] = {9, 10, 11, 12, 1};
  for (unsigned rotation = 0; rotation < 4; rotation++) {
    for (unsigned h = 0; h < sizeof(hours) / sizeof(hours[0]); h++) {
      for (unsigned m = 0; m < 60; m++) {
        bool valid = m != 30;
        uint16_t ink = m % 2 ? DIGITS_RED : 0x001f;
        digits_draw(hours[h], m, valid, rotation, ink);
        memcpy(expected, pixels, sizeof(pixels));
        pixels_written = 0;
        digits_draw(hours[h], m, valid, rotation, ink);
        assert(pixels_written < 500); // Unchanged digits need no pixel traffic.
        assert(memcmp(pixels, expected, sizeof(pixels)) == 0);
        digits_draw(0, 0, true, rotation, 0);
        for (unsigned y = 0; y < 480; y++)
          for (unsigned x = 0; x < 480; x++) assert(!pixels[y][x]);
        digits_draw(hours[h], m, valid, rotation, ink);
        assert(memcmp(pixels, expected, sizeof(pixels)) == 0);
      }
    }
    digits_draw(0, 0, true, rotation, 0);
  }
  puts("PASS: incremental/full-render equivalence, ink changes, unchanged-frame traffic;  larger centered digits, size transitions, invalid time, rotation bounds and erase");
}
