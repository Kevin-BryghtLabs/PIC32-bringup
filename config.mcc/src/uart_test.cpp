#include "uart_test.hpp"
#include "bl_seq_gen.hpp"
#include "bl_uart_dma.hpp"
#include "hub_log.h"

Gen generator;
Gen verifier;
static uint8_t buffer[256];
static const char * volatile errorMessage;
volatile bool error = false;
volatile uint32_t errorCount = 0;
uint32_t txTotal = 0;
uint32_t lastStatus = 0;

static void setError (const char * e) {
  errorMessage = e;
  error = true;
}

void uartSendTestData() {
  unsigned space = bl_uart_get_tx_size_writeable();

  while (space) {
    unsigned count = space < sizeof(buffer) ? space : sizeof(buffer);
    unsigned i = 0;
    unsigned statusDelta = txTotal - lastStatus;
    if (lastStatus == 0 && statusDelta >= 100 || statusDelta > 5000) {
      if (space >= 3) {
        uint8_t statusData[3];
        // Generator never builds anything with bit 6 set; these are special purpose
        statusData[0] = 0x81;
        statusData[1] = errorCount;
        statusData[2] = errorCount >> 8;
        bl_uart_write(statusData, sizeof(statusData));
        lastStatus = txTotal;
      }
      break;
    }
    else {
      for (unsigned i = 0; i < count; ++i) {
        buffer[i] = generator.gen();
      }
      bl_uart_write(buffer, count);
      txTotal += count;
      space -= count;
    }
  }
}

void uartVerifyTestData() {
  if (bl_uart_has_rx_overflowed()) {
    setError("UART RX DMA Overflow");
    bl_uart_clear_rx_overflow();
  }

  if (bl_uart_has_rx_event_sync_error()) {
    setError("UART RX DMA Event sync error");
    bl_uart_clear_rx_event_sync_error();
  }

  unsigned count = 0;
  do {
    count = bl_uart_read(buffer, sizeof(buffer));

    for (unsigned i = 0; i < count; ++i) {
      if (buffer[i] != verifier.gen()) {
        errorCount++;
        verifier.sync(buffer[i]);
      }
    }
  } while (count > 0);
}
