
#include <stddef.h>                     
#include <stdbool.h>                    
#include <stdlib.h>                    
#include <string.h>
#include <stdio.h>
#include "definitions.h" 

#include "app_active_mode_controller.h"
#include "files/states.h"


extern APP_ACTIVE_MODE_CONTROLLER_STATES app_active_mode_controllerState;
extern APP_ACTIVE_MODE_CONTROLLER_DATA app_active_mode_controllerData;


void APP_ACTIVE_MODE_CONTROLLER_Initialize ( void )
{
    app_active_mode_controllerData.currentRunningMode = HEATING;
    app_active_mode_controllerData.previousRunningMode = HEATING;
    
    app_active_mode_controllerState = APP_ACTIVE_MODE_CONTROLLER_STATE_INIT;
    //SYS_CONSOLE_PRINT("APP_ACTIVE_MODE_CONTROLLER_Initialize passed\n");
}



void APP_ACTIVE_MODE_CONTROLLER_Tasks ( void )
{
    switch ( app_active_mode_controllerState )
    {
        /* Application's initial state. */
        case APP_ACTIVE_MODE_CONTROLLER_STATE_INIT:
        {
            app_active_mode_controllerState = APP_ACTIVE_MODE_CONTROLLER_STATE_SERVICE_TASKS;
            //SYS_CONSOLE_PRINT("SET RUNNING MODE TO >> %i\n", app_active_mode_controllerData.currentRunningMode);
            break;
        }

        
        case APP_ACTIVE_MODE_CONTROLLER_STATE_SERVICE_TASKS:
        {
            break;
        }


        default:
        {
            break;
        }
    }
}


/*******************************************************************************
 End of File
 */
