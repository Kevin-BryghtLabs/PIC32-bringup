#include <stdint.h>
#include "bl_ble_packet_headers.h"
#include "bl_fletcher16.hpp"
#include "bl_uart.h"
#include "bl_uart_dma.hpp"
#include "definitions.h"

static fletcher16 tx_checksum;

/*******************************************************************************
 * Function Name: sendGenericStart
 ********************************************************************************
 * Summary:
 *  This function starts sending a packet by sending framing header, header ID,
 *  and length. Any application-layer data must be added with sendGenericData()
 *  or sendGenericData1(). Once done, sendGenericDone() must be called to finish
 *  the packet.
 *******************************************************************************/
void sendGenericStart(uint8_t command, uint8_t length)
{
  tx_checksum.reset();
  sendGenericData1(NRF52_HEADER);
  sendGenericData1(length + 1);
  sendGenericData1(command);
}

#define USE_DMA 1

/*******************************************************************************
 * Function Name: sendGenericData
 ********************************************************************************
 * Summary:
 *  This function sends data for a packet. sendGenericStart() must have been
 *  called already with the full application layer packet length. This function
 *  updates the TX checksum.
 *
 * NOTE:
 *  If UART DMA is enabled and this function is called without calling
 *  sendGenericDone at the end of the data/packet, then bl_uart_tx_flush
 *  should be called to send the queued data.
 *******************************************************************************/
void sendGenericData(const void * data_, uint8_t len)
{
  uint8_t * data = (uint8_t*)data_;
  tx_checksum.update(data, len);
  while(len)
  {
#if USE_DMA
    size_t sent = bl_uart_write(data, len);
#else
    size_t sent = SERCOM2_USART_Write(data, len);
#endif
    data += sent;
    len -= sent;
  }    
}

/*******************************************************************************
 * Function Name: sendGenericData1
 ********************************************************************************
 * Summary:
 *  This simple wrapper function sends 1 byte of data, and updates tx checksum.
 *
 * NOTE:
 *  If UART DMA is enabled and this function is called without calling
 *  sendGenericDone at the end of the data/packet, then bl_uart_tx_flush
 *  should be called to send the queued data.
 *******************************************************************************/
void sendGenericData1(uint8_t byte)
{
    sendGenericData(&byte, 1);
}

/*******************************************************************************
 * Function Name: sendGenericDone
 ********************************************************************************
 * Summary:
 *  This function must be called once a packet is done.
 *  It handles any additional framing/footer required.
 *  This function may return before the data has been fully transmitted,
 *  but will not return before the footer is queued.
 *******************************************************************************/
void sendGenericDone()
{
  uint16_t f16 = tx_checksum.gencheck();
  uint8_t * data = (uint8_t*)&f16;
  size_t len = sizeof(f16);
  while(len)
  {
#if USE_DMA
    size_t sent = bl_uart_write(data, len);
    bl_uart_tx_flush();
#else
    size_t sent = SERCOM2_USART_Write(data, len);
#endif
    data += sent;
    len -= sent;
  }
}

/*******************************************************************************
 * Function Name: sendGeneric
 ********************************************************************************
 * Summary:
 *  This function queues an entire packet to go out the UART.
 *
 *
 * Parameters:
 *  command: This is the header of the BLE packet so the app knows what it is.
 *  length: This is the length of the data to follow
 *  *data: A pointer to the data array
 *
 *******************************************************************************/
void sendGeneric(uint8_t command, uint8_t length, const void *data){
    sendGenericStart(command, length);
    sendGenericData(data, length);
    sendGenericDone();
}

/* *****************************************************************************
 End of File
 */
