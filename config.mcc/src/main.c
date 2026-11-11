#include <stddef.h>                     // Defines NULL
#include <stdbool.h>                    // Defines true
#include <stdlib.h>                     // Defines EXIT_FAILURE
#include "bl_als.hpp"
#include "bl_comm.hpp"
#include "bl_piece_id.hpp"
#include "bl_shu_stats.hpp"
#include "bl_touch.hpp"
#include "bl_uart_dma.hpp"
#include "definitions.h"                // SYS function prototypes
#include "hub_log.h"
#include "timer.h"
#include "leds.h"
#include <inttypes.h>

#if 0
void sendFirmwareVersion(void){
    static const uint8_t verData[3] = {SENSOR_HUB_VERSION_MAJOR, SENSOR_HUB_VERSION_MINOR, SENSOR_HUB_VERSION_MICRO};
    sendGeneric(NRF52_FW_VER, sizeof(verData), verData);
}
#endif

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

bool canRunAls(bool pieceIdActive) {
  // ALS and PieceID both use the ADC
  return !pieceIdActive;
}

bool canStartPieceId(bool alsActive, bool capTouchActive) {
  // ALS and PieceID both use the ADC
  // CapTouch lines shared with PieceID mux control lines; can't do both at once
  return !alsActive && !capTouchActive;
}

bool canStartLEDs(bool capTouchActive) {
  // LED signals interfere with captouch (LEDs can be on, but the SPI signals
  // when updating cause interference)
  return !capTouchActive;
}

bool canStartCapTouch(bool pieceIdActive, bool ledsActive) {
  // LED signals interfere with captouch
  // CapTouch lines shared with PieceID mux control lines; can't do both at once
  return !pieceIdActive && !ledsActive;
}

void startReadyTasks() {
  const uint32_t now = getCurrentTimeMs();

  if (alsReady(now) && canRunAls(pieceIdInProgress())) {
    alsStart();
  }

  if (ledReady(now) && canStartLEDs(capTouchInProgress())) {
    ledStart();
  }

  if (capTouchReady() && canStartCapTouch(pieceIdInProgress(), ledTransmitInProgress())) {
    capTouchStart();
  }

  if (pieceIdReady(now) && canStartPieceId(alsReadInProgress(), capTouchInProgress())) {
    pieceIdStart();
  }
}

uint32_t nextPrintMs = 0;
uint32_t printIntervalMs = 3000;

int main ( void )
{
    /* Initialize all modules */
    SYS_Initialize ( NULL );

    signalStateInit();

    timerInit();

    bl_uart_dma_rx_init();
    bl_uart_dma_tx_init();

    commInit();

    shuStatsInit();

    alsInit();
    ledInit();
    capTouchInit();
    pieceIdInit();

    // Note: WDT is configured by fuse bits; enable/period/etc NOT
    // configurable via API
    WDT_Clear();

    while ( true )
    {
        uint32_t now = getCurrentTimeMs();

        /* Maintain state machines of all polled MPLAB Harmony modules. */
        SYS_Tasks ( );

        commTask();

        startReadyTasks();

        alsTask();
        ledTask();
        capTouchTask();
        pieceIdTask();

        shuStatsTask(now);

        WDT_Clear();

        if (now >= nextPrintMs) {
          nextPrintMs += printIntervalMs;
          extern uint32_t ledMsgCount;
          hubLog('I', "LedMessages: %u", ledMsgCount);
        }
    }

    /* Execution should not come here during normal operation */

    return ( EXIT_FAILURE );
}
