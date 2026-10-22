#include "bl_touch.h"

#include <assert.h>
#include <stdbool.h>
#include "bl_ble_packet_headers.h"
#include "bl_uart.h"
#include "bl_util.h"
#include "definitions.h"
#include "hub_log.h"
#include "timer.h"

static const uint8_t sensIdxMap[DEF_NUM_SENSORS] = {
  // Chessboard is transposed
   0,  8, 16, 24, 32, 40, 48, 56,
   1,  9, 17, 25, 33, 41, 49, 57,
   2, 10, 18, 26, 34, 42, 50, 58,
   3, 11, 19, 27, 35, 43, 51, 59,
   4, 12, 20, 28, 36, 44, 52, 60,
   5, 13, 21, 29, 37, 45, 53, 61,
   6, 14, 22, 30, 38, 46, 54, 62,
   7, 15, 23, 31, 39, 47, 55, 63,

   // Others are fine
   64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76,
};
static_assert(sizeof(sensIdxMap) == DEF_NUM_SENSORS, "sensIdxMap size incorrect");

static const uint32_t capSendIntervalMs = 50;

static uint32_t nextCapSendMs;
static bool sendCalibrationStatusWhenFinished;
static uint16_t sens[DEF_NUM_SENSORS];

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

void touchInit() {
  nextCapSendMs = getCurrentTimeMs() + capSendIntervalMs;
}

void touchTask() {
  // Harmony touch library processing
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
      sens[sensIdxMap[i]] = get_sensor_node_signal(i);
    }

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
  touchGenerateCanary();
}

void touchStartCalibration() {
  hubLog('I', "Start node cal");
  for (unsigned i = 0; i < DEF_NUM_SENSORS; i++) {
    calibrate_node(i);
  }

  sendCalibrationStatusWhenFinished = true;
}

void touchSendCCVals() {
  sendGenericStart(TOUCH_CC_VALS, DEF_NUM_SENSORS * sizeof(uint16_t));

  for(unsigned i = 0; i < DEF_NUM_SENSORS; ++i) {
    uint16_t val = get_sensor_cc_val(i);
    sendGenericData(&val, sizeof(val));
  }

  sendGenericDone();
}

void touchSetCCVals(const uint8_t * ccData, size_t size) {
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
