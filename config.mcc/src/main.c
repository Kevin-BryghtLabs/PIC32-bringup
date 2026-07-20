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

static void delay(uint32_t ms) {
  const uint32_t endTime = ticksMs + ms + 1;

  while (ticksMs < endTime) {
  }
}

int main ( void )
{
    /* Initialize all modules */
    SYS_Initialize ( NULL );

    TC0_TimerCallbackRegister(tickISR, (uintptr_t)NULL);

    while ( true )
    {
        LED_CH_EN_Toggle();
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

