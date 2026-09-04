/*******************************************************************************
  Main Source File

  Company:
    Microchip Technology Inc.

  File Name:
    main.c

  Summary:
    This file contains the "main" function for a project.

  Description:
    This file contains the "main" function for a project.  The
    "main" function calls the "SYS_Initialize" function to initialize the state
    machines of all modules in the system
 *******************************************************************************/

// *****************************************************************************
// *****************************************************************************
// Section: Included Files
// *****************************************************************************
// *****************************************************************************

#include <assert.h>
#include <stdarg.h>
#include <stddef.h>                     // Defines NULL
#include <stdbool.h>                    // Defines true
#include <stdio.h>
#include <stdlib.h>                     // Defines EXIT_FAILURE
#include "bl_ble_parser.h"
#include "bl_piece_id.hpp"
#include "definitions.h"                // SYS function prototypes
#include "timer.h"
#include "leds.h"
//#include "bl_ble_packet_headers.h"
//#include "bl_sensor_hub_firmware_version.h"
#include "bl_uart.h"

// *****************************************************************************
// *****************************************************************************
// Section: Main Entry Point
// *****************************************************************************
// *****************************************************************************
//#define SENSOR_HUB_VERSION_MAJOR 0
//#define SENSOR_HUB_VERSION_MINOR 0
//#define SENSOR_HUB_VERSION_MICRO 1

#if 0
void sendFirmwareVersion(void){
    static const uint8_t verData[3] = {SENSOR_HUB_VERSION_MAJOR, SENSOR_HUB_VERSION_MINOR, SENSOR_HUB_VERSION_MICRO};
    sendGeneric(NRF52_FW_VER, sizeof(verData), verData);
}
#endif

enum {
    APP_ADD_FIRST_STATE_HERE = 1,
};

void signalStateInit(void) {
    // Leave levels unchanged from bootloader
    //PSPARE1_OutputEnable();
    PSPARE2_OutputEnable();

    // App keeps PSPARE1 low and sets it low to wiggle PSPARE2, opposite of bootloader
    //PSPARE1_Clear();
    PSPARE2_Set();

    // Wiggle PSPARE2 once with PSPARE1 high to indicate bootloader running
    PSPARE2_Clear();
    PSPARE2_Set();
}

void signalState(int pulses) {
    //PSPARE1_Set();
    for (unsigned i = 0; i < pulses; ++i) {
        PSPARE2_Clear();
        PSPARE2_Set();
    }
    //PSPARE1_Clear();
}

void commStartup() {
  //Wait a bit to ensure the UART lines have come up and other side should be ready.
  for(unsigned i = 0; i < 5; ++i){
    delay(1);
  }

  //Send 300B of zeros to reset the ESP32 parser in the event it is mid-frame
  for(unsigned i = 0; i < 300; ++i){
    sendGenericData1(0x00);
  }

  sendFirmwareVersion();
}

void commProcess() {
    while (processRxFifo()) {
        DecodePacketData(uartRxBuffer, bleDataLength);
    }
}



#if 0
static void sendTestMessage() {
  static uint8_t message[11] = { NRF52_HEADER, sizeof(message)-4, 'H', 'e', 'l', 'l', 'o' };
  static const uint8_t fletcherStart = sizeof(message) - 2;
  static const uint8_t adcStart = 7;

  if (getCurrentTimeMs() >= nextSendTime) {
      uint16_t alsVal = ADC0_ConversionResultGet();

      message[adcStart + 0] = alsVal >> 8;
      message[adcStart + 1] = alsVal & 0xff;

      fletcher_reset();
      fletcher_update(message, sizeof(message) - 2);
      uint16_t csum = fletcher_finish();
      // Little endian
      message[fletcherStart + 0] = csum & 0xFF;
      message[fletcherStart + 1] = csum >> 8;

      SERCOM2_USART_Write(message, sizeof(message));
      nextSendTime += sendIntervalMs;
  }
}
  #endif

static void capCal(void) {
  for (unsigned i = 0; i < DEF_NUM_SENSORS; i++) {
    calibrate_node(i);
  }
}

static bool isCalDone() {
  for(unsigned i = 0; i < DEF_NUM_SENSORS; ++i) {
    if(qtlib_key_data_set1[i].node_data_struct_ptr->node_acq_status & NODE_CAL_MASK) {
      return false;
    }
  }
  return true;
}

