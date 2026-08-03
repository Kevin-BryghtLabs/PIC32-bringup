#ifndef _BL_UART_H
#define _BL_UART_H

#include <stdint.h>
#include "definitions.h"

/* Provide C++ Compatibility */
#ifdef __cplusplus
extern "C" {
#endif

extern uint8_t uartRxBuffer[255];
extern int bleDataLength;

//Transmit API - send a packet, parts at a time.
//Data() may be called multiple times. Total Data() len must match Start() len.
void sendGenericStart(uint8_t command, uint8_t len);
void sendGenericData(const void * data_, uint8_t len);
void sendGenericData1(uint8_t byte);
void sendGenericDone(void);


//Transmit API - send an entire packet, all at once.
//Wraps the above Start()/Data()/Done() functions.
void sendGeneric(uint8_t command, uint8_t length, const void *data);

//Parse the UART FIFO until a packet is found or end of data.
bool processRxFifo(void);

void APP_UartErrCallbackHandler(uintptr_t context, USART_ERROR err);
void APP_UartRxCallbackHandler(SERCOM_USART_EVENT event, uintptr_t context);

/* Provide C++ Compatibility */
#ifdef __cplusplus
}
#endif

#endif /* _BL_UART_H */
