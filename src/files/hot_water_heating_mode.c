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
#include "sterilization.h"
#include "defrosting.h"
#include "modbus\heatpump_parameters.h"

extern HOT_WATER_HEATING_MODE_DATA hot_water_heating_mode_data;

bool getHeatingElementBoolFromHotwaterHeatingMode() {
    return hot_water_heating_mode_data.HeatingElementOn;
}

bool getHotwaterElementBoolFromHotwaterHeatingMode() {
    return hot_water_heating_mode_data.HotwaterElementOn;
}

const char * getHotwaterHeatingStateToString()
{
    switch (hot_water_heating_mode_data.state)
    {
        case(HOT_WATER_HEATING_INITIALIZE_HEATING): {
            return "0, Init heating";
            break;
        }
        
        case(HOT_WATER_HEATING_IDLE_HEATING): {
            return "1, Idle heating";
            break;
        }
        
        case(HOT_WATER_HEATING_RUNNING_ON_HEATING): {
            return "2, Running heating";
            break;
        }
        
        case(HOT_WATER_HEATING_RUNNING_ON_HEATING_WITH_ELEMENT_ON): {
            return "3, Running heating with element on";
            break;
        }
        
        case(HOT_WATER_HEATING_INITIALIZE_HOT_WATER): {
            return "4, Init hotwater";
            break;
        }
        
        case(HOT_WATER_HEATING_STATE_WAIT_FOR_MINIMAL_TIME_IN_HOT_WATER): {
            return "5, Minimal time in hot water";
            break;
        }
        
        case(HOT_WATER_HEATING_STATE_RUNNING_IN_HOT_WATER): {
            return "6, Running hotwater";
            break;
        }
        
        case(HOT_WATER_HEATING_STATE_RUNNING_WITH_ELEMENT_ON_IN_HOT_WATER): {
            return "7, Running hotwater with element on";
            break;
        }
        
        default:{
            return "-1, Unkown";
            break;
        }
        return "-1, Unkown";
    }
}

bool areWeOnHotWaterModeInHotWaterAndHeatingMode()
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


void adjustSetpointOffsetHotWater()
{
    int16_t hotwaterBufferTemperature = GetNtcTemperature(NTC_HOT_WATER_BUFFER);
    int16_t hotwaterSetpoint = getHotwaterSetpoint();
    
    if ((hotwaterBufferTemperature == TEMPERATURE_ALARM_VALUE) || (hotwaterSetpoint == TEMPERATURE_ALARM_VALUE)) {
        // No temperature or setpoint known yet
        return;
    }
    
    if (hotwaterBufferTemperature >= getHotwaterSetpoint())
    {   // Hot water buffer tempereature is equal or higher than actual setpoint, so reset offset to 0
        hot_water_heating_mode_data.setpointHotWaterOffset = 0; 
        return;
    }
    
    if ((getHeatpumpReturnWaterTemperature() >= (getHotwaterSetpoint() + hot_water_heating_mode_data.setpointHotWaterOffset - 20)) && (hot_water_heating_mode_data.setpointHotWaterOffset != 0) && (hot_water_heating_mode_data.setpointHotWaterOffset != TEMPERATURE_ALARM_VALUE)){   
        // Retour water temperature has come within 2 degree celcius of setpoint, increase offset with 2 degrees
        hot_water_heating_mode_data.setpointHotWaterOffset += ReadSmartEeprom16(SEEP_ADDR_HOT_WATER_SETPOINT_OFFSET_STEPS);  
        return;
    }
}


int16_t determineCorrectSetpoint() {
    if ((hot_water_heating_mode_data.state == HOT_WATER_HEATING_INITIALIZE_HEATING) ||
        (hot_water_heating_mode_data.state == HOT_WATER_HEATING_IDLE_HEATING) ||
        (hot_water_heating_mode_data.state == HOT_WATER_HEATING_RUNNING_ON_HEATING) ||
        (hot_water_heating_mode_data.state == HOT_WATER_HEATING_RUNNING_ON_HEATING_WITH_ELEMENT_ON) ) {
        return getHeatingSetpoint();
    }
    
    if ((hot_water_heating_mode_data.state == HOT_WATER_HEATING_INITIALIZE_HOT_WATER) ||
        (hot_water_heating_mode_data.state == HOT_WATER_HEATING_STATE_WAIT_FOR_MINIMAL_TIME_IN_HOT_WATER) ||
        (hot_water_heating_mode_data.state == HOT_WATER_HEATING_STATE_RUNNING_IN_HOT_WATER) ||
        (hot_water_heating_mode_data.state == HOT_WATER_HEATING_STATE_RUNNING_WITH_ELEMENT_ON_IN_HOT_WATER) ) {
        return (getHotwaterSetpoint() + hot_water_heating_mode_data.setpointHotWaterOffset); 
    }
    
    return TEMPERATURE_ALARM_VALUE;
}

