#include <stddef.h>                    
#include <stdbool.h>                   
#include <stdlib.h>                   
#include <string.h>
#include <stdio.h>
#include "definitions.h"    

#include "hot_water_mode.h"
#include "states.h"

#include "eeprom.h"
#include "time_counters.h"
#include "ntc.h"
#include "sterilization.h"
#include "defrosting.h"
#include "modbus\heatpump_parameters.h"

extern HOT_WATER_MODE_DATA hot_water_mode_data;


bool getHotwaterElementBoolFromHotwaterMode() {
    return hot_water_mode_data.HotwaterElementOn;
}

void adjustSetpointOffsetHotWaterMode()
{
    int16_t hotwaterBufferTemperature = GetNtcTemperature(NTC_HOT_WATER_BUFFER);
    int16_t hotwaterSetpoint = getHotwaterSetpoint();
    
    if ((hotwaterBufferTemperature == TEMPERATURE_ALARM_VALUE) || (hotwaterSetpoint == TEMPERATURE_ALARM_VALUE)) {
        // No temperature or setpoint known yet
        return;
    }
    
    if (hotwaterBufferTemperature >= getHotwaterSetpoint())
    {   // Hot water buffer tempereature is equal or higher than actual setpoint, so reset offset to 0
        hot_water_mode_data.setpointHotWaterOffset = 0; 
        return;
    }
    
    if ((getHeatpumpReturnWaterTemperature() >= (getHotwaterSetpoint() + hot_water_mode_data.setpointHotWaterOffset - 20)) 
            && (hot_water_mode_data.setpointHotWaterOffset != 0) 
            && (hot_water_mode_data.setpointHotWaterOffset != TEMPERATURE_ALARM_VALUE)){   
        // Retour water temperature has come within 2 degree celcius of setpoint, increase offset with 2 degrees
        hot_water_mode_data.setpointHotWaterOffset += ReadSmartEeprom16(SEEP_ADDR_HOT_WATER_SETPOINT_OFFSET_STEPS);  
        return;
    }
}



int16_t determineCorrectSetpointHotWaterMode() {  
    return (getHotwaterSetpoint() + hot_water_mode_data.setpointHotWaterOffset); 
}


const char * getHotWaterStateToString()
{
    switch (hot_water_mode_data.state)
    {
        case(HOT_WATER_INITIALIZE): {
            return "0, Init hotwater";
            break;
        }
        
        case(HOT_WATER_IDLE): {
            return "1, Idle hotwater";
            break;
        }
        
        case(HOT_WATER_STATE_RUNNING_IN_HOT_WATER): {
            return "2, Running hotwater";
            break;
        }
        
        case(HOT_WATER_STATE_RUNNING_WITH_ELEMENT_ON_IN_HOT_WATER): {
            return "3, Running hotwater with element";
            break;
        }
        
        default:{
            return "-1, Unkown";
            break;
        }
        return "-1, Unkown";
    }
}


void HOT_WATER_MODE_Initialize ( void )
{
    setSecondCounterHotwaterTask(0);
    hot_water_mode_data.HotwaterElementOn = false;
    hot_water_mode_data.setpointHotWaterOffset = ReadSmartEeprom16(SEEP_ADDR_HOT_WATER_SETPOINT_OFFSET_START);
    
    hot_water_mode_data.state = HOT_WATER_INITIALIZE;
    return;
}



