#include "bl_touch.hpp"

#include <assert.h>
#include <stdbool.h>
#include "bl_ble_packet_headers.h"
#include "bl_uart.h"
#include "bl_util.h"
#include "definitions.h"
#include "hub_log.h"
#include "timer.h"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

// These are multiplexed with MMA/MSA mux control lines
static constexpr PORT_PIN CAP_X0_PIN  = PORT_PIN_PA08;
static constexpr PORT_PIN CAP_X1_PIN  = PORT_PIN_PA09;
static constexpr PORT_PIN CAP_X2_PIN  = PORT_PIN_PA10;
static constexpr PORT_PIN CAP_X3_PIN  = PORT_PIN_PA11;
static constexpr PORT_PIN CAP_X4_PIN  = PORT_PIN_PA16;
static constexpr PORT_PIN CAP_X5_PIN  = PORT_PIN_PA17;

// These are always captouch
static constexpr PORT_PIN CAP_X6_PIN  = PORT_PIN_PA18;
static constexpr PORT_PIN CAP_X7_PIN  = PORT_PIN_PA19;
static constexpr PORT_PIN CAP_X8_PIN  = PORT_PIN_PA20;
static constexpr PORT_PIN CAP_X9_PIN  = PORT_PIN_PA21;
static constexpr PORT_PIN CAP_Y2_PIN  = PORT_PIN_PA04;
static constexpr PORT_PIN CAP_Y3_PIN  = PORT_PIN_PA05;
static constexpr PORT_PIN CAP_Y4_PIN  = PORT_PIN_PA06;
static constexpr PORT_PIN CAP_Y5_PIN  = PORT_PIN_PA07;
static constexpr PORT_PIN CAP_Y8_PIN  = PORT_PIN_PB02;
static constexpr PORT_PIN CAP_Y9_PIN  = PORT_PIN_PB03;
static constexpr PORT_PIN CAP_Y14_PIN = PORT_PIN_PB08;
static constexpr PORT_PIN CAP_Y15_PIN = PORT_PIN_PB09;

static const uint32_t capSendIntervalMs = 50;

static uint32_t nextCapSendMs;
static bool sendCalibrationStatusWhenFinished;
static uint16_t sens[DEF_NUM_SENSORS];

// Harmony library's RTC handler
extern "C" { 
  void rtc_cb(RTC_TIMER32_INT_MASK intCause, uintptr_t context );
}

static bool isCalibrating() {
  for(unsigned i = 0; i < DEF_NUM_SENSORS; ++i) {
    if(qtlib_key_data_set1[i].node_data_struct_ptr->node_acq_status & NODE_CAL_MASK) {
      return true;
    }
  }
  return false;
}

static void sendCapData() {
  sendGeneric(SEND_DATA, sizeof(sens), sens);
}

static void sendCalibrationFinished() {
  sendGeneric(TOUCH_CALIBRATION_DONE, 0, NULL);
}

static void configPinsForCapTouch(void)
{
  static constexpr PORT_PIN capTouchPins[] = {
    CAP_X0_PIN, CAP_X1_PIN, CAP_X2_PIN, CAP_X3_PIN, CAP_X4_PIN, CAP_X5_PIN
  };
  static constexpr size_t CapTouchPinCount = ARRAY_SIZE(capTouchPins);

  for (unsigned i = 0; i < CapTouchPinCount; ++i) {
    const PORT_PIN pin = capTouchPins[i];

    PORT_PinPeripheralFunctionConfig(pin, (PERIPHERAL_FUNCTION)0x01);
  }
}

static bool touchTimerElapsed;

void capTouchRtcHandler(RTC_TIMER32_INT_MASK intCause, uintptr_t context )
{
  touchTimerElapsed = true;

  rtc_cb(intCause, context);
}

void capTouchInit() {
  // Harmony touch library function `touch_init` called by SYS_Initialize, which
  // sets up the RTC timer with a handler that just sets a flag.  Lots of its
  // state is static so we can't affect it, but its rtc callback function is
  // public.  We steal the RTC event to set our own flag that it's time to do
  // touch, then we call its handler so it can set its internal flag.  We don't
  // call its task that starts the capture until it is safe to do so.  This way
  // we don't need any modifications to the MCC generated touch library, and can
  // still coordinate capTouch with pieceId and LED systems

  // Touch doesn't use its context at the moment, but double-check this if the library is updated
  RTC_Timer32CallbackRegister(capTouchRtcHandler, 0);

  nextCapSendMs = getCurrentTimeMs() + capSendIntervalMs;
}

static bool capTouchRunning;

bool capTouchReady() {
  return touchTimerElapsed;
}

bool capTouchInProgress() {
  return capTouchRunning;
}

void capTouchStart() {
  capTouchRunning = true;
  touchTimerElapsed = false;
  configPinsForCapTouch();
}

void capTouchTask() {
  if (!capTouchRunning) {
    return;
  }

  // Harmony touch library processing - don't call until `capTouchStart` has been called
  // so that conflicts with other systems (LEDs, pieceId) can be managed.
  touch_process();

  if (isCalibrating()) {
    return;
  }

  if (sendCalibrationStatusWhenFinished) {
    hubLog('I', "End node cal");
    sendCalibrationStatusWhenFinished = false;
    sendCalibrationFinished();
  }

  // Harmony touch library global flag
  if (measurement_done_touch) {
    // Copy signal values into local storage and trigger new read
    for (unsigned i = 0; i < DEF_NUM_SENSORS; i++) {
      sens[i] = get_sensor_node_signal(i);
    }

    capTouchRunning = false;

    // Clear flag so we can detect the next update.  The readings
    // are automatically triggered by a timer (and limited by the
    // `allowTouch` function, currently in main.c)
    measurement_done_touch = 0;
  }

  uint32_t now = getCurrentTimeMs();
  if (now >= nextCapSendMs) {
    nextCapSendMs += capSendIntervalMs;
    sendCapData();
  }

  // If this call is undefined or fails to link, the touch
  // library has been overwritten by MCC.  Redo our custom
  // hacks to that library.
  //touchGenerateCanary();
}

void capTouchStartCalibration() {
  hubLog('I', "Start node cal");
  for (unsigned i = 0; i < DEF_NUM_SENSORS; i++) {
    calibrate_node(i);
  }

  sendCalibrationStatusWhenFinished = true;
}

void capTouchSendCCVals() {
  sendGenericStart(TOUCH_CC_VALS, DEF_NUM_SENSORS * sizeof(uint16_t));

  for(unsigned i = 0; i < DEF_NUM_SENSORS; ++i) {
    uint16_t val = get_sensor_cc_val(i);
    sendGenericData(&val, sizeof(val));
  }

  sendGenericDone();
}

void capTouchSetCCVals(const uint8_t * ccData, size_t size) {
  size_t count = size / sizeof(uint16_t);

  if (count > DEF_NUM_SENSORS) {
    count = DEF_NUM_SENSORS;
  }

  for(unsigned i = 0; i < count; ++i) {
    uint16_t val = load_u16(&ccData[i * sizeof(uint16_t)]);
    update_sensor_cc_val(i, val);
  }

  hubLog('I', "Applied %u CC values", (unsigned)count);
}
