
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

#include "files/circulation_pump.h"

extern APP_ACTIVE_MODE_CONTROLLER_STATES app_active_mode_controllerState;
extern APP_ACTIVE_MODE_CONTROLLER_DATA app_active_mode_controllerData;

uint32_t sterilizationReachedTemperatureTimeStamp = UINT32_MAX;
uint16_t sterilizationTemperatureOffset = TEMPERATURE_ALARM_VALUE;

STERILIZATION_MODE sterilisationMode = OFF;

 bool valvePosition = 0;  
 bool neededValvePosition = 0;
 bool displayPumpOn = 0;



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
    if(sterilisationMode == PASSIVE && (getSecondCounterLegionella() > maxTimeOutOfSterilizationMode)) {
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
        if (sterilisationMode == ACTIVE && getSecondCounterLegionella() == UINT32_MAX) {
                    
            bool valvePosition = getStatus3WayValve();
            if (valvePosition != VALVE_IS_ON_HOT_WATER_CIRCUIT) {
                /* TODO: SCHAKEL 3 WEG KLEP */
                return true;
            }

            TurnOffHeatingElementHotWaterBuffer();
            setSecondCounterLegionella(0);    
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
                sterilizationReachedTemperatureTimeStamp = getSecondCounterLegionella();
            }
            
            int16_t sterilizationRunTime = UnitSystemParameterL[ADDRESS_STERILIZATION_RUN_TIME - START_ADDRESS_UNIT_SYSTEM_PARAMETER_L][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP];
            if (getSecondCounterLegionella() < (sterilizationReachedTemperatureTimeStamp + (sterilizationRunTime * 60))) {
                return true;
            }
            
            // Sterilization is done
            TurnOffHeatingElementHotWaterBuffer();
            setSecondCounterLegionella(UINT32_MAX);
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
        if (getSecondCounterLegionella() < ReadSmartEeprom16(SEEP_ADDR_STERILIZATION_MAX_TIME_IN_STERILIZATION_MODE)) {   
            return true;
        }
            
        // 120 minutes in ACTIVE sterilization mode passed, but still not finished, set sterilisation to PASSIVE mode
        setSecondCounterLegionella(0);
        TurnOnHeatingElementHotWaterBuffer();   // Set heating element on
        sterilizationTemperatureOffset = TEMPERATURE_ALARM_VALUE;
            
        sterilisationMode = PASSIVE;  
        return true;
    }
    return false;    
}





bool neededThreeWayValveState() {    
    bool valvePositionForActiveState = 0;
    
    if (sterilisationMode == ACTIVE) {
        valvePositionForActiveState = VALVE_IS_ON_HOT_WATER_CIRCUIT;
        return valvePositionForActiveState;
    }
    
    switch(app_active_mode_controllerData.currentRunningMode)
    { 
        case HEATING:{
            valvePositionForActiveState = VALVE_IS_ON_HEATING_CIRCUIT;
            break;
        }
        
        
        case COOLING:{
            valvePositionForActiveState = VALVE_IS_ON_HEATING_CIRCUIT;
            break;
        }
        
        
        case FLOOR_HEATING: {
            valvePositionForActiveState = VALVE_IS_ON_HEATING_CIRCUIT;    
            break;
        }
            
        
        case HOT_WATER:{
            valvePositionForActiveState = VALVE_IS_ON_HOT_WATER_CIRCUIT;
            break;
        }
        
        
        case HOT_WATER_COOLING:{
            // TODO: LATER OP TERUG KOMEN
            //valvePositionForActiveState = VALVE_IS_ON_HEATING_CIRCUIT;
            break;
        }
        
        
        case HOT_WATER_HEATING:{
            valvePositionForActiveState = VALVE_IS_ON_HOT_WATER_CIRCUIT;
            // TODO: LATER OP TERUG KOMEN
            break;
        }
        
        
        case HOT_WATER_FLOOR_HEATING:{
            valvePositionForActiveState = VALVE_IS_ON_HEATING_CIRCUIT;
            // TODO: LATER OP TERUG KOMEN
            break;
        }
         
        default:{
            break;
        }
    }        
    
    return valvePositionForActiveState;
}