void HOT_WATER_MODE_Tasks ( void )
{    
    int16_t hotwaterBufferTemperature = GetNtcTemperature(NTC_HOT_WATER_BUFFER);
    int16_t hotwaterSetpoint = getHotwaterSetpoint();
    int16_t hotwaterDelta = getHotwaterDelta();
    
    if ((hotwaterBufferTemperature == TEMPERATURE_ALARM_VALUE) || (hotwaterSetpoint == TEMPERATURE_ALARM_VALUE) || (hotwaterDelta == TEMPERATURE_ALARM_VALUE)) {
        // Needed values not yet known
        return;
    }
    
    /*
    if(hot_water_mode_data.state == HOT_WATER_IDLE 
            &&(hotwaterBufferTemperature < (hotwaterSetpoint - hotwaterDelta)) 
            && (getSterilisationMode() != PASSIVE)) {
        // Hot water buffer is lower than setpoint - delta
        hot_water_mode_data.state = HOT_WATER_INITIALIZE;
        return;        
    }
    */
    
    
    setActiveModeControllerHeatpumpSetpoint(determineCorrectSetpointHotWaterMode());
    //setActiveModeControllerHeatpumpSetpoint(getHotwaterSetpoint());
    setActiveModeControllerHeatpumpRunningMode(SET_MODE_HEATING);
    
    
    
    switch ( hot_water_mode_data.state )
    {
        case HOT_WATER_INITIALIZE:{
            setSecondCounterHotwaterTask(UINT32_MAX);
                        
            hot_water_mode_data.HotwaterElementOn = false;
            hot_water_mode_data.setpointHotWaterOffset = ReadSmartEeprom16(SEEP_ADDR_HOT_WATER_SETPOINT_OFFSET_START);
            
            hot_water_mode_data.state = HOT_WATER_IDLE;
            break;
        }
        
        case HOT_WATER_IDLE:{
            
            if (getHeatpumpCompressorFrequency() != 0){
                // Compressor is running
                setSecondCounterHotwaterTask(0);
                //hot_water_mode_data.initialBufferTemp = GetNtcTemperature(NTC_HOT_WATER_BUFFER);
                
                hot_water_mode_data.state = HOT_WATER_STATE_RUNNING_IN_HOT_WATER;
                break;
            }    
            //setSecondCounterHotwaterTask(UINT32_MAX);
            
            break;
        }
        
        case HOT_WATER_STATE_RUNNING_IN_HOT_WATER:{
            
            adjustSetpointOffsetHotWaterMode();
            
            if ((getHeatpumpCompressorFrequency() == 0) && (isDefrostingActive() == false)){
                // Compressor is not running and is also not in defrosting, so go back to heating
                //TurnOffHeatingElementHotWaterBuffer();
                hot_water_mode_data.HotwaterElementOn = false;
                setSecondCounterHotwaterTask(UINT32_MAX);
                
                hot_water_mode_data.setpointHotWaterOffset = TEMPERATURE_ALARM_VALUE;
                hot_water_mode_data.state = HOT_WATER_IDLE;
                break;
            }
            
            if ((getSecondCounterHotwaterTask() >= ReadSmartEeprom16(SEEP_ADDR_HOT_WATER_RUNNING_TIME_BEFORE_TURNING_ON_HEATING_ELEMENT)) && (getSecondCounterHotwaterTask() != UINT32_MAX)){
                // 2 hours passed in hot water and not reached setpoint, turn on element, time counter not needed anymore
                //TurnOnHeatingElementHotWaterBuffer();
                hot_water_mode_data.HotwaterElementOn = true;
                setSecondCounterHotwaterTask(UINT32_MAX);
                hot_water_mode_data.state = HOT_WATER_STATE_RUNNING_WITH_ELEMENT_ON_IN_HOT_WATER;
                break;
            }
            break;
        }
        
        case HOT_WATER_STATE_RUNNING_WITH_ELEMENT_ON_IN_HOT_WATER:{
            
            adjustSetpointOffsetHotWaterMode();
            
            if ((getHeatpumpCompressorFrequency() == 0) && (isDefrostingActive() == false)){
                // Compressor is not running and is also not in defrosting, so go back to heating
                //TurnOffHeatingElementHotWaterBuffer();
                hot_water_mode_data.HotwaterElementOn = false;
                setSecondCounterHotwaterTask(UINT32_MAX);
                
                hot_water_mode_data.setpointHotWaterOffset = TEMPERATURE_ALARM_VALUE;
                hot_water_mode_data.state = HOT_WATER_IDLE;
                break;
            }   
            break;
        }        
        
        default:{
            break;
        }
    }
    
}


