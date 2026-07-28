#ifndef TIMER_H
#define TIMER_H

#include "definitions.h"

void timerInit();
uint32_t getCurrentTimeMs();
void delay(uint32_t ms);

#endif /*TIMER_H*/