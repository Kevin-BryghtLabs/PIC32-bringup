#ifndef BL_UART_DMA_HPP
#define BL_UART_DMA_HPP

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Initialize DMA for UART receive; RX and TX can be enabled/used independently
// (but if not using RX DMA, switch back to the direct SERCOM_read calls)
void bl_uart_dma_rx_init();

// Initialize DMA for UART transfers; RX and TX can be enabled/used independently
// (but if not using TX DMA, switch back to the direct SERCOM_write calls)
void bl_uart_dma_tx_init();

// This call involves a read of TC2 which costs some SYNC time.  Where
// possible, just call `bl_uart_read` and handle 0 bytes returned
// instead of checking this first.
unsigned bl_uart_get_rx_size_readable();

// This call is just math, so it's OK to call more often
unsigned bl_uart_get_tx_size_writeable();

unsigned bl_uart_read(uint8_t * buffer, unsigned size);
unsigned bl_uart_write(const uint8_t * buffer, unsigned size);

// Not every write triggers a DMA transfer, since messages may be
// built one or a few bytes at a time.  Call this when you're done
// building a message with `bl_uart_write` to trigger the DMA transfer
void bl_uart_tx_flush();

// Check error flags when we have filled our DMA receive buffers
bool bl_uart_has_rx_overflowed();
void bl_uart_clear_rx_overflow();

// Reports if the DMA "beat" event counting is mismatched with the
// number of buffers transferred.  Only enabled if the
// `DO_EVENT_SYNC_CHECK` is enabled in the cpp file (see comment there).
bool bl_uart_has_rx_event_sync_error();
void bl_uart_clear_rx_event_sync_error();


#ifdef __cplusplus
}
#endif

#endif // BL_UART_DMA_HPP
