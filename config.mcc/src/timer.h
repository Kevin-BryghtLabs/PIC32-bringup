#ifndef TIMER_H
#define TIMER_H

#include "definitions.h"

#ifdef __cplusplus
extern "C" {
#endif

void timerInit();
uint32_t getCurrentTimeMs();
void delay(uint32_t ms);

#ifdef __cplusplus
}
#endif

#endif /*TIMER_H*/
