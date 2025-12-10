#include <stddef.h>                    
#include <stdbool.h>                   
#include <stdlib.h>                   
#include <string.h>
#include <stdio.h>
#include "definitions.h" 

#include "emergency_mode.h"
#include "modbus/heatpump_parameters.h"
#include "ntc.h"
#include "time_counters.h"
#include "eeprom.h"
#include "states.h"

/*
uint32_t sterilizationReachedTemperatureTimeStamp = UINT32_MAX;
uint16_t sterilizationTemperatureOffset = TEMPERATURE_ALARM_VALUE;
bool sterilizationHotwaterElementOn = false;

STERILIZATION_MODE sterilisationMode = OFF;
*/

bool HeatingElementOn = false;
bool HotwaterElementOn = false;

bool getHeatingElementBoolFromEmergencyMode() {
    return HeatingElementOn;
}

bool getHotWaterElementBoolFromEmergencyMode() {
    return HotwaterElementOn;
}

void EmergencyModeTasks()
{      
    int16_t  heatingSetpoint = getHeatingSetpoint();
    int16_t  hotwaterSetpoint = getHotwaterSetpoint();
    int16_t  delta = getDataFromMemoryCallable(ADDRESS_AIR_CONDITIONER_RETURN_DIFFERENCE);
    RUNNING_MODES currentRunningMode = ReadSmartEeprom16(SEEP_ADDR_HEATPUMP_MODE);
    
    int16_t heatingBufferTemperature  = GetNtcTemperature(NTC_HEATING_BUFFER);
    int16_t hotwaterBoilerTemperature = GetNtcTemperature(NTC_HOT_WATER_BUFFER);
    
    if (delta != TEMPERATURE_ALARM_VALUE){
        // If delta is not set (for example no communication with heatpump) set to 5 degrees.
        delta = 50; // 5 degrees Celcius
    }
    
    if (currentRunningMode == HEATING || currentRunningMode == HOT_WATER_HEATING){
        // Heating mode is active 
        if (heatingBufferTemperature <= (heatingSetpoint - delta)) {
            // Buffer temperature is lower than setpoint - delta
            HeatingElementOn = true;
        }

        if (heatingBufferTemperature >= heatingSetpoint) {
            // Buffer temperature reached setpoint
            HeatingElementOn = false;
        }
    }
    else {
        // No heating mode
        HeatingElementOn = false;
    }
    
    if (currentRunningMode == HOT_WATER || currentRunningMode == HOT_WATER_HEATING || currentRunningMode == HOT_WATER_COOLING){
        // Hot water mode is active
        if (hotwaterBoilerTemperature <= (hotwaterSetpoint - delta)) {
            // Hot water temperature is lower than setpoint - delta
            HotwaterElementOn = true;
        }

        if (hotwaterBoilerTemperature >= hotwaterSetpoint) {
            // Hot water temperature reached setpoint
            HotwaterElementOn = false;
        }
    } 
    else {
        // No hot water mode
        HotwaterElementOn = false;
    }
}