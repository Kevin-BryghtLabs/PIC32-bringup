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

#include <stddef.h>                     // Defines NULL
#include <stdbool.h>                    // Defines true
#include <stdlib.h>                     // Defines EXIT_FAILURE
#include "bl_ble_parser.h"
#include "bl_piece_id.hpp"
#include "definitions.h"                // SYS function prototypes
#include "timer.h"
#include "bl_ble_packet_headers.h"
#include "bl_sensor_hub_firmware_version.h"
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

void spiTest() {
  // LED zero code:  ^___
  // LED one  code:  ^^^_
  // - each char is 0.3us
  // Reset is 80us of low

#define LED_00  0x88
#define LED_01  0x8E
#define LED_10  0xE8
#define LED_11  0xEE

#define WHITE  LED_11, LED_11, LED_11, LED_11, LED_11, LED_11, LED_11, LED_11, LED_11, LED_11, LED_11, LED_11
#define GREEN  LED_11, LED_11, LED_11, LED_11, LED_00, LED_00, LED_00, LED_00, LED_00, LED_00, LED_00, LED_00
#define RED    LED_00, LED_00, LED_00, LED_00, LED_11, LED_11, LED_11, LED_11, LED_00, LED_00, LED_00, LED_00
#define BLUE   LED_00, LED_00, LED_00, LED_00, LED_00, LED_00, LED_00, LED_00, LED_11, LED_11, LED_11, LED_11
#define OFF    LED_00, LED_00, LED_00, LED_00, LED_00, LED_00, LED_00, LED_00, LED_00, LED_00, LED_00, LED_00

// Each byte is 2.4us.  80 / 2.4 = 33.3333 bytes for RESET
#define RESET \
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
  0x00, 0x00

// End with line high to not accidentally reset the LEDs
//   - TODO: switch to GPIO and pull high(?)
//#define LED_END       0xFF

  // two bits payload per byte, 24 bits of color = 12 bytes of data per LED
  static const uint8_t onLights[] = {
    WHITE, RED, GREEN, BLUE,
  };

  static const uint8_t offLights[] = {
    OFF, OFF, OFF, OFF,
  };

  static const uint8_t reset[] = {
    RESET
  };

  static const uint8_t greenOnly[] = {
    GREEN
  };

  static const uint8_t oneLowByte[] = {
    0x00
  };

  static const unsigned delayLoops = 50;
  static const unsigned cycles = 16;
  static unsigned delay = 0;
  static unsigned currentCycle = 0;

  LED_CH_EN_Set();
  if (delay++ >= delayLoops) {
    delay = 0;
    currentCycle++;
    if (currentCycle >= cycles) {
      currentCycle = 0;
    }
  }

#if 0
  for (unsigned i = 0; i < cycles; ++i) {
    if (i == currentCycle) {
      SERCOM1_SPI_WriteRead(onLights, sizeof(onLights), NULL, 0);
    }
    else {
      SERCOM1_SPI_WriteRead(offLights, sizeof(offLights), NULL, 0);
    }
  }
  SERCOM1_SPI_WriteRead(reset, sizeof(reset), NULL, 0);
#else
  SERCOM1_SPI_WriteRead(greenOnly, sizeof(greenOnly), NULL, 0);
  SERCOM1_SPI_WriteRead(oneLowByte, sizeof(oneLowByte), NULL, 0);
#endif
}

int main ( void )
{
    /* Initialize all modules */
    SYS_Initialize ( NULL );

    signalStateInit();

    timerInit();

    ADC0_Enable();

    commStartup();

    //TCC0_PWMStart();

    initPieceId();
    startPieceId();

    uint32_t switchInterval = 15000;
    uint32_t nextSwitchMs = getCurrentTimeMs() + switchInterval;

    while ( true )
    {
        /* Maintain state machines of all polled MPLAB Harmony modules. */
        SYS_Tasks ( );

        spiTest();

        commProcess();
        
        uint32_t now = getCurrentTimeMs();
        if (now >= nextSwitchMs) {
          nextSwitchMs += switchInterval;
          switchPieceId();
        }

        delay(1);
    }

    /* Execution should not come here during normal operation */

    return ( EXIT_FAILURE );
}


/*******************************************************************************
 End of File
*/

