#include "orientation.h"
static int magnitude(int value) { return value < 0 ? -value : value; }
unsigned orientation_choose(const int16_t xyz[3], unsigned previous) {
  int x = xyz[0], y = xyz[1], z = xyz[2];
  int ax = magnitude(x), ay = magnitude(y), az = magnitude(z);
  int largest = ax > ay ? ax : ay;
  int other = ax > ay ? ay : ax;
  int64_t norm = (int64_t)x * x + (int64_t)y * y + (int64_t)z * z;
  // +/-2 g: 16384 counts/g. Ignore flat, diagonal, and moving readings.
  // A clear edge must carry >=0.55 g and exceed other axes by >=0.15 g.
  if (norm < (int64_t)12000 * 12000 || norm > (int64_t)21000 * 21000 ||
      largest < 9000 || largest < other + 2500 || largest < az + 2500)
    return previous;
  if (ay > ax)
    return y > 0 ? 0 : 2;
  // Board clockwise gives +X; counter-rotate the glyphs.
  return x > 0 ? 3 : 1;
}
void orientation_rect(unsigned rotation, unsigned *x, unsigned *y, unsigned *w,
                      unsigned *h) {
  unsigned ox = *x, oy = *y, ow = *w, oh = *h;
  switch (rotation & 3) {
  case 1:
    *x = 480 - oy - oh;
    *y = ox;
    *w = oh;
    *h = ow;
    break;
  case 2:
    *x = 480 - ox - ow;
    *y = 480 - oy - oh;
    break;
  case 3:
    *x = oy;
    *y = 480 - ox - ow;
    *w = oh;
    *h = ow;
    break;
  }
}
