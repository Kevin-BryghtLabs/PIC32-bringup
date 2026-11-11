#include "bl_comm.hpp"

#include "bl_ble_parser.h"
#include "bl_uart.h"
#include "bl_uart_dma.hpp"
#include "hub_log.h"
#include "timer.h"

void commInit() {
  //Wait a bit to ensure the UART lines have come up and other side should be ready.
  for(unsigned i = 0; i < 5; ++i){
    delay(1);
  }

  //Send 300B of zeros to reset the ESP32 parser in the event it is mid-frame
  for(unsigned i = 0; i < 300; ++i){
    sendGenericData1(0x00);
    WDT_Clear();
  }
  bl_uart_tx_flush();
}

void commTask() {
    if (bl_uart_has_rx_overflowed()) {
      hubLog('E', "UART RX DMA Overflow");
      bl_uart_clear_rx_overflow();
    }

    if (bl_uart_has_rx_event_sync_error()) {
      hubLog('E', "UART RX DMA Event sync error");
      bl_uart_clear_rx_event_sync_error();
    }

    while (processRxFifo()) {
        DecodePacketData(uartRxBuffer, bleDataLength);
    }
}
