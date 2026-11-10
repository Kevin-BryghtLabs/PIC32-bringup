#include "bl_shu_stats.hpp"

#include "bl_ble_packet_headers.h"
#include "bl_stat.h"
#include "bl_uart.h"
#include <cstddef>
#include <cstdint>

// TODO: Incorporate shared headers
struct StatReport {
  uint16_t type;
  uint16_t _reserved;
  uint32_t count;
  uint32_t min;
  uint32_t max;
  uint32_t mean;
  uint32_t variance;
};

static constexpr uint32_t StatsPublishIntervalMs = 5000;
static uint32_t lastPublishMs;

static IntStat stats[StatCount];

static void populateReport(uint16_t type, StatReport & report, const IntStat & stat) {
  report.type         = type;
  report._reserved    = 0;
  report.count        = stat.count;
  report.min          = stat.min;
  report.max          = stat.max;
  report.mean         = statGetMean(&stat);
  report.variance     = statGetVariance(&stat);
}

static void shuStatsPublish() {
  static constexpr size_t PacketPayloadSize = 248;
  static constexpr size_t StatsPerPacket = PacketPayloadSize / sizeof(StatReport);
  static constexpr size_t PacketCount = (StatCount + StatsPerPacket - 1) / StatsPerPacket;
  size_t statIdx = 0;

  for (unsigned p = 0; p < PacketCount; ++p) {
    const size_t statsRemaining = StatCount - statIdx;
    const size_t statsThisPacket = statsRemaining <= StatsPerPacket ? statsRemaining : StatsPerPacket;
    const size_t statsIdxEnd = statIdx + statsThisPacket;

    sendGenericStart(STATS_DATA, statsThisPacket * sizeof(StatReport));

    for ( ; statIdx < statsIdxEnd; ++statIdx) {
      StatReport report;
      populateReport(statIdx, report, stats[statIdx]);
      sendGenericData(&report, sizeof(report));
    }

    sendGenericDone();
  }
}

static void resetStats() {
  for (unsigned i = 0; i < StatCount; ++i) {
    statReset(&stats[i]);
  }
}

void shuStatsInit() {
  for (unsigned i = 0; i < StatCount; ++i) {
    statInit(&stats[i]);
  }
}

void shuStatsTask(uint32_t now) {
  if (now - lastPublishMs >= StatsPublishIntervalMs) {
    lastPublishMs += StatsPublishIntervalMs;
    shuStatsPublish();
    resetStats();
  }
}

void shuStatsAddSample(StatIdx stat, uint32_t sample) {
  if (stat >= StatCount) {
    return;
  }

  statAdd(&stats[stat], sample);
}

