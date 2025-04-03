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

uint32_t sterilizationReachedTemperatureTimeStamp = UINT32_MAX;
uint16_t sterilizationTemperatureOffset = TEMPERATURE_ALARM_VALUE;
STERILIZATION_MODE sterilisationMode = OFF;


STERILIZATION_MODE getSterilisationMode() {
    return sterilisationMode;
}

void setSterilisationMode(STERILIZATION_MODE newMode) {
    sterilisationMode = newMode;
}


void checkNeedForSterilization() {
    if(sterilisationMode != ACTIVE && goToActiveSterilization()){
        setSterilisationMode(ACTIVE);
    }        
}


bool goToActiveSterilization() {
    uint16_t currentHotWaterBufferTemp = GetNtcTemperature(NTC_HOT_WATER_BUFFER);
    uint16_t sterilizationStartTime = UnitSystemParameterL[ADDRESS_STERILIZATION_START_TIME - START_ADDRESS_UNIT_SYSTEM_PARAMETER_L][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP];
    uint16_t sterilizationFunction = UnitSystemParameterL[ADDRESS_HIGH_TEMPERATURE_STERILIZATION_FUNCTION - START_ADDRESS_UNIT_SYSTEM_PARAMETER_L][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP];
    uint16_t sterilizationIntervalDays = UnitSystemParameterL[ADDRESS_STERILIZATION_INTERVAL_DAYS - START_ADDRESS_UNIT_SYSTEM_PARAMETER_L][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP];
    uint8_t currentDisplayTimeHours = (uint8_t)(UserParameters[ADDRESS_DISPLAY_TIME - START_ADDRESS_USER_PARAMETERS][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP] >> 8);
    uint8_t currentDisplayTimeMinutes = (uint8_t)UserParameters[ADDRESS_DISPLAY_TIME - START_ADDRESS_USER_PARAMETERS][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP];
    uint16_t dayCounter = ReadSmartEeprom16(SEEP_ADDR_DAY_COUNTER_STERILIZATION);
    uint16_t maxTimeOutOfSterilizationMode = ReadSmartEeprom16(SEEP_ADDR_STERILIZATION_MAX_TIME_OUT_OF_STERILIZATION_MODE_WITH_ELEMENT_ON);    

    // Passive sterilization check, if this triggers we need to return to active sterilization    
    //if (secondCounterLegionella != UINT32_MAX && (secondCounterLegionella > maxTimeOutOfSterilizationMode) && (heatingElementStatus == true)) {   
    if(sterilisationMode == PASSIVE && (getSecondCounterLegionella() >= maxTimeOutOfSterilizationMode)) {
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
        return true;
    }

    return false;
}





bool sterilisationIsActivelyRunning() {
    if (sterilisationMode == OFF) {
        return false;
    } 
    
    // If we start a fresh sterilization cycle we have to set some parameters before proceeding.
    if (sterilisationMode == ACTIVE && getSecondCounterLegionella() == UINT32_MAX) {
                    
        bool valvePosition = getStatus3WayValve();
        if (valvePosition != VALVE_IS_ON_HOT_WATER_CIRCUIT) {
            return true;
        }

        TurnOffHeatingElementHotWaterBuffer();
        setSecondCounterLegionella(0);    
        sterilizationTemperatureOffset = ReadSmartEeprom16(SEEP_ADDR_STERILIZATION_SETPOINT_OFFSET_START);  
    }        
               
    uint16_t retourWaterTemperature = (RealTimeData[ADDRESS_RETURN_WATER_TEMPERATURE_T6 - START_ADDRESS_REAL_TIME_DATA][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP] * 10);
    int16_t sterilizationTemperature = (UnitSystemParameterL[ADDRESS_STERILIZATION_TEMPERATURE_SETTING - START_ADDRESS_UNIT_SYSTEM_PARAMETER_L][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP] * 10);        
       
    if ((retourWaterTemperature >= (sterilizationTemperature + sterilizationTemperatureOffset - 20)) && (sterilizationTemperatureOffset != 0) && (sterilizationTemperatureOffset != TEMPERATURE_ALARM_VALUE)) {   
        // Retour water temperature has come within 2 degree celcius of setpoint
        sterilizationTemperatureOffset += ReadSmartEeprom16(SEEP_ADDR_STERILIZATION_SETPOINT_OFFSET_STEPS);  // Increase offset with 2 degree Celcius
    }    
        
    if (sterilisationMode != PASSIVE) {
        setActiveModeControllerHeatpumpSetpoint(sterilizationTemperature + sterilizationTemperatureOffset);
    }
        
    int16_t currentHotWaterBufferTemp = GetNtcTemperature(NTC_HOT_WATER_BUFFER);
    if(currentHotWaterBufferTemp >= sterilizationTemperature){
        if (sterilizationReachedTemperatureTimeStamp == UINT32_MAX) {
            // Sterilization reached the temperature for the first time, so store the timestamp
            sterilizationReachedTemperatureTimeStamp = getSecondCounterLegionella();
        }
            
        int16_t sterilizationRunTime = UnitSystemParameterL[ADDRESS_STERILIZATION_RUN_TIME - START_ADDRESS_UNIT_SYSTEM_PARAMETER_L][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP];
        if (getSecondCounterLegionella() < (sterilizationReachedTemperatureTimeStamp + (sterilizationRunTime * 60))) {
            return true;
        }
            
        // Sterilization is done
        TurnOffHeatingElementHotWaterBuffer();
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
    TurnOnHeatingElementHotWaterBuffer();   // Set heating element on
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
        
        default: {
            return "-1, UNKOWN";
            break;
        }
    }

    return "-1, UNKOWN";
}

