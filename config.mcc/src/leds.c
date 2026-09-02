#include "definitions.h"

// Number of LEDs driven by the PIC32
#define LED_COUNT                     64

// Arbitrary, but should be long enough to set up the next transfer
// but not huge to minimize buffer requriements
#define LEDS_PER_BLOCK_TRANSFER       8

// 888 GRB (3 bytes total)
#define COLOR_BYTES_PER_LED           3

// The LED waveform needs 4 SPI bits to encode 2 payload LED bits
//   8 Total payload bits / 2 payload bits per TX byte = 4 SPI bytes
#define SPI_BYTES_PER_LED_BYTE        4

#define LED_SPI_BLOCK_SIZE    (LEDS_PER_BLOCK_TRANSFER * COLOR_BYTES_PER_LED * SPI_BYTES_PER_LED_BYTE)

// Control word for DMA descriptor that sends LED data
#define DMA_SPI_LED_BTCTRL \
   (DMAC_BTCTRL_BEATSIZE_BYTE | DMAC_BTCTRL_BLOCKACT_INT | DMAC_BTCTRL_VALID_Msk | DMAC_BTCTRL_SRCINC_Msk)

// Control word for DMA descriptor that sends zero values before and after
#define DMA_SPI_RESET_BTCTRL \
   (DMAC_BTCTRL_BEATSIZE_BYTE | DMAC_BTCTRL_BLOCKACT_INT | DMAC_BTCTRL_VALID_Msk)

// Each bit of SPI is 0.3us.  Reset must be >= 80us.
// 80us / 0.3 (us per bit) / 8 (bits / byte) = 33.333
#define RESET_SPI_BYTE_LENGTH         34

// Meanings of the different DMA descriptors
enum {
  DMA_DESC_STARTUP,
  DMA_DESC_BUFFER0,
  DMA_DESC_BUFFER1,

  // LED datasheet uses "Reset" to refer to the low period
  // after all LEDs in a strip have been updated.
  DMA_DESC_RESET,

  DMA_DESC_COUNT
};

// An interrupt occurs at the end of each DMA descriptor.  A state machine
// tracks what each interrupt means
typedef enum DMAStates {
  STATE_START,
  STATE_TRANSFER,
  STATE_END
} DMAStates;

enum {
  RED,
  GREEN,
  BLUE,

  COLOR_COUNT
};

// Color values of all LEDs
static uint8_t ledValues[LED_COUNT][COLOR_COUNT];

// Buffer into which LED data is encoded for transfer
static uint8_t ledPayloadBuffer[2][LED_SPI_BLOCK_SIZE];

// This is the "data" that is sent by the zero-padding DMA transfer
static const uint8_t ZeroByte = 0;

// Index into LED array of the current SPI buffer fill position
static unsigned ledFillIdx = 0;

// Index into LED array of the last LED data fully transmitted
static unsigned ledSentIdx = 0;

// Which SPI buffer we are currently filling for transmit
static unsigned currentBuffer = 0;

// Is LED data pending update to the LEDs themselves
static volatile bool ledDirty = false;

// DMA Descriptors
__attribute__ ((aligned (8))) dmac_descriptor_registers_t dmaDescriptors[DMA_DESC_COUNT];

static DMAStates dmaState;

static volatile bool ledDMABusy = false;

// Single-wire encoding of data to the LEDs.
//   0: HLLL
//   1: HHHL
// Where each unit is 0.3us (total of 1.2us per payload bit).
// Two bits fit in an SPI byte; this is the encoding of each
// two-bit pair.
static const uint8_t bitPairTxPattern[] = {
  [0] = 0x88,
  [1] = 0x8E,
  [2] = 0xE8,
  [3] = 0xEE,
};

static void setLedTXPinToGPIO() {
  PORT_REGS->GROUP[0].PORT_OUTCLR = ((uint32_t)1U << 0);
  PORT_REGS->GROUP[0].PORT_DIRSET = ((uint32_t)1U << 0);
  PORT_PinGPIOConfig(PORT_PIN_PA00);
}

static void setLedTXPinToSPI() {
  PORT_PinPeripheralFunctionConfig(PORT_PIN_PA00, 0x03);
}

static void fillSPIBuffer(unsigned startIdx, uint8_t * spiBuffer) {
  for (unsigned i = 0; i < LEDS_PER_BLOCK_TRANSFER; ++i) {
    const uint8_t * colorVals = ledValues[startIdx++];

    // LED color order is GRB
    *spiBuffer++ = bitPairTxPattern[(colorVals[GREEN] >> 6) & 0x03];
    *spiBuffer++ = bitPairTxPattern[(colorVals[GREEN] >> 4) & 0x03];
    *spiBuffer++ = bitPairTxPattern[(colorVals[GREEN] >> 2) & 0x03];
    *spiBuffer++ = bitPairTxPattern[(colorVals[GREEN] >> 0) & 0x03];

    *spiBuffer++ = bitPairTxPattern[(colorVals[RED]   >> 6) & 0x03];
    *spiBuffer++ = bitPairTxPattern[(colorVals[RED]   >> 4) & 0x03];
    *spiBuffer++ = bitPairTxPattern[(colorVals[RED]   >> 2) & 0x03];
    *spiBuffer++ = bitPairTxPattern[(colorVals[RED]   >> 0) & 0x03];

    *spiBuffer++ = bitPairTxPattern[(colorVals[BLUE]  >> 6) & 0x03];
    *spiBuffer++ = bitPairTxPattern[(colorVals[BLUE]  >> 4) & 0x03];
    *spiBuffer++ = bitPairTxPattern[(colorVals[BLUE]  >> 2) & 0x03];
    *spiBuffer++ = bitPairTxPattern[(colorVals[BLUE]  >> 0) & 0x03];
  }
}

