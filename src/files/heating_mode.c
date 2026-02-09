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
bool changeCompensationsHeating = false;

void setTemperatureOperatingCycleHeating() {
    if ((getsystemOnCounter() % 10) == 0) {
        changeSetting = true;
        return;
    }

    if (checkIfDefrostingActive()) {
        if(getActiveModeControllerPumpOffDueToDipSwitch1()) {
            setActiveModeControllerPumpOffDueToDipSwitch1(false);
        }
        changeSetting = false;
        return;
    }
    
    
    if (!regulateOnTempSensorInBufferHeating) {
        changeSetting = false;
        return;
    }
    
    int16_t heatingBufferTemperature = GetNtcTemperature(NTC_HEATING_BUFFER);
    int16_t heatingSetpoint = TEMPERATURE_ALARM_VALUE; 
    
    if (heating_mode_data.heatingCurveSet) {
        heatingSetpoint = getHeatpumpHeatingSetpoint()*10;
    } else {
        heatingSetpoint = getHeatingSetpoint();
    }   
    
    if (heatingBufferTemperature <= (heatingSetpoint - (getDataFromMemoryCallable(ADDRESS_AIR_CONDITIONER_RETURN_DIFFERENCE, MASTER_HEATPUMP_IN_CASCADE) * 10)) 
            // && getDataFromMemoryCallable(ADDRESS_CONSTANT_TEMPERATURE_OPERATION_CYCLE) != 1
            && getActiveModeControllerPumpOffDueToDipSwitch1()
            && changeSetting) {
        // ChangeHeatpumpSetting(ADDRESS_CONSTANT_TEMPERATURE_OPERATION_CYCLE, 1);
        setActiveModeControllerPumpOffDueToDipSwitch1(false);
        changeSetting = false;
        return;
    }  
    
    if (heatingBufferTemperature >= heatingSetpoint
            //&& getDataFromMemoryCallable(ADDRESS_CONSTANT_TEMPERATURE_OPERATION_CYCLE) != 240
            && !getActiveModeControllerPumpOffDueToDipSwitch1()
            && changeSetting) {
        setActiveModeControllerPumpOffDueToDipSwitch1(true);
        //ChangeHeatpumpSetting(ADDRESS_CONSTANT_TEMPERATURE_OPERATION_CYCLE, 240);
        changeSetting = false;
        return;        
    }
}



