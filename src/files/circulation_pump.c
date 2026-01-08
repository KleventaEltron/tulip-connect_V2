/* TODO:  Include other files here if needed. */
#include <xc.h>
#include <stddef.h>                    
#include <stdbool.h>                   
#include <stdlib.h>                   
#include <string.h>
#include <stdbool.h>                    // Defines true
#include "definitions.h"
#include <stdio.h>
#include "definitions.h"    

#include "states.h"
#include "circulation_pump.h"
#include "files\eeprom.h"
#include "logging.h"
#include "sterilization.h"

#include "time_counters.h"
#include "files\eeprom.h"
#include "ntc.h"
#include "heating_mode.h"
//#include "logging.h"

//static char * StringPumpInit = "Init";
//static char * StringPumpOff = "Pump off";
//static char * StringPumpOn = "Pump on";
//static char * StringPumpLagTime = "Pump is in lag time";
//static char * StringPumpTooLongIdle = "Pump too long idle";
//static char * StringPumpOffTooLowTemp = "Pump off too low temp";

//static char * StringPumpUnknown = "Unknown";

//static char * StringPumpInit = "Init";
//static char * StringPumpOff = "Pump off";
//static char * StringPumpOn = "Pump on";
//static char * StringPumpLagTime = "Pump is in lag time";
//static char * StringPumpTooLongIdle = "Pump too long idle";
//static char * StringPumpOffTooLowTemp = "Pump off too low temp";
extern CIRCULATION_PUMP_DATA circulation_pump_data;

//static char * StringPumpUnknown = "Unknown";
//static bool temperatureTooLow = false;

void CIRCULATION_PUMP_Initialize()
{
    TurnOffCirculationPump();
    setSecondCounterCirculationPumpTask(0);
    
    circulation_pump_data.temperatureTooLowForPumpToBeOn = false;
    circulation_pump_data.state = CIRCULATION_PUMP_INITIALIZE;
    return;
}

CIRCULATION_PUMP_DATA getCirculationPumpData() {
    return circulation_pump_data;
}

const char * getCirculationPumpStateToString()
{
    switch (circulation_pump_data.state)
    {
        case(CIRCULATION_PUMP_INITIALIZE): {
            return "0, Init";
            break;
        }
        
        case(CIRCULATION_PUMP_OFF): {
            return "1, OFF";
            break;
        }
        
        case(CIRCULATION_PUMP_ON): {
            return "2, ON";
            break;
        }
        
        case(CIRCULATION_PUMP_LAG_TIME): {
            return "3, Lag time";
            break;
        }
        
        case(CIRCULATION_PUMP_TOO_LONG_OFF): {
            return "4, ON after too long off";
            break;
        }
        
        default:{
            return "-1, Unkown";
            break;
        }
        return "-1, Unkown";
    }
}

void checkLowBufferTemperature(int16_t currentTemperature, int16_t pumpOffTemperature, int16_t pumpBackOnTemperature)
{
    if (pumpOffTemperature >= pumpBackOnTemperature) {
        // Protection: This should not be possible, set the pump on
        circulation_pump_data.temperatureTooLowForPumpToBeOn = false;
        return;
    }
        // Temperature is lower than setpoint - threshold
    
    if (currentTemperature <= pumpOffTemperature){
        // Temperature is lower Pump Off Temperature setting
        circulation_pump_data.temperatureTooLowForPumpToBeOn = true;
        return;
    }
    
    if (currentTemperature >= pumpBackOnTemperature){
        // Temperature is higher again than Pump Back On Temperature setting
        circulation_pump_data.temperatureTooLowForPumpToBeOn = false;
        return;
    }
}

