#ifndef BL_SHU_STATS_HPP
#define BL_SHU_STATS_HPP

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum StatIdx {
  Stat_ALS_SampleTime,
  Stat_CapTouch_SampleTime,
  Stat_LED_UpdateTime,
  Stat_PieceId_SquareTime,

  Stat_ALS_StartLatency,
  Stat_CapTouch_StartLatency,
  Stat_LED_StartLatency,
  Stat_PieceId_StartLatency,

  StatCount
} StatIdx;

void shuStatsInit();
void shuStatsTask(uint32_t now);
void shuStatsAddSample(StatIdx stat, uint32_t sample);

#ifdef __cplusplus
}
#endif

#endif // BL_SHU_STATS_HPP
