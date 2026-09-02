#ifndef LEDS_H
#define LEDS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Set the led color values
//void ledUpdateAll(const uint8_t * data, size_t size);
void ledUpdate(const uint8_t * data);

// LED task functions
bool ledTaskShouldRun();
void ledTask();
bool ledTransmitInProgress();

#ifdef __cplusplus
}
#endif

#endif // LEDS_H
