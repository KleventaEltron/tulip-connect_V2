#include <stddef.h>                    
#include <stdbool.h>                   
#include <stdlib.h>                   
#include <string.h>
#include <stdio.h>
#include "definitions.h"    

#include "hot_water_cooling_mode.h"
#include "states.h"

#include "eeprom.h"
#include "time_counters.h"
#include "ntc.h"
#include "sterilization.h"
#include "defrosting.h"
#include "modbus\heatpump_parameters.h"

extern HOT_WATER_COOLING_MODE_DATA hot_water_cooling_mode_data;

bool getHotwaterElementBoolFromHotwaterCoolingMode() {
    return hot_water_cooling_mode_data.HotwaterElementOn;
}

void HOT_WATER_COOLING_MODE_Initialize ( void )
{
    hot_water_cooling_mode_data.state = HOT_WATER_COOLING_INITIALIZE_COOLING;
    return;
}

const char * getHotWaterCoolingStateToString()
{
    switch (hot_water_cooling_mode_data.state)
    {
        case(HOT_WATER_COOLING_INITIALIZE_COOLING): {
            return "0, Init cooling";
            break;
        }
        
        case(HOT_WATER_COOLING_IDLE_COOLING): {
            return "1, Idle cooling";
            break;
        }
        
        case(HOT_WATER_COOLING_MODE_RUNNING_ON_COOLING): {
            return "2, Running cooling";
            break;
        }
        
        case(HOT_WATER_COOLING_INITIALIZE_HOT_WATER): {
            return "3, Init hotwater";
            break;
        }
        
        case(HOT_WATER_COOLING_STATE_WAIT_FOR_MINIMAL_TIME_IN_HOT_WATER): {
            return "4, Minimal time in hot water";
            break;
        }
        
        case(HOT_WATER_COOLING_STATE_RUNNING_IN_HOT_WATER): {
            return "5, Running hotwater";
            break;
        }
        
        case(HOT_WATER_COOLING_STATE_RUNNING_WITH_ELEMENT_ON_IN_HOT_WATER): {
            return "6, Running hotwater with element on";
            break;
        }
        
        default:{
            return "-1, Unkown";
            break;
        }
        return "-1, Unkown";
    }
}

bool areWeOnHotWaterModeInHotWaterAndCoolingMode()
{
    if (hot_water_cooling_mode_data.state == HOT_WATER_COOLING_INITIALIZE_HOT_WATER){
        return true;
    }
    
    if (hot_water_cooling_mode_data.state == HOT_WATER_COOLING_STATE_WAIT_FOR_MINIMAL_TIME_IN_HOT_WATER){
        return true;
    }
    
    if (hot_water_cooling_mode_data.state == HOT_WATER_COOLING_STATE_RUNNING_IN_HOT_WATER){
        return true;
    }
    
    if (hot_water_cooling_mode_data.state == HOT_WATER_COOLING_STATE_RUNNING_WITH_ELEMENT_ON_IN_HOT_WATER){
        return true;
    }
    
    return false;
}


void adjustSetpointOffset()
{
    int16_t hotwaterBufferTemperature = GetNtcTemperature(NTC_HOT_WATER_BUFFER);
    int16_t hotwaterSetpoint = getHotwaterSetpoint();
    
    if ((hotwaterBufferTemperature == TEMPERATURE_ALARM_VALUE) || (hotwaterSetpoint == TEMPERATURE_ALARM_VALUE)) {
        // No temperature or setpoint known yet
        return;
    }
    
    if (hotwaterBufferTemperature >= hotwaterSetpoint)
    {   // Hot water buffer tempereature is equal or higher than actual setpoint, so reset offset to 0
        hot_water_cooling_mode_data.setpointHotWaterOffset = 0; 
        return;
    }
    
    if ((getHeatpumpReturnWaterTemperature() >= (hotwaterSetpoint + hot_water_cooling_mode_data.setpointHotWaterOffset - 20)) && (hot_water_cooling_mode_data.setpointHotWaterOffset != 0) && (hot_water_cooling_mode_data.setpointHotWaterOffset != TEMPERATURE_ALARM_VALUE)){   
        // Retour water temperature has come within 2 degree celcius of setpoint, increase offset with 2 degrees
        hot_water_cooling_mode_data.setpointHotWaterOffset += ReadSmartEeprom16(SEEP_ADDR_HOT_WATER_SETPOINT_OFFSET_STEPS);  
        return;
    }
}


