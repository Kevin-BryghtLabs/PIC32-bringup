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

// Arbitrary, but should be long enough to set up the next transfer
#define LEDS_PER_BLOCK_TRANSFER       8

// 888 GRB
#define COLOR_BYTES_PER_LED           3

// The LED waveform needs 4 SPI bits to encode 2 payload LED bits
//   8 Total payload bits / 2 payload bits per TX byte = 4 SPI bytes
#define SPI_BYTES_PER_LED_BYTE        4

#define LED_SPI_BLOCK_SIZE    (LEDS_PER_BLOCK_TRANSFER * COLOR_BYTES_PER_LED * SPI_BYTES_PER_LED_BYTE)

static const uint8_t AllGreen[LED_SPI_BLOCK_SIZE] = {
  GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN, GREEN,
};

static const uint8_t AllBlue[LED_SPI_BLOCK_SIZE] = {
  BLUE, BLUE, BLUE, BLUE, BLUE, BLUE, BLUE, BLUE,
};

static const uint8_t AllRed[LED_SPI_BLOCK_SIZE] = {
  RED, RED, RED, RED, RED, RED, RED, RED,
};

static const uint8_t AllWhite[LED_SPI_BLOCK_SIZE] = {
  WHITE, WHITE, WHITE, WHITE, WHITE, WHITE, WHITE, WHITE,
};

static uint8_t ledPayloadBuffer[2][LED_SPI_BLOCK_SIZE];

static const uint8_t ZeroByte = 0;

__attribute__ ((aligned (8))) dmac_descriptor_registers_t dmaDescriptors[5];

#define DMA_SPI_LED_BTCTRL \
   (DMAC_BTCTRL_BEATSIZE_BYTE | DMAC_BTCTRL_BLOCKACT_INT | DMAC_BTCTRL_VALID_Msk | DMAC_BTCTRL_SRCINC_Msk)

#define DMA_SPI_RESET_BTCTRL \
   (DMAC_BTCTRL_BEATSIZE_BYTE | DMAC_BTCTRL_BLOCKACT_INT | DMAC_BTCTRL_VALID_Msk)

static unsigned updateIdx = 0;
static unsigned ledFillIdx = 0;
static unsigned ledSentIdx = 0;
static unsigned nextBuffer = 0;

static void setLedTXPinToGPIO() {
  PORT_REGS->GROUP[0].PORT_OUTCLR = ((uint32_t)1U << 0);
  PORT_REGS->GROUP[0].PORT_DIRSET = ((uint32_t)1U << 0);
  PORT_PinGPIOConfig(PORT_PIN_PA00);
}

static void setLedTXPinToSPI() {
  PORT_PinPeripheralFunctionConfig(PORT_PIN_PA00, 0x03);
}

enum {
  COLOR_OFF,
  COLOR_RED,
  COLOR_GREEN,
  COLOR_BLUE,
  COLOR_CYAN,
  COLOR_MAGENTA,
  COLOR_YELLOW,
  COLOR_WHITE,

  COLOR_COUNT
};

static uint8_t palette[COLOR_COUNT][3] = {
  [COLOR_OFF]       = { 0x00, 0x00, 0x00 },
  [COLOR_RED]       = { 0x55, 0x00, 0x00 },
  [COLOR_GREEN]     = { 0x11, 0x55, 0x00 },
  [COLOR_BLUE]      = { 0x00, 0x00, 0x55 },
  [COLOR_CYAN]      = { 0x00, 0x55, 0x55 },
  [COLOR_MAGENTA]   = { 0x55, 0x00, 0x55 },
  [COLOR_YELLOW]    = { 0x55, 0x55, 0x00 },
  [COLOR_WHITE]     = { 0x55, 0x55, 0x55 },
};

static const uint8_t bitPairTxPattern[] = {
  [0] = LED_00,
  [1] = LED_01,
  [2] = LED_10,
  [3] = LED_11,
};

static uint8_t ledValues[64][3];

