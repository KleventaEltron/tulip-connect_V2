#include <stddef.h>                    
#include <stdbool.h>                   
#include <stdlib.h>                   
#include <string.h>
#include <stdio.h>
#include "definitions.h" 

#include "sterilization.h"
#include "modbus/heatpump_parameters.h"
#include "time_counters.h"
#include "ntc.h"
#include "eeprom.h"
#include "states.h"

#include "pi_frequency_controller.h"
#include "heatpump_pi_adapter.h"

uint32_t sterilizationReachedTemperatureTimeStamp = UINT32_MAX;
uint16_t sterilizationTemperatureOffset = TEMPERATURE_ALARM_VALUE;
bool sterilizationHotwaterElementOn = false;

STERILIZATION_MODE sterilisationMode = OFF;

bool getSterilizationElementOnState()
{
    return sterilizationHotwaterElementOn;
}

uint16_t getSterilizationTemperatureOffset()
{
    return sterilizationTemperatureOffset;
}

uint32_t getSterilizationReachedTemperatureTimeStamp()
{
    return sterilizationReachedTemperatureTimeStamp;
}

STERILIZATION_MODE getSterilisationMode() {
    return sterilisationMode;
}

void setSterilisationMode(STERILIZATION_MODE newMode) {
    sterilisationMode = newMode;
}


void checkNeedForSterilization() {
    
    if(sterilisationMode != ACTIVE && goToActiveSterilization()){
        HPPI_Clear();
        // Sterilization is off and must go to active
        
        if (ReadSmartEeprom16(SEEP_ADDR_EMERGENCY_MODE_HOTWATER_ENABLED) == true) {
            // Sterilization must go on, but emergency mode is enabled, so don't go on, but set ON HOLD.
            if (ReadSmartEeprom16(SEEP_ADDR_STERILIZATION_ON_HOLD) == false) {
                WriteSmartEeprom16(SEEP_ADDR_STERILIZATION_ON_HOLD, true);
            }
            setSterilisationMode(ON_HOLD);
            return;
        }
        
        setSterilisationMode(ACTIVE);
        setActiveModeControllerHeatpumpRunningMode(SET_MODE_HEATING);
        setActiveModeControllerPumpOffDueToDipSwitch1(false);
        
        ChangeHeatpumpSetting(ADDRESS_HEATING_CURVE_SETTING, 0);
        ChangeHeatpumpSetting(ADDRESS_COOLING_CURVE_SETTING, 0);
        
        if(ReadSmartEeprom16(SEEP_ADDR_HEATING_CURVE) > 0 && ReadSmartEeprom16(SEEP_ADDR_HEATING_CURVE) != UINT16_MAX) {
            WriteSmartEeprom16(SEEP_ADDR_HEATING_SETPOINT_CURVE_BACKUP, getHeatpumpHeatingSetpoint()*10);
        } else {
            WriteSmartEeprom16(SEEP_ADDR_HEATING_SETPOINT_CURVE_BACKUP, UINT16_MAX);
        }
        
        if(ReadSmartEeprom16(SEEP_ADDR_COOLING_CURVE) > 0 && ReadSmartEeprom16(SEEP_ADDR_COOLING_CURVE) != UINT16_MAX) {
            WriteSmartEeprom16(SEEP_ADDR_COOLING_SETPOINT_CURVE_BACKUP, getHeatpumpCoolingSetpoint()*10);
        } else {
            WriteSmartEeprom16(SEEP_ADDR_COOLING_SETPOINT_CURVE_BACKUP, UINT16_MAX);
        }  
        
        resetActiveModeStates();
    }        
}


