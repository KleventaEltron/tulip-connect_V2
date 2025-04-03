
#include <stddef.h>                     
#include <stdbool.h>                    
#include <stdlib.h>                    
#include <string.h>
#include <stdio.h>
#include "definitions.h" 

#include "app_active_mode_controller.h"

#include "config/default/user.h"
#include "files/ntc.h"

#include "files/states.h"
#include "files/eeprom.h"
#include "files/time_counters.h"
#include "files/modbus/heatpump_parameters.h"

#include "files/heating_mode.h"
#include "files/hot_water_mode.h"
#include "files/cooling_mode.h"
#include "files/floor_heating_mode.h"
#include "files/hot_water_cooling_mode.h"
#include "files/hot_water_heating_mode.h"
#include "files/hot_water_floor_heating_mode.h"
#include "files/threeWayValve.h"
#include "files/sterilization.h"

#include "files/circulation_pump.h"



extern APP_ACTIVE_MODE_CONTROLLER_STATES app_active_mode_controllerState;
extern APP_ACTIVE_MODE_CONTROLLER_DATA app_active_mode_controllerData;
 


 
 void printDebugInfo() {
    if (DebugDipSwitch() == true) {
        SYS_CONSOLE_PRINT("\r\nINFO:\n", getActiveModeToString(app_active_mode_controllerData.currentRunningMode));
        SYS_CONSOLE_PRINT(" Active mode:          %s\n", getActiveModeToString(app_active_mode_controllerData.currentRunningMode));
        SYS_CONSOLE_PRINT(" 3-way valve mode:     %s\n", getThreeWayValveState(getStatus3WayValve()));
        SYS_CONSOLE_PRINT(" 3-way needed state:   %s\n\n", getThreeWayValveState(getNeededValvePosition()));
        SYS_CONSOLE_PRINT(" Heatpump state:       %s\n", (UserParameters[ADDRESS_ON_OFF - START_ADDRESS_USER_PARAMETERS][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP] ? "True" : "False"));
        SYS_CONSOLE_PRINT(" Display pump on:      %s\n", (ReadSmartEeprom8(SEEP_ADDR_DISPLAY_PUMP_ON) ? "True" : "False"));
        SYS_CONSOLE_PRINT(" Sterilisation active: %s\n\n", getSterilizationState(getSterilisationMode()));
        SYS_CONSOLE_PRINT(" Counters:\n"
                          "   Legionella:               %i\n"
                          "   3-Way switch:             %i\n"
                          "   Sys stuck protection:     %i\n"
                          "   Day Counter:              %i\n\n", getSecondCounterLegionella(), getWaitingThreeWayValveSwitch(), getSystemStuckProtectionCounter(),  ReadSmartEeprom16(SEEP_ADDR_DAY_COUNTER_STERILIZATION));
        SYS_CONSOLE_PRINT(" Pumpstate:            %s\n", getCirculationPumpStateToString());
        SYS_CONSOLE_PRINT(" Heatpump Setpoint:    %i\n", app_active_mode_controllerData.setPoint);
        SYS_CONSOLE_PRINT(" Buffer:               %d\n", GetNtcTemperature(NTC_HEATING_BUFFER));
        SYS_CONSOLE_PRINT(" Temp too low:         %d\n", getCircPumpData().temperatureTooLowForPumpToBeOn);
        SYS_CONSOLE_PRINT(" Counter:              %d\n", (int)getSecondCounterCirculationPumpTask());
    }
    return;
 }
 
 
 
 void checkHeatpumpSetpoint() {
    if (getWriteNewSetPointHeatpumpCounter() < 10) {
        return;
    }
    
    if (app_active_mode_controllerData.setPoint != (UserParameters[ADDRESS_HEATING_SET_TEMPERATURE - START_ADDRESS_USER_PARAMETERS][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP] * 10)) {   
        // Setpoint in heatpump is not correct, send the correct one
        ChangeHeatpumpSetting(ADDRESS_HEATING_SET_TEMPERATURE, (app_active_mode_controllerData.setPoint / 10));
    }
    getWriteNewSetPointHeatpumpCounter(0); 
 }
 
 
 
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
    if(heatpumpMode == RESERVED || heatpumpMode > 7 || heatpumpMode < 0) {
        WriteSmartEeprom16(SEEP_ADDR_HEATPUMP_MODE, HEATING);
        heatpumpMode = HEATING;
    }
    
    app_active_mode_controllerData.currentRunningMode = heatpumpMode;
    app_active_mode_controllerData.previousRunningMode  = heatpumpMode;
    app_active_mode_controllerData.setPoint = 0;
    
    setSystemStuckProtectionCounter(0);
    
    HEATING_MODE_Initialize();
    HOT_WATER_MODE_Initialize();
    COOLING_MODE_Initialize();
    FLOOR_HEATING_MODE_Initialize();
    HOT_WATER_COOLING_MODE_Initialize();
    HOT_WATER_HEATING_MODE_Initialize();
    HOT_WATER_FLOOR_HEATING_MODE_Initialize();
    
    CIRCULATION_PUMP_Initialize();
    
    app_active_mode_controllerState = APP_ACTIVE_MODE_CONTROLLER_STATE_INIT;
}