void switchThreeWayValve(bool neededValvePosition) {
    // Heatpump must be off, and water flow must be 0 before we are allowed to switch the valve
    if (UserParameters[ADDRESS_ON_OFF - START_ADDRESS_USER_PARAMETERS][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP] != SET_HEATPUMP_OFF || 
            RealTimeData[ADDRESS_WATER_FLOW - START_ADDRESS_REAL_TIME_DATA][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP] != 0) {
        ChangeHeatpumpSetting(ADDRESS_ON_OFF, SET_HEATPUMP_OFF);
        setWaitingThreeWayValveSwitch(0);
        return;
    }
    
    if (neededValvePosition == VALVE_IS_ON_HEATING_CIRCUIT) {
        Switch3WayValveToHeating();
    } else if (neededValvePosition == VALVE_IS_ON_HOT_WATER_CIRCUIT) {
        Switch3WayValveToHotWater();
    } 
    setWaitingThreeWayValveSwitch(0);
    
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
    UpdateCounters();
    
    // SYSTEM IS STUCK, RESET
    if (getSystemStuckProtectionCounter() >= 120) {
        SYS_RESET_SoftwareReset();
    }    
    
    // Get the most resent selected active mode from the display
    app_active_mode_controllerData.currentRunningMode = ReadSmartEeprom16(SEEP_ADDR_HEATPUMP_MODE);
    displayPumpOn = ReadSmartEeprom8(SEEP_ADDR_DISPLAY_PUMP_ON);
    valvePosition = getStatus3WayValve();   
    neededValvePosition = neededThreeWayValveState();
    
    // Triggers every second
    if(HeatingHotWaterTimerExpired()) {
        
        if (DebugDipSwitch() == true) {
            SYS_CONSOLE_PRINT("\r\nINFO:\n", getActiveModeToString(app_active_mode_controllerData.currentRunningMode));
            SYS_CONSOLE_PRINT(" Active mode:          %s\n", getActiveModeToString(app_active_mode_controllerData.currentRunningMode));
            SYS_CONSOLE_PRINT(" 3-way valve mode:     %s\n", getThreeWayValveState(valvePosition));
            SYS_CONSOLE_PRINT(" 3-way needed state:   %s\n\n", getThreeWayValveState(neededValvePosition));
            SYS_CONSOLE_PRINT(" Heatpump state:       %s\n", (UserParameters[ADDRESS_ON_OFF - START_ADDRESS_USER_PARAMETERS][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP] ? "True" : "False"));
            SYS_CONSOLE_PRINT(" Display pump on:      %s\n", (displayPumpOn ? "True" : "False"));
            SYS_CONSOLE_PRINT(" Sterilisation active: %i\n\n", sterilisationMode);
            SYS_CONSOLE_PRINT(" Counters:             %i, %i, %i\n\n", getSecondCounterLegionella(), getWaitingThreeWayValveSwitch(), getSystemStuckProtectionCounter());
            SYS_CONSOLE_PRINT(" Pumpstate:            %s\n", getCirculationPumpStateToString());
            SYS_CONSOLE_PRINT(" Buffer:               %d\n", GetNtcTemperature(NTC_HEATING_BUFFER));
            SYS_CONSOLE_PRINT(" Temp too low:         %d\n", getCircPumpData().temperatureTooLowForPumpToBeOn);
            SYS_CONSOLE_PRINT(" Counter:              %d\n", (int)getSecondCounterCirculationPumpTask());
        }
        
        // Sterilization was either on passive mode or off, but has to be started
        if(sterilisationMode != ACTIVE && goToActiveSterilization()){
            sterilisationMode = ACTIVE;
        }        
    }
    
    // If the pump was turned off using the display we stop regulating everything
    // Untill the user turns the system on themselves again
    if (displayPumpOn == false) {
        setSystemStuckProtectionCounter(0);
        return;
    }
    
    // Delay for after either turning off the heatpump or switching the three way valve
    if (getWaitingThreeWayValveSwitch() >= 0 && getWaitingThreeWayValveSwitch() < 20) {
        return;
    }
    setWaitingThreeWayValveSwitch(UINT32_MAX);
    
    // Check the valve position and switch it if needed
    if (valvePosition != neededValvePosition) {   
        switchThreeWayValve(neededValvePosition);      
        return;
    }
    
    // Heatpump must be on before we can may start any other action again
    if(UserParameters[ADDRESS_ON_OFF - START_ADDRESS_USER_PARAMETERS][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP] != SET_HEATPUMP_ON){
        ChangeHeatpumpSetting(ADDRESS_ON_OFF, SET_HEATPUMP_ON);
        setWaitingThreeWayValveSwitch(0);
        return;
    }
    
    // Reset system stuck counter
    setSystemStuckProtectionCounter(0);
    // If sterilisation was actively running return
    if(sterilisationIsActivelyRunning()){
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
