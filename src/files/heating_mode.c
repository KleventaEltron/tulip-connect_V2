#include <stddef.h>                    
#include <stdbool.h>                   
#include <stdlib.h>                   
#include <string.h>
#include <stdio.h>
#include "definitions.h"    

#include "heating_mode.h"
#include "states.h"

#include "user.h"
#include "eeprom.h"
#include "time_counters.h"
#include "ntc.h"
#include "modbus\heatpump_parameters.h"
#include "modbus/display.h"


extern HEATING_MODE_DATA heating_mode_data;
bool regulateOnTempSensorInBufferHeating = false;



bool getHeatingElementBoolFromHeatingMode() {
    return heating_mode_data.HeatingElementOn;
}



bool changeSetting = false;
void setTemperatureOperatingCycleHeating() {
    if ((getsystemOnCounter() % 10) == 0) {
        changeSetting = true;
        return;
    }    
    
    if (!regulateOnTempSensorInBufferHeating) {
        changeSetting = false;
        return;
    }
    
    int16_t heatingBufferTemperature = GetNtcTemperature(NTC_HEATING_BUFFER);
    int16_t heatingSetpoint = TEMPERATURE_ALARM_VALUE; 
    
    if (heating_mode_data.heatingCurveSet) {
        heatingSetpoint = getHeatpumpHeatingSetpoint();
    } else {
        heatingSetpoint = getHeatingSetpoint();
    }   
    
    if (heatingBufferTemperature <= (heatingSetpoint - (getDataFromMemoryCallable(ADDRESS_AIR_CONDITIONER_RETURN_DIFFERENCE) * 10)) 
            && getDataFromMemoryCallable(ADDRESS_CONSTANT_TEMPERATURE_OPERATION_CYCLE) != 1
            && changeSetting) {
        ChangeHeatpumpSetting(ADDRESS_CONSTANT_TEMPERATURE_OPERATION_CYCLE, 1);
        changeSetting = false;
        return;
    }  
    
    if (heatingBufferTemperature >= heatingSetpoint
            && getDataFromMemoryCallable(ADDRESS_CONSTANT_TEMPERATURE_OPERATION_CYCLE) != 240
            && changeSetting) {
        ChangeHeatpumpSetting(ADDRESS_CONSTANT_TEMPERATURE_OPERATION_CYCLE, 240);
        changeSetting = false;
        return;        
    }
}



int16_t determineCorrectHeatingSetpoint() {
    int16_t heatingBufferTemperature = GetNtcTemperature(NTC_HEATING_BUFFER);
    int16_t heatingSetpoint = getHeatingSetpoint(); 
    
    if (!regulateOnTempSensorInBufferHeating) {
        heating_mode_data.stepperSetpoint = heatingSetpoint;
        return heatingSetpoint;
    }
    
    if ((heatingBufferTemperature == TEMPERATURE_ALARM_VALUE) || (heatingSetpoint == TEMPERATURE_ALARM_VALUE)) {
        // No temperature or setpoint known yet
        heating_mode_data.stepperSetpoint = heatingSetpoint;
        return heatingSetpoint;
    }    
    
    if (heatingBufferTemperature >= heatingSetpoint) {   
        // Hot water buffer tempereature is equal or higher than actual setpoint, so reset offset to 0
        heating_mode_data.stepperSetpoint = heatingSetpoint;
        return heatingSetpoint;
    }    
    
    if (getHeatpumpCompressorFrequency() == 0) {
        heating_mode_data.stepperSetpoint = heatingSetpoint;
        return heatingSetpoint;
    }
    
    if ((heatingSetpoint - getHeatpumpReturnWaterTemperature()) > 50) {
        heating_mode_data.stepperSetpoint = getHeatpumpReturnWaterTemperature() + 20;
        return heating_mode_data.stepperSetpoint;
    }

    
    if (getHeatpumpReturnWaterTemperature() >= (heating_mode_data.stepperSetpoint - 20) && (heating_mode_data.stepperSetpoint - getHeatpumpReturnWaterTemperature()) <= 20) {
        heating_mode_data.stepperSetpoint = getHeatpumpReturnWaterTemperature() + 20;
        return heating_mode_data.stepperSetpoint;
        //heatingSetpoint += 20;
    }
    
    return heating_mode_data.stepperSetpoint;
}



void HEATING_MODE_Initialize ( void )
{
    setSecondCounterHeatingTask(UINT32_MAX);
    heating_mode_data.initialBufferTemp = TEMPERATURE_ALARM_VALUE;
    //TurnOffHeatingElementHeatingBuffer();
    heating_mode_data.HeatingElementOn = false;
    heating_mode_data.stepperSetpoint = TEMPERATURE_ALARM_VALUE;
    
    heating_mode_data.heatingCurveSet = false;
    
    heating_mode_data.state = HEATING_INITIALIZE;
    return;
}



const char * getHeatingStateToString()
{
    switch (heating_mode_data.state)
    {
        case(HEATING_INITIALIZE): {
            return "0, Init";
            break;
        }
        
        case(HEATING_IDLE): {
            return "1, Idle";
            break;
        }
        
        case(HEATING_RUNNING): {
            return "2, Running";
            break;
        }
        
        case(HEATING_RUNNING_WITH_ELEMENT_ON): {
            return "3, Running with element on";
            break;
        }
        
        default:{
            return "-1, Unkown";
            break;
        }
        return "-1, Unkown";
    }
}



