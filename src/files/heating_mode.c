#include <stddef.h>                    
#include <stdbool.h>                   
#include <stdlib.h>                   
#include <string.h>
#include <stdio.h>
#include "definitions.h"    

#include "heating_mode.h"
#include "states.h"

#include "eeprom.h"
#include "time_counters.h"
#include "ntc.h"


extern HEATING_MODE_DATA heating_mode_data;

bool getHeatingElementBoolFromHeatingMode() {
    return heating_mode_data.HeatingElementOn;
}

void HEATING_MODE_Initialize ( void )
{
    setSecondCounterHeatingTask(UINT32_MAX);
    heating_mode_data.initialBufferTemp = TEMPERATURE_ALARM_VALUE;
    //TurnOffHeatingElementHeatingBuffer();
    heating_mode_data.HeatingElementOn = false;
    
    heating_mode_data.state = HEATING_INITIALIZE;
    return;
}



void HEATING_MODE_Tasks ( void )
{        
    /*
    if (HeatingHotWaterTimerExpired() == true){ // HOAKS
        if (DebugDipSwitch() == true)
        {
            memset(debugBuffer, 0, sizeof(debugBuffer));
            sprintf(debugBuffer, "\r\nState: %d\r\nStart: %d\r\nTimer: %d\r\nElement: %d\r\n", heating_mode_data.state, heating_mode_data.initialBufferTemp, (int)getSecondCounterHeatingTask(), (int)getStatusHeatingElementHeatingBuffer());
            SYS_DEBUG_PRINT(SYS_ERROR_ERROR, debugBuffer);
        }
    }
    */
    
    setActiveModeControllerHeatpumpSetpoint(getHeatingSetpoint());
    
    switch ( heating_mode_data.state )
    {
        case HEATING_INITIALIZE:{
            
            //TurnOffHeatingElementHeatingBuffer();
            heating_mode_data.HeatingElementOn = false;
            heating_mode_data.initialBufferTemp = TEMPERATURE_ALARM_VALUE;
            setSecondCounterHeatingTask(UINT32_MAX);
            
            heating_mode_data.state = HEATING_IDLE;
            break;
        }
        
        case HEATING_IDLE:{
            
            if (getHeatpumpCompressorFrequency() != 0){
                // Compressor is running
                setSecondCounterHeatingTask(0);
                heating_mode_data.initialBufferTemp = GetNtcTemperature(NTC_HEATING_BUFFER);
                
                heating_mode_data.state = HEATING_RUNNING;
                break;
            }
            
            break;
        }
        
        case HEATING_RUNNING:{
            
            if ((getHeatpumpCompressorFrequency() == 0) && (isDefrostingActive() == false)){
                // Compressor is not running and is also not in defrosting
                //TurnOffHeatingElementHeatingBuffer();
                heating_mode_data.HeatingElementOn = false;
                heating_mode_data.initialBufferTemp = TEMPERATURE_ALARM_VALUE;
                setSecondCounterHeatingTask(UINT32_MAX);

                heating_mode_data.state = HEATING_IDLE;
                break;
            }
            
            if (GetNtcTemperature(NTC_HEATING_BUFFER) >= heating_mode_data.initialBufferTemp + ReadSmartEeprom16(SEEP_ADDR_HEATING_RISE_TEMP_IN_GIVEN_TIME)){
                // Temperature rised a set temperature in a set time
                setSecondCounterHeatingTask(0);
                heating_mode_data.initialBufferTemp = GetNtcTemperature(NTC_HEATING_BUFFER);
                break;
            }
            
            if (getSecondCounterHeatingTask() >= ReadSmartEeprom16(SEEP_ADDR_HEATING_TIME_CONSTANT_SEC)){
                // Temperature not rised a set temperature in a set time
                setSecondCounterHeatingTask(0);
                heating_mode_data.initialBufferTemp = GetNtcTemperature(NTC_HEATING_BUFFER);
                //TurnOnHeatingElementHeatingBuffer();
                heating_mode_data.HeatingElementOn = true;
                
                heating_mode_data.state = HEATING_RUNNING_WITH_ELEMENT_ON;
                break;
            }
            
            break;
        }
        
        case HEATING_RUNNING_WITH_ELEMENT_ON:{
            
            if ((getHeatpumpCompressorFrequency() == 0) && (isDefrostingActive() == false)){
                // Compressor is not running and is also not in defrosting
                //TurnOffHeatingElementHeatingBuffer();
                heating_mode_data.HeatingElementOn = false;
                heating_mode_data.initialBufferTemp = TEMPERATURE_ALARM_VALUE;
                setSecondCounterHeatingTask(UINT32_MAX);

                heating_mode_data.state = HEATING_IDLE;
                break;
            }
            
            if (GetNtcTemperature(NTC_HEATING_BUFFER) >= heating_mode_data.initialBufferTemp + ReadSmartEeprom16(SEEP_ADDR_HEATING_RISE_TEMP_IN_GIVEN_TIME)){
                // Temperature rised a set temperature in a set time
                setSecondCounterHeatingTask(0);
                heating_mode_data.initialBufferTemp = GetNtcTemperature(NTC_HEATING_BUFFER);
                break;
            }
            
            if (getSecondCounterHeatingTask() >= ReadSmartEeprom16(SEEP_ADDR_HEATING_TIME_CONSTANT_SEC)){
                // Temperature not rised a set temperature in a set time
                setSecondCounterHeatingTask(0);
                heating_mode_data.initialBufferTemp = GetNtcTemperature(NTC_HEATING_BUFFER);
                //TurnOffHeatingElementHeatingBuffer();
                heating_mode_data.HeatingElementOn = false;
                
                heating_mode_data.state = HEATING_RUNNING;
                break;
            }
            
            break;
        }
        
        default:{
            HEATING_MODE_Initialize();
            break;
        }
    }
    
}


