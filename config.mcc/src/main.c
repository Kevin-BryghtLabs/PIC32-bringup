#include <stddef.h>                     // Defines NULL
#include <stdbool.h>                    // Defines true
#include <stdlib.h>                     // Defines EXIT_FAILURE
#include "bl_als.hpp"
#include "bl_ble_parser.h"
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

void signalStateInit(void) {
    // Leave levels unchanged from bootloader
    PSPARE1_OutputEnable();
    PSPARE2_OutputEnable();
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

static void wdtEnable() {
  // Cycles are of the 1024KHz clock, so essentially 1ms
  WDT_TimeoutPeriodSet(WDT_CONFIG_PER_CYC2048_Val);
  WDT_Enable();
}

int main ( void )
{
    /* Initialize all modules */
    SYS_Initialize ( NULL );

    signalStateInit();

    timerInit();

    bl_uart_dma_rx_init();
    bl_uart_dma_tx_init();

    // This takes a long time to send 300 zeros;
    // configure WDT accordingly (or not at all before calling)
    commInit();
    sendStartupMessage();

    shuStatsInit();

    alsInit();
    ledInit();
    capTouchInit();

    // This takes a long time to calculate the parameters for all frequencies;
    // configure WDT accordingly (or not at all before calling)
    pieceIdInit();

    wdtEnable();

    while ( true )
    {
        uint32_t loopTimeMs = getCurrentTimeMs();

        /* Maintain state machines of all polled MPLAB Harmony modules. */
        SYS_Tasks ( );

        commTask();

        startReadyTasks();

        alsTask();
        ledTask();
        capTouchTask();
        pieceIdTask();

        shuStatsTask(loopTimeMs);

        if (loopTimeMs >= nextPrintMs) {
          nextPrintMs += printIntervalMs;
          extern uint32_t ledMsgCount;
          hubLog('I', "LedMessages: %u", ledMsgCount);
        }

        shuStatsAddSample(Stat_TaskLoop, getCurrentTimeMs() - loopTimeMs);
        WDT_Clear();
    }

    /* Execution should not come here during normal operation */

    return ( EXIT_FAILURE );
}
