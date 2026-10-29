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
#include "hub_log.h"
#include "timer.h"
#include "leds.h"
//#include "bl_ble_packet_headers.h"
//#include "bl_sensor_hub_firmware_version.h"
#include "bl_uart.h"
#include <inttypes.h>

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

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
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

//Packet header PIECE_ID_DATA carriers multiples of this structure.
typedef struct
{
    /*
    TODO: remove header2. header2 is a copy of the regular packet header,
    because November touch and associated USB<>BLE adapter
    use 2-byte headers, while everything else uses 1B size, 1B header.
    */
    uint8_t header2;//PIECE_ID_DATA
    uint8_t square;//also not PIECE_ID_DATA.
    uint16_t new_tones;//bitfield showing which were updated this scan

    //Raw ADC values per rank/file/tone
    uint8_t data[/*PIECE_ID_TONES*/12];
} piece_id_packet_t;

void buildPidScanTable(uint32_t startFreq, uint32_t endFreq, uint32_t stepFreq, uint16_t delay, uint16_t ring) {
  unsigned count = 0;

  for (uint32_t f = startFreq; f <= endFreq; f += stepFreq, count++) {
    pieceIdSetQuiet(true);
    pieceIdSetFrequencyParams(count, f, delay, ring);
  }

  pieceIdSetFrequencyCount(count);
}

void buildDelayScanTable(uint16_t startDelay, uint16_t endDelay, uint16_t stepDelay, uint32_t freq, uint16_t ring) {
  unsigned count = 0;

  for (uint16_t d = startDelay; d <= endDelay; d += stepDelay, count++) {
    pieceIdSetQuiet(true);
    pieceIdSetFrequencyParams(count, freq, d, ring);
  }

  pieceIdSetFrequencyCount(count);
}

void buildRingScanTable(uint16_t startRing, uint16_t endRing, uint16_t stepRing, uint32_t freq, uint16_t delay) {
  unsigned count = 0;

  for (uint16_t r = startRing; r <= endRing; r += stepRing, count++) {
    pieceIdSetQuiet(true);
    pieceIdSetFrequencyParams(count, freq, delay, r);
  }

  pieceIdSetFrequencyCount(count);
}

enum Phase {
  COARSE_FREQ,
  COARSE_DELAY,
  COARSE_RING,
  FINE_FREQ,
  FINE_DELAY,
  FINE_RING,
  PERFECT_FREQ,

  DONE
};

static const uint16_t defaultDelay = 50;
static const uint16_t defaultRing = 30;
bool pieceScanActive;
enum Phase pieceScanPhase;
uint8_t scanSpots[] = { 15, 0 };
uint16_t pieceScanResults[ARRAY_SIZE(scanSpots)][40];
uint32_t bestFreq = 0;
uint32_t bestDelay = 0;
uint32_t bestRing = 0;

unsigned scanSpotIdx = 0;

void pieceScanStartPhase() {
  pieceScanActive = true;
  scanSpotIdx = 0;

  switch (pieceScanPhase) {
    case COARSE_FREQ:
      hubLog('I', "** COARSE_FREQ **");
      buildPidScanTable(1000000, 4000000, 250000, defaultDelay, defaultRing);
      startPieceId(scanSpots[0]);
      break;

    case COARSE_DELAY:
      hubLog('I', "** COARSE_DELAY **");
      buildDelayScanTable(5, 100, 5, bestFreq, defaultRing);
      startPieceId(scanSpots[0]);
      break;

    case COARSE_RING:
      hubLog('I', "** COARSE_RING **");
      buildRingScanTable(5, 100, 5, bestFreq, bestDelay);
      startPieceId(scanSpots[0]);
      break;

    case FINE_FREQ:
      hubLog('I', "** FINE_FREQ **");
      buildPidScanTable(bestFreq - 250000, bestFreq + 250000, 50000, bestDelay, bestRing);
      startPieceId(scanSpots[0]);
      break;

    case FINE_DELAY:
      hubLog('I', "** FINE_DELAY **");
      uint16_t delayStart = bestDelay <= 10 ? 1 : bestDelay - 10;
      buildDelayScanTable(delayStart, bestDelay + 10, 1, bestFreq, bestRing);
      startPieceId(scanSpots[0]);
      break;

    case FINE_RING:
      hubLog('I', "** FINE_RING **");
      uint16_t ringStart = bestRing <= 10 ? 1 : bestRing - 10;
      buildRingScanTable(ringStart, bestRing + 10, 1, bestFreq, bestDelay);
      startPieceId(scanSpots[0]);
      break;

    case PERFECT_FREQ:
      hubLog('I', "** PERFECT_FREQ **");
      buildPidScanTable(bestFreq - 50000, bestFreq + 50000, 10000, bestDelay, bestRing);
      startPieceId(scanSpots[0]);
      break;

    case DONE:
      pieceScanActive = false;
  }
}

void pieceScanStart() {
  pieceScanPhase = COARSE_FREQ;
  pieceScanStartPhase();
}

char buffer[128];
int16_t deltas[40];

