#include "timer.h"

static volatile uint32_t ticks100Us;

static void tickISR(TC_TIMER_STATUS status, uintptr_t context) {
  ticks100Us++;
}

void timerInit() {
    TC4_TimerCallbackRegister(tickISR, (uintptr_t)NULL);
    TC4_TimerStart();
}

uint32_t getCurrentTimeMs() {
  return ticks100Us / 10;
}

void delay(uint32_t ms) {
  const uint32_t endTime = ticks100Us + (10 * ms) + 1;

  while (ticks100Us < endTime) {
  }
}