void HOT_WATER_HEATING_MODE_Initialize ( void )
{
    //TurnOffHeatingElementHeatingBuffer();
    //TurnOffHeatingElementHotWaterBuffer();
    
    setSecondCounterHeatingTask(UINT32_MAX);
    setSecondCounterHotwaterTask(UINT32_MAX);
    
    hot_water_heating_mode_data.HeatingElementOn = false;
    hot_water_heating_mode_data.HotwaterElementOn = false;
    
    hot_water_heating_mode_data.initialHeatingBufferTemp = TEMPERATURE_ALARM_VALUE;
    hot_water_heating_mode_data.hotwaterPassive = false;
    hot_water_heating_mode_data.setpointHotWaterOffset = TEMPERATURE_ALARM_VALUE;
    
    hot_water_heating_mode_data.state = HOT_WATER_HEATING_INITIALIZE_HEATING;
    return;
}



void HOT_WATER_HEATING_MODE_Tasks ( void )
{   
    int16_t heatingBufferTemperature = GetNtcTemperature(NTC_HEATING_BUFFER);
    
    int16_t hotwaterBufferTemperature = GetNtcTemperature(NTC_HOT_WATER_BUFFER);
    int16_t hotwaterSetpoint = getHotwaterSetpoint();
    int16_t hotwaterDelta = getHotwaterDelta();
    
    if ((hotwaterBufferTemperature == TEMPERATURE_ALARM_VALUE) || (hotwaterSetpoint == TEMPERATURE_ALARM_VALUE) || (hotwaterDelta == TEMPERATURE_ALARM_VALUE)) {
        // Needed values not yet known
        return;
    }
    
    if (areWeOnHotWaterModeInHotWaterAndHeatingMode() == true){
        // Already in one of the hot water modes
        // If sterilization goes to passive mode, go to heating
        if (getSterilisationMode() == PASSIVE){
            // Sterilization mode on passive, so go back to heating            
            hot_water_heating_mode_data.hotwaterPassive = false;
            hot_water_heating_mode_data.state = HOT_WATER_HEATING_INITIALIZE_HEATING;
            return;
        }
    }
    
    if (hot_water_heating_mode_data.hotwaterPassive == true){
        // Hot water is on passive, so in one of the heating states
        if (hotwaterBufferTemperature >= hotwaterSetpoint){
            // Setpoint reached in passive mode, now turn off
            setSecondCounterHotwaterTask(UINT32_MAX);
            //TurnOffHeatingElementHotWaterBuffer();
            hot_water_heating_mode_data.HotwaterElementOn = false;
            
            hot_water_heating_mode_data.hotwaterPassive = false;
            hot_water_heating_mode_data.setpointHotWaterOffset = TEMPERATURE_ALARM_VALUE;
            return;
        }
        
        if ((getSecondCounterHotwaterTask() != UINT32_MAX) && (getSecondCounterHotwaterTask() >= ReadSmartEeprom16(SEEP_ADDR_HOT_WATER_MAX_TIME_HEATING_ELEMENT_ON_IN_HEATING_MODE_SEC))) {
            // Is more than 2 hours in passive hot water, so go back to active
            hot_water_heating_mode_data.hotwaterPassive = false;
            hot_water_heating_mode_data.state = HOT_WATER_HEATING_INITIALIZE_HOT_WATER;
            return;
        }
    }
    
    if ((areWeOnHotWaterModeInHotWaterAndHeatingMode() == false) && (hot_water_heating_mode_data.hotwaterPassive == false) && (hotwaterBufferTemperature < (hotwaterSetpoint - hotwaterDelta)) && (getSterilisationMode() != PASSIVE)) {
        // Not in hot water state yet
        // Not on one of the hot water states or doing passive hot water
        // Hot water buffer is lower than setpoint - delta
        hot_water_heating_mode_data.state = HOT_WATER_HEATING_INITIALIZE_HOT_WATER;
        return;
    }
    
    
    setActiveModeControllerHeatpumpSetpoint(determineCorrectSetpoint());
    
    setActiveModeControllerHeatpumpRunningMode(SET_MODE_HEATING);

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
        
        // 0
        case HOT_WATER_HEATING_INITIALIZE_HEATING:{
            
            //TurnOffHeatingElementHeatingBuffer();
            hot_water_heating_mode_data.HeatingElementOn = false;
            setSecondCounterHeatingTask(UINT32_MAX);
            
            hot_water_heating_mode_data.initialHeatingBufferTemp = TEMPERATURE_ALARM_VALUE;
            hot_water_heating_mode_data.state = HOT_WATER_HEATING_IDLE_HEATING;
            break;
        }
        
        // 1
        case HOT_WATER_HEATING_IDLE_HEATING:{
            
            if (getHeatpumpCompressorFrequency() != 0){
                // Compressor is running
                setSecondCounterHeatingTask(0);
                hot_water_heating_mode_data.initialHeatingBufferTemp = heatingBufferTemperature;
                
                hot_water_heating_mode_data.state = HOT_WATER_HEATING_RUNNING_ON_HEATING;
                break;
            }
            
            break;
        }
        
        // 2
        case HOT_WATER_HEATING_RUNNING_ON_HEATING:{
            
            if ((getHeatpumpCompressorFrequency() == 0) && (isDefrostingActive() == false)){
                // Compressor is not running and is also not in defrosting
                //TurnOffHeatingElementHeatingBuffer();
                hot_water_heating_mode_data.HeatingElementOn = false;
                hot_water_heating_mode_data.initialHeatingBufferTemp = TEMPERATURE_ALARM_VALUE;
                setSecondCounterHeatingTask(UINT32_MAX);

                hot_water_heating_mode_data.state = HOT_WATER_HEATING_IDLE_HEATING;
                break;
            }
            
            if (heatingBufferTemperature >= hot_water_heating_mode_data.initialHeatingBufferTemp + ReadSmartEeprom16(SEEP_ADDR_HEATING_RISE_TEMP_IN_GIVEN_TIME)){
                // Temperature rised a set temperature in a set time
                setSecondCounterHeatingTask(0);
                hot_water_heating_mode_data.initialHeatingBufferTemp = heatingBufferTemperature;
                break;
            }
            
            if ((getSecondCounterHeatingTask() >= ReadSmartEeprom16(SEEP_ADDR_HEATING_TIME_CONSTANT_SEC)) && (getSecondCounterHeatingTask() != UINT32_MAX)){
                // Temperature not rised a set temperature in a set time
                setSecondCounterHeatingTask(0);
                hot_water_heating_mode_data.initialHeatingBufferTemp = heatingBufferTemperature;
                //TurnOnHeatingElementHeatingBuffer();
                hot_water_heating_mode_data.HeatingElementOn = true;
                
                hot_water_heating_mode_data.state = HOT_WATER_HEATING_RUNNING_ON_HEATING_WITH_ELEMENT_ON;
                break;
            }
            
            break;
        }
        
        // 3
        case HOT_WATER_HEATING_RUNNING_ON_HEATING_WITH_ELEMENT_ON:{
            
            if ((getHeatpumpCompressorFrequency() == 0) && (isDefrostingActive() == false)){
                // Compressor is not running and is also not in defrosting
                //TurnOffHeatingElementHeatingBuffer();
                hot_water_heating_mode_data.HeatingElementOn = false;
                hot_water_heating_mode_data.initialHeatingBufferTemp = TEMPERATURE_ALARM_VALUE;
                setSecondCounterHeatingTask(UINT32_MAX);

                hot_water_heating_mode_data.state = HOT_WATER_HEATING_IDLE_HEATING;
                break;
            }
            
            if (heatingBufferTemperature >= hot_water_heating_mode_data.initialHeatingBufferTemp + ReadSmartEeprom16(SEEP_ADDR_HEATING_RISE_TEMP_IN_GIVEN_TIME)){
                // Temperature rised a set temperature in a set time
                setSecondCounterHeatingTask(0);
                hot_water_heating_mode_data.initialHeatingBufferTemp = heatingBufferTemperature;
                break;
            }
            
            if ((getSecondCounterHeatingTask() >= ReadSmartEeprom16(SEEP_ADDR_HEATING_TIME_CONSTANT_SEC)) && (getSecondCounterHeatingTask() != UINT32_MAX)){
                // Temperature not rised a set temperature in a set time
                setSecondCounterHeatingTask(0);
                hot_water_heating_mode_data.initialHeatingBufferTemp = heatingBufferTemperature;
                //TurnOffHeatingElementHeatingBuffer();
                hot_water_heating_mode_data.HeatingElementOn = false;
                
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
        
        // 4
        case HOT_WATER_HEATING_INITIALIZE_HOT_WATER:{
            
            setSecondCounterHeatingTask(UINT32_MAX);
            setSecondCounterHotwaterTask(0);
            
            //TurnOffHeatingElementHeatingBuffer();
            //TurnOffHeatingElementHotWaterBuffer();
            hot_water_heating_mode_data.HeatingElementOn = false;
            hot_water_heating_mode_data.HotwaterElementOn = false;
            
            hot_water_heating_mode_data.initialHeatingBufferTemp = TEMPERATURE_ALARM_VALUE;
            
            hot_water_heating_mode_data.hotwaterPassive = false;
            hot_water_heating_mode_data.setpointHotWaterOffset = ReadSmartEeprom16(SEEP_ADDR_HOT_WATER_SETPOINT_OFFSET_START);
            hot_water_heating_mode_data.state = HOT_WATER_HEATING_STATE_WAIT_FOR_MINIMAL_TIME_IN_HOT_WATER;
            break;
        }
        
        // 5
        case HOT_WATER_HEATING_STATE_WAIT_FOR_MINIMAL_TIME_IN_HOT_WATER:{
            
            adjustSetpointOffsetHotWater();
            
            if ((getSecondCounterHotwaterTask() >= ReadSmartEeprom16(SEEP_ADDR_HOT_WATER_MIN_TIME_IN_HOT_WATER_MODE)) && (getSecondCounterHotwaterTask() != UINT32_MAX)) {
                // Minimal time in hot water passed
                hot_water_heating_mode_data.state = HOT_WATER_HEATING_STATE_RUNNING_IN_HOT_WATER;
                break;
            }
            
            break;
        }
        
        // 6
        case HOT_WATER_HEATING_STATE_RUNNING_IN_HOT_WATER:{
            
            adjustSetpointOffsetHotWater();
            
            if ((getHeatpumpCompressorFrequency() == 0) && (isDefrostingActive() == false)){
                // Compressor is not running and is also not in defrosting, so go back to heating
                //TurnOffHeatingElementHotWaterBuffer();
                hot_water_heating_mode_data.HotwaterElementOn = false;
                setSecondCounterHotwaterTask(UINT32_MAX);
                
                hot_water_heating_mode_data.hotwaterPassive = false;
                hot_water_heating_mode_data.setpointHotWaterOffset = TEMPERATURE_ALARM_VALUE;
                hot_water_heating_mode_data.state = HOT_WATER_HEATING_INITIALIZE_HEATING;
                break;
            }
            
            if ((getSecondCounterHotwaterTask() >= ReadSmartEeprom16(SEEP_ADDR_HOT_WATER_RUNNING_TIME_BEFORE_TURNING_ON_HEATING_ELEMENT)) && (getSecondCounterHotwaterTask() != UINT32_MAX)){
                // 2 hours passed in hot water and not reached setpoint, turn on element, time counter not needed anymore
                //TurnOnHeatingElementHotWaterBuffer();
                hot_water_heating_mode_data.HotwaterElementOn = true;
                setSecondCounterHotwaterTask(UINT32_MAX);
                hot_water_heating_mode_data.state = HOT_WATER_HEATING_STATE_RUNNING_WITH_ELEMENT_ON_IN_HOT_WATER;
                break;
            }
            
            break;
        }
        
        // 7
        case HOT_WATER_HEATING_STATE_RUNNING_WITH_ELEMENT_ON_IN_HOT_WATER:{
            
            adjustSetpointOffsetHotWater();
            
            if ((getHeatpumpCompressorFrequency() == 0) && (isDefrostingActive() == false)){
                // Compressor is not running and is also not in defrosting, so go back to heating
                //TurnOffHeatingElementHotWaterBuffer();
                hot_water_heating_mode_data.HotwaterElementOn = false;
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


