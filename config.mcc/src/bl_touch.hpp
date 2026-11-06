#ifndef BL_TOUCH_H
#define BL_TOUCH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void capTouchInit();
bool capTouchReady();
void capTouchStart();
void capTouchTask();
bool capTouchInProgress();

void capTouchStartCalibration();
void capTouchSendCCVals();
void capTouchSetCCVals(const uint8_t * ccData, size_t size);

#ifdef __cplusplus
}
#endif

#endif // BL_TOUCH_H
