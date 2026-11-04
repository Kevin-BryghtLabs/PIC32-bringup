#include <stdint.h>
//#include "BL_app_state.h"
#include "bl_ble_packet_headers.h"
#include "bl_fletcher16.hpp"
#include "bl_uart.h"
#include "bl_uart_dma.hpp"
#include "definitions.h"

static volatile bool uartErrFlag;
static uint8_t dataCounter = 0;
uint8_t uartRxBuffer[255];
int bleDataLength = 0;
static fletcher16 rx_checksum;
static int dataUartLength;

uint32_t uartErrors[4];

// All BLE communication Rx comes through here.
void APP_UartRxCallbackHandler(SERCOM_USART_EVENT event, uintptr_t context) {
    switch(event)
    {
        case SERCOM_USART_EVENT_READ_ERROR:
            {
            //any in-progress packet is trashed, so reset parser
            uartErrFlag = true;
            USART_ERROR err = SERCOM2_USART_ErrorGet();
            if(err & USART_ERROR_PARITY ) uartErrors[0]++;
            if(err & USART_ERROR_FRAMING ) uartErrors[1]++;
            if(err & USART_ERROR_OVERRUN ) uartErrors[2]++;
            }
            break;
        default:
            break;
    }
}

static bool addByte(uint8_t byte) {
    rx_checksum.update(&byte, sizeof(byte));

    if (dataCounter == 0) {
        if(byte != NRF52_HEADER)
        {
            rx_checksum.reset();
            asm volatile("nop");
            return false;
        }
    }
    else if(dataCounter == 1){
      dataUartLength = byte;
    }
    else if (dataCounter >= 2) {
      uartRxBuffer[dataCounter - 2] = byte;
    }
    dataCounter++;
    if (dataCounter >= dataUartLength + 4) {
      dataCounter = 0;
      bleDataLength = dataUartLength;
      dataUartLength = 0;

      //Checksum(data+checkBytes)==0 if all is good
      return !rx_checksum.finish();
    }

    return false;
}

//Process data from the fifo until...
//packet found: return true
//end of FIFO: return false
bool processRxFifo(void)
{
    uint8_t buffer[16];

    if(uartErrFlag)
    {
        uartErrFlag = false;
        dataUartLength = 0;
        dataCounter = 0;

        //Discard current FIFO data
        while(bl_uart_read(buffer, sizeof(buffer))) {
        }

        rx_checksum.reset();
        return false;
    }

    while (true) {
      unsigned readSize = 1;
      if (dataUartLength > 0) {
        readSize = dataUartLength + 4 - dataCounter;
        if (readSize > sizeof(buffer)) {
          readSize = sizeof(buffer);
        }
      }

      unsigned size = bl_uart_read(buffer, readSize);
      if (!size) {
        break;
      }

      for (unsigned i = 0; i < size; ++i) {
        if (addByte(buffer[i])) {
          return true;
        }
      }
    }

    return false;
}


/* *****************************************************************************
 End of File
 */
