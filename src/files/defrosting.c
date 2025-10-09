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
bool defrostingHotwaterElementOn = false;

int16_t initialDefrostingTemperature = TEMPERATURE_ALARM_VALUE;

int16_t getInitialDefrostingTemperature() {
    return initialDefrostingTemperature;
}

bool getDefrostingElementOnState()
{
    return defrostingHotwaterElementOn;
}

bool isDefrostingActive()
{
    if (RealTimeData1[ADDRESS_RUNNING_STATUS_1 - START_ADDRESS_REAL_TIME_DATA_1][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP][MASTER_HEATPUMP_IN_CASCADE] & (1 << RUNNING_STATUS_1_SYSTEM_DEFROST_BIT)){
        // Defrosting bit high and thus active
        return true;
    }
    else{
        return false;
    }
}

void CheckDefrosting(HOT_WATER_HEATING_MODE_STATES currentHotWaterHeatingModeState, STERILIZATION_MODE currentSterilizationModeState)
{    
    int16_t hotwaterBufferTemperature = GetNtcTemperature(NTC_HOT_WATER_BUFFER);
    
    if ((currentSterilizationModeState == OFF) || 
        (currentSterilizationModeState == PASSIVE)){
        // There is no active sterilization 
        
        if ((getActiveStateValue() == COOLING) ||
            (getActiveStateValue() == HEATING) ||
            (getActiveStateValue() == FLOOR_HEATING)) {
            // No need for defrosting mode in these modes
            defrostingHotwaterElementOn = false;
            initialDefrostingTemperature = TEMPERATURE_ALARM_VALUE;
            return;
        }
        
        if ((getHotWaterHeatingModeData().state == HOT_WATER_HEATING_IDLE_HEATING) || 
            (getHotWaterHeatingModeData().state == HOT_WATER_HEATING_RUNNING_ON_HEATING) ||
            (getHotWaterHeatingModeData().state == HOT_WATER_HEATING_RUNNING_ON_HEATING_WITH_ELEMENT_ON)) {
            // No need for defrosting in these hotwater/heating modes (only heating modes)
            defrostingHotwaterElementOn = false;
            initialDefrostingTemperature = TEMPERATURE_ALARM_VALUE;
            return;
        }

        if ((getHotWaterCoolingModeData().state == HOT_WATER_COOLING_IDLE_COOLING) || 
                (getHotWaterCoolingModeData().state == HOT_WATER_COOLING_MODE_RUNNING_ON_COOLING)) {
            // No need for defrosting in these hotwater/cooling modes (only cooling modes)
            defrostingHotwaterElementOn = false;
            initialDefrostingTemperature = TEMPERATURE_ALARM_VALUE;
            return;
        }
    }
    
    bool defrostingActiveInHeatpump = isDefrostingActive();
    
    if (defrostingActiveInHeatpump == true){
        // Defrosting is active in heatpump
        if (initialDefrostingTemperature == TEMPERATURE_ALARM_VALUE){
            // Initial defrosting temperature is alarm value (undefined), so give it a start value
            initialDefrostingTemperature = GetNtcTemperature(NTC_HOT_WATER_BUFFER);
        }
        
        if (hotwaterBufferTemperature <= (initialDefrostingTemperature - ReadSmartEeprom16(SEEP_ADDR_DEFROSTING_TEMP_FALL_BEFORE_ELEMENT_ON))){
            // Temperature decreased under threshold, turn on heating element
            //TurnOnHeatingElementHotWaterBuffer();
            defrostingHotwaterElementOn = true;
        }
        
        if (hotwaterBufferTemperature >= (initialDefrostingTemperature - ReadSmartEeprom16(SEEP_ADDR_DEFROSTING_TEMP_FALL_BEFORE_ELEMENT_ON)+ ReadSmartEeprom16(SEEP_ADDR_DEFROSTING_TEMP_RISE_BEFORE_ELEMENT_OFF)))
        {   // Temperature rised above threshold, turn off heating element
            //TurnOffHeatingElementHotWaterBuffer();
            defrostingHotwaterElementOn = false;
        }
        return;
    }
    
    if ((defrostingActiveInHeatpump == false) && (initialDefrostingTemperature != TEMPERATURE_ALARM_VALUE)){
        // Defrosting not active and initial defrosting temperature has a valid value
        if (defrostingHotwaterElementOn == false){
            // Hot water element already off
            initialDefrostingTemperature = TEMPERATURE_ALARM_VALUE;
            return;
        }
        
        if (hotwaterBufferTemperature >= (initialDefrostingTemperature - ReadSmartEeprom16(SEEP_ADDR_DEFROSTING_TEMP_FALL_BEFORE_ELEMENT_ON) + ReadSmartEeprom16(SEEP_ADDR_DEFROSTING_TEMP_RISE_BEFORE_ELEMENT_OFF))){ 
            // Element is on and heating rised enough for element to go off again
            //TurnOffHeatingElementHotWaterBuffer();
            defrostingHotwaterElementOn = false;
            initialDefrostingTemperature = TEMPERATURE_ALARM_VALUE;
        }                     

        return;
    }
}