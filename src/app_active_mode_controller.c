
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


extern APP_ACTIVE_MODE_CONTROLLER_STATES app_active_mode_controllerState;
extern APP_ACTIVE_MODE_CONTROLLER_DATA app_active_mode_controllerData;

// Time counters, when MAX value the counter is off, when 0 counter is on
uint32_t secondCounter   = UINT32_MAX;
uint32_t secondCounterLegionella = UINT32_MAX;
uint32_t sterilizationReachedTemperatureTimeStamp = UINT32_MAX;

uint16_t sterilizationTemperatureOffset = TEMPERATURE_ALARM_VALUE;

STERILIZATION_MODE sterilisationMode = OFF;


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


bool goToActiveSterilization() {
    uint16_t currentHotWaterBufferTemp = GetNtcTemperature(NTC_HOT_WATER_BUFFER);
    uint16_t sterilizationStartTime = UnitSystemParameterL[ADDRESS_STERILIZATION_START_TIME - START_ADDRESS_UNIT_SYSTEM_PARAMETER_L][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP];
    uint16_t sterilizationFunction = UnitSystemParameterL[ADDRESS_HIGH_TEMPERATURE_STERILIZATION_FUNCTION - START_ADDRESS_UNIT_SYSTEM_PARAMETER_L][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP];
    uint16_t sterilizationIntervalDays = UnitSystemParameterL[ADDRESS_STERILIZATION_INTERVAL_DAYS - START_ADDRESS_UNIT_SYSTEM_PARAMETER_L][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP];
    uint8_t currentDisplayTimeHours = (uint8_t)(UserParameters[ADDRESS_DISPLAY_TIME - START_ADDRESS_USER_PARAMETERS][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP] >> 8);
    uint8_t currentDisplayTimeMinutes = (uint8_t)UserParameters[ADDRESS_DISPLAY_TIME - START_ADDRESS_USER_PARAMETERS][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP];
    uint16_t dayCounter = ReadSmartEeprom16(SEEP_ADDR_DAY_COUNTER_STERILIZATION);
    uint16_t maxTimeOutOfSterilizationMode = ReadSmartEeprom16(SEEP_ADDR_STERILIZATION_MAX_TIME_OUT_OF_STERILIZATION_MODE_WITH_ELEMENT_ON);    

    // Passive sterilization check, if this triggers we need to return to active sterilization    
    //if (secondCounterLegionella != UINT32_MAX && (secondCounterLegionella > maxTimeOutOfSterilizationMode) && (heatingElementStatus == true)) {   
    if(sterilisationMode == PASSIVE && (secondCounterLegionella > maxTimeOutOfSterilizationMode)) {
        // Sterilization hot water element was already on
        return true;
    }
    
    if ((currentHotWaterBufferTemp == TEMPERATURE_ALARM_VALUE) || (sterilizationFunction != STERILIZATION_FUNCTION_AUTO)){
        return false;
    }
    
    if (dayCounter < (sterilizationIntervalDays - 1)) {
        return false;
    }
  
    if ((currentDisplayTimeHours == sterilizationStartTime) && (currentDisplayTimeMinutes == 0)) {   
        // Current time is the set time for sterilization
        return true;
    }

    return false;
}