static void fillSPIBuffer(unsigned startIdx, uint8_t * spiBuffer) {
  for (unsigned i = 0; i < LEDS_PER_BLOCK_TRANSFER; ++i) {
    const uint8_t * colorVals = ledValues[startIdx++];

    *spiBuffer++ = bitPairTxPattern[(colorVals[1] >> 6) & 0x03];
    *spiBuffer++ = bitPairTxPattern[(colorVals[1] >> 4) & 0x03];
    *spiBuffer++ = bitPairTxPattern[(colorVals[1] >> 2) & 0x03];
    *spiBuffer++ = bitPairTxPattern[(colorVals[1] >> 0) & 0x03];
    *spiBuffer++ = bitPairTxPattern[(colorVals[0] >> 6) & 0x03];
    *spiBuffer++ = bitPairTxPattern[(colorVals[0] >> 4) & 0x03];
    *spiBuffer++ = bitPairTxPattern[(colorVals[0] >> 2) & 0x03];
    *spiBuffer++ = bitPairTxPattern[(colorVals[0] >> 0) & 0x03];
    *spiBuffer++ = bitPairTxPattern[(colorVals[2] >> 6) & 0x03];
    *spiBuffer++ = bitPairTxPattern[(colorVals[2] >> 4) & 0x03];
    *spiBuffer++ = bitPairTxPattern[(colorVals[2] >> 2) & 0x03];
    *spiBuffer++ = bitPairTxPattern[(colorVals[2] >> 0) & 0x03];
  }
}

static bool ledDirty = false;

void ledUpdate(const uint8_t * data) {
  memcpy(ledValues, data, sizeof(ledValues));
  ledDirty = true;
}

typedef enum DMAStates {
  STATE_START,
  STATE_TRANSFER,
  STATE_END
} DMAStates;

static DMAStates dmaState;
static volatile bool LedDMABusy = false;

static void dmaBlockDone(DMAC_TRANSFER_EVENT event, uintptr_t contextHandle) {
  if (event == DMAC_TRANSFER_EVENT_COMPLETE) {
    switch (dmaState) {
      case STATE_START:
        dmaState = STATE_TRANSFER;
        break;

      case STATE_TRANSFER:
        ledSentIdx += LEDS_PER_BLOCK_TRANSFER;
        if (ledSentIdx >= 64) {
          // We have just sent the final transfer.  The next interrupt will be
          // the one after the "end" transfer of zeros while we turn
          dmaState = STATE_END;
        }
        else {
          fillSPIBuffer(ledFillIdx, ledPayloadBuffer[nextBuffer]);

          ledFillIdx += LEDS_PER_BLOCK_TRANSFER;
          if (ledFillIdx >= 64) {
            // We are filling the final transfer, tell it to transition
            // to the end state after sending
            dmaDescriptors[nextBuffer].DMAC_DESCADDR = (uintptr_t)&dmaDescriptors[2];
          }

          nextBuffer ^= 1;
        }
        break;

      case STATE_END:
        setLedTXPinToGPIO();
        LedDMABusy = false;
        break;
    }

#if 0
    if (updateIdx == 0) {
      // End of warmup period
    }
    else if (updateIdx == 1) {
      memcpy(ledPayloadBuffer0, AllBlue, LED_SPI_BLOCK_SIZE);
    }
    else if (updateIdx == 2) {
      memcpy(ledPayloadBuffer1, AllWhite, LED_SPI_BLOCK_SIZE);
    }
    else if (updateIdx == 3) {
      // SPI leaves this pin idling high which makes LEDs sad.  Switch
      // to a GPIO during the final transfer so we can leave it low.
      setLedTXPinToGPIO();
    }
    updateIdx++;
#endif
  }

}

