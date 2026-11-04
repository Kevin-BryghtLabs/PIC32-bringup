#ifndef BL_UART_DMA_HPP
#define BL_UART_DMA_HPP

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void bl_uart_dma_init();
unsigned bl_uart_get_received_bytes();
unsigned bl_uart_read(uint8_t * buffer, unsigned size);

bool bl_uart_has_overflowed();
void bl_uart_clear_overflow();

bool bl_uart_has_event_sync_error();
void bl_uart_clear_event_sync_error();

#ifdef __cplusplus
}
#endif

#endif // BL_UART_DMA_HPP