bool canPumpRunInThisHeatingState(HEATING_MODE_DATA heatingModeData, COOLING_MODE_DATA coolingModeData, HOT_WATER_HEATING_MODE_DATA hotwaterHeatingModeData, HOT_WATER_COOLING_MODE_DATA hotwaterCoolingModeData)
{
    if (ReadSmartEeprom16(SEEP_ADDR_EMERGENCY_MODE_HEATING_ENABLED) == true) {
        // Emergency mode heating is enabled
        if (( hotwaterHeatingModeData.state == HOT_WATER_HEATING_STATE_WAIT_FOR_MINIMAL_TIME_IN_HOT_WATER) ||
            ( hotwaterHeatingModeData.state == HOT_WATER_HEATING_STATE_RUNNING_IN_HOT_WATER) ||
            ( hotwaterHeatingModeData.state == HOT_WATER_HEATING_STATE_RUNNING_WITH_ELEMENT_ON_IN_HOT_WATER)) {
            // If in hot water state in heating/hotwater mode, pump can run
            return true;
        }
    }
    
    if ((heatingModeData.state == HEATING_IDLE) || 
            (heatingModeData.state == HEATING_RUNNING) || 
            (coolingModeData.state == COOLING_IDLE) || 
            (coolingModeData.state == COOLING_RUNNING) || 
            (hotwaterHeatingModeData.state == HOT_WATER_HEATING_IDLE_HEATING) || 
            (hotwaterHeatingModeData.state == HOT_WATER_HEATING_RUNNING_ON_HEATING) ||
            (hotwaterCoolingModeData.state == HOT_WATER_COOLING_IDLE_COOLING) || 
            (hotwaterCoolingModeData.state == HOT_WATER_COOLING_MODE_RUNNING_ON_COOLING)) {
        // Pump can run
        return true;
    }
    else{
        // Pump cannot run
        return false;
    }
}

bool hasActiveNonDefrostingUnitDuringDefrost(uint16_t defrostMask, uint16_t compressorMask)
{
    // Check for active compressor but is not in defrosting:
    uint16_t activeNotDef = (uint16_t)(compressorMask & (uint16_t)(~defrostMask));
    
    return activeNotDef != 0;
}

bool circulationPumpConditions()
{
    if ((circulation_pump_data.temperatureTooLowForPumpToBeOn == true) && 
            ((getActiveStateValue() == HEATING) || (getActiveStateValue() == HOT_WATER_HEATING))){
        // Temperature too low in heating modes
        return false;
    }
    
    if (getSterilisationMode() == ACTIVE){
        return false;
    }
    
//    uint16_t defrostingMask = getDefrostingActiveMask();
//    if (defrostingMask != 0){
//        // One or more heatpumps in defrosting
//        if (hasActiveNonDefrostingUnitDuringDefrost(defrostingMask, getActiveCompressorsMask()) == false){
//            return false;
//        }
//    }
    
    if (canPumpRunInThisHeatingState(getHeatingModeData(), getCoolingModeData(), getHotWaterHeatingModeData(), getHotWaterCoolingModeData()) == false){
        // Pump can not run in this state
        return false;
    }
    
    // Else return true
    return true;
}

