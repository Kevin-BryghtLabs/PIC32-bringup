#include "bl_uart_dma.hpp"

#include "definitions.h"

// Size of RX buffer as a power of 2 (i.e. Size = 2 ^ RxBufferOrder)
#define RxBufferOrder     8

// Size of TX buffer as a power of 2 (i.e. Size = 2 ^ TxBufferOrder)
#define TxBufferOrder     8

// If set, enable an ISR that checks that the TC2 count has wrapped to 0 and thus counted a full
// buffer of bytes.  However, suspending the channel and syncing, reading, and setting TC2
// introduces its own latency which could drop bytes.  Only enable if it appears that events
// are being missed by TC2.  This should not happen, as the TC2 clock is GCLK0 at 48MHz.
#define DO_EVENT_SYNC_CHECK 0

#define RxBufferSize      (1 << RxBufferOrder)
#define RxBufferPosMask   (RxBufferSize - 1)

#define TxBufferSize      (1 << TxBufferOrder)
#define TxBufferPosMask   (TxBufferSize - 1)
#define TxFlushThreshold  (TxBufferSize / 4)

static_assert(TxBufferOrder <= 16);

// DMA Control word written to RX descriptors
#define UART_DMA_RX_BTCTRL \
   (DMAC_BTCTRL_BEATSIZE_BYTE | RX_BLOCKACT | DMAC_BTCTRL_VALID_Msk | DMAC_BTCTRL_DSTINC_Msk | DMAC_BTCTRL_EVOSEL(3))

// DMA Control word written to TX descriptors
#define UART_DMA_TX_BTCTRL \
   (DMAC_BTCTRL_BEATSIZE_BYTE | DMAC_BTCTRL_BLOCKACT_INT | DMAC_BTCTRL_VALID_Msk | DMAC_BTCTRL_SRCINC_Msk )

static __attribute__ ((aligned (8))) dmac_descriptor_registers_t uartDmaRxDescriptor;
static __attribute__ ((aligned (8))) dmac_descriptor_registers_t uartDmaTxDescriptor;

static bool uartRxOverflow = false;
static bool uartRxEventSyncError = false;

static uint16_t uartRxReadPos = 0;

static volatile uint16_t uartTxReadPos = 0;
static volatile uint16_t uartTxWritePos = 0;

static uint8_t uartRxData[RxBufferSize];
static uint8_t uartTxData[TxBufferSize];

static volatile uint16_t currentDmaTxSize = 0;
static volatile uint8_t currentDmaTxRetry = 0;

static void checkTxFlush();
static void doTxFlush();

#if DO_EVENT_SYNC_CHECK
// Suspend the channel and interrupt to check the event count
#define RX_BLOCKACT DMAC_BTCTRL_BLOCKACT_BOTH
#else
#define RX_BLOCKACT DMAC_BTCTRL_BLOCKACT_NOACT
#endif

#if DO_EVENT_SYNC_CHECK
static void uartDmaRxBlockDone(DMAC_TRANSFER_EVENT event, uintptr_t contextHandle) {
  if (event == DMAC_TRANSFER_EVENT_COMPLETE) {
    uint16_t count = TC2_Compare16bitCounterGet();

    if (count & RxBufferPosMask) {
      uartRxEventSyncError = true;
      uint16_t rounded = (count + RxBufferSize / 2) & ~RxBufferPosMask;
      TC2_Compare16bitCounterSet(rounded);
    }

    DMAC_ChannelResume(DMAC_CHANNEL_1);
  }
}
#endif

static void uartDmaTxBlockDone(DMAC_TRANSFER_EVENT event, uintptr_t contextHandle) {
  if (event == DMAC_TRANSFER_EVENT_COMPLETE) {
    uartTxReadPos += currentDmaTxSize;
    currentDmaTxSize = 0;
    currentDmaTxRetry = 0;

    doTxFlush();
  }
  else if (event == DMAC_TRANSFER_EVENT_ERROR) {
    if (currentDmaTxRetry++ > 2) {
      // After retry, move past this data
      uartTxReadPos += currentDmaTxSize;
      currentDmaTxRetry = 0;
    }
    currentDmaTxSize = 0;

    doTxFlush();
  }
}

void bl_uart_dma_rx_init() {
  // Disable RX interrupt (turned on by MCC init routines; not configurable)
  SERCOM2_REGS->USART_INT.SERCOM_INTENCLR = SERCOM_USART_INT_INTENCLR_RXC_Msk;

  // TC2 counts DMA "beat" events to know how many bytes have been received
  TC2_CompareInitialize();
  TC2_CompareStart();

  DMAC_LinkedListDescriptorSetup(&uartDmaRxDescriptor,
      UART_DMA_RX_BTCTRL,
      (void *)&SERCOM2_REGS->USART_INT.SERCOM_DATA,
      uartRxData,
      sizeof(uartRxData),
      &uartDmaRxDescriptor);

#if DO_EVENT_SYNC_CHECK
  DMAC_ChannelCallbackRegister(DMAC_CHANNEL_1, uartDmaRxBlockDone, 0);
#endif

  // Enable transfer (triggered when SERCOM byte received)
  DMAC_ChannelLinkedListTransfer(DMAC_CHANNEL_1, &uartDmaRxDescriptor);
}