bool sterilisationIsActivelyRunning() {
    if (sterilisationMode != OFF) {
        
        // If we start a fresh sterilization cycle we have to set some parameters before proceeding.
        if (sterilisationMode == ACTIVE && secondCounterLegionella == UINT32_MAX) {
                    
            bool valvePosition = getStatus3WayValve();
            if (valvePosition != VALVE_IS_ON_HOT_WATER_CIRCUIT) {
                /* TODO: SCHAKEL 3 WEG KLEP */
                return true;
            }

            TurnOffHeatingElementHotWaterBuffer();
            secondCounterLegionella = 0;    
            sterilizationTemperatureOffset = ReadSmartEeprom16(SEEP_ADDR_STERILIZATION_SETPOINT_OFFSET_START);  
        }        
        
        
        uint16_t retourWaterTemperature = (RealTimeData[ADDRESS_RETURN_WATER_TEMPERATURE_T6 - START_ADDRESS_REAL_TIME_DATA][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP] * 10);
        int16_t sterilizationTemperature = (UnitSystemParameterL[ADDRESS_STERILIZATION_TEMPERATURE_SETTING - START_ADDRESS_UNIT_SYSTEM_PARAMETER_L][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP] * 10);
        
        if ((retourWaterTemperature >= (sterilizationTemperature + sterilizationTemperatureOffset - 20)) && (sterilizationTemperatureOffset != 0) && (sterilizationTemperatureOffset != TEMPERATURE_ALARM_VALUE)) {   
            // Retour water temperature has come within 2 degree celcius of setpoint
            sterilizationTemperatureOffset += ReadSmartEeprom16(SEEP_ADDR_STERILIZATION_SETPOINT_OFFSET_STEPS);  // Increase offset with 2 degree Celcius
        }    
        
        int16_t currentHotWaterBufferTemp = GetNtcTemperature(NTC_HOT_WATER_BUFFER);
        if(currentHotWaterBufferTemp >= sterilizationTemperature){
            if (sterilizationReachedTemperatureTimeStamp == UINT32_MAX) {
                // Sterilization reached the temperature for the first time, so store the timestamp
                sterilizationReachedTemperatureTimeStamp = secondCounterLegionella;
            }
            
            int16_t sterilizationRunTime = UnitSystemParameterL[ADDRESS_STERILIZATION_RUN_TIME - START_ADDRESS_UNIT_SYSTEM_PARAMETER_L][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP];
            if (secondCounterLegionella < (sterilizationReachedTemperatureTimeStamp + (sterilizationRunTime * 60))) {
                return true;
            }
            
            // Sterilization is done
            TurnOffHeatingElementHotWaterBuffer();
            secondCounterLegionella = UINT32_MAX;
            sterilizationReachedTemperatureTimeStamp = UINT32_MAX;
            WriteSmartEeprom16(SEEP_ADDR_DAY_COUNTER_STERILIZATION, 0);
            sterilizationTemperatureOffset = TEMPERATURE_ALARM_VALUE;
             
            sterilisationMode = OFF;
            return true;
        } 
        
        // Check if sterilisation was already on PASSIVE mode
        sterilizationReachedTemperatureTimeStamp = UINT32_MAX;      
        if (sterilisationMode == PASSIVE ) {
            return false;
        } 

        // Max time in ACTIVE sterilisation has not been reached yet 
        if (secondCounterLegionella < ReadSmartEeprom16(SEEP_ADDR_STERILIZATION_MAX_TIME_IN_STERILIZATION_MODE)) {   
            return true;
        }
            
        // 120 minutes in ACTIVE sterilization mode passed, but still not finished, set sterilisation to PASSIVE mode
        secondCounterLegionella = 0;
        TurnOnHeatingElementHotWaterBuffer();   // Set heating element on
        sterilizationTemperatureOffset = TEMPERATURE_ALARM_VALUE;
            
        sterilisationMode = PASSIVE;  
        return true;
    }
    return false;    
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
    // Triggers every second
    if(HeatingHotWaterTimerExpired()) {
        if (secondCounter >= 0 && secondCounter != UINT32_MAX) {
            secondCounter++;  
        }
        if (secondCounterLegionella >= 0 && secondCounterLegionella != UINT32_MAX){
            secondCounterLegionella++;
        }
        
        if (DebugDipSwitch() == true) {
            SYS_CONSOLE_PRINT("Active mode %s\r\n", getActiveModeToString(app_active_mode_controllerData.currentRunningMode));
        }
        
        // Sterilization was either on passive mode or off, but has to be started
        if(sterilisationMode != ACTIVE && goToActiveSterilization()){
            sterilisationMode = ACTIVE;
        }
    }
    
    // Get the most resent selected active mode from the display
    app_active_mode_controllerData.currentRunningMode = ReadSmartEeprom16(SEEP_ADDR_HEATPUMP_MODE);
    
    // If sterilisation was actively running return
    if(sterilisationIsActivelyRunning()){
        return;
    }
    
    


    
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
