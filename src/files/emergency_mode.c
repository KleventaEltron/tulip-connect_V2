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

bool HeatingElementEmergencyControl = false;
bool HotwaterElementEmergencyControl = false;

bool getHeatingElementBoolFromEmergencyMode() {
    return HeatingElementEmergencyControl;
}

bool getHotWaterElementBoolFromEmergencyMode() {
    return HotwaterElementEmergencyControl;
}

void HeatingEmergencyMode(RUNNING_MODES currentRunningMode, int16_t delta)
{
    int16_t  heatingSetpoint;
    
    if (ReadSmartEeprom16(SEEP_ADDR_HEATING_CURVE) > 0 && ReadSmartEeprom16(SEEP_ADDR_HEATING_CURVE) != UINT16_MAX) {
        // Heating curve is set
        heatingSetpoint = getHeatpumpHeatingSetpoint();
        
        if (heatingSetpoint != UINT16_MAX) {
            // Working setpoint in heatpump
            heatingSetpoint *= 10; 
        } 
        else{
            // No communication with heatpump, get setpoint if available in eeprom
            heatingSetpoint = getHeatingSetpoint(); 
        }
         
    } else {
        heatingSetpoint = getHeatingSetpoint(); 
    }
    
    int16_t heatingBufferTemperature  = GetNtcTemperature(NTC_HEATING_BUFFER);
    
    if (heatingBufferTemperature == TEMPERATURE_ALARM_VALUE) {
        HeatingElementEmergencyControl = false;
        return;
    }
    
    if (heatingSetpoint == TEMPERATURE_ALARM_VALUE) {
        HeatingElementEmergencyControl = false;
        return;
    }
    
    if (currentRunningMode != HEATING && currentRunningMode != HOT_WATER_HEATING) {
        // Heating mode not active
        HeatingElementEmergencyControl = false;
        return;
    }
    
    if (heatingBufferTemperature <= (heatingSetpoint - delta)) {
        // Buffer temperature is lower than setpoint - delta
        HeatingElementEmergencyControl = true;
        return;
    }

    if (heatingBufferTemperature >= heatingSetpoint) {
        // Buffer temperature reached setpoint
        HeatingElementEmergencyControl = false;
        return;
    }
}

void HotwaterEmergencyMode(RUNNING_MODES currentRunningMode, int16_t delta)
{
    int16_t  hotwaterSetpoint = getHotwaterSetpoint();
    int16_t hotwaterBoilerTemperature = GetNtcTemperature(NTC_HOT_WATER_BUFFER);
    
    if (hotwaterBoilerTemperature == TEMPERATURE_ALARM_VALUE) {
        HotwaterElementEmergencyControl = false;
        return;
    }
    
    if (hotwaterSetpoint == TEMPERATURE_ALARM_VALUE) {
        HotwaterElementEmergencyControl = false;
        return;
    }
    
    if (currentRunningMode != HOT_WATER && currentRunningMode != HOT_WATER_HEATING && currentRunningMode != HOT_WATER_COOLING) {
        // Heating mode not active
        HotwaterElementEmergencyControl = false;
        return;
    }
    
    if (hotwaterBoilerTemperature <= (hotwaterSetpoint - delta)) {
        // Hot water temperature is lower than setpoint - delta
        HotwaterElementEmergencyControl = true;
        return;
    }

    if (hotwaterBoilerTemperature >= hotwaterSetpoint) {
        // Hot water temperature reached setpoint
        HotwaterElementEmergencyControl = false;
        return;
    }
}

void EmergencyModeTasks()
{      
    bool heatingEmergencyModeEnabled = ReadSmartEeprom16(SEEP_ADDR_EMERGENCY_MODE_HEATING_ENABLED);
    bool hotwaterEmergencyModeEnabled = ReadSmartEeprom16(SEEP_ADDR_EMERGENCY_MODE_HOTWATER_ENABLED);
    
    RUNNING_MODES currentRunningMode = ReadSmartEeprom16(SEEP_ADDR_HEATPUMP_MODE);
    int16_t  delta = getAirConditionerReturnDifference();
    
    if (delta == TEMPERATURE_ALARM_VALUE){
        // If delta is not set (for example no communication with heatpump) set to 5 degrees.
        delta = 50; // 5 degrees Celcius
    }
    
    if (heatingEmergencyModeEnabled == true) {
        // Do heating emergency mode
        HeatingEmergencyMode(currentRunningMode, delta);
    }
    else {
        // No heating emergency mode
        HeatingElementEmergencyControl = false;
    }
    
    if (hotwaterEmergencyModeEnabled == true) {
        // Do hotwater emergency mode
        HotwaterEmergencyMode(currentRunningMode, delta);
    }
    else {
        // No hotwater emergency mode
        HotwaterElementEmergencyControl = false;
    }
}