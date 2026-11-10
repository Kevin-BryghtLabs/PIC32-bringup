#ifndef BL_STAT_H
#define BL_STAT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// A stat implementation designed for Cortex-M0, which has:
//
//  - No hardware floating point support
//  - No hardware division support.
//
// All values are stored in and computed with integer math.
// Division is kept to a minimum.
//
// An offset is applied to keep the values s1 and s2 smaller.
// Therefor, the maximum number of samples guaranteed to not
// overflow is based on the SPAN (max value - min value) of
// input values, not the absolute value.
//
// The math supports:
//
//   Span size (bits)   |   Max sample count
//  --------------------+-----------------------
//        16            |   2^32 - 1
//        20            |   16,777,248
//        24            |       65,536
//        26            |        4,096
//        28            |          256
//
// See comments in C file for more detail

typedef struct IntStat {
  int32_t offset;
  int32_t max;
  int32_t min;
  uint32_t count;
  int64_t s1;
  uint64_t s2;
} IntStat;

// Initialize a stat struct
void statInit(IntStat * stat);

// Reset a stat after the sampling interval
void statReset(IntStat * stat);

// Add a sample
void statAdd(IntStat * stat, int32_t sample);

// Call after the sampling interval to calculate the mean for reporting
int32_t statGetMean(const IntStat * stat);

// Call after the sampling interval to calculate the variance for reporting
// (Somewhat expensive ... may want to skip calling if not needed)
uint32_t statGetVariance(const IntStat * stat);

#ifdef __cplusplus
}
#endif

#endif // BL_STAT_H