int16_t determineCorrectHeatingSetpoint() {
    int16_t heatingBufferTemperature = GetNtcTemperature(NTC_HEATING_BUFFER);
    int16_t heatingSetpoint = TEMPERATURE_ALARM_VALUE; 
    
    if (heating_mode_data.heatingCurveSet) {
        heatingSetpoint = getHeatpumpHeatingSetpoint()*10;
        
        if ((getsystemOnCounter() % 10) == 0) {
            changeCompensationsHeating = true;
            return heatingSetpoint;
        }    
        
        if (heatingBufferTemperature >= heatingSetpoint && changeCompensationsHeating && 
                (getDataFromMemoryCallable(ADDRESS_RETURN_WATER_TEMPERATURE_COMPENSATION_VALUE, MASTER_HEATPUMP_IN_CASCADE) != 2 
                || getDataFromMemoryCallable(ADDRESS_OUTLET_WATER_TEMPERATURE_COMPENSATION_VALUE, MASTER_HEATPUMP_IN_CASCADE) != 2)) { 
            ChangeHeatpumpSetting(ADDRESS_RETURN_WATER_TEMPERATURE_COMPENSATION_VALUE, 2);
            ChangeHeatpumpSetting(ADDRESS_OUTLET_WATER_TEMPERATURE_COMPENSATION_VALUE, 2);
            changeCompensationsHeating = false;
        }
        
        if (getActiveCompressorsMask() == 0) {
            return heatingSetpoint;
        }             
        
        if (heatingBufferTemperature >= heatingSetpoint) {
            return heatingSetpoint;
        }
        
        if((heatingSetpoint - getHeatpumpReturnWaterTemperature(MASTER_HEATPUMP_IN_CASCADE)) < 20  && changeCompensationsHeating) {
            ChangeHeatpumpSetting(ADDRESS_RETURN_WATER_TEMPERATURE_COMPENSATION_VALUE, (getDataFromMemoryCallable(ADDRESS_RETURN_WATER_TEMPERATURE_COMPENSATION_VALUE, MASTER_HEATPUMP_IN_CASCADE) - 1));
            ChangeHeatpumpSetting(ADDRESS_OUTLET_WATER_TEMPERATURE_COMPENSATION_VALUE, (getDataFromMemoryCallable(ADDRESS_OUTLET_WATER_TEMPERATURE_COMPENSATION_VALUE, MASTER_HEATPUMP_IN_CASCADE) - 1));            
            changeCompensationsHeating = false;
        }
        
        return heatingSetpoint;
    } else {
        heatingSetpoint = getHeatingSetpoint();
    }   
    
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
    
    if (getActiveCompressorsMask() == 0) {
        heating_mode_data.stepperSetpoint = heatingSetpoint;
        return heatingSetpoint;
    }
    
    if ((heatingSetpoint - getHeatpumpReturnWaterTemperature(MASTER_HEATPUMP_IN_CASCADE)) <= 20 ) {
        if (getHeatpumpReturnWaterTemperature(MASTER_HEATPUMP_IN_CASCADE) >= (heating_mode_data.stepperSetpoint - 20)) {
            heating_mode_data.stepperSetpoint += 20;
        }
    }
    
//    if ((heatingSetpoint - getHeatpumpReturnWaterTemperature(MASTER_HEATPUMP_IN_CASCADE)) > 50) {
//        heating_mode_data.stepperSetpoint = getHeatpumpReturnWaterTemperature(MASTER_HEATPUMP_IN_CASCADE) + 20;
//        return heating_mode_data.stepperSetpoint;
//    }

    
//    if (getHeatpumpReturnWaterTemperature(MASTER_HEATPUMP_IN_CASCADE) >= (heating_mode_data.stepperSetpoint - 20) && (heating_mode_data.stepperSetpoint - getHeatpumpReturnWaterTemperature(MASTER_HEATPUMP_IN_CASCADE)) <= 20) {
//        //heating_mode_data.stepperSetpoint = getHeatpumpReturnWaterTemperature(MASTER_HEATPUMP_IN_CASCADE) + 20;
//        heating_mode_data.stepperSetpoint += 20;
//        return heating_mode_data.stepperSetpoint;
//        //heatingSetpoint += 20;
//    }
    
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
        
        case(HEATING_RUNNING_WITH_CIRCULATION_PUMP_OFF): {
            return "4, Running with circulation pump off";
            break;
        }
        
        case(HEATING_RUNNING_WITH_ELEMENT_ON_AND_CIRCULATION_PUMP_OFF): {
            return "5, Running with element on and circulation pump off";
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
    
//    if (getDataFromMemoryCallable(ADDRESS_HEATING_CURVE_SETTING) > 0) {
//        heating_mode_data.heatingCurveSet = true; 
//        if (getDataFromMemoryCallable(ADDRESS_HEATING_CURVE_SETTING) != ReadSmartEeprom8(ADDRESS_HEATING_CURVE_SETTING)) {
//            WriteSmartEeprom8(SEEP_ADDR_HEATING_CURVE, getDataFromMemoryCallable(ADDRESS_HEATING_CURVE_SETTING));
//        }    
//    } else {
//        heating_mode_data.heatingCurveSet = false; 
//        if (getDataFromMemoryCallable(ADDRESS_HEATING_CURVE_SETTING) != ReadSmartEeprom8(ADDRESS_HEATING_CURVE_SETTING)) {
//            WriteSmartEeprom8(SEEP_ADDR_HEATING_CURVE, 0);
//        }         
//    }

    if(ReadSmartEeprom16(SEEP_ADDR_HEATING_CURVE) > 0 && ReadSmartEeprom16(SEEP_ADDR_HEATING_CURVE) != UINT16_MAX) {
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
            setActiveModeControllerPumpOffDueToDipSwitch1(false);
            //ChangeHeatpumpSetting(ADDRESS_CONSTANT_TEMPERATURE_OPERATION_CYCLE, 10);
        }
    }
    
    setTemperatureOperatingCycleHeating();
    
    setActiveModeControllerHeatpumpSetpointHeating(determineCorrectHeatingSetpoint());
    
    setActiveModeControllerHeatpumpRunningMode(SET_MODE_HEATING);
    
    switch ( heating_mode_data.state )
    {
        // 0
        case HEATING_INITIALIZE:{
            
            //TurnOffHeatingElementHeatingBuffer();
            heating_mode_data.HeatingElementOn = false;
            heating_mode_data.initialBufferTemp = TEMPERATURE_ALARM_VALUE; 
            heating_mode_data.stepperSetpoint = TEMPERATURE_ALARM_VALUE;
            setSecondCounterHeatingTask(UINT32_MAX);
            
            if(regulateOnTempSensorInBufferHeating && !checkIfDefrostingActive()) {
                // ChangeHeatpumpSetting(ADDRESS_CONSTANT_TEMPERATURE_OPERATION_CYCLE, 240);
                setActiveModeControllerPumpOffDueToDipSwitch1(true);
            }   
            
            ChangeHeatpumpSetting(ADDRESS_RETURN_WATER_TEMPERATURE_COMPENSATION_VALUE, 2);
            ChangeHeatpumpSetting(ADDRESS_OUTLET_WATER_TEMPERATURE_COMPENSATION_VALUE, 2);
            
            ChangeHeatpumpSetting(ADDRESS_HEATING_CURVE_SETTING, ReadSmartEeprom16(SEEP_ADDR_HEATING_CURVE));
            ChangeHeatpumpSetting(ADDRESS_COOLING_CURVE_SETTING, ReadSmartEeprom16(SEEP_ADDR_COOLING_CURVE));
            WriteSmartEeprom16(SEEP_ADDR_HEATING_SETPOINT_CURVE_BACKUP, UINT16_MAX);
            WriteSmartEeprom16(SEEP_ADDR_COOLING_SETPOINT_CURVE_BACKUP, UINT16_MAX);
            
            heating_mode_data.state = HEATING_IDLE;
            break;
        }
        
        // 1
        case HEATING_IDLE:{                
            if (getActiveCompressorsMask() != 0){
                // Compressor is running
                setSecondCounterHeatingTask(0);
                heating_mode_data.initialBufferTemp = heatingBufferTemperature;
                heating_mode_data.stepperSetpoint = getHeatingSetpoint();
                //heating_mode_data.stepperSetpoint = (getHeatpumpReturnWaterTemperature(MASTER_HEATPUMP_IN_CASCADE) + 20);
                
                if(regulateOnTempSensorInBufferHeating) {
                    // ChangeHeatpumpSetting(ADDRESS_CONSTANT_TEMPERATURE_OPERATION_CYCLE, 1);
                    setActiveModeControllerPumpOffDueToDipSwitch1(false);
                }
                
                heating_mode_data.state = HEATING_RUNNING;
                break;
            }
            
            break;
        }
        
        // 2
        case HEATING_RUNNING:{
            
            if ((getActiveCompressorsMask() == 0) && (getDefrostingActiveMask() == 0)){
                // Compressor is not running and is also not in defrosting
                //TurnOffHeatingElementHeatingBuffer();
                heating_mode_data.HeatingElementOn = false;
                heating_mode_data.initialBufferTemp = TEMPERATURE_ALARM_VALUE;
                setSecondCounterHeatingTask(UINT32_MAX);
                
                if(regulateOnTempSensorInBufferHeating && !checkIfDefrostingActive()) {
                    //ChangeHeatpumpSetting(ADDRESS_CONSTANT_TEMPERATURE_OPERATION_CYCLE, 240);
                    setActiveModeControllerPumpOffDueToDipSwitch1(true);
                }

                heating_mode_data.state = HEATING_IDLE;
                break;
            }
            
            if (heatingBufferTemperature < heating_mode_data.initialBufferTemp) {
                // Adjust the reference temp to the lowest measured temperature 
                heating_mode_data.initialBufferTemp = heatingBufferTemperature;
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
                //heating_mode_data.initialBufferTemp = TEMPERATURE_ALARM_VALUE;
                heating_mode_data.HeatingElementOn = true;
                
                if (checkIfBufferIsWithinSetpointMinusDelta(heatingBufferTemperature) == true) {
                    // Buffer temp is within setpoint - delta, must rise 1 degree within the given time next phase
                    heating_mode_data.initialBufferTemp = heatingBufferTemperature;
                }
                else {
                    // Buffer temp is not within setpoint - delta must rise untill the setpoint - delta next phase
                    heating_mode_data.initialBufferTemp = TEMPERATURE_ALARM_VALUE;
                }
                
                heating_mode_data.state = HEATING_RUNNING_WITH_ELEMENT_ON;
                break;
            }
            
            break;
        }
        
        // 3
        case HEATING_RUNNING_WITH_ELEMENT_ON:{
            
            if ((getActiveCompressorsMask() == 0) && (getDefrostingActiveMask() == 0)){
                // Compressor is not running and is also not in defrosting
                heating_mode_data.HeatingElementOn = false;
                heating_mode_data.initialBufferTemp = TEMPERATURE_ALARM_VALUE;
                setSecondCounterHeatingTask(UINT32_MAX);

                if(regulateOnTempSensorInBufferHeating && !checkIfDefrostingActive()) {
                    //ChangeHeatpumpSetting(ADDRESS_CONSTANT_TEMPERATURE_OPERATION_CYCLE, 240);
                    setActiveModeControllerPumpOffDueToDipSwitch1(true);
                }
                
                heating_mode_data.state = HEATING_IDLE;
                break;
            }
            
            if (getSecondCounterHeatingTask() >= ReadSmartEeprom16(SEEP_ADDR_HEATING_MODE_MAX_TIME_WITH_ELEMENT_ON)){
                // Temperature not rised a set temperature in a set time
                setSecondCounterHeatingTask(0);
                heating_mode_data.HeatingElementOn = false;
                
                if (checkIfBufferIsWithinSetpointMinusDelta(heatingBufferTemperature) == true) {
                    // Buffer temp is within setpoint - delta, must rise 1 degree within the given time next phase
                    heating_mode_data.initialBufferTemp = heatingBufferTemperature;
                }
                else {
                    // Buffer temp is not within setpoint - delta must rise untill the setpoint - delta next phase
                    heating_mode_data.initialBufferTemp = TEMPERATURE_ALARM_VALUE;
                }
                
                heating_mode_data.state = HEATING_RUNNING_WITH_CIRCULATION_PUMP_OFF;
                break;
            }
            
            if (heating_mode_data.initialBufferTemp != TEMPERATURE_ALARM_VALUE) {
                // Has a valid value, so must rise 1 degree now
                if (heatingBufferTemperature < heating_mode_data.initialBufferTemp) {
                    // Adjust the reference temp to the lowest measured temperature 
                    heating_mode_data.initialBufferTemp = heatingBufferTemperature;
                }
                
                if (heatingBufferTemperature >= heating_mode_data.initialBufferTemp + ReadSmartEeprom16(SEEP_ADDR_HEATING_RISE_TEMP_IN_GIVEN_TIME)){
                    // Temperature rised a set temperature in a set time
                    setSecondCounterHeatingTask(0);
                    heating_mode_data.HeatingElementOn = false;
                    heating_mode_data.initialBufferTemp = heatingBufferTemperature;
                    //heating_mode_data.stepperSetpoint = getHeatingSetpoint();

                    heating_mode_data.state = HEATING_RUNNING;
                    break;
                }
            }
            else {
                // Has not a valid value, must rise untill the setpoint-delta
                if (checkIfBufferIsWithinSetpointMinusDelta(heatingBufferTemperature) == true) {
                    // Buffer temp is within setpoint - delta, go back to running
                    setSecondCounterHeatingTask(0);
                    heating_mode_data.HeatingElementOn = false;
                    heating_mode_data.initialBufferTemp = heatingBufferTemperature;
                    //heating_mode_data.stepperSetpoint = getHeatingSetpoint();

                    heating_mode_data.state = HEATING_RUNNING;
                    break;
                }
            }
            
            break;
        }
        
        // 4
        case HEATING_RUNNING_WITH_CIRCULATION_PUMP_OFF:{
            
            if ((getActiveCompressorsMask() == 0) && (getDefrostingActiveMask() == 0)){
                // Compressor is not running and is also not in defrosting
                heating_mode_data.HeatingElementOn = false;
                heating_mode_data.initialBufferTemp = TEMPERATURE_ALARM_VALUE;
                setSecondCounterHeatingTask(UINT32_MAX);

                if(regulateOnTempSensorInBufferHeating && !checkIfDefrostingActive()) {
                    //ChangeHeatpumpSetting(ADDRESS_CONSTANT_TEMPERATURE_OPERATION_CYCLE, 240);
                    setActiveModeControllerPumpOffDueToDipSwitch1(true);
                }
                
                heating_mode_data.state = HEATING_IDLE;
                break;
            }
            
            if (getSecondCounterHeatingTask() >= ReadSmartEeprom16(SEEP_ADDR_HEATING_MODE_MAX_TIME_WITH_CIRCULATION_PUMP_OFF)){
                // Temperature not rised a set temperature in a set time
                setSecondCounterHeatingTask(0);
                heating_mode_data.HeatingElementOn = true;
                
                if (checkIfBufferIsWithinSetpointMinusDelta(heatingBufferTemperature) == true) {
                    // Buffer temp is within setpoint - delta, must rise 1 degree within the given time next phase
                    heating_mode_data.initialBufferTemp = heatingBufferTemperature;
                }
                else {
                    // Buffer temp is not within setpoint - delta must rise untill the setpoint - delta next phase
                    heating_mode_data.initialBufferTemp = TEMPERATURE_ALARM_VALUE;
                }
                
                heating_mode_data.state = HEATING_RUNNING_WITH_ELEMENT_ON_AND_CIRCULATION_PUMP_OFF;
                break;
            }
            
            if (heating_mode_data.initialBufferTemp != TEMPERATURE_ALARM_VALUE) {
                // Has a valid value, so must rise 1 degree now
                if (heatingBufferTemperature < heating_mode_data.initialBufferTemp) {
                    // Adjust the reference temp to the lowest measured temperature 
                    heating_mode_data.initialBufferTemp = heatingBufferTemperature;
                }
                
                if (heatingBufferTemperature >= heating_mode_data.initialBufferTemp + ReadSmartEeprom16(SEEP_ADDR_HEATING_RISE_TEMP_IN_GIVEN_TIME)){
                    // Temperature rised a set temperature in a set time
                    setSecondCounterHeatingTask(0);
                    heating_mode_data.HeatingElementOn = false;
                    heating_mode_data.initialBufferTemp = heatingBufferTemperature;
                    //heating_mode_data.stepperSetpoint = getHeatingSetpoint();

                    heating_mode_data.state = HEATING_RUNNING;
                    break;
                }
            }
            else {
                // Has not a valid value, must rise untill the setpoint-delta
                if (checkIfBufferIsWithinSetpointMinusDelta(heatingBufferTemperature) == true) {
                    // Buffer temp is within setpoint - delta, go back to running
                    setSecondCounterHeatingTask(0);
                    heating_mode_data.HeatingElementOn = false;
                    heating_mode_data.initialBufferTemp = heatingBufferTemperature;
                    //heating_mode_data.stepperSetpoint = getHeatingSetpoint();

                    heating_mode_data.state = HEATING_RUNNING;
                    break;
                }
            }

            break;
        }
        
        // 5
        case HEATING_RUNNING_WITH_ELEMENT_ON_AND_CIRCULATION_PUMP_OFF:{
            
            if ((getActiveCompressorsMask() == 0) && (getDefrostingActiveMask() == 0)){
                // Compressor is not running and is also not in defrosting
                heating_mode_data.HeatingElementOn = false;
                heating_mode_data.initialBufferTemp = TEMPERATURE_ALARM_VALUE;
                setSecondCounterHeatingTask(UINT32_MAX);

                if(regulateOnTempSensorInBufferHeating && !checkIfDefrostingActive()) {
                    //ChangeHeatpumpSetting(ADDRESS_CONSTANT_TEMPERATURE_OPERATION_CYCLE, 240);
                    setActiveModeControllerPumpOffDueToDipSwitch1(true);
                }
                
                heating_mode_data.state = HEATING_IDLE;
                break;
            }
            
            if (getSecondCounterHeatingTask() >= ReadSmartEeprom16(SEEP_ADDR_HEATING_MODE_MAX_TIME_WITH_ELEMENT_ON_AND_CIRCULATION_PUMP_OFF)){
                // Temperature not rised a set temperature in a set time
                setSecondCounterHeatingTask(0);
                heating_mode_data.HeatingElementOn = false;
                heating_mode_data.initialBufferTemp = heatingBufferTemperature;
                
                heating_mode_data.state = HEATING_RUNNING;
                break;
            }
            
            if (heating_mode_data.initialBufferTemp != TEMPERATURE_ALARM_VALUE) {
                // Has a valid value, so must rise 1 degree now
                if (heatingBufferTemperature < heating_mode_data.initialBufferTemp) {
                    // Adjust the reference temp to the lowest measured temperature 
                    heating_mode_data.initialBufferTemp = heatingBufferTemperature;
                }
                
                if (heatingBufferTemperature >= heating_mode_data.initialBufferTemp + ReadSmartEeprom16(SEEP_ADDR_HEATING_RISE_TEMP_IN_GIVEN_TIME)){
                    // Temperature rised a set temperature in a set time
                    setSecondCounterHeatingTask(0);
                    heating_mode_data.HeatingElementOn = false;
                    heating_mode_data.initialBufferTemp = heatingBufferTemperature;
                    //heating_mode_data.stepperSetpoint = getHeatingSetpoint();

                    heating_mode_data.state = HEATING_RUNNING;
                    break;
                }
            }
            else {
                // Has not a valid value, must rise untill the setpoint-delta
                if (checkIfBufferIsWithinSetpointMinusDelta(heatingBufferTemperature) == true) {
                    // Buffer temp is within setpoint - delta, go back to running
                    setSecondCounterHeatingTask(0);
                    heating_mode_data.HeatingElementOn = false;
                    heating_mode_data.initialBufferTemp = heatingBufferTemperature;
                    //heating_mode_data.stepperSetpoint = getHeatingSetpoint();

                    heating_mode_data.state = HEATING_RUNNING;
                    break;
                }
            }

            break;
        }
        
        // Others:
        default:{
            HEATING_MODE_Initialize();
            break;
        }
    }
    
}