void pieceScanComplete() {
  const unsigned count = pieceIdGetFrequencyCount();
  unsigned bestIdx = 0;
  int16_t bestDelta = 0;
  unsigned bestSigIdx = 0;
  int16_t bestSigDelta = 0;

  for (unsigned i = 0; i < count && i < 40; i++) {
    deltas[i] = pieceScanResults[1][i] - pieceScanResults[0][i];
    if (deltas[i] > bestDelta) {
      bestIdx = i;
      bestDelta = deltas[i];
    }
    if (deltas[i] > (bestSigDelta * (100 + (i - bestSigIdx)) / 100)) {
      bestSigIdx = i;
      bestSigDelta = deltas[i];
    }
  }

  for (unsigned s = 0; s < ARRAY_SIZE(scanSpots); s++) {
    unsigned pos = 0;
    for (unsigned i = 0; i < count; ++i) {
      pos += snprintf(&buffer[pos], sizeof(buffer) - pos, " %3u", pieceScanResults[s][i]);
    }
    hubLog('I', "Spot %2u:%s", scanSpots[s], buffer);
  }

  unsigned pos = 0;
  for (unsigned i = 0; i < count; ++i) {
    pos += snprintf(&buffer[pos], sizeof(buffer) - pos, " %+3" PRIi16, deltas[i]);
  }
  hubLog('I', "Deltas: %s", buffer);

  switch (pieceScanPhase) {
    case COARSE_FREQ:
    case FINE_FREQ:
    case PERFECT_FREQ:
      hubLog('I', "Best: [%u] %7u Hz  %4" PRIu32 " DD %4" PRIu32 " RT", bestIdx, pieceIdGetFrequency(bestIdx), pieceIdGetDelay(bestIdx), pieceIdGetRing(bestIdx));
      bestFreq = pieceIdGetFrequency(bestIdx);
      pieceScanPhase++;
      pieceScanStartPhase();
      break;

    case COARSE_DELAY:
    case FINE_DELAY:
      hubLog('I', "Best: [%u] %7u Hz  %4" PRIu32 " DD %4" PRIu32 " RT", bestIdx, pieceIdGetFrequency(bestIdx), pieceIdGetDelay(bestIdx), pieceIdGetRing(bestIdx));
      bestDelay = pieceIdGetDelay(bestIdx);
      pieceScanPhase++;
      pieceScanStartPhase();
      break;

    case COARSE_RING:
    case FINE_RING:
      hubLog('I', "BestSig: [%u] %7u Hz  %4" PRIu32 " DD %4" PRIu32 " RT", bestSigIdx, pieceIdGetFrequency(bestSigIdx), pieceIdGetDelay(bestSigIdx), pieceIdGetRing(bestSigIdx));
      bestRing = pieceIdGetRing(bestSigIdx);
      pieceScanPhase++;
      pieceScanStartPhase();
      break;

    case DONE:
      hubLog('E', "Should not have run a 'DONE' phase");
      break;
  }
}

#define DO_SCAN 1

void pieceScanTask() {
  if (!pieceScanActive) {
    return;
  }

  pieceIdTask();
  if (pieceIdComplete()) {
    const uint16_t *res;
    size_t resCount;
    getPieceIdResult(&res, &resCount);
    memcpy(pieceScanResults[scanSpotIdx], res, resCount * sizeof(res[0]));
    scanSpotIdx++;

    if (scanSpotIdx < ARRAY_SIZE(scanSpots)) {
      startPieceId(scanSpots[scanSpotIdx]);
    }
    else {
      pieceScanActive = false;
      pieceScanComplete();
    }
  }
}

int main ( void )
{
    /* Initialize all modules */
    SYS_Initialize ( NULL );

    signalStateInit();

    timerInit();

    ledInit();

    touchInit();

    commStartup();

    //TCC0_PWMStart();

    initPieceId();
    //startPieceId();

    uint32_t switchInterval = 10000;
    uint32_t nextSwitchMs = getCurrentTimeMs() + switchInterval;

    uint32_t pidInterval = 2000;
    uint32_t nextPidMs = getCurrentTimeMs() + pidInterval;

    uint32_t pidLogInterval = 1000;
    uint32_t nextPidLogMs = getCurrentTimeMs() + pidLogInterval;

    uint32_t alsInterval = 250;
    uint32_t nextAlsMs = getCurrentTimeMs() + alsInterval;

    unsigned spot = 0;
    bool triggered = 0;

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

          // When we are doing both captouch and pieceID this needs to be called
          // when switching to start pieceID
          setupPieceId();

          //switchPieceId(spot);
#if (DO_SCAN)
          pieceScanStart();
#else
          startPieceId(spot);
          triggered = true;
#endif
        }

#if (DO_SCAN)
        pieceScanTask();
#else
        pieceIdTask();

        if (triggered) {
          if (pieceIdComplete()) {
            triggered = false;
            const uint16_t * pieceIdResults;
            size_t resultCount;

            if (getPieceIdResult(&pieceIdResults, &resultCount)) {
              char buffer[256];
              unsigned pos = 0;
              buffer[0] = 0;

              if (now >= nextPidLogMs) {
                nextPidLogMs += pidLogInterval;
                for (unsigned i = 0; i < resultCount; ++i) {
                  pos += snprintf(&buffer[pos], sizeof(buffer) - pos, " %4u", pieceIdResults[i]);
                }
                hubLog('I', "Read:%s", buffer);
              }

              piece_id_packet_t packet;
              packet.header2 = PIECE_ID_DATA;
              packet.square = 0;
              packet.new_tones = 0x0fff;
              for (unsigned i = 0; i < 12; i++) {
                packet.data[i] = pieceIdResults[i];
              }

              sendGeneric(PIECE_ID_DATA, sizeof(packet), &packet);
            }
          }
        }
#endif
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

        //delay(1);
    }

    /* Execution should not come here during normal operation */

    return ( EXIT_FAILURE );
}


/*******************************************************************************
 End of File
*/

