#include "bl_uart_dma.hpp"

#include "definitions.h"

#define RxBufferOrder     8
#define RxBufferSize      (1 << RxBufferOrder)
#define RxBufferPosMask   (RxBufferSize - 1)

// If set, enable an ISR that checks that the TC2 count has wrapped to 0 and thus counted a full
// buffer of bytes.  However, suspending the channel and syncing, reading, and setting TC2
// introduces its own latency which could drop bytes.  Only enable if it appears that events
// are being missed by TC2.  This should not happen, as the TC2 clock is GCLK0 at 48MHz.
#define DO_EVENT_SYNC_CHECK 0

static __attribute__ ((aligned (8))) dmac_descriptor_registers_t uartDmaDescriptor;

static bool uartRxOverflow = false;
static bool uartRxEventSyncError = false;
static uint16_t uartRxReadPos = 0;
static uint8_t uartRxData[RxBufferSize];

#if DO_EVENT_SYNC_CHECK
// Suspend the channel and interrupt to check the event count
#define RX_BLOCKACT DMAC_BTCTRL_BLOCKACT_BOTH
#else
#define RX_BLOCKACT DMAC_BTCTRL_BLOCKACT_NOACT
#endif

#define UART_DMA_RX_BTCTRL \
   (DMAC_BTCTRL_BEATSIZE_BYTE | RX_BLOCKACT | DMAC_BTCTRL_VALID_Msk | DMAC_BTCTRL_DSTINC_Msk | DMAC_BTCTRL_EVOSEL(3))

#if DO_EVENT_SYNC_CHECK
static void uartDmaBlockDone(DMAC_TRANSFER_EVENT event, uintptr_t contextHandle) {
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

void bl_uart_dma_init() {
  // Disable RX interrupt (turned on by MCC init routines; not configurable)
  SERCOM2_REGS->USART_INT.SERCOM_INTENCLR = SERCOM_USART_INT_INTENCLR_RXC_Msk;

  // TC2 counts DMA "beat" events to know how many bytes have been received
  TC2_CompareInitialize();
  TC2_CompareStart();

  DMAC_LinkedListDescriptorSetup(&uartDmaDescriptor,
      UART_DMA_RX_BTCTRL,
      (void *)&SERCOM2_REGS->USART_INT.SERCOM_DATA,
      uartRxData,
      sizeof(uartRxData),
      &uartDmaDescriptor);

#if DO_EVENT_SYNC_CHECK
  DMAC_ChannelCallbackRegister(DMAC_CHANNEL_1, uartDmaBlockDone, 0);
#endif

  // Enable transfer (triggered when SERCOM byte received)
  DMAC_ChannelLinkedListTransfer(DMAC_CHANNEL_1, &uartDmaDescriptor);
}

unsigned bl_uart_get_received_bytes() {
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

unsigned bl_uart_read(uint8_t * buffer, unsigned size) {
  unsigned available = bl_uart_get_received_bytes();
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

bool bl_uart_has_overflowed() {
  return uartRxOverflow;
}

void bl_uart_clear_overflow() {
  uartRxOverflow = false;
}

bool bl_uart_has_event_sync_error() {
  return uartRxEventSyncError;
}

void bl_uart_clear_event_sync_error() {
  uartRxEventSyncError = false;
}
