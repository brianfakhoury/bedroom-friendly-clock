#include "orientation.h"
#include <assert.h>
#include <stdio.h>

static void chosen(int x, int y, int z, unsigned expected) {
  int16_t xyz[] = {x, y, z};
  assert(orientation_choose(xyz, 0) == expected);
}
int main(void) {
  chosen(0, 16384, 0, 0);
  chosen(0, -16384, 0, 2);
  chosen(16384, 0, 0, 3);
  chosen(-16384, 0, 0, 1);
  chosen(-900, 16600, -200, 0); // Physical upright calibration.
  int16_t ambiguous[][3] = {{0, 0, 16384}, {0, 0, -16384},
      {11585, 11585, 0}, {0, 0, 0}, {25000, 0, 0}};
  for (unsigned old = 0; old < 4; old++)
    for (unsigned i = 0; i < sizeof(ambiguous)/sizeof(ambiguous[0]); i++)
      assert(orientation_choose(ambiguous[i], old) == old);
  for (unsigned r = 0; r < 4; r++) {
    unsigned x = 64, y = 156, w = 70, h = 168;
    orientation_rect(r, &x, &y, &w, &h);
    assert(x + w <= 480 && y + h <= 480 && w * h == 70 * 168);
    orientation_rect((4-r)&3, &x, &y, &w, &h);
    assert(x == 64 && y == 156 && w == 70 && h == 168);
    x = y = 0; w = h = 480;
    orientation_rect(r, &x, &y, &w, &h);
    assert(x == 0 && y == 0 && w == 480 && h == 480);
  }
  puts("PASS: four orientations, tilt/motion rejection, rotation bounds and inverse");
}
