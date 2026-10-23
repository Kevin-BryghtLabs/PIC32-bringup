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
#include "bl_touch.h"
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

static void sendTestData(uint16_t val, uint16_t cc){
  uint16_t buffer[2];
  buffer[0] = val;
  buffer[1] = cc;
  sendGeneric(UI_ADVANCE, sizeof(buffer), buffer);
}

// Hacked into Harmony library touch behavior so we can detect
// when touch is running
bool touchInProgress();

bool doLeds = true;
bool doSync = true;

bool allowTouch(void) {
  return !doSync || !ledTransmitInProgress();
}

int main ( void )
{
    /* Initialize all modules */
    SYS_Initialize ( NULL );

    signalStateInit();

    timerInit();

    ADC0_Enable();
    ADC0_ChannelSelect(ADC_INPUTCTRL_MUXPOS_AIN0, ADC_INPUTCTRL_MUXNEG_GND);

    ledInit();

    touchInit();

    commStartup();

    //TCC0_PWMStart();

    //initPieceId();
    //startPieceId();

    uint32_t switchInterval = 10000;
    uint32_t nextSwitchMs = getCurrentTimeMs() + switchInterval;

    uint32_t pidInterval = 1000;
    uint32_t nextPidMs = getCurrentTimeMs() + pidInterval;

    uint32_t alsInterval = 250;
    uint32_t nextAlsMs = getCurrentTimeMs() + alsInterval;

    while ( true )
    {
        /* Maintain state machines of all polled MPLAB Harmony modules. */
        SYS_Tasks ( );

        commProcess();

#if 0
        // Wait until CapTouch is not running to start next LED transmit
        if (ledTaskShouldRun() && (!doSync || !touchInProgress())) {
          if (doLeds) {
            ledTask();
          }
        }

        touchTask();
#endif

        uint32_t now = getCurrentTimeMs();
        if (now >= nextSwitchMs) {
          nextSwitchMs += switchInterval;
          //touchSendCCVals();
          //doLeds = !doLeds;
          //doSync = !doSync;
          //hubLog('I', "Sync: %u", doSync);
        }

        if (now >= nextPidMs) {
          nextPidMs += pidInterval;

          //Start interrogation
          //adc_done = false;

          switchPieceId();

          //volatile uint32_t count0 = 0;
          //volatile uint32_t count1 = 0;

          //Wait for result
          //while(!adc_done){
            //asm volatile ("nop");
          //};

          //uint8_t data = ADC0_ConversionResultGet();
          //(void)data;
          //sendGenericData1(data);
        }
#if 0
        if (now >= nextAlsMs) {
          nextAlsMs += alsInterval;
          if (ADC0_ConversionSequenceIsFinished()) {
            uint16_t val = ADC0_ConversionResultGet();
            sendGeneric(ALS_VALUE, sizeof(val), &val);
            ADC0_ConversionStart();
          }
        }
#endif

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