bool goToActiveSterilization() {
    int16_t currentHotWaterBufferTemp = GetNtcTemperature(NTC_HOT_WATER_BUFFER);
    uint16_t sterilizationStartTime = UnitSystemParameterL[ADDRESS_STERILIZATION_START_TIME - START_ADDRESS_UNIT_SYSTEM_PARAMETER_L][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP];
    uint16_t sterilizationFunction = UnitSystemParameterL[ADDRESS_HIGH_TEMPERATURE_STERILIZATION_FUNCTION - START_ADDRESS_UNIT_SYSTEM_PARAMETER_L][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP];
    uint16_t sterilizationIntervalDays = UnitSystemParameterL[ADDRESS_STERILIZATION_INTERVAL_DAYS - START_ADDRESS_UNIT_SYSTEM_PARAMETER_L][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP];
    uint8_t currentDisplayTimeHours = (uint8_t)(UserParameters[ADDRESS_DISPLAY_TIME - START_ADDRESS_USER_PARAMETERS][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP] >> 8);
    uint8_t currentDisplayTimeMinutes = (uint8_t)UserParameters[ADDRESS_DISPLAY_TIME - START_ADDRESS_USER_PARAMETERS][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP];
    uint16_t dayCounter = ReadSmartEeprom16(SEEP_ADDR_DAY_COUNTER_STERILIZATION);
    uint16_t maxTimeOutOfSterilizationMode = ReadSmartEeprom16(SEEP_ADDR_STERILIZATION_MAX_TIME_OUT_OF_STERILIZATION_MODE_WITH_ELEMENT_ON);    
    
    if (ReadSmartEeprom16(SEEP_ADDR_STERILIZATION_ON_HOLD) == true && ReadSmartEeprom16(SEEP_ADDR_EMERGENCY_MODE_HOTWATER_ENABLED) == false) {
        // Hot water emergency mode disabled but sterilization was on hold, do sterilization and set ON HOLD on false.
        WriteSmartEeprom16(SEEP_ADDR_STERILIZATION_ON_HOLD, false);
        return true;
    }

    // Passive sterilization check, if this triggers we need to return to active sterilization    
    //if (secondCounterLegionella != UINT32_MAX && (secondCounterLegionella > maxTimeOutOfSterilizationMode) && (heatingElementStatus == true)) {   
    if(sterilisationMode == PASSIVE && (getSecondCounterLegionella() >= maxTimeOutOfSterilizationMode) && (getSecondCounterLegionella() != UINT32_MAX)) {
        setSecondCounterLegionella(UINT32_MAX);
        // Sterilization hot water element was already on
        return true;
    }
    
    if ((currentHotWaterBufferTemp == TEMPERATURE_ALARM_VALUE) || (sterilizationFunction != STERILIZATION_FUNCTION_AUTO)){
        return false;
    }
    
    if (dayCounter < (sterilizationIntervalDays - 1)) {
        return false;
    }
    
    if ((currentDisplayTimeHours == sterilizationStartTime) && (currentDisplayTimeMinutes == 0)) {   
        // Current time is the set time for sterilization
        setSecondCounterLegionella(UINT32_MAX);
        return true;
    }

    return false;
}





