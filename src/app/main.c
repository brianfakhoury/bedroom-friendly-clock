#include "app.h"
#include "board.h"
#include "hal.h"
#include "standby.h"

int main(void) {
  board_init();
  app_start();
}

// Called by boot/wake.S after deep sleep to restore hardware and restart
// the application event loop.
__attribute__((used, externally_visible)) void wake_main(void) {
  standby_resume();
  board_resume();
  events_init();
  app_resume();
}
