#include <stddef.h>                    
#include <stdbool.h>                   
#include <stdlib.h>                   
#include <string.h>
#include <stdio.h>
#include "definitions.h"    

#include "cooling_mode.h"
#include "states.h"

#include "eeprom.h"
#include "ntc.h"
#include "modbus\heatpump_parameters.h"
#include "modbus/display.h"
#include "time_counters.h"

extern COOLING_MODE_DATA cooling_mode_data;
bool regulateOnTempSensorInBufferCooling = false;


bool changeSettingCooling = false;
void setTemperatureOperatingCycleCooling() {
    if ((getsystemOnCounter() % 10) == 0) {
        changeSettingCooling = true;
        return;
    }    
    
    if (!regulateOnTempSensorInBufferCooling) {
        changeSettingCooling = false;
        return;
    }
    
    int16_t coolingBufferTemperature = GetNtcTemperature(NTC_HEATING_BUFFER);
    int16_t coolingSetpoint = getCoolingSetpoint();        
    
    if (coolingBufferTemperature >= (coolingSetpoint + (getDataFromMemoryCallable(ADDRESS_AIR_CONDITIONER_RETURN_DIFFERENCE) * 10)) 
            && getDataFromMemoryCallable(ADDRESS_CONSTANT_TEMPERATURE_OPERATION_CYCLE) != 1
            && changeSettingCooling) {
        ChangeHeatpumpSetting(ADDRESS_CONSTANT_TEMPERATURE_OPERATION_CYCLE, 1);
        changeSettingCooling = false;
        return;
    }  
    
    if (coolingBufferTemperature <= coolingSetpoint
            && getDataFromMemoryCallable(ADDRESS_CONSTANT_TEMPERATURE_OPERATION_CYCLE) != 240
            && changeSettingCooling) {
        ChangeHeatpumpSetting(ADDRESS_CONSTANT_TEMPERATURE_OPERATION_CYCLE, 240);
        changeSettingCooling = false;
        return;        
    }
}


void COOLING_MODE_Initialize ( void )
{
    cooling_mode_data.state = COOLING_INITIALIZE;
    return;
}



const char * getCoolingStateToString()
{
    switch (cooling_mode_data.state)
    {
        case(COOLING_INITIALIZE): {
            return "0, Init";
            break;
        }
        
        case(COOLING_IDLE): {
            return "1, Idle";
            break;
        }
        
        case(COOLING_RUNNING): {
            return "2, Running";
            break;
        }
        
        default:{
            return "-1, Unkown";
            break;
        }
        return "-1, Unkown";
    }
}



void COOLING_MODE_Tasks ( void )
{    
    //setActiveModeControllerHeatpumpSetpoint(getHeatpumpHeatingSetpoint() * 10);
    
    bool currentDip1SwitchState = getCurrentDip1SwitchState();
    if (currentDip1SwitchState) {
        regulateOnTempSensorInBufferCooling = false;
    } else {
        regulateOnTempSensorInBufferCooling = true;
    }
    
    if (currentDip1SwitchState != getPreviousDip1SwitchState()) {
        setPreviousDip1SwitchState(currentDip1SwitchState);
        if(currentDip1SwitchState == true) {
            ChangeHeatpumpSetting(ADDRESS_CONSTANT_TEMPERATURE_OPERATION_CYCLE, 10);
        }
    }
    
    setTemperatureOperatingCycleCooling();
    
    setActiveModeControllerHeatpumpRunningMode(SET_MODE_COOLING);
    
    switch ( cooling_mode_data.state )
    {
        case COOLING_INITIALIZE:{
            //SYS_CONSOLE_PRINT("COOLING_INITIALIZE\r\n");
            
            if(regulateOnTempSensorInBufferCooling) {
                ChangeHeatpumpSetting(ADDRESS_CONSTANT_TEMPERATURE_OPERATION_CYCLE, 240);
            }
            
            cooling_mode_data.state = COOLING_IDLE;
            break;
        }
        
        case COOLING_IDLE:{
            
            if (getHeatpumpCompressorFrequency() != 0){
                // Compressor is running
                
                if(regulateOnTempSensorInBufferCooling) {
                    ChangeHeatpumpSetting(ADDRESS_CONSTANT_TEMPERATURE_OPERATION_CYCLE, 1);
                }                
                
                cooling_mode_data.state = COOLING_RUNNING;
                break;
            }
            
            break;
        }
        
        case COOLING_RUNNING:{
            
            if ((getHeatpumpCompressorFrequency() == 0) && (isDefrostingActive() == false)){
                // Compressor is not running and is also not in defrosting
                if(regulateOnTempSensorInBufferCooling) {
                    ChangeHeatpumpSetting(ADDRESS_CONSTANT_TEMPERATURE_OPERATION_CYCLE, 240);
                }                
                
                cooling_mode_data.state = COOLING_IDLE;
                break;
            }
            
            break;
        }
        
        default:{
            COOLING_MODE_Initialize();
            break;
        }
    }
    
}