void APP_ACTIVE_MODE_CONTROLLER_Tasks ( void )
{    
    // Functions as a watchdog timer, if it is not constantly reset the system is stuck
    if (getSystemStuckProtectionCounter() >= SYS_STUCK_TIMER_MAX_LIMIT) {
        SYS_RESET_SoftwareReset();
    }    
    
    
    /*
     *
     *  Update all counters
     *
     */
    UpdateCounters(); 
    
    
    /*
     *
     * Get the most recent selected active mode from the display 
     * 
     */
    app_active_mode_controllerData.currentRunningMode = ReadSmartEeprom16(SEEP_ADDR_HEATPUMP_MODE);
    
    
    /*
     *
     * Prints debug info if needed and checks if sterilization is needed
     *
     */
    if(HeatingHotWaterTimerExpired()) {
        // If DIP switch 4 set, print debug info
        printDebugInfo();
        // Sterilization was either on passive mode or off, but has to be set to ACTIVE mode
        checkNeedForSterilization();
        // Every 10 seconds the setpoint in the heatpump is checked
        checkHeatpumpSetpoint();
    }
    

    /*
     * 
     * If the pump was turned off using the display we stop regulating everything
     * Untill the user turns the system on themselves again
     * 
     */
    if (ReadSmartEeprom8(SEEP_ADDR_DISPLAY_PUMP_ON) == false) {
        // Guard against system reset, because it is not actually stuck
        setSystemStuckProtectionCounter(0);
        return;
    }
    
    
    /*
     * 
     * If the 3-way valve is not properly set to the right mode we need to return
     * until the switch has happend succesfully
     *
     */
    if(!validateThreeWayValveStateOkay(app_active_mode_controllerData.currentRunningMode)) {
        return;
    }
    
    
    /*
     *
     * If sterilization is actively running we must return until it is either
     * finished or it switches to passive mode
     *
     */
    if(sterilisationIsActivelyRunning()) {
        return;
    }      
    
    
    
    CIRCULATION_PUMP_Tasks();
    
    
    
    switch ( app_active_mode_controllerState )
    {
        /* Application's initial state. */
        case APP_ACTIVE_MODE_CONTROLLER_STATE_INIT: {
            app_active_mode_controllerState = APP_ACTIVE_MODE_CONTROLLER_STATE_SERVICE_TASKS;
            break;
        }

        
        case APP_ACTIVE_MODE_CONTROLLER_STATE_SERVICE_TASKS: {
            
            // Check if the active mode was switched
            if (app_active_mode_controllerData.currentRunningMode != app_active_mode_controllerData.previousRunningMode) {     
                SYS_CONSOLE_PRINT("Switching to mode %s\r\n", getActiveModeToString(app_active_mode_controllerData.currentRunningMode));
                resetActiveModeStates();
                app_active_mode_controllerData.previousRunningMode = app_active_mode_controllerData.currentRunningMode;
            }
            
            callActiveModeTaskHandler();
            break;
        }


        default: {
            APP_ACTIVE_MODE_CONTROLLER_Initialize();
            break;
        }
        
    }
}


/*******************************************************************************
 End of File
 */
