#include "digits.h"
#include "display.h"
#include "orientation.h"

// The panel and this small cache survive CPU-off standby.
static uint8_t previous_segments[4];
static uint16_t previous_ink;

// Four digits span 350 px. Three digits use 5/4 scale and span 335 px.
// Coordinates stay in the upright frame; rotation is applied last.
static void rect(unsigned rotation, bool large, unsigned x, unsigned y,
                 unsigned w, unsigned h, uint16_t ink) {
  unsigned scale = large ? 5 : 4;
  unsigned left = large ? 72 : 65;
  unsigned top = large ? 135 : 156;
  w = (x + w) * scale / 4 - x * scale / 4;
  h = (y + h) * scale / 4 - y * scale / 4;
  x = left + x * scale / 4;
  y = top + y * scale / 4;
  orientation_rect(rotation, &x, &y, &w, &h);
  display_rect(x, y, w, h, ink);
}
static void digit(unsigned rotation, bool large, unsigned x, unsigned d,
                  bool valid, uint16_t ink, unsigned slot) {
  static const uint8_t seg[] = {0x3f, 0x06, 0x5b, 0x4f, 0x66,
                                0x6d, 0x7d, 0x07, 0x7f, 0x6f};
  static const uint8_t r[7][4] = {
      {12, 0, 48, 10}, {60, 12, 10, 66}, {60, 90, 10, 66}, {12, 158, 48, 10},
      {0, 90, 10, 66}, {0, 12, 10, 66},  {12, 80, 48, 10}};
  uint8_t bits = ink ? (valid ? seg[d] : 0x40) : 0;
  uint8_t changed = bits ^ previous_segments[slot];
  if (ink != previous_ink)
    changed |= bits;
  previous_segments[slot] = bits;
  for (unsigned i = 0; i < 7; i++)
    if (changed & (1u << i))
      rect(rotation, large, x + r[i][0], r[i][1], r[i][2], r[i][3],
           bits & (1u << i) ? ink : 0);
}
static void draw(unsigned h, unsigned m, bool valid, unsigned rotation,
                 bool large, uint16_t ink) {
  unsigned offset = large ? 0 : 82;
  if (!large)
    digit(rotation, false, 0, h / 10, valid, ink, 0);
  digit(rotation, large, offset, h % 10, valid, ink, 1);
  digit(rotation, large, offset + 116, m / 10, valid, ink, 2);
  digit(rotation, large, offset + 198, m % 10, valid, ink, 3);
  rect(rotation, large, offset + 86, 46, 10, 10, ink);
  rect(rotation, large, offset + 86, 108, 10, 10, ink);
}
void digits_draw(unsigned h, unsigned m, bool valid, unsigned rotation,
                 uint16_t ink) {
  static bool previous_large;
  bool large = valid && h < 10;
  // Erase old geometry when changing size. A zero-ink call clears either size
  // (used by the clock before rotating), using the cached lit segments.
  if (!ink || large != previous_large)
    draw(0, 0, false, rotation, previous_large, 0);
  draw(h, m, valid, rotation, large, ink);
  previous_large = large;
  previous_ink = ink;
}