static void dmaBlockDone(DMAC_TRANSFER_EVENT event, uintptr_t contextHandle) {
  if (event == DMAC_TRANSFER_EVENT_COMPLETE) {
    switch (dmaState) {
      case STATE_START:
        dmaState = STATE_TRANSFER;
        break;

      case STATE_TRANSFER:
        ledSentIdx += LEDS_PER_BLOCK_TRANSFER;
        if (ledSentIdx >= LED_COUNT) {
          // We have just sent the final transfer.  The next interrupt will be
          // the one after the "RESET" low-signal transfer
          dmaState = STATE_END;
        }
        else {
          fillSPIBuffer(ledFillIdx, ledPayloadBuffer[currentBuffer]);

          ledFillIdx += LEDS_PER_BLOCK_TRANSFER;
          if (ledFillIdx >= LED_COUNT) {
            // We just filled the final transfer, tell it to transition
            // to the end state after sending
            dmaDescriptors[currentBuffer].DMAC_DESCADDR = (uintptr_t)&dmaDescriptors[DMA_DESC_RESET];
          }

          currentBuffer ^= 1;
        }
        break;

      case STATE_END:
        setLedTXPinToGPIO();
        ledDMABusy = false;
        break;
    }
  }
}

static void sendLedData() {
  // LED zero code:  ^___
  // LED one  code:  ^^^_
  // - each char is 0.3us
  // Reset is 80us of low

  if (ledDMABusy) {
    return;
  }

  ledSentIdx = 0;
  currentBuffer = 0;

  fillSPIBuffer(LEDS_PER_BLOCK_TRANSFER * 0, ledPayloadBuffer[0]);
  fillSPIBuffer(LEDS_PER_BLOCK_TRANSFER * 1, ledPayloadBuffer[1]);
  ledFillIdx = LEDS_PER_BLOCK_TRANSFER * 2;

  LED_CH_EN_Set();

  // Set interrupt handler
  DMAC_ChannelCallbackRegister(DMAC_CHANNEL_0, dmaBlockDone, 0);

  // The low period at the start of the transfer while the pin
  // is switched from GPIO to be SPI-peripheral driven.  Next
  // descriptor is buffer0 transfer
  DMAC_LinkedListDescriptorSetup(&dmaDescriptors[DMA_DESC_STARTUP],
      DMA_SPI_RESET_BTCTRL,
      &ZeroByte,
      (void *)&SERCOM1_REGS->SPIM.SERCOM_DATA,
      16,
      &dmaDescriptors[DMA_DESC_BUFFER0]);

  // Buffer 0 transfer: Buffers 0 and 1 are ping-ponged back and
  // forth; the interrupt fills one while the other is sent.  After
  // all data has been sent, the interrupt updates the last sent
  // descriptor's "next" pointer is updated to point to the RESET
  // descriptor
  DMAC_LinkedListDescriptorSetup(&dmaDescriptors[DMA_DESC_BUFFER0],
      DMA_SPI_LED_BTCTRL,
      ledPayloadBuffer[0],
      (void *)&SERCOM1_REGS->SPIM.SERCOM_DATA,
      sizeof(ledPayloadBuffer[0]),
      &dmaDescriptors[DMA_DESC_BUFFER1]);

  // Buffer 1 transfer
  DMAC_LinkedListDescriptorSetup(&dmaDescriptors[DMA_DESC_BUFFER1],
      DMA_SPI_LED_BTCTRL,
      ledPayloadBuffer[1],
      (void *)&SERCOM1_REGS->SPIM.SERCOM_DATA,
      sizeof(ledPayloadBuffer[1]),
      &dmaDescriptors[DMA_DESC_BUFFER0]);

  // The low period at the end of the transfer, indicating RESET
  // to the LEDs.
  DMAC_LinkedListDescriptorSetup(&dmaDescriptors[DMA_DESC_RESET],
      DMA_SPI_RESET_BTCTRL,
      &ZeroByte,
      (void *)&SERCOM1_REGS->SPIM.SERCOM_DATA,
      RESET_SPI_BYTE_LENGTH,
      NULL);

  // Set false here so that if data is updated while writing
  // we know we need to immediately do another update
  ledDirty = false;

  ledDMABusy = true;

  // Start transmitting a low level so SPI will drive it low
  SERCOM1_REGS->SPIM.SERCOM_DATA = 0;

  // Enable transfer (when the just-written byte empties and causes the DMA trigger
  dmaState = STATE_START;
  DMAC_ChannelLinkedListTransfer(DMAC_CHANNEL_0, &dmaDescriptors[DMA_DESC_STARTUP]);

  // SPI should be driving the correct low signal by now; switch pin mux
  setLedTXPinToSPI();

  return;
}

bool ledTransmitInProgress() {
  return ledDMABusy;
}

bool ledTaskShouldRun() {
  // TODO: Add periodic re-write even if not dirty to make sure HW state is correct?
  return ledDirty && !ledDMABusy;
}

void ledTask() {
  sendLedData();
}

void ledUpdateAll(const uint8_t * data, size_t size) {
  memcpy(ledValues, data, sizeof(ledValues));
  ledDirty = true;
#if 0
  size_t copyBytes = sizeof(ledValues);
  if (copyBytes > size) {
    // % 3 to make sure we don't write a partial LED color tuple
    copyBytes = size - (size % 3);
  }

  ledDirty = (memcmp(data, ledValues, copyBytes) != 0);
  if (ledDirty) {
    memcpy(ledValues, data, copyBytes);
  }
#endif
}

