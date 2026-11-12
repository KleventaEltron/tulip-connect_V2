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
#include "files/network_manager.h"
#include "app_s2_tasks.h"
#include "app_s2_session_tasks.h"
#include "app_s2_comm_test.h"

#include "files\delay.h"


void initComplete(void)
{
    uint32_t delayms = 75;
    
    for (uint8_t i = 0; i < 3; i++)
    {
        LedRun_Set();
        delayMS(delayms);
        LedRun_Clear();
        LedAlarm_Set();
        delayMS(delayms);
        LedAlarm_Clear();
        LedStatus_Set();
        delayMS(delayms);
        LedStatus_Clear();
        delayMS(delayms);
        LedStatus_Set();
        delayMS(delayms);
        LedStatus_Clear();
        LedAlarm_Set();
        delayMS(delayms);
        LedAlarm_Clear();
        LedRun_Set();
        delayMS(delayms);
        LedRun_Clear();
        delayMS(delayms);
    }
}



// *****************************************************************************
// *****************************************************************************
// Section: Main Entry Point
// *****************************************************************************
// *****************************************************************************

int main ( void )
{
    /* Initialize all modules */
    SYS_Initialize ( NULL );
    NETWORK_MANAGER_Initialize();
    APP_S2_Initialize();
    APP_S2_SESSION_Initialize();
    APP_S2_COMM_TEST_Initialize();
    
    NVIC_INT_Disable();
    initComplete();
    NVIC_INT_Enable();
    
    while ( true )
    {
        /* Maintain state machines of all polled MPLAB Harmony modules. */
        SYS_Tasks ( );
        NETWORK_MANAGER_Tasks();
        APP_S2_SESSION_Tasks();
        APP_S2_Tasks();
        APP_S2_COMM_TEST_Tasks();
        
        // Watchdog clear
        WDT_Clear();
    }

    /* Execution should not come here during normal operation */

    return ( EXIT_FAILURE );
}


/*******************************************************************************
 End of File
*/