void bl_uart_dma_tx_init() {
  DMAC_ChannelCallbackRegister(DMAC_CHANNEL_2, uartDmaTxBlockDone, 0);
}

unsigned bl_uart_get_rx_size_readable() {
  uint16_t writePos = TC2_Compare16bitCounterGet();
  static_assert(sizeof(uartRxReadPos) == sizeof(writePos));

  uint16_t bytes = writePos - uartRxReadPos;

  if (bytes >= RxBufferSize) {
    uartRxOverflow = true;

    // Skip past overwritten bytes
    bytes = RxBufferSize - 1;
    uartRxReadPos = writePos - bytes;
  }

  return bytes;
}

unsigned bl_uart_get_tx_size_writeable() {
  return (uartTxReadPos - uartTxWritePos - 1) & TxBufferPosMask;
}

static unsigned bl_uart_get_tx_size_readable() {
  return (uartTxWritePos - uartTxReadPos) & TxBufferPosMask;
}

unsigned bl_uart_read(uint8_t * buffer, unsigned size) {
  unsigned available = bl_uart_get_rx_size_readable();
  if (available == 0) {
    return 0;
  }

  if (size > available) {
    size = available;
  }

  unsigned readIdx = uartRxReadPos & RxBufferPosMask;
  if (readIdx + size > RxBufferSize) {
    const unsigned firstReadSize = RxBufferSize - readIdx;
    const unsigned secondReadSize = size - firstReadSize;
    memcpy(buffer, &uartRxData[readIdx], firstReadSize);
    memcpy(&buffer[firstReadSize], uartRxData, secondReadSize);
  }
  else {
    memcpy(buffer, &uartRxData[readIdx], size);
  }

  uartRxReadPos += size;

  return size;
}

unsigned bl_uart_write(const uint8_t * buffer, unsigned size) {
  unsigned available = bl_uart_get_tx_size_writeable();
  if (available == 0) {
    checkTxFlush();
    return 0;
  }

  if (size > available) {
    size = available;
  }

  unsigned writeIdx = uartTxWritePos & TxBufferPosMask;
  if (writeIdx + size > TxBufferSize) {
    const unsigned firstWriteSize = TxBufferSize - writeIdx;
    const unsigned secondWriteSize = size - firstWriteSize;
    memcpy(&uartTxData[writeIdx], buffer, firstWriteSize);
    memcpy(uartTxData, &buffer[firstWriteSize], secondWriteSize);
  }
  else {
    memcpy(&uartTxData[writeIdx], buffer, size);
  }

  __DMB();

  uartTxWritePos += size;

  checkTxFlush();

  return size;
}

void bl_uart_tx_flush() {
  if (currentDmaTxSize == 0) {
    doTxFlush();
  }
}

static void checkTxFlush() {
  if (bl_uart_get_tx_size_readable() > TxFlushThreshold) {
    bl_uart_tx_flush();
  }
}

static void doTxFlush() {
  if (uartTxReadPos == uartTxWritePos) {
    return;
  }

  const unsigned writeIdx = uartTxWritePos & TxBufferPosMask;
  const unsigned readIdx = uartTxReadPos & TxBufferPosMask;
  if (writeIdx > readIdx) {
    currentDmaTxSize = writeIdx - readIdx;
  }
  else {
    currentDmaTxSize = TxBufferSize - readIdx;
  }

  DMAC_LinkedListDescriptorSetup(&uartDmaTxDescriptor,
      UART_DMA_TX_BTCTRL,
      &uartTxData[readIdx],
      (void *)&SERCOM2_REGS->USART_INT.SERCOM_DATA,
      currentDmaTxSize,
      nullptr);

  // Enable transfer (triggered when TX register empty)
  if (!DMAC_ChannelLinkedListTransfer(DMAC_CHANNEL_2, &uartDmaTxDescriptor)) {
    // No DMA in progress if this call fails
    currentDmaTxSize = 0;
  }
}

bool bl_uart_has_rx_overflowed() {
  return uartRxOverflow;
}

void bl_uart_clear_rx_overflow() {
  uartRxOverflow = false;
}

bool bl_uart_has_rx_event_sync_error() {
  return uartRxEventSyncError;
}

void bl_uart_clear_rx_event_sync_error() {
  uartRxEventSyncError = false;
}
