#ifndef TIMER_H
#define TIMER_H

#include "definitions.h"

#ifdef __cplusplus
extern "C" {
#endif

void timerInit();
uint32_t getCurrentTimeMs();

// Reading the ticks requires a sync making it somewhat expensive.
// Only use this call if the accuracy is really needed.
// Can be safely called from any ISR with a lower priority than tc4,
// but not guaranteed to be valid if interrupts are disabled.
uint32_t getCurrentTimeTicks();

void delay(uint32_t ms);

#ifdef __cplusplus
}
#endif

#endif /*TIMER_H*/
