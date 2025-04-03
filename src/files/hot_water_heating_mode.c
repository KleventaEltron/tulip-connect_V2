#include <stddef.h>                    
#include <stdbool.h>                   
#include <stdlib.h>                   
#include <string.h>
#include <stdio.h>
#include "definitions.h"    

#include "hot_water_heating_mode.h"
#include "states.h"

#include "eeprom.h"
#include "time_counters.h"
#include "ntc.h"

extern HOT_WATER_HEATING_MODE_DATA hot_water_heating_mode_data;

bool areWeOnHotWaterMode()
{
    if (hot_water_heating_mode_data.state == HOT_WATER_HEATING_INITIALIZE_HOT_WATER){
        return true;
    }
    
    if (hot_water_heating_mode_data.state == HOT_WATER_HEATING_STATE_WAIT_FOR_MINIMAL_TIME_IN_HOT_WATER){
        return true;
    }
    
    if (hot_water_heating_mode_data.state == HOT_WATER_HEATING_STATE_RUNNING_IN_HOT_WATER){
        return true;
    }
    
    if (hot_water_heating_mode_data.state == HOT_WATER_HEATING_STATE_RUNNING_WITH_ELEMENT_ON_IN_HOT_WATER){
        return true;
    }
    
    return false;
}

void checkDefrosting()
{
    if (isDefrostingActive() == true){
        // Defrosting is active in heatpump
        if (hot_water_heating_mode_data.initialDefrostingBoilerTemp == TEMPERATURE_ALARM_VALUE){
            // Initial defrosting temperature is alarm value (undefined), so give it a start value
            hot_water_heating_mode_data.initialDefrostingBoilerTemp = GetNtcTemperature(NTC_HOT_WATER_BUFFER);
        }
        
        if (GetNtcTemperature(NTC_HOT_WATER_BUFFER) <= (hot_water_heating_mode_data.initialDefrostingBoilerTemp - ReadSmartEeprom16(SEEP_ADDR_DEFROSTING_TEMP_FALL_BEFORE_ELEMENT_ON))){
            // Temperature decreased under threshold, turn on heating element
            TurnOnHeatingElementHotWaterBuffer();
        }
        
        if (GetNtcTemperature(NTC_HOT_WATER_BUFFER) >= (hot_water_heating_mode_data.initialDefrostingBoilerTemp - ReadSmartEeprom16(SEEP_ADDR_DEFROSTING_TEMP_FALL_BEFORE_ELEMENT_ON)+ ReadSmartEeprom16(SEEP_ADDR_DEFROSTING_TEMP_RISE_BEFORE_ELEMENT_OFF)))
        {   // Temperature rised above threshold, turn off heating element
            TurnOffHeatingElementHotWaterBuffer();
        }
        return;
    }
    
    if ((isDefrostingActive() == false) && (hot_water_heating_mode_data.initialDefrostingBoilerTemp != TEMPERATURE_ALARM_VALUE)){
        // Defrosting not active and initial defrosting temperature has a valid value
        
    }
    
    if(CheckIfDefrostModeActive() == true)
    {   // Heatpump is doing defrosting
        if (initialDefrostingBoilerTemp == TEMPERATURE_ALARM_VALUE)
            initialDefrostingBoilerTemp = app_Data.currentHotWaterBufferTemp;

        if (app_Data.currentHotWaterBufferTemp <= (initialDefrostingBoilerTemp - ReadSmartEeprom16(SEEP_ADDR_DEFROSTING_TEMP_FALL_BEFORE_ELEMENT_ON)))
        {   // Temperature falled 4 degrees Celcius beneath de defrosting starting temperature
            TurnOnHeatingElementHotWaterBuffer();
        }
        else if (app_Data.currentHotWaterBufferTemp >= 
                (initialDefrostingBoilerTemp - 
                ReadSmartEeprom16(SEEP_ADDR_DEFROSTING_TEMP_FALL_BEFORE_ELEMENT_ON)+ 
                ReadSmartEeprom16(SEEP_ADDR_DEFROSTING_TEMP_RISE_BEFORE_ELEMENT_OFF)))
        {   // Temperature rised to 1 degree beneath de defrosting starting temperature
            TurnOffHeatingElementHotWaterBuffer();
        }
        else{}
    }

        if (initialDefrostingBoilerTemp != TEMPERATURE_ALARM_VALUE)
        {   // Just came out of defrosting mode
            if (getStatusHeatingElementHotWaterBuffer() == true)
            {   // Hot water element is on
                if (app_Data.currentHotWaterBufferTemp >= 
                    (initialDefrostingBoilerTemp - 
                    ReadSmartEeprom16(SEEP_ADDR_DEFROSTING_TEMP_FALL_BEFORE_ELEMENT_ON)+ 
                    ReadSmartEeprom16(SEEP_ADDR_DEFROSTING_TEMP_RISE_BEFORE_ELEMENT_OFF)))
                {   // Temperature falled 3 degrees Celcius beneath de defrosting starting temperature
                    TurnOffHeatingElementHotWaterBuffer();
                    initialDefrostingBoilerTemp = TEMPERATURE_ALARM_VALUE;
                }                     
            }
            else
            {   // Hot water element is already off
                initialDefrostingBoilerTemp = TEMPERATURE_ALARM_VALUE;  // Reset initial defrosting temperature
            }
            //while(!releaseLoggingLock());
            return;
        }
}

