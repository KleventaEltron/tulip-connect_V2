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


void HOT_WATER_HEATING_MODE_Initialize ( void )
{
    setSecondCounterHeatingTask(UINT32_MAX);
    setSecondCounterHotwaterTask(UINT32_MAX);
    
    hot_water_heating_mode_data.initialHeatingBufferTemp = TEMPERATURE_ALARM_VALUE;
    
    TurnOffHeatingElementHeatingBuffer();
    TurnOffHeatingElementHotWaterBuffer();
    
    hot_water_heating_mode_data.state = HOT_WATER_HEATING_INITIALIZE;
    return;
}



void HOT_WATER_HEATING_MODE_Tasks ( void )
{    
    switch ( hot_water_heating_mode_data.state )
    {
        case HOT_WATER_HEATING_INITIALIZE:{
            
            setSecondCounterHeatingTask(UINT32_MAX);
            setSecondCounterHotwaterTask(UINT32_MAX);
            
            hot_water_heating_mode_data.initialHeatingBufferTemp = TEMPERATURE_ALARM_VALUE;

            TurnOffHeatingElementHeatingBuffer();
            TurnOffHeatingElementHotWaterBuffer();
            
            //SYS_CONSOLE_PRINT("HOT_WATER_HEATING_INITIALIZE\r\n");
            hot_water_heating_mode_data.state = HOT_WATER_HEATING_IDLE;
            break;
        }
        
        case HOT_WATER_HEATING_IDLE:{
            
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

                hot_water_heating_mode_data.state = HOT_WATER_HEATING_IDLE;
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

                hot_water_heating_mode_data.state = HOT_WATER_HEATING_IDLE;
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
        
        default:{
            break;
        }
    }
    
}


