#ifndef LEDS_H
#define LEDS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void ledInit();

// Set the led color values
void ledUpdateAll(const uint8_t * data, size_t size);
void ledUpdate(const uint8_t * data);

// Returns true if the ledTask should be run
bool ledReady(uint32_t now);
void ledStart();
void ledTask();
bool ledTransmitInProgress();

#ifdef __cplusplus
}
#endif

#endif // LEDS_H