void HEATING_MODE_Tasks ( void )
{        
    
    int16_t heatingBufferTemperature = GetNtcTemperature(NTC_HEATING_BUFFER);
    
    if (getDataFromMemoryCallable(ADDRESS_HEATING_CURVE_SETTING) > 0) {
        heating_mode_data.heatingCurveSet = true; 
    } else {
        heating_mode_data.heatingCurveSet = false; 
    }

    //int16_t heatingSetpoint = getHeatingSetpoint();
   
    bool currentDip1SwitchState = getCurrentDip1SwitchState();
    if (currentDip1SwitchState) {
        regulateOnTempSensorInBufferHeating = false;
    } else {
        regulateOnTempSensorInBufferHeating = true;
    }
    
    if (currentDip1SwitchState != getPreviousDip1SwitchState()) {
        setPreviousDip1SwitchState(currentDip1SwitchState);
        if(currentDip1SwitchState == true) {
            ChangeHeatpumpSetting(ADDRESS_CONSTANT_TEMPERATURE_OPERATION_CYCLE, 10);
        }
    }
    
    setTemperatureOperatingCycleHeating();
    
    setActiveModeControllerHeatpumpSetpointHeating(determineCorrectHeatingSetpoint());
    
    setActiveModeControllerHeatpumpRunningMode(SET_MODE_HEATING);
    
    switch ( heating_mode_data.state )
    {
        case HEATING_INITIALIZE:{
            
            //TurnOffHeatingElementHeatingBuffer();
            heating_mode_data.HeatingElementOn = false;
            heating_mode_data.initialBufferTemp = TEMPERATURE_ALARM_VALUE; 
            heating_mode_data.stepperSetpoint = TEMPERATURE_ALARM_VALUE;
            setSecondCounterHeatingTask(UINT32_MAX);
            
            if(regulateOnTempSensorInBufferHeating) {
                ChangeHeatpumpSetting(ADDRESS_CONSTANT_TEMPERATURE_OPERATION_CYCLE, 240);
            }
            
            ChangeHeatpumpSetting(ADDRESS_COOLING_CURVE_SETTING, ReadSmartEeprom8(SEEP_ADDR_COOLING_CURVE));
            ChangeHeatpumpSetting(ADDRESS_HEATING_CURVE_SETTING, ReadSmartEeprom8(SEEP_ADDR_HEATING_CURVE));
            
            heating_mode_data.state = HEATING_IDLE;
            break;
        }
        
        case HEATING_IDLE:{                
            if (getHeatpumpCompressorFrequency() != 0){
                // Compressor is running
                setSecondCounterHeatingTask(0);
                heating_mode_data.initialBufferTemp = heatingBufferTemperature;
                heating_mode_data.stepperSetpoint = (getHeatpumpReturnWaterTemperature() + 20);
                
                if(regulateOnTempSensorInBufferHeating) {
                    ChangeHeatpumpSetting(ADDRESS_CONSTANT_TEMPERATURE_OPERATION_CYCLE, 1);
                }
                
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
                
                if(regulateOnTempSensorInBufferHeating) {
                    ChangeHeatpumpSetting(ADDRESS_CONSTANT_TEMPERATURE_OPERATION_CYCLE, 240);
                }

                heating_mode_data.state = HEATING_IDLE;
                break;
            }
            
            if (heatingBufferTemperature >= heating_mode_data.initialBufferTemp + ReadSmartEeprom16(SEEP_ADDR_HEATING_RISE_TEMP_IN_GIVEN_TIME)){
                // Temperature rised a set temperature in a set time
                setSecondCounterHeatingTask(0);
                heating_mode_data.initialBufferTemp = heatingBufferTemperature;
                break;
            }
            
            if (getSecondCounterHeatingTask() >= ReadSmartEeprom16(SEEP_ADDR_HEATING_TIME_CONSTANT_SEC)){
                // Temperature not rised a set temperature in a set time
                setSecondCounterHeatingTask(0);
                heating_mode_data.initialBufferTemp = heatingBufferTemperature;
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

                if(regulateOnTempSensorInBufferHeating) {
                    ChangeHeatpumpSetting(ADDRESS_CONSTANT_TEMPERATURE_OPERATION_CYCLE, 240);
                }
                
                heating_mode_data.state = HEATING_IDLE;
                break;
            }
            
            if (heatingBufferTemperature >= heating_mode_data.initialBufferTemp + ReadSmartEeprom16(SEEP_ADDR_HEATING_RISE_TEMP_IN_GIVEN_TIME)){
                // Temperature rised a set temperature in a set time
                setSecondCounterHeatingTask(0);
                heating_mode_data.initialBufferTemp = heatingBufferTemperature;
                break;
            }
            
            if (getSecondCounterHeatingTask() >= ReadSmartEeprom16(SEEP_ADDR_HEATING_TIME_CONSTANT_SEC)){
                // Temperature not rised a set temperature in a set time
                setSecondCounterHeatingTask(0);
                heating_mode_data.initialBufferTemp = heatingBufferTemperature;
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


