
#include <stddef.h>                     
#include <stdbool.h>                    
#include <stdlib.h>                    
#include <string.h>
#include <stdio.h>
#include "definitions.h" 

#include "app_active_mode_controller.h"
#include "files/states.h"

#include "files/eeprom.h"
#include "files/time_counters.h"

#include "files/heating_mode.h"
#include "files/hot_water_mode.h"
#include "files/cooling_mode.h"
#include "files/floor_heating_mode.h"
#include "files/hot_water_cooling_mode.h"
#include "files/hot_water_heating_mode.h"
#include "files/hot_water_floor_heating_mode.h"


extern APP_ACTIVE_MODE_CONTROLLER_STATES app_active_mode_controllerState;
extern APP_ACTIVE_MODE_CONTROLLER_DATA app_active_mode_controllerData;


void callActiveModeTaskHandler() {
    switch(app_active_mode_controllerData.currentRunningMode)
    {
        
        case HEATING:{
            HEATING_MODE_Tasks();
            break;
        }
        
        
        case COOLING:{
            COOLING_MODE_Tasks();
            break;
        }
        
        
        case FLOOR_HEATING: {
            FLOOR_HEATING_MODE_Tasks();
            break;
        }
            
        
        case HOT_WATER:{
            HOT_WATER_MODE_Tasks();
            break;
        }
        
        
        case HOT_WATER_COOLING:{
            HOT_WATER_COOLING_MODE_Tasks();
            break;
        }
        
        
        case HOT_WATER_HEATING:{
            HOT_WATER_HEATING_MODE_Tasks();
            break;
        }
        
        
        case HOT_WATER_FLOOR_HEATING:{
            HOT_WATER_FLOOR_HEATING_MODE_Tasks();
            break;
        }
        
              
        default:{
            APP_ACTIVE_MODE_CONTROLLER_Initialize();
            break;
        }
    }
    return;
}




void APP_ACTIVE_MODE_CONTROLLER_Initialize ( void )
{
    int16_t heatpumpMode = ReadSmartEeprom16(SEEP_ADDR_HEATPUMP_MODE);
    
    // If the value in the eeprom is invalid we reset it it to default heating mode
    if(heatpumpMode == RESERVE || heatpumpMode > 7) {
        WriteSmartEeprom16(SEEP_ADDR_HEATPUMP_MODE, HEATING);
        heatpumpMode = HEATING;
    }
    
    app_active_mode_controllerData.currentRunningMode = heatpumpMode;
    app_active_mode_controllerData.previousRunningMode  = heatpumpMode;
    
    HEATING_MODE_Initialize();
    HOT_WATER_MODE_Initialize();
    COOLING_MODE_Initialize();
    FLOOR_HEATING_MODE_Initialize();
    HOT_WATER_COOLING_MODE_Initialize();
    HOT_WATER_HEATING_MODE_Initialize();
    HOT_WATER_FLOOR_HEATING_MODE_Initialize();
    
    app_active_mode_controllerState = APP_ACTIVE_MODE_CONTROLLER_STATE_INIT;
}




void APP_ACTIVE_MODE_CONTROLLER_Tasks ( void )
{    
    // Get the most resent selected active mode from the display
    app_active_mode_controllerData.currentRunningMode = ReadSmartEeprom16(SEEP_ADDR_HEATPUMP_MODE);
    
    UpdateCounters();
    
    switch ( app_active_mode_controllerState )
    {
        /* Application's initial state. */
        case APP_ACTIVE_MODE_CONTROLLER_STATE_INIT:
        {
            app_active_mode_controllerState = APP_ACTIVE_MODE_CONTROLLER_STATE_SERVICE_TASKS;
            break;
        }

        
        case APP_ACTIVE_MODE_CONTROLLER_STATE_SERVICE_TASKS:
        {
            // Check if the active mode was switched
            if (app_active_mode_controllerData.currentRunningMode != app_active_mode_controllerData.previousRunningMode) {     
                SYS_CONSOLE_PRINT("Switching to mode %s\r\n", getActiveModeToString(app_active_mode_controllerData.currentRunningMode));
                resetActiveModeStates();
                app_active_mode_controllerData.previousRunningMode = app_active_mode_controllerData.currentRunningMode;
            }
            
            callActiveModeTaskHandler();
            break;
        }


        default:
        {
            APP_ACTIVE_MODE_CONTROLLER_Initialize();
            break;
        }
    }
}


/*******************************************************************************
 End of File
 */
