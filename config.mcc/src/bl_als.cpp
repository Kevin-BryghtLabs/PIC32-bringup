#include "bl_als.hpp"

#include "definitions.h"
#include "hub_log.h"
#include "timer.h"

static constexpr uint32_t AlsIntervalMs = 500;

static uint8_t lastAlsValue;
static uint32_t lastAlsTimeMs = 0;
static bool alsRunning = false;

void alsInit() {
}

bool alsReady(uint32_t nowMs) {
  return nowMs - lastAlsTimeMs > AlsIntervalMs;
}

bool alsReadInProgress() {
  return alsRunning;
}

void alsStart() {
  ADC0_ChannelSelect(ADC_POSINPUT_AIN0, ADC_NEGINPUT_GND);
  ADC0_Enable();
  ADC0_ConversionStart();

  alsRunning = true;
}

void alsTask() {
  if (!alsRunning) {
    return;
  }

  if (ADC0_ConversionSequenceIsFinished()) {
    lastAlsValue = ADC0_ConversionResultGet();
    lastAlsTimeMs = getCurrentTimeMs();
    hubLog('I', "ALS: %u", lastAlsValue);

    alsRunning = false;
  }
}

