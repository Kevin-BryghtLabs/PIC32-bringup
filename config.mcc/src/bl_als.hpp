#ifndef BL_ALS_HPP
#define BL_ALS_HPP

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void alsInit();
bool alsReady(uint32_t nowMs);
bool alsReadInProgress();
void alsStart();
void alsTask();

#ifdef __cplusplus
}
#endif

#endif // BL_ALS_HPP