void sendLedData() {
  // LED zero code:  ^___
  // LED one  code:  ^^^_
  // - each char is 0.3us
  // Reset is 80us of low

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

  static const unsigned delayLoops = 5000;
  static const unsigned cycles = 16;
  static unsigned delay = 0;
  static unsigned currentCycle = 0;

  dmaState = STATE_START;
  //updateIdx = 0;
  ledSentIdx = 0;
  nextBuffer = 0;

  fillSPIBuffer(LEDS_PER_BLOCK_TRANSFER * 0, ledPayloadBuffer[0]);
  fillSPIBuffer(LEDS_PER_BLOCK_TRANSFER * 1, ledPayloadBuffer[1]);
  ledFillIdx = LEDS_PER_BLOCK_TRANSFER * 2;

  LED_CH_EN_Set();
  DMAC_ChannelCallbackRegister(DMAC_CHANNEL_0, dmaBlockDone, 0);
  DMAC_LinkedListDescriptorSetup(&dmaDescriptors[0],
      DMA_SPI_LED_BTCTRL,
      ledPayloadBuffer[0],
      (void *)&SERCOM1_REGS->SPIM.SERCOM_DATA,
      sizeof(ledPayloadBuffer[0]),
      &dmaDescriptors[1]);

  DMAC_LinkedListDescriptorSetup(&dmaDescriptors[1],
      DMA_SPI_LED_BTCTRL,
      ledPayloadBuffer[1],
      (void *)&SERCOM1_REGS->SPIM.SERCOM_DATA,
      sizeof(ledPayloadBuffer[1]),
      &dmaDescriptors[0]);

  DMAC_LinkedListDescriptorSetup(&dmaDescriptors[2],
      DMA_SPI_RESET_BTCTRL,
      &ZeroByte,
      (void *)&SERCOM1_REGS->SPIM.SERCOM_DATA,
      16,
      NULL);

  DMAC_LinkedListDescriptorSetup(&dmaDescriptors[3],
      DMA_SPI_RESET_BTCTRL,
      &ZeroByte,
      (void *)&SERCOM1_REGS->SPIM.SERCOM_DATA,
      16,
      &dmaDescriptors[0]);

#if 0
  DMAC_LinkedListDescriptorSetup(&dmaDescriptors[2],
      DMA_SPI_LED_BTCTRL,
      ledPayloadBuffer0,
      (void *)&SERCOM1_REGS->SPIM.SERCOM_DATA,
      sizeof(ledPayloadBuffer0),
      &dmaDescriptors[3]);

  DMAC_LinkedListDescriptorSetup(&dmaDescriptors[3],
      DMA_SPI_LED_BTCTRL,
      ledPayloadBuffer[1],
      (void *)&SERCOM1_REGS->SPIM.SERCOM_DATA,
      sizeof(ledPayloadBuffer1),
      NULL);
#endif

  LedDMABusy = true;

  // Start transmitting a low level so SPI will drive it low
  SERCOM1_REGS->SPIM.SERCOM_DATA = 0,

  // Set up DMA to start transfer of buffers when the first byte completes
  DMAC_ChannelLinkedListTransfer(DMAC_CHANNEL_0, &dmaDescriptors[3]);

  // SPI should be driving the correct low signal by now; switch pin mux
  setLedTXPinToSPI();

  while (LedDMABusy) {
  }

  ledDirty = false;

  //DMAC_ChannelTransfer(DMAC_CHANNEL_0, ledPayloadBuffer, (void *)&SERCOM1_REGS->SPIM.SERCOM_DATA, sizeof(ledPayloadBuffer));
  return;

#if 0
  if (delay++ >= delayLoops) {
    delay = 0;

    DMAC_ChannelTransfer(DMAC_CHANNEL_0, ledPayloadBuffer, (void *)&SERCOM1_REGS->SPIM.SERCOM_DATA, sizeof(ledPayloadBuffer));
#if 0
    currentCycle++;
    if (currentCycle >= cycles) {
      currentCycle = 0;
    }
#endif
  }
#endif

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
  //SERCOM1_SPI_WriteRead(greenOnly, sizeof(greenOnly), NULL, 0);
  //SERCOM1_SPI_WriteRead(oneLowByte, sizeof(oneLowByte), NULL, 0);
#endif
}

static void capCal(void) {
  for (unsigned i = 0; i < DEF_NUM_CHANNELS; i++) {
    calibrate_node(i);
  }
}

uint16_t sens[DEF_NUM_CHANNELS];

static void sendTestData(uint16_t val, uint16_t cc){
  uint16_t buffer[2];
  buffer[0] = val;
  buffer[1] = cc;
  sendGeneric(UI_ADVANCE, sizeof(buffer), buffer);
}

static void readCap(void) {
  touch_process();
  if (measurement_done_touch) {
    for (unsigned i = 0; i < DEF_NUM_CHANNELS; i++) {
      sens[i] = get_sensor_node_signal(i);
    }
    sendGeneric(SEND_DATA, sizeof(sens), sens);
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

    uint32_t switchInterval = 5000;
    uint32_t nextSwitchMs = getCurrentTimeMs() + switchInterval;

    uint32_t capInterval = 1000;
    uint32_t nextCapMs = getCurrentTimeMs() + capInterval;

    while ( true )
    {
        /* Maintain state machines of all polled MPLAB Harmony modules. */
        SYS_Tasks ( );

        commProcess();

        if (ledDirty) {
          sendLedData();
        }
        else {
          readCap();
        }

        uint32_t now = getCurrentTimeMs();
        if (now >= nextSwitchMs) {
          nextSwitchMs += switchInterval;
          switchPieceId();
          //spiTest();
        }
        if (now >= nextCapMs) {
          nextCapMs += capInterval;
          //spiTest();
        }

        delay(1);
    }

    /* Execution should not come here during normal operation */

    return ( EXIT_FAILURE );
}


/*******************************************************************************
 End of File
*/