bool sterilisationIsActivelyRunning() {
    if (sterilisationMode == OFF) {
        return false;
    }
    
    if (sterilisationMode == ON_HOLD) {
        // No sterilization in ON HOLD mode
        return false;
    }
    
    if (ReadSmartEeprom16(SEEP_ADDR_EMERGENCY_MODE_HOTWATER_ENABLED) == true) {
        // Sterilization is ON or in Passive, but emergency mode is enabled, so don't go on, but set ON HOLD.
        if (ReadSmartEeprom16(SEEP_ADDR_STERILIZATION_ON_HOLD) == false) {
            WriteSmartEeprom16(SEEP_ADDR_STERILIZATION_ON_HOLD, true);
        }
        sterilizationHotwaterElementOn = false;
        sterilizationReachedTemperatureTimeStamp = UINT32_MAX;
        sterilizationTemperatureOffset = TEMPERATURE_ALARM_VALUE;
        
        setSecondCounterLegionella(UINT32_MAX);
        setSterilisationMode(ON_HOLD);
        return false;
    }
    
    // If we start a fresh sterilization cycle we have to set some parameters before proceeding.
    if (sterilisationMode == ACTIVE && getSecondCounterLegionella() == UINT32_MAX) {
                    
        bool valvePosition = getStatus3WayValve();
        if (valvePosition != VALVE_IS_ON_HOT_WATER_CIRCUIT) {
            return true;
        }

        //TurnOffHeatingElementHotWaterBuffer();
        sterilizationHotwaterElementOn = false;
        setSecondCounterLegionella(0);    
        sterilizationTemperatureOffset = ReadSmartEeprom16(SEEP_ADDR_STERILIZATION_SETPOINT_OFFSET_START);  
    }        
               
    uint16_t retourWaterTemperature = getHeatpumpReturnWaterTemperature(MASTER_HEATPUMP_IN_CASCADE);
    int16_t sterilizationTemperature = (UnitSystemParameterL[ADDRESS_STERILIZATION_TEMPERATURE_SETTING - START_ADDRESS_UNIT_SYSTEM_PARAMETER_L][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP] * 10);        
       
    if ((retourWaterTemperature >= (sterilizationTemperature + sterilizationTemperatureOffset - 20)) && (sterilizationTemperatureOffset != 0) && (sterilizationTemperatureOffset != TEMPERATURE_ALARM_VALUE)) {   
        // Retour water temperature has come within 2 degree celcius of setpoint
        sterilizationTemperatureOffset += ReadSmartEeprom16(SEEP_ADDR_STERILIZATION_SETPOINT_OFFSET_STEPS);  // Increase offset with 2 degree Celcius
    }    
        
    if (sterilisationMode != PASSIVE) {
        setActiveModeControllerHeatpumpSetpointHeating(sterilizationTemperature + sterilizationTemperatureOffset);
    }
        
    int16_t currentHotWaterBufferTemp = GetNtcTemperature(NTC_HOT_WATER_BUFFER);
    if(currentHotWaterBufferTemp >= sterilizationTemperature){
        // Sterilization reached the set temperature
        if (sterilizationReachedTemperatureTimeStamp == UINT32_MAX) {
            // Sterilization reached the temperature for the first time, so store the timestamp
            sterilizationReachedTemperatureTimeStamp = getSecondCounterLegionella();
        }
            
        int16_t sterilizationRunTime = UnitSystemParameterL[ADDRESS_STERILIZATION_RUN_TIME - START_ADDRESS_UNIT_SYSTEM_PARAMETER_L][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP];
        if (getSecondCounterLegionella() < (sterilizationReachedTemperatureTimeStamp + (sterilizationRunTime * 60))) {
            return true;
        }
            
        // Sterilization is done
        //TurnOffHeatingElementHotWaterBuffer();
        sterilizationHotwaterElementOn = false;
        setSecondCounterLegionella(UINT32_MAX);
        sterilizationReachedTemperatureTimeStamp = UINT32_MAX;
        WriteSmartEeprom16(SEEP_ADDR_DAY_COUNTER_STERILIZATION, 0);
        sterilizationTemperatureOffset = TEMPERATURE_ALARM_VALUE;
             
        setSterilisationMode(OFF);
        return true;
    } 
        
    // Check if sterilisation was already on PASSIVE mode
    sterilizationReachedTemperatureTimeStamp = UINT32_MAX;      
    if (sterilisationMode == PASSIVE ) {
        return false;
    } 

    // Max time in ACTIVE sterilisation has not been reached yet 
    if (getSecondCounterLegionella() < ReadSmartEeprom16(SEEP_ADDR_STERILIZATION_MAX_TIME_IN_STERILIZATION_MODE)) {   
        return true;
    }
            
    // 120 minutes in ACTIVE sterilization mode passed, but still not finished, set sterilisation to PASSIVE mode
    setSecondCounterLegionella(0);
    //TurnOnHeatingElementHotWaterBuffer();   // Set heating element on
    sterilizationHotwaterElementOn = true;
    sterilizationTemperatureOffset = TEMPERATURE_ALARM_VALUE;
            
    setSterilisationMode(PASSIVE); 
    return true;
}



const char * getSterilizationState(STERILIZATION_MODE state) {
    switch (state)
    {
        case(OFF): {
            return "0, OFF";
            break;
        }
        
        case(PASSIVE): {
            return "1, PASSIVE";
            break;
        }
        
        case(ACTIVE): {
            return "2, ACTIVE";
            break;
        }
        
        case(ON_HOLD): {
            return "3, ON HOLD";
            break;
        }
        
        default: {
            return "-1, UNKOWN";
            break;
        }
    }

    return "-1, UNKOWN";
}

