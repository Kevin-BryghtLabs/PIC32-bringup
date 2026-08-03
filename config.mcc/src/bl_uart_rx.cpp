#include <stdint.h>
//#include "BL_app_state.h"
#include "bl_ble_packet_headers.h"
#include "bl_fletcher16.hpp"
#include "bl_uart.h"
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

//Process data from the fifo until...
//packet found: return true
//end of FIFO: return false
bool processRxFifo(void)
{
    if(uartErrFlag)
    {
        uartErrFlag = false;
        dataUartLength = 0;
        dataCounter = 0;

        //Discard current FIFO data
        while(SERCOM2_USART_ReadCountGet())
        {
            uint8_t trashBuffer;
            SERCOM2_USART_Read(&trashBuffer, sizeof(trashBuffer));
        }

        rx_checksum.reset();
        return false;
    }

    while(SERCOM2_USART_ReadCountGet())
    {
        uint8_t data;
        SERCOM2_USART_Read(&data, sizeof(data));
        rx_checksum.update(&data, sizeof(data));

        if (dataCounter == 0) {
            if(data != NRF52_HEADER)
            {
                rx_checksum.reset();
                asm volatile("nop");
                continue;
            }
        }
        else if(dataCounter == 1){
          dataUartLength = data;
        }
        else if (dataCounter >= 2) {
          uartRxBuffer[dataCounter - 2] = data;
        }
        dataCounter++;
        if (dataCounter >= dataUartLength + 4) {
          dataCounter = 0;
          bleDataLength = dataUartLength;

          //Checksum(data+checkBytes)==0 if all is good
          return !rx_checksum.finish();
        }
    }
    return false;
}


/* *****************************************************************************
 End of File
 */
