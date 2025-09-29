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
bool changeCompensationsCooling = false;

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
    int16_t coolingSetpoint = TEMPERATURE_ALARM_VALUE;      
    
    if (cooling_mode_data.coolingCurveSet) {
        coolingSetpoint = getHeatpumpCoolingSetpoint()*10;
    } else {
        coolingSetpoint = getCoolingSetpoint();
    }   
    
    if (coolingBufferTemperature >= (coolingSetpoint + (getDataFromMemoryCallable(ADDRESS_AIR_CONDITIONER_RETURN_DIFFERENCE) * 10)) 
            //&& getDataFromMemoryCallable(ADDRESS_CONSTANT_TEMPERATURE_OPERATION_CYCLE) != 1
            && getActiveModeControllerPumpOffDueToDipSwitch1()
            && changeSettingCooling) {
        //ChangeHeatpumpSetting(ADDRESS_CONSTANT_TEMPERATURE_OPERATION_CYCLE, 1);
        setActiveModeControllerPumpOffDueToDipSwitch1(false);
        changeSettingCooling = false;
        return;
    }  
    
    if (coolingBufferTemperature <= coolingSetpoint
            //&& getDataFromMemoryCallable(ADDRESS_CONSTANT_TEMPERATURE_OPERATION_CYCLE) != 240
            && !getActiveModeControllerPumpOffDueToDipSwitch1()
            && changeSettingCooling) {
        //ChangeHeatpumpSetting(ADDRESS_CONSTANT_TEMPERATURE_OPERATION_CYCLE, 240);
        setActiveModeControllerPumpOffDueToDipSwitch1(true);
        changeSettingCooling = false;
        return;        
    }
}



int16_t determineCorrectCoolingSetpoint() {
    int16_t coolingBufferTemperature = GetNtcTemperature(NTC_HEATING_BUFFER);
    int16_t coolingSetpoint = TEMPERATURE_ALARM_VALUE;      
    
    if (cooling_mode_data.coolingCurveSet) {
        coolingSetpoint = getHeatpumpCoolingSetpoint()*10;   
        
        if ((getsystemOnCounter() % 10) == 0) {
            changeCompensationsCooling = true;
            return coolingSetpoint;
        }    
        
        if (coolingBufferTemperature <= coolingSetpoint && changeCompensationsCooling &&
                (getDataFromMemoryCallable(ADDRESS_RETURN_WATER_TEMPERATURE_COMPENSATION_VALUE) != 2 
                || getDataFromMemoryCallable(ADDRESS_OUTLET_WATER_TEMPERATURE_COMPENSATION_VALUE) != 2)) { 
            ChangeHeatpumpSetting(ADDRESS_RETURN_WATER_TEMPERATURE_COMPENSATION_VALUE, 2);
            ChangeHeatpumpSetting(ADDRESS_OUTLET_WATER_TEMPERATURE_COMPENSATION_VALUE, 2);            
            changeCompensationsCooling = false;
        }
        
        if (getHeatpumpCompressorFrequency() == 0) {
            return coolingSetpoint;
        }          
        
        if (coolingBufferTemperature <= coolingSetpoint) {
            return coolingSetpoint;
        }
        
        /*
            if((getHeatpumpReturnWaterTemperature() - coolingSetpoint) < 20  && changeCompensationsCooling) {
                ChangeHeatpumpSetting(ADDRESS_RETURN_WATER_TEMPERATURE_COMPENSATION_VALUE, (getDataFromMemoryCallable(ADDRESS_RETURN_WATER_TEMPERATURE_COMPENSATION_VALUE) + 1));
                ChangeHeatpumpSetting(ADDRESS_OUTLET_WATER_TEMPERATURE_COMPENSATION_VALUE, (getDataFromMemoryCallable(ADDRESS_OUTLET_WATER_TEMPERATURE_COMPENSATION_VALUE) + 1));            
                changeCompensationsCooling = false;
            }  
        */
        
        return coolingSetpoint;
    } else {
        coolingSetpoint = getCoolingSetpoint();
    }           
  
    return coolingSetpoint;
}



void COOLING_MODE_Initialize ( void )
{
    cooling_mode_data.state = COOLING_INITIALIZE;
    //CoolingActiveRelayClear();
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
    if(ReadSmartEeprom16(SEEP_ADDR_COOLING_CURVE) > 0 && ReadSmartEeprom16(SEEP_ADDR_COOLING_CURVE) != UINT16_MAX) {
        cooling_mode_data.coolingCurveSet = true; 
    } else {
        cooling_mode_data.coolingCurveSet = false; 
    }
        
    bool currentDip1SwitchState = getCurrentDip1SwitchState();
    if (currentDip1SwitchState) {
        regulateOnTempSensorInBufferCooling = false;
    } else {
        regulateOnTempSensorInBufferCooling = true;
    }
    
    if (currentDip1SwitchState != getPreviousDip1SwitchState()) {
        setPreviousDip1SwitchState(currentDip1SwitchState);
        if(currentDip1SwitchState == true) {
            // ChangeHeatpumpSetting(ADDRESS_CONSTANT_TEMPERATURE_OPERATION_CYCLE, 10);
            setActiveModeControllerPumpOffDueToDipSwitch1(false);
        }
    }
    
    setTemperatureOperatingCycleCooling();
    
    setActiveModeControllerHeatpumpSetpointCooling(determineCorrectCoolingSetpoint());
    
    setActiveModeControllerHeatpumpRunningMode(SET_MODE_COOLING);
    
    switch ( cooling_mode_data.state )
    {
        case COOLING_INITIALIZE:{
            if(regulateOnTempSensorInBufferCooling) {
                //ChangeHeatpumpSetting(ADDRESS_CONSTANT_TEMPERATURE_OPERATION_CYCLE, 240);
                setActiveModeControllerPumpOffDueToDipSwitch1(true);
            }
             
            CoolingActiveRelaySet();
            
            ChangeHeatpumpSetting(ADDRESS_RETURN_WATER_TEMPERATURE_COMPENSATION_VALUE, 2);
            ChangeHeatpumpSetting(ADDRESS_OUTLET_WATER_TEMPERATURE_COMPENSATION_VALUE, 2);            
            
            ChangeHeatpumpSetting(ADDRESS_HEATING_CURVE_SETTING, ReadSmartEeprom16(SEEP_ADDR_HEATING_CURVE));
            ChangeHeatpumpSetting(ADDRESS_COOLING_CURVE_SETTING, ReadSmartEeprom16(SEEP_ADDR_COOLING_CURVE));
            WriteSmartEeprom16(SEEP_ADDR_HEATING_SETPOINT_CURVE_BACKUP, UINT16_MAX);
            WriteSmartEeprom16(SEEP_ADDR_COOLING_SETPOINT_CURVE_BACKUP, UINT16_MAX);
            
            cooling_mode_data.state = COOLING_IDLE;
            break;
        }
        
        case COOLING_IDLE:{
            
            if (getHeatpumpCompressorFrequency() != 0){
                // Compressor is running
                
                if(regulateOnTempSensorInBufferCooling) {
                    //ChangeHeatpumpSetting(ADDRESS_CONSTANT_TEMPERATURE_OPERATION_CYCLE, 1);
                    setActiveModeControllerPumpOffDueToDipSwitch1(false);
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
                    //ChangeHeatpumpSetting(ADDRESS_CONSTANT_TEMPERATURE_OPERATION_CYCLE, 240);
                    setActiveModeControllerPumpOffDueToDipSwitch1(true);
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