void CIRCULATION_PUMP_Tasks()
{      
    // Circulation pump must run if:
    //   App state is APP_HEATING_AND_HOT_WATER_STATE_WAIT_FOR_HEATING_SETPOINT_REACHED
    //   &&
    //   Thermostat contact has been made 
    //   && 
    //   Heating element is off
    //   ||
    //   If pump has been off for 2 hours, go on for 10 minutes
    //
    // Pump must keep running 2 minutes if pump was on and thermostat contact has been disconnected
    //if (GetNtcTemperature(NTC_HEATING_BUFFER < ))
    //    temperatureTooLow
    
    int16_t heatingSetpoint = INT16_MAX;
    if (getHotWaterHeatingModeData().heatingCurveSet || getHeatingModeData().heatingCurveSet) {
        heatingSetpoint = getHeatpumpHeatingSetpoint()*10;
    } else {
        heatingSetpoint = getHeatingSetpoint();
    }
    
    int16_t coolingSetpoint = INT16_MAX;
    if (getHotWaterCoolingModeData().coolingCurveSet || getCoolingModeData().coolingCurveSet) {
        coolingSetpoint = getHeatpumpCoolingSetpoint()*10;
    } else {
        coolingSetpoint = getCoolingSetpoint();
    }    
    
    
    checkLowBufferTemperature(GetNtcTemperature(NTC_HEATING_BUFFER), ReadSmartEeprom16(SEEP_ADDR_CIRCULATION_PUMP_OFF_TEMPERATURE), ReadSmartEeprom16(SEEP_ADDR_CIRCULATION_PUMP_ON_TEMPERATURE));
    
    switch ( circulation_pump_data.state )
    {
        // 0: Circulation pump init
        case CIRCULATION_PUMP_INITIALIZE:{
            TurnOffCirculationPump();
            setSecondCounterCirculationPumpTask(0);

            circulation_pump_data.temperatureTooLowForPumpToBeOn = false;
            circulation_pump_data.state = CIRCULATION_PUMP_OFF;
            break;
        }
        
        case CIRCULATION_PUMP_OFF:{
            
            if ((circulationPumpConditions() == true) && (GetThermostatContact() == true)){
                // Conditions are made and thermostat contact made
                setSecondCounterCirculationPumpTask(0);
                TurnOnCirculationPump();
                circulation_pump_data.state = CIRCULATION_PUMP_ON;
                break;
            }
            
            if (getSecondCounterCirculationPumpTask() >= ReadSmartEeprom32(SEEP_ADDR_PUMP_MAXIMUM_OFF_TIME_SEC)){
                // Maximum off time passed
                setSecondCounterCirculationPumpTask(0);
                TurnOnCirculationPump();
                circulation_pump_data.state = CIRCULATION_PUMP_TOO_LONG_OFF;
                break;
            }
            
            break;
        }
        
        case CIRCULATION_PUMP_ON:{
            
            if (circulationPumpConditions() == false){
                // Conditions are not made
                setSecondCounterCirculationPumpTask(0);
                TurnOffCirculationPump();
                circulation_pump_data.state = CIRCULATION_PUMP_OFF;
                break;
            }
            
            if (GetThermostatContact() == false){
                // Thermostat contact not made
                setSecondCounterCirculationPumpTask(0);
                circulation_pump_data.state = CIRCULATION_PUMP_LAG_TIME;
                break;
            }

            break;
        }
        
        case CIRCULATION_PUMP_LAG_TIME:{
            
            if (circulationPumpConditions() == false){
                // Conditions are not made
                setSecondCounterCirculationPumpTask(0);
                TurnOffCirculationPump();
                circulation_pump_data.state = CIRCULATION_PUMP_OFF;
                break;
            }
            
            if (GetThermostatContact() == true){
                // Thermostat contact not made
                setSecondCounterCirculationPumpTask(0);
                circulation_pump_data.state = CIRCULATION_PUMP_ON;
                break;
            }
            
            if (getSecondCounterCirculationPumpTask() >= ReadSmartEeprom16(SEEP_ADDR_PUMP_LAG_TIME_AFTER_THERMOSTAT_CONTACT_DISCONNECTED_SEC)){
                // Lag time passed
                setSecondCounterCirculationPumpTask(0);
                TurnOffCirculationPump();
                circulation_pump_data.state = CIRCULATION_PUMP_OFF;
                break;
            }
            
            break;
        }
        
        case CIRCULATION_PUMP_TOO_LONG_OFF:{
            
            if (getSecondCounterCirculationPumpTask() >= ReadSmartEeprom16(SEEP_ADDR_PUMP_ON_TIME_AFTER_OFF_TIME_REACHED_SEC)){   
                // Pump on time after off time reached
                setSecondCounterCirculationPumpTask(0);  
                TurnOffCirculationPump();
                circulation_pump_data.state = CIRCULATION_PUMP_OFF;
                break;
            }
            
            break;
        }
        
        default:{
            CIRCULATION_PUMP_Initialize();
            break;
        }
    }
}
