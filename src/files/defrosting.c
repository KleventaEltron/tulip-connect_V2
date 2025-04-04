#include <stddef.h>                    
#include <stdbool.h>                   
#include <stdlib.h>                   
#include <string.h>
#include <stdio.h>
#include "definitions.h" 

#include "defrosting.h"
#include "ntc.h"
#include "eeprom.h"
#include "states.h"
#include "modbus/heatpump_parameters.h"

bool defrostingActiveInHeatpump = false;
bool defrostingHeatingElementOn = false;

int16_t initialDefrostingTemperature = TEMPERATURE_ALARM_VALUE;

bool getDefrostingElementOnState()
{
    return defrostingHeatingElementOn;
}

bool isDefrostingActive()
{
    if (RealTimeDataStatussen[ADDRESS_RUNNING_STATUS_1 - START_ADDRESS_REAL_TIME_DATA_STATUSSEN][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP] & (1 << RUNNING_STATUS_1_SYSTEM_DEFROST_BIT)){
        // Defrosting bit high and thus active
        return true;
    }
    else{
        return false;
    }
}

void CheckDefrosting(HOT_WATER_HEATING_MODE_STATES currentHotWaterHeatingModeState, STERILIZATION_MODE currentSterilizationModeState)
{    
    if (((currentSterilizationModeState == OFF) || 
        (currentSterilizationModeState == PASSIVE))
         &&
        ((currentHotWaterHeatingModeState == HOT_WATER_HEATING_INITIALIZE_HEATING) || 
        (currentHotWaterHeatingModeState == HOT_WATER_HEATING_IDLE_HEATING) ||
        (currentHotWaterHeatingModeState == HOT_WATER_HEATING_RUNNING_ON_HEATING) || 
        (currentHotWaterHeatingModeState == HOT_WATER_HEATING_RUNNING_ON_HEATING_WITH_ELEMENT_ON))  ){
        
        defrostingHeatingElementOn = false;
        initialDefrostingTemperature = TEMPERATURE_ALARM_VALUE;
        return;
    }
    
    bool defrostingActiveInHeatpump = isDefrostingActive();
    
    if (defrostingActiveInHeatpump == true){
        // Defrosting is active in heatpump
        if (initialDefrostingTemperature == TEMPERATURE_ALARM_VALUE){
            // Initial defrosting temperature is alarm value (undefined), so give it a start value
            initialDefrostingTemperature = GetNtcTemperature(NTC_HOT_WATER_BUFFER);
        }
        
        if (GetNtcTemperature(NTC_HOT_WATER_BUFFER) <= (initialDefrostingTemperature - ReadSmartEeprom16(SEEP_ADDR_DEFROSTING_TEMP_FALL_BEFORE_ELEMENT_ON))){
            // Temperature decreased under threshold, turn on heating element
            //TurnOnHeatingElementHotWaterBuffer();
            defrostingHeatingElementOn = true;
        }
        
        if (GetNtcTemperature(NTC_HOT_WATER_BUFFER) >= (initialDefrostingTemperature - ReadSmartEeprom16(SEEP_ADDR_DEFROSTING_TEMP_FALL_BEFORE_ELEMENT_ON)+ ReadSmartEeprom16(SEEP_ADDR_DEFROSTING_TEMP_RISE_BEFORE_ELEMENT_OFF)))
        {   // Temperature rised above threshold, turn off heating element
            //TurnOffHeatingElementHotWaterBuffer();
            defrostingHeatingElementOn = false;
        }
        return;
    }
    
    if ((defrostingActiveInHeatpump == false) && (initialDefrostingTemperature != TEMPERATURE_ALARM_VALUE)){
        // Defrosting not active and initial defrosting temperature has a valid value
        if (getStatusHeatingElementHotWaterBuffer() == false){
            // Hot water element already off
            initialDefrostingTemperature = TEMPERATURE_ALARM_VALUE;
            return;
        }
        
        if (app_Data.currentHotWaterBufferTemp >= (initialDefrostingTemperature - ReadSmartEeprom16(SEEP_ADDR_DEFROSTING_TEMP_FALL_BEFORE_ELEMENT_ON) + ReadSmartEeprom16(SEEP_ADDR_DEFROSTING_TEMP_RISE_BEFORE_ELEMENT_OFF))){ 
            // Element is on and heating rised enough for element to go off again
            //TurnOffHeatingElementHotWaterBuffer();
            defrostingHeatingElementOn = false;
            initialDefrostingTemperature = TEMPERATURE_ALARM_VALUE;
        }                     

        return;
    }
}