uint16_t sens[DEF_NUM_SENSORS];

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
static_assert(sizeof(sensIdxMap) == DEF_NUM_SENSORS);

static void sendTestData(uint16_t val, uint16_t cc){
  uint16_t buffer[2];
  buffer[0] = val;
  buffer[1] = cc;
  sendGeneric(UI_ADVANCE, sizeof(buffer), buffer);
}

static void readCap(void) {
  touch_process();
  if (!isCalDone()) {
    return;
  }

  if (measurement_done_touch) {
    for (unsigned i = 0; i < DEF_NUM_SENSORS; i++) {
      sens[sensIdxMap[i]] = get_sensor_node_signal(i);
    }
    measurement_done_touch = 0;
  }
  //volatile uint16_t sens4 = get_sensor_node_signal(4);
  //volatile uint16_t sens5 = get_sensor_node_signal(5);
  //volatile uint16_t sens6 = get_sensor_node_signal(6);
  //volatile uint16_t sens7 = get_sensor_node_signal(7);
  //volatile uint16_t sens8 = get_sensor_node_signal(8);
  //volatile uint16_t sens9 = get_sensor_node_signal(9);

  //uint16_t test = sens0 + sens1 + sens2 + sens3;
}

void sendCapData() {
  sendGeneric(SEND_DATA, sizeof(sens), sens);
}

bool touchInProgress();

bool doLeds = true;
bool doSync = true;

bool allowTouch(void) {
  return !doSync || !ledTransmitInProgress();
}

int hubLog(char type, const char * fmt, ...) __attribute__((format(printf, 2, 3)));

int hubLog(char type, const char * fmt, ...) {
  // Packet must fit in a uint8_t length
  char buffer[245];

  va_list args;

  va_start(args, fmt);
  int res = vsnprintf(buffer, sizeof(buffer), fmt, args);
  va_end(args);

  if (res >= 0) {
    unsigned length = res;
    if (length >= sizeof(buffer)) {
      length = sizeof(buffer) - 1;
    }
    // Send the null byte
    length += 1;
    sendGenericStart(DEBUG_MESSAGE, length + 1);
    sendGenericData1(type);
    sendGenericData(buffer, length);
    sendGenericDone();
  }
}

int main ( void )
{
    /* Initialize all modules */
    SYS_Initialize ( NULL );

    signalStateInit();

    timerInit();

    ADC0_Enable();

    commStartup();

    capCal();

    //TCC0_PWMStart();

    //initPieceId();
    //startPieceId();

    uint32_t switchInterval = 10000;
    uint32_t nextSwitchMs = getCurrentTimeMs() + switchInterval;

    uint32_t capSendInterval = 50;
    uint32_t nextCapSendMs = getCurrentTimeMs() + capSendInterval;

    while ( true )
    {
        /* Maintain state machines of all polled MPLAB Harmony modules. */
        SYS_Tasks ( );

        commProcess();

        // Wait until CapTouch is not running to start next LED transmit
        if (ledTaskShouldRun() && (!doSync || !touchInProgress())) {
          if (doLeds) {
            ledTask();
          }
        }
        // This just advances the Touch processing; starting a touch is gated
        // on allowTouch()
        readCap();

        uint32_t now = getCurrentTimeMs();
        if (now >= nextCapSendMs) {
          nextCapSendMs += capSendInterval;
          sendCapData();
        }
        if (now >= nextSwitchMs) {
          nextSwitchMs += switchInterval;
          //doLeds = !doLeds;
          //doSync = !doSync;
          //hubLog('I', "Sync: %u", doSync);
        }

        //if (ledDirty) {
          //sendLedData();
        //}
        //else {
        //}

        //uint32_t now = getCurrentTimeMs();
        //if (now >= nextSwitchMs) {
          //nextSwitchMs += switchInterval;
          //switchPieceId();
          //spiTest();
        //}
        //if (now >= nextCapMs) {
          //nextCapMs += capInterval;
          //spiTest();
        //}

        delay(1);
    }

    /* Execution should not come here during normal operation */

    return ( EXIT_FAILURE );
}


/*******************************************************************************
 End of File
*/