int16_t determineCorrectHotWaterSetpoint() {
    
    return (getHotwaterSetpoint() + hot_water_cooling_mode_data.setpointHotWaterOffset); 
}

int16_t determineCorrectRunningMode() {
    if ((hot_water_cooling_mode_data.state == HOT_WATER_COOLING_INITIALIZE_COOLING) ||
        (hot_water_cooling_mode_data.state == HOT_WATER_COOLING_IDLE_COOLING) ||
        (hot_water_cooling_mode_data.state == HOT_WATER_COOLING_MODE_RUNNING_ON_COOLING) ) {
        // Cooling modes
        return SET_MODE_COOLING;
    }
    
    if ((hot_water_cooling_mode_data.state == HOT_WATER_COOLING_INITIALIZE_HOT_WATER) ||
        (hot_water_cooling_mode_data.state == HOT_WATER_COOLING_STATE_WAIT_FOR_MINIMAL_TIME_IN_HOT_WATER) ||
        (hot_water_cooling_mode_data.state == HOT_WATER_COOLING_STATE_RUNNING_IN_HOT_WATER) ||
        (hot_water_cooling_mode_data.state == HOT_WATER_COOLING_STATE_RUNNING_WITH_ELEMENT_ON_IN_HOT_WATER) ) {
        // Hotwater (heating) modes
        return SET_MODE_HEATING; 
    }
    
    return SET_MODE_COOLING;
}


