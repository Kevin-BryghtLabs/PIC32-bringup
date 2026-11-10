#include "timer.h"

static volatile uint32_t ticks100Us;
static volatile uint32_t ticks1Ms;
static uint8_t countTo1Ms;
static uint16_t tc4Period;

// Set to priority 1.  All others except for Reset, NMI, HardFault are 3 (lower).
static void tickISR(TC_TIMER_STATUS status, uintptr_t context) {
  ticks100Us++;
  if (++countTo1Ms >= 10) {
    ticks1Ms++;
    countTo1Ms = 0;
  }
}

void timerInit() {
    TC4_TimerCallbackRegister(tickISR, (uintptr_t)NULL);
    TC4_TimerStart();
    tc4Period = TC4_Timer16bitPeriodGet() + 1;
}

// Reading the ticks requires a sync making it somewhat expensive.
// Only use this call if the accuracy is really needed.
// Can be safely called from any ISR with a lower priority than tc4,
// but not guaranteed to be valid if interrupts are disabled.
uint32_t getCurrentTimeTicks() {
  uint32_t ticksCopy;
  uint16_t counter;
  uint8_t tries = 5;

  // If the ISR occurred between the time we read the first and second
  // ticks values, try again until we get a good read.
  do {
    ticksCopy = ticks100Us;
    counter = TC4_Timer16bitCounterGet();
  } while (ticksCopy != ticks100Us && --tries);

  return (ticksCopy * tc4Period) + counter;
}

uint32_t getCurrentTimeMs() {
  return ticks1Ms;
}

void delay(uint32_t ms) {
  const uint32_t interval = (10 * ms) + 1;
  const uint32_t start = ticks100Us;

  while (ticks100Us - start < interval) {
  }
}
