/*******************************************************************************
  Main Source File

  Company:
    Microchip Technology Inc.

  File Name:
    main.c

  Summary:
    This file contains the "main" function for a project.

  Description:
    This file contains the "main" function for a project.  The
    "main" function calls the "SYS_Initialize" function to initialize the state
    machines of all modules in the system
 *******************************************************************************/

// *****************************************************************************
// *****************************************************************************
// Section: Included Files
// *****************************************************************************
// *****************************************************************************

#include <stddef.h>                     // Defines NULL
#include <stdbool.h>                    // Defines true
#include <stdlib.h>                     // Defines EXIT_FAILURE
#include "definitions.h"                // SYS function prototypes


// *****************************************************************************
// *****************************************************************************
// Section: Main Entry Point
// *****************************************************************************
// *****************************************************************************

static volatile uint32_t ticksMs;

static void tickISR(TC_TIMER_STATUS status, uintptr_t context) {
  ticksMs++;
}

static uint32_t getCurrentTimeMs() {
  return ticksMs;
}

static void delay(uint32_t ms) {
  const uint32_t endTime = ticksMs + ms + 1;

  while (getCurrentTimeMs() < endTime) {
  }
}

#define NRF52_HEADER   0x30

static const uint32_t sendIntervalMs = 1000;
static uint32_t nextSendTime;


#define FLETCHER_MOD 255

uint8_t fletcher_state1 = 0;
uint8_t fletcher_state2 = 0;

void fletcher_reset()
{
    fletcher_state1 = 0;
    fletcher_state2 = 0;
}

void fletcher_update(const uint8_t *data, int count)
{
   while(count)
   {
      fletcher_state1 = ((uint32_t)fletcher_state1 + *data) % FLETCHER_MOD;
      fletcher_state2 = ((uint32_t)fletcher_state2 + fletcher_state1) % FLETCHER_MOD;
      data++;
      count--;
   }
}

//Emit the checksum value
uint16_t fletcher_finish(void)
{
   return (fletcher_state2 << 8) | fletcher_state1;
}

//Emit a modified checksum value, so that a checksum including this value == 0
uint16_t fletcher_gencheck(void)
{
    uint32_t csum = fletcher_finish();

    uint32_t f0 = csum & 0xff;
    uint32_t f1 = (csum >> 8) & 0xff;
    uint32_t c0 = 0xff - ((f0 + f1) % FLETCHER_MOD);
    uint32_t c1 = 0xff - ((f0 + c0) % FLETCHER_MOD);
    //Pack so we can transmit from a little-endian CPU.
    //Everything on ChessUp2 is little-endian.
    return (uint16_t)((c1 << 8) | c0);
}

bool fletcher_check(void)
{
    return fletcher_finish() == 0;
}








static void sendTestMessage() {
  static uint8_t message[9] = { NRF52_HEADER, 5, 'H', 'e', 'l', 'l', 'o' };
  if (message[7] == 0 && message[8] == 0) {
    fletcher_reset();
    fletcher_update(message, 7);
    uint16_t csum = fletcher_finish();
    // Little endian
    message[7] = csum & 0xFF;
    message[8] = csum >> 8;
  }

  if (getCurrentTimeMs() >= nextSendTime) {
      SERCOM2_USART_Write(message, sizeof(message));
      nextSendTime += sendIntervalMs;
  }
}

int main ( void )
{
    /* Initialize all modules */
    SYS_Initialize ( NULL );

    TC0_TimerCallbackRegister(tickISR, (uintptr_t)NULL);
    TC0_TimerStart();

    while ( true )
    {
        LED_CH_EN_Toggle();
        sendTestMessage();

        /* Maintain state machines of all polled MPLAB Harmony modules. */
        SYS_Tasks ( );

        delay(1);
    }

    /* Execution should not come here during normal operation */

    return ( EXIT_FAILURE );
}


/*******************************************************************************
 End of File
*/