void HOT_WATER_COOLING_MODE_Tasks ( void )
{        
    int16_t hotwaterBufferTemperature = GetNtcTemperature(NTC_HOT_WATER_BUFFER);
    int16_t hotwaterSetpoint = getHotwaterSetpoint();
    int16_t hotwaterDelta = getHotwaterDelta();
    
    if ((hotwaterBufferTemperature == TEMPERATURE_ALARM_VALUE) || (hotwaterSetpoint == TEMPERATURE_ALARM_VALUE) || (hotwaterDelta == TEMPERATURE_ALARM_VALUE)) {
        // Needed values not yet known
        return;
    }
    
    if (areWeOnHotWaterModeInHotWaterAndCoolingMode() == true){
        // Already in one of the hot water modes
        // If sterilization goes to passive mode, go to heating
        if (getSterilisationMode() == PASSIVE){
            // Sterilization mode on passive, so go back to heating            
            hot_water_cooling_mode_data.hotwaterPassive = false;
            hot_water_cooling_mode_data.state = HOT_WATER_COOLING_INITIALIZE_COOLING;
            return;
        }
    }
    
    if (hot_water_cooling_mode_data.hotwaterPassive == true){
        // Hot water is on passive, so in one of the heating states
        if (hotwaterBufferTemperature >= hotwaterSetpoint){
            // Setpoint reached in passive mode, now turn off
            setSecondCounterHotwaterTask(UINT32_MAX);
            //TurnOffHeatingElementHotWaterBuffer();
            hot_water_cooling_mode_data.HotwaterElementOn = false;
            
            hot_water_cooling_mode_data.hotwaterPassive = false;
            hot_water_cooling_mode_data.setpointHotWaterOffset = TEMPERATURE_ALARM_VALUE;
            return;
        }
        
        if ((getSecondCounterHotwaterTask() != UINT32_MAX) && (getSecondCounterHotwaterTask() >= ReadSmartEeprom16(SEEP_ADDR_HOT_WATER_MAX_TIME_HEATING_ELEMENT_ON_IN_HEATING_MODE_SEC))) {
            // Is more than 2 hours in passive hot water, so go back to active
            hot_water_cooling_mode_data.hotwaterPassive = false;
            hot_water_cooling_mode_data.state = HOT_WATER_COOLING_INITIALIZE_COOLING;
            return;
        }
    }
    
    if ((areWeOnHotWaterModeInHotWaterAndCoolingMode() == false) && (hot_water_cooling_mode_data.hotwaterPassive == false) && (hotwaterBufferTemperature < (hotwaterSetpoint - hotwaterDelta)) && (getSterilisationMode() != PASSIVE)) {
        // Not in hot water state yet
        // Not on one of the hot water states or doing passive hot water
        // Hot water buffer is lower than setpoint - delta
        hot_water_cooling_mode_data.state = HOT_WATER_COOLING_INITIALIZE_HOT_WATER;
        return;
    }
    
    setActiveModeControllerHeatpumpSetpoint(determineCorrectHotWaterSetpoint());
    
    setActiveModeControllerHeatpumpRunningMode(determineCorrectRunningMode());
    
    switch ( hot_water_cooling_mode_data.state )
    {   
        /*
           _____ ____   ____  _      _____ _   _  _____ 
          / ____/ __ \ / __ \| |    |_   _| \ | |/ ____|
         | |   | |  | | |  | | |      | | |  \| | |  __ 
         | |   | |  | | |  | | |      | | | . ` | | |_ |
         | |___| |__| | |__| | |____ _| |_| |\  | |__| |
          \_____\____/ \____/|______|_____|_| \_|\_____|
        */                                                

        case HOT_WATER_COOLING_INITIALIZE_COOLING:{
            // 0: Init cooling
            
            hot_water_cooling_mode_data.state = HOT_WATER_COOLING_IDLE_COOLING;
            
            break;
        }
        
        case HOT_WATER_COOLING_IDLE_COOLING:{
            // 1: Idle cooling
            
            if (getHeatpumpCompressorFrequency() != 0){
                // Compressor is running
                hot_water_cooling_mode_data.state = HOT_WATER_COOLING_MODE_RUNNING_ON_COOLING;
                break;
            }
            
            break;
        }
        
        case HOT_WATER_COOLING_MODE_RUNNING_ON_COOLING:{
            // 2: Running cooling
            
            if ((getHeatpumpCompressorFrequency() == 0) && (isDefrostingActive() == false)){
                // Compressor is not running and is also not in defrosting
                hot_water_cooling_mode_data.state = HOT_WATER_COOLING_IDLE_COOLING;
                break;
            }
            
            break;
        }
        
        /*
         _    _  ____ _______  __          __  _______ ______ _____  
        | |  | |/ __ \__   __| \ \        / /\|__   __|  ____|  __ \ 
        | |__| | |  | | | |     \ \  /\  / /  \  | |  | |__  | |__) |
        |  __  | |  | | | |      \ \/  \/ / /\ \ | |  |  __| |  _  / 
        | |  | | |__| | | |       \  /\  / ____ \| |  | |____| | \ \ 
        |_|  |_|\____/  |_|        \/  \/_/    \_\_|  |______|_|  \_\
        */  
        
        case HOT_WATER_COOLING_INITIALIZE_HOT_WATER:{
            // 3: Initialize hot water
            
            setSecondCounterHotwaterTask(0);
            
            hot_water_cooling_mode_data.HotwaterElementOn = false;
            
            hot_water_cooling_mode_data.hotwaterPassive = false;
            hot_water_cooling_mode_data.setpointHotWaterOffset = ReadSmartEeprom16(SEEP_ADDR_HOT_WATER_SETPOINT_OFFSET_START);
            hot_water_cooling_mode_data.state = HOT_WATER_COOLING_STATE_WAIT_FOR_MINIMAL_TIME_IN_HOT_WATER;
            break;
        }
        

        case HOT_WATER_COOLING_STATE_WAIT_FOR_MINIMAL_TIME_IN_HOT_WATER:{
            // 4: Minimal time in hot water
            
            adjustSetpointOffset();
            
            if ((getSecondCounterHotwaterTask() >= ReadSmartEeprom16(SEEP_ADDR_HOT_WATER_MIN_TIME_IN_HOT_WATER_MODE)) && (getSecondCounterHotwaterTask() != UINT32_MAX)) {
                // Minimal time in hot water passed
                hot_water_cooling_mode_data.state = HOT_WATER_COOLING_STATE_RUNNING_IN_HOT_WATER;
                break;
            }

            break;
        }
        
        case HOT_WATER_COOLING_STATE_RUNNING_IN_HOT_WATER:{
            // 5: Running in hot water
            
            adjustSetpointOffset();
            
            if ((getHeatpumpCompressorFrequency() == 0) && (isDefrostingActive() == false)){
                // Compressor is not running and is also not in defrosting, so go back to heating
                
                hot_water_cooling_mode_data.HotwaterElementOn = false;
                setSecondCounterHotwaterTask(UINT32_MAX);
                
                hot_water_cooling_mode_data.hotwaterPassive = false;
                hot_water_cooling_mode_data.setpointHotWaterOffset = TEMPERATURE_ALARM_VALUE;
                hot_water_cooling_mode_data.state = HOT_WATER_COOLING_INITIALIZE_COOLING;
                break;
            }
            
            if ((getSecondCounterHotwaterTask() >= ReadSmartEeprom16(SEEP_ADDR_HOT_WATER_RUNNING_TIME_BEFORE_TURNING_ON_HEATING_ELEMENT)) && (getSecondCounterHotwaterTask() != UINT32_MAX)){
                // 2 hours passed in hot water and not reached setpoint, turn on element, time counter not needed anymore

                hot_water_cooling_mode_data.HotwaterElementOn = true;
                setSecondCounterHotwaterTask(UINT32_MAX);
                hot_water_cooling_mode_data.state = HOT_WATER_COOLING_STATE_RUNNING_WITH_ELEMENT_ON_IN_HOT_WATER;
                break;
            }
            
            break;
        }
        
        case HOT_WATER_COOLING_STATE_RUNNING_WITH_ELEMENT_ON_IN_HOT_WATER:{
            // 6: Running in hot water with element on
            
            adjustSetpointOffset();
            
            if ((getHeatpumpCompressorFrequency() == 0) && (isDefrostingActive() == false)){
                // Compressor is not running and is also not in defrosting, so go back to heating

                hot_water_cooling_mode_data.HotwaterElementOn = false;
                setSecondCounterHotwaterTask(UINT32_MAX);
                
                hot_water_cooling_mode_data.hotwaterPassive = false;
                hot_water_cooling_mode_data.setpointHotWaterOffset = TEMPERATURE_ALARM_VALUE;
                hot_water_cooling_mode_data.state = HOT_WATER_COOLING_INITIALIZE_COOLING;
                break;
            }
            
            if (GetThermostatContact() == true){   
                // Thermostat contact has been made, turn on passive mode, reset timer and go to heating
                
                setSecondCounterHotwaterTask(0);
                
                hot_water_cooling_mode_data.hotwaterPassive = true;
                hot_water_cooling_mode_data.setpointHotWaterOffset = TEMPERATURE_ALARM_VALUE;
                hot_water_cooling_mode_data.state = HOT_WATER_COOLING_INITIALIZE_COOLING;
                break;
            }
            
            break;
        }
        
        default:{
            HOT_WATER_COOLING_MODE_Initialize();
            break;
        }
    }
    
}