void adjustSetpointOffset()
{
    
}

void HOT_WATER_HEATING_MODE_Initialize ( void )
{
    TurnOffHeatingElementHeatingBuffer();
    TurnOffHeatingElementHotWaterBuffer();
    
    setSecondCounterHeatingTask(UINT32_MAX);
    setSecondCounterHotwaterTask(UINT32_MAX);
    
    hot_water_heating_mode_data.initialHeatingBufferTemp = TEMPERATURE_ALARM_VALUE;
    hot_water_heating_mode_data.hotwaterPassive = false;
    hot_water_heating_mode_data.setpointHotWaterOffset = TEMPERATURE_ALARM_VALUE;
    hot_water_heating_mode_data.initialDefrostingBoilerTemp = TEMPERATURE_ALARM_VALUE;
    
    hot_water_heating_mode_data.state = HOT_WATER_HEATING_INITIALIZE_HEATING;
    return;
}



void HOT_WATER_HEATING_MODE_Tasks ( void )
{    
    
    if (areWeOnHotWaterMode() == true){
        // Already in one of the hot water modes
        // If sterilization goes to passive mode, go to heating
        if (getSterilizationModeData().state == PASSIVE){
            // Sterilization mode on passive, so go back to heating            
            hot_water_heating_mode_data.hotwaterPassive = false;
            hot_water_heating_mode_data.state = HOT_WATER_HEATING_INITIALIZE_HEATING;
        }
        return;
    }
    
    if (hot_water_heating_mode_data.hotwaterPassive == true){
        // Hot water is on passive, so in one of the heating states
        if (GetNtcTemperature(NTC_HOT_WATER_BUFFER) >= getHotwaterSetpoint()){
            // Setpoint reached in passive mode, now turn off
            setSecondCounterHotwaterTask(UINT32_MAX);
            TurnOffHeatingElementHotWaterBuffer();
            
            hot_water_heating_mode_data.hotwaterPassive = false;
            hot_water_heating_mode_data.setpointHotWaterOffset = TEMPERATURE_ALARM_VALUE;
        }
        
        if (getSecondCounterHotwaterTask() >= ReadSmartEeprom16(SEEP_ADDR_HOT_WATER_MAX_TIME_HEATING_ELEMENT_ON_IN_HEATING_MODE_SEC)){
            // Is more than 2 hours in passive hot water, so go back to active
            hot_water_heating_mode_data.state = HOT_WATER_HEATING_INITIALIZE_HOT_WATER;
        }
        return;
    }
    
    if (GetNtcTemperature(NTC_HOT_WATER_BUFFER) < (getHotwaterSetpoint() - getHotwaterDelta())){
        // Not on one of the hot water states or doing passive hot water
        // Hot water buffer is lower than setpoint - delta
        hot_water_heating_mode_data.state = HOT_WATER_HEATING_INITIALIZE_HOT_WATER;
        return;
    }

    switch ( hot_water_heating_mode_data.state )
    {
        /*        
         __    __   _______     ___   .___________. __  .__   __.   _______ 
        |  |  |  | |   ____|   /   \  |           ||  | |  \ |  |  /  _____|
        |  |__|  | |  |__     /  ^  \ `---|  |----`|  | |   \|  | |  |  __  
        |   __   | |   __|   /  /_\  \    |  |     |  | |  . `  | |  | |_ | 
        |  |  |  | |  |____ /  _____  \   |  |     |  | |  |\   | |  |__| | 
        |__|  |__| |_______/__/     \__\  |__|     |__| |__| \__|  \______| 
        */                                                                    

        case HOT_WATER_HEATING_INITIALIZE_HEATING:{
            
            TurnOffHeatingElementHeatingBuffer();
            setSecondCounterHeatingTask(UINT32_MAX);
            
            hot_water_heating_mode_data.initialHeatingBufferTemp = TEMPERATURE_ALARM_VALUE;
            hot_water_heating_mode_data.state = HOT_WATER_HEATING_IDLE_HEATING;
            break;
        }
        
        case HOT_WATER_HEATING_IDLE_HEATING:{
            
            if (getHeatpumpCompressorFrequency() != 0){
                // Compressor is running
                setSecondCounterHeatingTask(0);
                hot_water_heating_mode_data.initialHeatingBufferTemp = GetNtcTemperature(NTC_HEATING_BUFFER);
                
                hot_water_heating_mode_data.state = HOT_WATER_HEATING_RUNNING_ON_HEATING;
                break;
            }
            
            break;
        }
        
        case HOT_WATER_HEATING_RUNNING_ON_HEATING:{
            
            if ((getHeatpumpCompressorFrequency() == 0) && (isDefrostingActive() == false)){
                // Compressor is not running and is also not in defrosting
                TurnOffHeatingElementHeatingBuffer();
                hot_water_heating_mode_data.initialHeatingBufferTemp = TEMPERATURE_ALARM_VALUE;
                setSecondCounterHeatingTask(UINT32_MAX);

                hot_water_heating_mode_data.state = HOT_WATER_HEATING_IDLE_HEATING;
                break;
            }
            
            if (GetNtcTemperature(NTC_HEATING_BUFFER) >= hot_water_heating_mode_data.initialHeatingBufferTemp + ReadSmartEeprom16(SEEP_ADDR_HEATING_RISE_TEMP_IN_GIVEN_TIME)){
                // Temperature rised a set temperature in a set time
                setSecondCounterHeatingTask(0);
                hot_water_heating_mode_data.initialHeatingBufferTemp = GetNtcTemperature(NTC_HEATING_BUFFER);
                break;
            }
            
            if (getSecondCounterHeatingTask() >= ReadSmartEeprom16(SEEP_ADDR_HEATING_TIME_CONSTANT_SEC)){
                // Temperature not rised a set temperature in a set time
                setSecondCounterHeatingTask(0);
                hot_water_heating_mode_data.initialHeatingBufferTemp = GetNtcTemperature(NTC_HEATING_BUFFER);
                TurnOnHeatingElementHeatingBuffer();
                
                hot_water_heating_mode_data.state = HOT_WATER_HEATING_RUNNING_ON_HEATING_WITH_ELEMENT_ON;
                break;
            }
            
            break;
        }
        
        case HOT_WATER_HEATING_RUNNING_ON_HEATING_WITH_ELEMENT_ON:{
            
            if ((getHeatpumpCompressorFrequency() == 0) && (isDefrostingActive() == false)){
                // Compressor is not running and is also not in defrosting
                TurnOffHeatingElementHeatingBuffer();
                hot_water_heating_mode_data.initialHeatingBufferTemp = TEMPERATURE_ALARM_VALUE;
                setSecondCounterHeatingTask(UINT32_MAX);

                hot_water_heating_mode_data.state = HOT_WATER_HEATING_IDLE_HEATING;
                break;
            }
            
            if (GetNtcTemperature(NTC_HEATING_BUFFER) >= hot_water_heating_mode_data.initialHeatingBufferTemp + ReadSmartEeprom16(SEEP_ADDR_HEATING_RISE_TEMP_IN_GIVEN_TIME)){
                // Temperature rised a set temperature in a set time
                setSecondCounterHeatingTask(0);
                hot_water_heating_mode_data.initialHeatingBufferTemp = GetNtcTemperature(NTC_HEATING_BUFFER);
                break;
            }
            
            if (getSecondCounterHeatingTask() >= ReadSmartEeprom16(SEEP_ADDR_HEATING_TIME_CONSTANT_SEC)){
                // Temperature not rised a set temperature in a set time
                setSecondCounterHeatingTask(0);
                hot_water_heating_mode_data.initialHeatingBufferTemp = GetNtcTemperature(NTC_HEATING_BUFFER);
                TurnOffHeatingElementHeatingBuffer();
                
                hot_water_heating_mode_data.state = HOT_WATER_HEATING_RUNNING_ON_HEATING;
                break;
            }
            
            break;
        }
        /*        
         __    __    ______   .___________.   ____    __    ____  ___   .___________. _______ .______      
        |  |  |  |  /  __  \  |           |   \   \  /  \  /   / /   \  |           ||   ____||   _  \     
        |  |__|  | |  |  |  | `---|  |----`    \   \/    \/   / /  ^  \ `---|  |----`|  |__   |  |_)  |    
        |   __   | |  |  |  |     |  |          \            / /  /_\  \    |  |     |   __|  |      /     
        |  |  |  | |  `--'  |     |  |           \    /\    / /  _____  \   |  |     |  |____ |  |\  \----.
        |__|  |__|  \______/      |__|            \__/  \__/ /__/     \__\  |__|     |_______|| _| `._____|
        */                                                                                                  

        case HOT_WATER_HEATING_INITIALIZE_HOT_WATER:{
            
            setSecondCounterHeatingTask(UINT32_MAX);
            setSecondCounterHotwaterTask(0);
            
            TurnOffHeatingElementHeatingBuffer();
            TurnOffHeatingElementHotWaterBuffer();
            
            hot_water_heating_mode_data.hotwaterPassive = false;
            hot_water_heating_mode_data.setpointHotWaterOffset = ReadSmartEeprom16(SEEP_ADDR_HOT_WATER_SETPOINT_OFFSET_START);
            hot_water_heating_mode_data.state = HOT_WATER_HEATING_STATE_WAIT_FOR_MINIMAL_TIME_IN_HOT_WATER;
            
            break;
        }
        
        case HOT_WATER_HEATING_STATE_WAIT_FOR_MINIMAL_TIME_IN_HOT_WATER:{
            checkDefrosting();
            adjustSetpointOffset();
            
            if (getSecondCounterHotwaterTask() >= ReadSmartEeprom16(SEEP_ADDR_HOT_WATER_MIN_TIME_IN_HOT_WATER_MODE)){
                // Minimal time in hot water passed
                hot_water_heating_mode_data.state = HOT_WATER_HEATING_STATE_RUNNING_IN_HOT_WATER;
                break;
            }
            
            break;
        }
        
        case HOT_WATER_HEATING_STATE_RUNNING_IN_HOT_WATER:{
            checkDefrosting();
            adjustSetpointOffset();
            
            if ((getHeatpumpCompressorFrequency() == 0) && (isDefrostingActive() == false)){
                // Compressor is not running and is also not in defrosting, so go back to heating
                TurnOffHeatingElementHotWaterBuffer();
                setSecondCounterHotwaterTask(UINT32_MAX);
                
                hot_water_heating_mode_data.hotwaterPassive = false;
                hot_water_heating_mode_data.setpointHotWaterOffset = TEMPERATURE_ALARM_VALUE;
                hot_water_heating_mode_data.state = HOT_WATER_HEATING_INITIALIZE_HEATING;
                break;
            }
            
            if (getSecondCounterHotwaterTask() >= ReadSmartEeprom16(SEEP_ADDR_HOT_WATER_RUNNING_TIME_BEFORE_TURNING_ON_HEATING_ELEMENT)){
                // 2 hours passed in hot water and not reached setpoint, turn on element, time counter not needed anymore
                TurnOnHeatingElementHotWaterBuffer();
                setSecondCounterHotwaterTask(UINT32_MAX);
                hot_water_heating_mode_data.state = HOT_WATER_HEATING_STATE_RUNNING_WITH_ELEMENT_ON_IN_HOT_WATER;
                break;
            }
            
            break;
        }
        
        case HOT_WATER_HEATING_STATE_RUNNING_WITH_ELEMENT_ON_IN_HOT_WATER:{
            checkDefrosting();
            adjustSetpointOffset();
            
            if ((getHeatpumpCompressorFrequency() == 0) && (isDefrostingActive() == false)){
                // Compressor is not running and is also not in defrosting, so go back to heating
                TurnOffHeatingElementHotWaterBuffer();
                setSecondCounterHotwaterTask(UINT32_MAX);
                
                hot_water_heating_mode_data.hotwaterPassive = false;
                hot_water_heating_mode_data.setpointHotWaterOffset = TEMPERATURE_ALARM_VALUE;
                hot_water_heating_mode_data.state = HOT_WATER_HEATING_INITIALIZE_HEATING;
                break;
            }
            
            if (GetThermostatContact() == true)
            {   // Thermostat contact has been made, turn on passive mode, reset timer and go to heating
                setSecondCounterHotwaterTask(0);
                
                hot_water_heating_mode_data.hotwaterPassive = true;
                hot_water_heating_mode_data.setpointHotWaterOffset = TEMPERATURE_ALARM_VALUE;
                hot_water_heating_mode_data.state = HOT_WATER_HEATING_INITIALIZE_HEATING;
                break;
            }
            
            break;
        }
        
        default:{
            break;
        }
    }
    
}


