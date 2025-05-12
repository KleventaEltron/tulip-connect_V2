
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
#include "files/defrosting.h"

#include "files/circulation_pump.h"



extern APP_ACTIVE_MODE_CONTROLLER_STATES app_active_mode_controllerState;
extern APP_ACTIVE_MODE_CONTROLLER_DATA app_active_mode_controllerData;

 
 
 


 
 void printDebugInfo() {
    if (DebugDipSwitch() == true) {
        /*
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
                          "   Day Counter:              %i\n"
                          "   System On:                %i\n\n", getSecondCounterLegionella(), getWaitingThreeWayValveSwitch(), getSystemStuckProtectionCounter(),  ReadSmartEeprom16(SEEP_ADDR_DAY_COUNTER_STERILIZATION), getsystemOnCounter());
        SYS_CONSOLE_PRINT(" Pumpstate:            %s\n", getCirculationPumpStateToString());
        SYS_CONSOLE_PRINT(" Heatpump Setpoint:    %i\n", app_active_mode_controllerData.setPoint);
        SYS_CONSOLE_PRINT(" Buffer:               %d\n", GetNtcTemperature(NTC_HEATING_BUFFER));
        SYS_CONSOLE_PRINT(" Temp too low:         %d\n", getCircPumpData().temperatureTooLowForPumpToBeOn);
        SYS_CONSOLE_PRINT(" Counter:              %d\n", (int)getSecondCounterCirculationPumpTask());
        */
        
            
        SYS_CONSOLE_PRINT("\r\nINFO:\n", getActiveModeToString(app_active_mode_controllerData.currentRunningMode));
        SYS_CONSOLE_PRINT(" FW:                   %d-%d-%d\n", (int)((THIS_FIRMWARE_VERSION / 1000000)), (int)((THIS_FIRMWARE_VERSION / 1000) % 1000), (int)(THIS_FIRMWARE_VERSION % 1000));
        SYS_CONSOLE_PRINT(" Active mode:          %s\n", getActiveModeToString(app_active_mode_controllerData.currentRunningMode));
        SYS_CONSOLE_PRINT(" Heatpump ON:          %s\n", (UserParameters[ADDRESS_ON_OFF - START_ADDRESS_USER_PARAMETERS][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP] ? "True" : "False"));
        SYS_CONSOLE_PRINT(" Display pump on:      %s\n", (ReadSmartEeprom8(SEEP_ADDR_DISPLAY_PUMP_ON) ? "True" : "False"));
        SYS_CONSOLE_PRINT(" Sys stuck protection: %i\n", getSystemStuckProtectionCounter());
        SYS_CONSOLE_PRINT(" Sys on time:          %i\n", getsystemOnCounter());
        
        //printHeadOfStringBuffer();
        
        SYS_CONSOLE_PRINT("\r\nHEATPUMP:\n");
        SYS_CONSOLE_PRINT(" Setpoint:             %i\n", getHeatpumpHeatingSetpoint());
        SYS_CONSOLE_PRINT(" Heatpump mode:        %i\n", getHeatpumpRunningMode());
        SYS_CONSOLE_PRINT(" Compressor:           %i\n", getHeatpumpCompressorFrequency());
        //SYS_CONSOLE_PRINT(" Waterflow:            %i\n", getHeatpumpWaterFlow());
        SYS_CONSOLE_PRINT(" Retour temp.:         %i\n\n", getHeatpumpReturnWaterTemperature());
        
        SYS_CONSOLE_PRINT("\r\nCIRCULATION PUMP:\n");
        SYS_CONSOLE_PRINT(" State:                %s\n", getCirculationPumpStateToString());
        SYS_CONSOLE_PRINT(" Pump ON:              %s\n", getStatusCirculationPump() ? "True" : "False");
        SYS_CONSOLE_PRINT(" Time counter:         %i\n", getSecondCounterCirculationPumpTask());
        SYS_CONSOLE_PRINT(" Temp. too low:        %s\n\n", getCirculationPumpData().temperatureTooLowForPumpToBeOn ? "True" : "False");
        
        SYS_CONSOLE_PRINT("\r\nDefrosting:\n");
        SYS_CONSOLE_PRINT(" Defrosting active:    %s\n", isDefrostingActive() ? "True" : "False");
        SYS_CONSOLE_PRINT(" Hotwater buffer:      %i\n", GetNtcTemperature(NTC_HOT_WATER_BUFFER));
        SYS_CONSOLE_PRINT(" Initial defrost temp :%i\n", getInitialDefrostingTemperature());
        SYS_CONSOLE_PRINT(" Defrosting element:   %s\n\n", getDefrostingElementOnState() ? "True" : "False");
        
        SYS_CONSOLE_PRINT("\r\n3-WAY VALVE:\n");
        SYS_CONSOLE_PRINT(" 3-way valve mode:     %s\n", getThreeWayValveState(getStatus3WayValve()));
        SYS_CONSOLE_PRINT(" 3-way needed state:   %s\n", getThreeWayValveState(getNeededValvePosition()));
        SYS_CONSOLE_PRINT(" Time counter:         %i\n\n", getWaitingThreeWayValveSwitch());
        
        if (getSterilisationMode() != OFF) {
            SYS_CONSOLE_PRINT("\r\nSTERILIZATION:\n");
            SYS_CONSOLE_PRINT(" Sterilisation active: %s\n", getSterilizationState(getSterilisationMode()));
            SYS_CONSOLE_PRINT(" Time counter:         %i\n", getSecondCounterLegionella());
            SYS_CONSOLE_PRINT(" Temp reached time:    %i\n", getSterilizationReachedTemperatureTimeStamp());
            SYS_CONSOLE_PRINT(" Hotwater buffer:      %i\n", GetNtcTemperature(NTC_HOT_WATER_BUFFER));
            SYS_CONSOLE_PRINT(" Ster. temp.:          %i\n", UnitSystemParameterL[ADDRESS_STERILIZATION_TEMPERATURE_SETTING - START_ADDRESS_UNIT_SYSTEM_PARAMETER_L][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP]);
            SYS_CONSOLE_PRINT(" Ster. offset:         %i\n", getSterilizationTemperatureOffset());
            SYS_CONSOLE_PRINT(" Ster. element:        %s\n", getSterilizationElementOnState() ? "True" : "False");
            SYS_CONSOLE_PRINT(" Day counter:          %i\n\n", ReadSmartEeprom16(SEEP_ADDR_DAY_COUNTER_STERILIZATION));

            //SYS_CONSOLE_PRINT(" Day counter:          %i\n", ReadSmartEeprom16(SEEP_ADDR_DAY_COUNTER_STERILIZATION));
            //SYS_CONSOLE_PRINT(" Ster. run time:       %i\n", UnitSystemParameterL[ADDRESS_STERILIZATION_RUN_TIME - START_ADDRESS_UNIT_SYSTEM_PARAMETER_L][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP]);
            //SYS_CONSOLE_PRINT(" Ster. function:       %i\n", UnitSystemParameterL[ADDRESS_HIGH_TEMPERATURE_STERILIZATION_FUNCTION - START_ADDRESS_UNIT_SYSTEM_PARAMETER_L][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP]);
            //SYS_CONSOLE_PRINT(" Ster. start time:     %i\n", UnitSystemParameterL[ADDRESS_STERILIZATION_START_TIME - START_ADDRESS_UNIT_SYSTEM_PARAMETER_L][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP]);
            //SYS_CONSOLE_PRINT(" Ster. interval days:  %i\n", UnitSystemParameterL[ADDRESS_STERILIZATION_INTERVAL_DAYS - START_ADDRESS_UNIT_SYSTEM_PARAMETER_L][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP]);
            //SYS_CONSOLE_PRINT(" Current hour:         %i\n", (uint8_t)(UserParameters[ADDRESS_DISPLAY_TIME - START_ADDRESS_USER_PARAMETERS][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP] >> 8));
            //SYS_CONSOLE_PRINT(" Current minute:       %i\n\n", (uint8_t)UserParameters[ADDRESS_DISPLAY_TIME - START_ADDRESS_USER_PARAMETERS][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP]);
        }
        else {
            int16_t heatpumpMode = ReadSmartEeprom16(SEEP_ADDR_HEATPUMP_MODE);

            if (heatpumpMode == HEATING) {

                SYS_CONSOLE_PRINT("\r\nHEATING:\n");
                SYS_CONSOLE_PRINT(" State:                %s\n", getHeatingStateToString());
                SYS_CONSOLE_PRINT(" Element ON:           %s\n", getStatusHeatingElementHeatingBuffer() ? "True" : "False");
                SYS_CONSOLE_PRINT(" Buffer:               %i\n", GetNtcTemperature(NTC_HEATING_BUFFER));
                SYS_CONSOLE_PRINT(" Initial buffer temp.: %i\n", getHeatingModeData().initialBufferTemp);
                SYS_CONSOLE_PRINT(" Time counter:         %i\n\n", getSecondCounterHeatingTask());
            }
            
            if (heatpumpMode == COOLING) {

                SYS_CONSOLE_PRINT("\r\nCooling:\n");
                SYS_CONSOLE_PRINT(" State:                %s\n\n", getCoolingStateToString());
                
                SYS_CONSOLE_PRINT(" Cooling setpoint:     %i\n", getCoolingSetpoint());
                SYS_CONSOLE_PRINT(" Cooling buffer:       %i\n\n", GetNtcTemperature(NTC_HEATING_BUFFER));
            }
            
            if (heatpumpMode == HOT_WATER) {
                SYS_CONSOLE_PRINT("\r\nHOTWATER:\n");
                SYS_CONSOLE_PRINT(" State:                %s\n\n", getHotWaterStateToString());
                SYS_CONSOLE_PRINT(" Hotwater setpoint:    %i\n", getHotwaterSetpoint());
                SYS_CONSOLE_PRINT(" Hotwater buffer:      %i\n", GetNtcTemperature(NTC_HOT_WATER_BUFFER));
                SYS_CONSOLE_PRINT(" Offset setpoint:      %i\n", getHotWaterModeData().setpointHotWaterOffset);
                SYS_CONSOLE_PRINT(" Time counter:         %i\n", getSecondCounterHotwaterTask());
                SYS_CONSOLE_PRINT(" Hotwater element:     %s\n", getHotwaterElementBoolFromHotwaterMode() ? "True" : "False");              
            }

            if (heatpumpMode == HOT_WATER_HEATING) {
                SYS_CONSOLE_PRINT("\r\nHOTWATER AND HEATING:\n");
                SYS_CONSOLE_PRINT(" State:                %s\n\n", getHotwaterHeatingStateToString());
                
                SYS_CONSOLE_PRINT(" Heating setpoint:     %i\n", getHeatingSetpoint());
                SYS_CONSOLE_PRINT(" Heating buffer:       %i\n", GetNtcTemperature(NTC_HEATING_BUFFER));
                SYS_CONSOLE_PRINT(" Initial buffer temp.: %i\n", getHotWaterHeatingModeData().initialHeatingBufferTemp);
                SYS_CONSOLE_PRINT(" Time counter:         %i\n", getSecondCounterHeatingTask());
                SYS_CONSOLE_PRINT(" Heating element:      %s\n\n", getStatusHeatingElementHeatingBuffer() ? "True" : "False");

                SYS_CONSOLE_PRINT(" Hotwater setpoint:    %i\n", getHotwaterSetpoint());
                SYS_CONSOLE_PRINT(" Hotwater buffer:      %i\n", GetNtcTemperature(NTC_HOT_WATER_BUFFER));
                SYS_CONSOLE_PRINT(" Offset setpoint:      %i\n", getHotWaterHeatingModeData().setpointHotWaterOffset);
                SYS_CONSOLE_PRINT(" Time counter:         %i\n", getSecondCounterHotwaterTask());
                SYS_CONSOLE_PRINT(" Hotwater element:     %s\n", getStatusHeatingElementHotWaterBuffer() ? "True" : "False");
                SYS_CONSOLE_PRINT(" Hot water passive:    %s\n\n", getHotWaterHeatingModeData().hotwaterPassive ? "True" : "False");
            }
            
            if (heatpumpMode == HOT_WATER_COOLING) {
                SYS_CONSOLE_PRINT("\r\nHOTWATER AND COOLING:\n");
                SYS_CONSOLE_PRINT(" State:                %s\n", getHotWaterCoolingStateToString());
                
                SYS_CONSOLE_PRINT(" Cooling setpoint:     %i\n", getCoolingSetpoint());
                SYS_CONSOLE_PRINT(" Cooling buffer:       %i\n\n", GetNtcTemperature(NTC_HEATING_BUFFER));

                SYS_CONSOLE_PRINT(" Hotwater setpoint:    %i\n", getHotwaterSetpoint());
                SYS_CONSOLE_PRINT(" Hotwater buffer:      %i\n", GetNtcTemperature(NTC_HOT_WATER_BUFFER));
                SYS_CONSOLE_PRINT(" Offset setpoint:      %i\n", getHotWaterCoolingModeData().setpointHotWaterOffset);
                SYS_CONSOLE_PRINT(" Time counter:         %i\n", getSecondCounterHotwaterTask());
                SYS_CONSOLE_PRINT(" Hotwater element:     %s\n", getStatusHeatingElementHotWaterBuffer() ? "True" : "False");
                SYS_CONSOLE_PRINT(" Hot water passive:    %s\n\n", getHotWaterCoolingModeData().hotwaterPassive ? "True" : "False");
            }
        }
    }
    return;
 }
 
 
 void checkHeatingElementStates() {
    
     // Heating element
    if ((getHeatingElementBoolFromHotwaterHeatingMode() == true) || 
            (getHeatingElementBoolFromHeatingMode() == true)) {
        TurnOnHeatingElementHeatingBuffer();
    }
    else{
        TurnOffHeatingElementHeatingBuffer();
    }

    // Hot water element
    if ((getHotwaterElementBoolFromHotwaterHeatingMode() == true) ||
            (getHotwaterElementBoolFromHotwaterCoolingMode() == true) ||
            (getHotwaterElementBoolFromHotwaterMode() == true) ||
            (getDefrostingElementOnState() == true) ||
            (getSterilizationElementOnState() == true)) {
        TurnOnHeatingElementHotWaterBuffer();
    } else {
        TurnOffHeatingElementHotWaterBuffer();
    }
     
    return;
 }
 
 
 
 void checkHeatpumpHeatingSetpoint() {
    if (getWriteNewSetPointHeatpumpCounter() < 10) {
        return;
    }
    
    if (app_active_mode_controllerData.setPoint != (getHeatpumpHeatingSetpoint() * 10)) {   
        // Setpoint in heatpump is not correct, send the correct one
        ChangeHeatpumpSetting(ADDRESS_HEATING_SET_TEMPERATURE, (app_active_mode_controllerData.setPoint / 10));
    }
    
    setWriteNewSetPointHeatpumpCounter(0); 
 }
 
 
 
 void checkHeatpumpRunningMode() {
    if (getWriteHeatpumpRunningModeCounter() < 10) {
        return;
    }
    
    if (app_active_mode_controllerData.heatpumpRunningMode != getHeatpumpRunningMode()) {   
        // Heatpump is not on correct running mode, send correct one
        ChangeHeatpumpSetting(ADDRESS_SET_MODE, app_active_mode_controllerData.heatpumpRunningMode);
    }
    
    setWriteHeatpumpRunningModeCounter(0); 
 }
 
 
 

 void checkIfSoftwareResetNeeded() {
     if (!getPowerFailStatus()){
         WriteSmartEeprom32(SEEP_ADDR_SECONDS_COUNTER_DAYS, getsystemOnCounter());
     }
     
    // If sterilization is running ACTIVELY or PASSIVLY we have to wait for it to finish
    if (getSterilisationMode() != OFF) {
        return;
    }
    
    // Wait till a day has passed before a reset is possible
    if (getsystemOnCounter() < SECONDS_IN_DAY) {
        return;
    }
    
    uint16_t dayCounter = (ReadSmartEeprom16(SEEP_ADDR_DAY_COUNTER_STERILIZATION) + 1);
    WriteSmartEeprom32(SEEP_ADDR_SECONDS_COUNTER_DAYS, 0);
    WriteSmartEeprom16(SEEP_ADDR_DAY_COUNTER_STERILIZATION, dayCounter);
    
    SYS_RESET_SoftwareReset();
    
    return;
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
            WriteSmartEeprom16(SEEP_ADDR_HEATPUMP_MODE, HEATING);
            //FLOOR_HEATING_MODE_Tasks();
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
            WriteSmartEeprom16(SEEP_ADDR_HEATPUMP_MODE, HOT_WATER_HEATING);
            //HOT_WATER_FLOOR_HEATING_MODE_Tasks();
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
    // Get the previous system counter
    int32_t storedSystemCounter = ReadSmartEeprom32(SEEP_ADDR_SECONDS_COUNTER_DAYS);
    if(storedSystemCounter == UINT32_MAX) {
        storedSystemCounter = 0;
    }
    setSystemOnCounter(storedSystemCounter);
    
    // Get the previous heatpump mode
    // If the value in the eeprom is invalid we reset it it to default heating mode
    int16_t heatpumpMode = ReadSmartEeprom16(SEEP_ADDR_HEATPUMP_MODE);
    if(heatpumpMode == RESERVED || heatpumpMode > 7 || heatpumpMode < 0) {
        WriteSmartEeprom16(SEEP_ADDR_HEATPUMP_MODE, HEATING);
        heatpumpMode = HEATING;
    }
    
    app_active_mode_controllerData.currentRunningMode = heatpumpMode;
    app_active_mode_controllerData.previousRunningMode  = heatpumpMode;
    app_active_mode_controllerData.setPoint = 0;
    app_active_mode_controllerData.heatpumpRunningMode = 0;
    
    // Make sure arrays contain known values
    SetDataInArraysAtStartup();
    
    // Reset the system stuck protection counter
    setSystemStuckProtectionCounter(0);
    
    // Start the counter for checking and writing the correct setpoint
    setWriteNewSetPointHeatpumpCounter(0); 
    setWriteHeatpumpRunningModeCounter(0); 
    
    // Initialize every active mode
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
     *  Checks if the system should return to the bootloader
     *
     */
    checkIfSoftwareResetNeeded();   
    
    
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
        checkHeatpumpHeatingSetpoint();
        // Every 10 seconds the running mode of the heatpump is checked
        checkHeatpumpRunningMode();
    }
    
    
    // Wait 30 seconds to receive data from heatpump before doing anything
    if(getsystemOnCounter() < 30) { return; }
    

    /*
     * 
     * If the pump was turned off using the display we stop regulating everything
     * Untill the user turns the system on themselves again
     * 
     */
    if (ReadSmartEeprom8(SEEP_ADDR_DISPLAY_PUMP_ON) == false) {
        // Guard against system reset, because it is not actually stuck
        CIRCULATION_PUMP_Initialize();
        TurnOffHeatingElementHeatingBuffer();
        TurnOffHeatingElementHotWaterBuffer();
        
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
     * Check if the heating and/or hot water heating elements must be on or off
     * 
     *
     */
    checkHeatingElementStates();
    
    
    /* 
     * 
     * In the hot water states, including sterilization, defrosting must be checked.
     * If the boiler temperature drops to far, the element must be turned on.
     *
     */
    CheckDefrosting(getHotWaterHeatingModeData().state, getSterilisationMode());
    
    
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
