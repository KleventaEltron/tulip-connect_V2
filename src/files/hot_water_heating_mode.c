#include <stddef.h>                    
#include <stdbool.h>                   
#include <stdlib.h>                   
#include <string.h>
#include <stdio.h>
#include "definitions.h"    

#include "hot_water_heating_mode.h"
#include "states.h"

#include "eeprom.h"
#include "time_counters.h"
#include "ntc.h"
#include "sterilization.h"
#include "defrosting.h"
#include "modbus\heatpump_parameters.h"
#include "modbus/display.h"

extern HOT_WATER_HEATING_MODE_DATA hot_water_heating_mode_data;
bool regulateOnTempSensorInBufferHotWaterHeating = false;

bool getHeatingElementBoolFromHotwaterHeatingMode() {
    return hot_water_heating_mode_data.HeatingElementOn;
}

bool getHotwaterElementBoolFromHotwaterHeatingMode() {
    return hot_water_heating_mode_data.HotwaterElementOn;
}

const char * getHotwaterHeatingStateToString()
{
    switch (hot_water_heating_mode_data.state)
    {
        case(HOT_WATER_HEATING_INITIALIZE_HEATING): {
            return "0, Init heating";
            break;
        }
        
        case(HOT_WATER_HEATING_IDLE_HEATING): {
            return "1, Idle heating";
            break;
        }
        
        case(HOT_WATER_HEATING_RUNNING_ON_HEATING): {
            return "2, Running heating";
            break;
        }
        
        case(HOT_WATER_HEATING_RUNNING_ON_HEATING_WITH_ELEMENT_ON): {
            return "3, Running heating with element on";
            break;
        }
        
        case(HOT_WATER_HEATING_INITIALIZE_HOT_WATER): {
            return "4, Init hotwater";
            break;
        }
        
        case(HOT_WATER_HEATING_STATE_WAIT_FOR_MINIMAL_TIME_IN_HOT_WATER): {
            return "5, Minimal time in hot water";
            break;
        }
        
        case(HOT_WATER_HEATING_STATE_RUNNING_IN_HOT_WATER): {
            return "6, Running hotwater";
            break;
        }
        
        case(HOT_WATER_HEATING_STATE_RUNNING_WITH_ELEMENT_ON_IN_HOT_WATER): {
            return "7, Running hotwater with element on";
            break;
        }
        
        default:{
            return "-1, Unkown";
            break;
        }
        return "-1, Unkown";
    }
}

bool areWeOnHotWaterModeInHotWaterAndHeatingMode()
{
    if (hot_water_heating_mode_data.state == HOT_WATER_HEATING_INITIALIZE_HOT_WATER){
        return true;
    }
    
    if (hot_water_heating_mode_data.state == HOT_WATER_HEATING_STATE_WAIT_FOR_MINIMAL_TIME_IN_HOT_WATER){
        return true;
    }
    
    if (hot_water_heating_mode_data.state == HOT_WATER_HEATING_STATE_RUNNING_IN_HOT_WATER){
        return true;
    }
    
    if (hot_water_heating_mode_data.state == HOT_WATER_HEATING_STATE_RUNNING_WITH_ELEMENT_ON_IN_HOT_WATER){
        return true;
    }
    
    return false;
}


void adjustSetpointOffsetHotWater()
{
    int16_t hotwaterBufferTemperature = GetNtcTemperature(NTC_HOT_WATER_BUFFER);
    int16_t hotwaterSetpoint = getHotwaterSetpoint();
    
    if ((hotwaterBufferTemperature == TEMPERATURE_ALARM_VALUE) || (hotwaterSetpoint == TEMPERATURE_ALARM_VALUE)) {
        // No temperature or setpoint known yet
        return;
    }
    
    if (hotwaterBufferTemperature >= getHotwaterSetpoint())
    {   // Hot water buffer tempereature is equal or higher than actual setpoint, so reset offset to 0
        hot_water_heating_mode_data.setpointHotWaterOffset = 0; 
        return;
    }
    
    if ((getHeatpumpReturnWaterTemperature() >= (getHotwaterSetpoint() + hot_water_heating_mode_data.setpointHotWaterOffset - 20)) && (hot_water_heating_mode_data.setpointHotWaterOffset != 0) && (hot_water_heating_mode_data.setpointHotWaterOffset != TEMPERATURE_ALARM_VALUE)){   
        // Retour water temperature has come within 2 degree celcius of setpoint, increase offset with 2 degrees
        hot_water_heating_mode_data.setpointHotWaterOffset += ReadSmartEeprom16(SEEP_ADDR_HOT_WATER_SETPOINT_OFFSET_STEPS);  
        return;
    }
}


bool changeSettingHotWaterHeating = false;
void setTemperatureOperatingCycleHotWaterHeating() {
    if ((getsystemOnCounter() % 10) == 0) {
        changeSettingHotWaterHeating = true;
        return;
    }    
          
    if (!regulateOnTempSensorInBufferHotWaterHeating) {
        changeSettingHotWaterHeating = false;
        return;
    }
    
    // Hot water mode so do nothing with the heatpump
    if (hot_water_heating_mode_data.state >= 4) {
        changeSettingHotWaterHeating = false;
        return;
    }
    
    int16_t heatingBufferTemperature = GetNtcTemperature(NTC_HEATING_BUFFER);
    int16_t heatingSetpoint = TEMPERATURE_ALARM_VALUE; 
    
    if (hot_water_heating_mode_data.heatingCurveSet) {
        heatingSetpoint = getHeatpumpHeatingSetpoint()*10;
    } else {
        heatingSetpoint = getHeatingSetpoint();
    }   
    
    
    if (heatingBufferTemperature <= (heatingSetpoint - (getDataFromMemoryCallable(ADDRESS_AIR_CONDITIONER_RETURN_DIFFERENCE) * 10)) 
            // && getDataFromMemoryCallable(ADDRESS_CONSTANT_TEMPERATURE_OPERATION_CYCLE) != 1
            && getActiveModeControllerPumpOffDueToDipSwitch1()
            && changeSettingHotWaterHeating) {
        // ChangeHeatpumpSetting(ADDRESS_CONSTANT_TEMPERATURE_OPERATION_CYCLE, 1);
        setActiveModeControllerPumpOffDueToDipSwitch1(false);
        changeSettingHotWaterHeating = false;
        return;
    }  

    if (heatingBufferTemperature >= heatingSetpoint
            // && getDataFromMemoryCallable(ADDRESS_CONSTANT_TEMPERATURE_OPERATION_CYCLE) != 240
            && !getActiveModeControllerPumpOffDueToDipSwitch1()
            && changeSettingHotWaterHeating) {
        // ChangeHeatpumpSetting(ADDRESS_CONSTANT_TEMPERATURE_OPERATION_CYCLE, 240);
        setActiveModeControllerPumpOffDueToDipSwitch1(true);
        changeSettingHotWaterHeating = false;
        return;        
    }    
}

bool changeCompensationsHotWaterHeating = false;
int16_t determineCorrectSetpoint() {
    if ((hot_water_heating_mode_data.state == HOT_WATER_HEATING_INITIALIZE_HEATING) ||
        (hot_water_heating_mode_data.state == HOT_WATER_HEATING_IDLE_HEATING) ||
        (hot_water_heating_mode_data.state == HOT_WATER_HEATING_RUNNING_ON_HEATING) ||
        (hot_water_heating_mode_data.state == HOT_WATER_HEATING_RUNNING_ON_HEATING_WITH_ELEMENT_ON) ) {
        
        int16_t heatingBufferTemperature = GetNtcTemperature(NTC_HEATING_BUFFER);
        int16_t heatingSetpoint = TEMPERATURE_ALARM_VALUE; 
        
        if (hot_water_heating_mode_data.heatingCurveSet) {
            
            heatingSetpoint = getHeatpumpHeatingSetpoint()*10;

            if ((getsystemOnCounter() % 10) == 0) {
                changeCompensationsHotWaterHeating = true;
                return heatingSetpoint;
            }    

            if ((heatingBufferTemperature >= heatingSetpoint) && changeCompensationsHotWaterHeating && 
                    (getDataFromMemoryCallable(ADDRESS_RETURN_WATER_TEMPERATURE_COMPENSATION_VALUE) != 2 
                    || getDataFromMemoryCallable(ADDRESS_OUTLET_WATER_TEMPERATURE_COMPENSATION_VALUE) != 2)) { 
                ChangeHeatpumpSetting(ADDRESS_RETURN_WATER_TEMPERATURE_COMPENSATION_VALUE, 2);
                ChangeHeatpumpSetting(ADDRESS_OUTLET_WATER_TEMPERATURE_COMPENSATION_VALUE, 2);
                changeCompensationsHotWaterHeating = false;
            }
            
            if (getHeatpumpCompressorFrequency() == 0){
                return heatingSetpoint;
            }
            
            if (heatingBufferTemperature >= heatingSetpoint) {
                return heatingSetpoint;
            }

            if((heatingSetpoint - getHeatpumpReturnWaterTemperature()) < 20  && changeCompensationsHotWaterHeating) {
                ChangeHeatpumpSetting(ADDRESS_RETURN_WATER_TEMPERATURE_COMPENSATION_VALUE, (getDataFromMemoryCallable(ADDRESS_RETURN_WATER_TEMPERATURE_COMPENSATION_VALUE) - 1));
                ChangeHeatpumpSetting(ADDRESS_OUTLET_WATER_TEMPERATURE_COMPENSATION_VALUE, (getDataFromMemoryCallable(ADDRESS_OUTLET_WATER_TEMPERATURE_COMPENSATION_VALUE) - 1));            
                changeCompensationsHotWaterHeating = false;
            }
        
            return heatingSetpoint;
            
        } else {
            heatingSetpoint = getHeatingSetpoint();
        }   
        
        if (!regulateOnTempSensorInBufferHotWaterHeating) {
            hot_water_heating_mode_data.stepperSetpoint = heatingSetpoint;
            return heatingSetpoint;
        }

        if ((heatingBufferTemperature == TEMPERATURE_ALARM_VALUE) || (heatingSetpoint == TEMPERATURE_ALARM_VALUE)) {
            // No temperature or setpoint known yet
            hot_water_heating_mode_data.stepperSetpoint = heatingSetpoint;
            return heatingSetpoint;
        }    

        if (heatingBufferTemperature >= heatingSetpoint) {
            // Hot water buffer tempereature is equal or higher than actual setpoint, so reset offset to 0
            hot_water_heating_mode_data.stepperSetpoint = heatingSetpoint;
            return heatingSetpoint;
        }    

        if (getHeatpumpCompressorFrequency() == 0) {
            hot_water_heating_mode_data.stepperSetpoint = heatingSetpoint;
            return heatingSetpoint;
        }

        if ((heatingSetpoint - getHeatpumpReturnWaterTemperature()) > 50) {
            hot_water_heating_mode_data.stepperSetpoint = getHeatpumpReturnWaterTemperature() + 20;
            return hot_water_heating_mode_data.stepperSetpoint;
        }

        if (getHeatpumpReturnWaterTemperature() >= (hot_water_heating_mode_data.stepperSetpoint - 20) && (hot_water_heating_mode_data.stepperSetpoint - getHeatpumpReturnWaterTemperature()) <= 20) {
            hot_water_heating_mode_data.stepperSetpoint = getHeatpumpReturnWaterTemperature() + 20;
            return hot_water_heating_mode_data.stepperSetpoint;
            //heatingSetpoint += 20;
        }

        return hot_water_heating_mode_data.stepperSetpoint;
        
    }
    
    if ((hot_water_heating_mode_data.state == HOT_WATER_HEATING_INITIALIZE_HOT_WATER) ||
        (hot_water_heating_mode_data.state == HOT_WATER_HEATING_STATE_WAIT_FOR_MINIMAL_TIME_IN_HOT_WATER) ||
        (hot_water_heating_mode_data.state == HOT_WATER_HEATING_STATE_RUNNING_IN_HOT_WATER) ||
        (hot_water_heating_mode_data.state == HOT_WATER_HEATING_STATE_RUNNING_WITH_ELEMENT_ON_IN_HOT_WATER) ) {
        return (getHotwaterSetpoint() + hot_water_heating_mode_data.setpointHotWaterOffset); 
    }
    
    return TEMPERATURE_ALARM_VALUE;
}


void HOT_WATER_HEATING_MODE_Initialize ( void )
{
    //TurnOffHeatingElementHeatingBuffer();
    //TurnOffHeatingElementHotWaterBuffer();
    
    setSecondCounterHeatingTask(UINT32_MAX);
    setSecondCounterHotwaterTask(UINT32_MAX);
    
    hot_water_heating_mode_data.HeatingElementOn = false;
    hot_water_heating_mode_data.HotwaterElementOn = false;
    
    hot_water_heating_mode_data.heatingCurveSet = false;
    
    hot_water_heating_mode_data.initialHeatingBufferTemp = TEMPERATURE_ALARM_VALUE;
    hot_water_heating_mode_data.hotwaterPassive = false;
    hot_water_heating_mode_data.setpointHotWaterOffset = TEMPERATURE_ALARM_VALUE;
    
    hot_water_heating_mode_data.state = HOT_WATER_HEATING_INITIALIZE_HEATING;
    return;
}



void HOT_WATER_HEATING_MODE_Tasks ( void )
{   
    int16_t heatingBufferTemperature = GetNtcTemperature(NTC_HEATING_BUFFER);
    
    int16_t hotwaterBufferTemperature = GetNtcTemperature(NTC_HOT_WATER_BUFFER);
    int16_t hotwaterSetpoint = getHotwaterSetpoint();
    int16_t hotwaterDelta = getHotwaterDelta();
    
    if(ReadSmartEeprom16(SEEP_ADDR_HEATING_CURVE) > 0 && ReadSmartEeprom16(SEEP_ADDR_HEATING_CURVE) != UINT16_MAX) {
        hot_water_heating_mode_data.heatingCurveSet = true; 
    } else {
        hot_water_heating_mode_data.heatingCurveSet = false; 
    }
    
    bool currentDip1SwitchState = getCurrentDip1SwitchState();
    if (currentDip1SwitchState) {
        regulateOnTempSensorInBufferHotWaterHeating = false;
    } else {
        regulateOnTempSensorInBufferHotWaterHeating = true;
    }
    
    if (currentDip1SwitchState != getPreviousDip1SwitchState()) {
        setPreviousDip1SwitchState(currentDip1SwitchState);
        if(currentDip1SwitchState == true) {
            setActiveModeControllerPumpOffDueToDipSwitch1(false);
            //ChangeHeatpumpSetting(ADDRESS_CONSTANT_TEMPERATURE_OPERATION_CYCLE, 10);
        }
    }
    
    if ((hotwaterBufferTemperature == TEMPERATURE_ALARM_VALUE) || (hotwaterSetpoint == TEMPERATURE_ALARM_VALUE) || (hotwaterDelta == TEMPERATURE_ALARM_VALUE)) {
        // Needed values not yet known
        return;
    }
    
    if (areWeOnHotWaterModeInHotWaterAndHeatingMode() == true){
        // Already in one of the hot water modes
        // If sterilization goes to passive mode, go to heating
        if (getSterilisationMode() == PASSIVE){
            // Sterilization mode on passive, so go back to heating            
            hot_water_heating_mode_data.hotwaterPassive = false;
            hot_water_heating_mode_data.state = HOT_WATER_HEATING_INITIALIZE_HEATING;
            return;
        }
    }
    
    if (hot_water_heating_mode_data.hotwaterPassive == true){
        // Hot water is on passive, so in one of the heating states
        if (hotwaterBufferTemperature >= hotwaterSetpoint){
            // Setpoint reached in passive mode, now turn off
            setSecondCounterHotwaterTask(UINT32_MAX);
            //TurnOffHeatingElementHotWaterBuffer();
            hot_water_heating_mode_data.HotwaterElementOn = false;
            
            hot_water_heating_mode_data.hotwaterPassive = false;
            hot_water_heating_mode_data.setpointHotWaterOffset = TEMPERATURE_ALARM_VALUE;
            return;
        }
        
        if ((getSecondCounterHotwaterTask() != UINT32_MAX) && (getSecondCounterHotwaterTask() >= ReadSmartEeprom16(SEEP_ADDR_HOT_WATER_MAX_TIME_HEATING_ELEMENT_ON_IN_HEATING_MODE_SEC))) {
            // Is more than 2 hours in passive hot water, so go back to active
            hot_water_heating_mode_data.hotwaterPassive = false;
            hot_water_heating_mode_data.state = HOT_WATER_HEATING_INITIALIZE_HOT_WATER;
            return;
        }
    }
    
    if ((areWeOnHotWaterModeInHotWaterAndHeatingMode() == false) && (hot_water_heating_mode_data.hotwaterPassive == false) && (hotwaterBufferTemperature < (hotwaterSetpoint - hotwaterDelta)) && (getSterilisationMode() != PASSIVE)) {
        // Not in hot water state yet
        // Not on one of the hot water states or doing passive hot water
        // Hot water buffer is lower than setpoint - delta
        hot_water_heating_mode_data.state = HOT_WATER_HEATING_INITIALIZE_HOT_WATER;
        return;
    }
    
    setTemperatureOperatingCycleHotWaterHeating();
    
    setActiveModeControllerHeatpumpSetpointHeating(determineCorrectSetpoint());
    
    setActiveModeControllerHeatpumpRunningMode(SET_MODE_HEATING);

    switch ( hot_water_heating_mode_data.state )
    {
        /*        
         __    __   _______     ___   .___________. __  .__   __.   _______ 
        |  |  |  | |   ____|   /   \  |           ||  | |  \ |  |  /  _____|
        |  |__|  | |  |__     /  ^  \ `---|  |----`|  | |   \|  | |  |  __  
        |   __   | |   __|   /  /_\  \    |  |     |  | |  . `  | |  | |_ | 
        |  |  |  | |  |____ /  _____  \   |  |     |  | |  |\   | |  |__| | 
        |__|  |__| |_______/__/     \__\  |__|     |__| |__| \__|  \______| 
        */    
        
        // 0
        case HOT_WATER_HEATING_INITIALIZE_HEATING:{
            
            //TurnOffHeatingElementHeatingBuffer();
            hot_water_heating_mode_data.HeatingElementOn = false;
            setSecondCounterHeatingTask(UINT32_MAX);
            
            if (regulateOnTempSensorInBufferHotWaterHeating) {
                // ChangeHeatpumpSetting(ADDRESS_CONSTANT_TEMPERATURE_OPERATION_CYCLE, 240);
                setActiveModeControllerPumpOffDueToDipSwitch1(true);
            }
            
            ChangeHeatpumpSetting(ADDRESS_RETURN_WATER_TEMPERATURE_COMPENSATION_VALUE, 2);
            ChangeHeatpumpSetting(ADDRESS_OUTLET_WATER_TEMPERATURE_COMPENSATION_VALUE, 2);            
            
            ChangeHeatpumpSetting(ADDRESS_HEATING_CURVE_SETTING, ReadSmartEeprom16(SEEP_ADDR_HEATING_CURVE));
            ChangeHeatpumpSetting(ADDRESS_COOLING_CURVE_SETTING, ReadSmartEeprom16(SEEP_ADDR_COOLING_CURVE));
            WriteSmartEeprom16(SEEP_ADDR_HEATING_SETPOINT_CURVE_BACKUP, UINT16_MAX);
            WriteSmartEeprom16(SEEP_ADDR_COOLING_SETPOINT_CURVE_BACKUP, UINT16_MAX);
            
            hot_water_heating_mode_data.initialHeatingBufferTemp = TEMPERATURE_ALARM_VALUE;
            hot_water_heating_mode_data.state = HOT_WATER_HEATING_IDLE_HEATING;
            break;
        }
        
        // 1
        case HOT_WATER_HEATING_IDLE_HEATING:{
            
            if (getHeatpumpCompressorFrequency() != 0){
                // Compressor is running
                setSecondCounterHeatingTask(0);
                hot_water_heating_mode_data.initialHeatingBufferTemp = heatingBufferTemperature;
                
                if (regulateOnTempSensorInBufferHotWaterHeating) {
                    //ChangeHeatpumpSetting(ADDRESS_CONSTANT_TEMPERATURE_OPERATION_CYCLE, 1);
                    setActiveModeControllerPumpOffDueToDipSwitch1(false);
                }
                
                hot_water_heating_mode_data.state = HOT_WATER_HEATING_RUNNING_ON_HEATING;
                break;
            }
            
            break;
        }
        
        // 2
        case HOT_WATER_HEATING_RUNNING_ON_HEATING:{
            
            if ((getHeatpumpCompressorFrequency() == 0) && (isDefrostingActive() == false)){
                // Compressor is not running and is also not in defrosting
                //TurnOffHeatingElementHeatingBuffer();
                hot_water_heating_mode_data.HeatingElementOn = false;
                hot_water_heating_mode_data.initialHeatingBufferTemp = TEMPERATURE_ALARM_VALUE;
                setSecondCounterHeatingTask(UINT32_MAX);
                
                if (regulateOnTempSensorInBufferHotWaterHeating) {
                    // ChangeHeatpumpSetting(ADDRESS_CONSTANT_TEMPERATURE_OPERATION_CYCLE, 240);
                    setActiveModeControllerPumpOffDueToDipSwitch1(true);
                }

                hot_water_heating_mode_data.state = HOT_WATER_HEATING_IDLE_HEATING;
                break;
            }
            
            if (heatingBufferTemperature >= hot_water_heating_mode_data.initialHeatingBufferTemp + ReadSmartEeprom16(SEEP_ADDR_HEATING_RISE_TEMP_IN_GIVEN_TIME)){
                // Temperature rised a set temperature in a set time
                setSecondCounterHeatingTask(0);
                hot_water_heating_mode_data.initialHeatingBufferTemp = heatingBufferTemperature;
                break;
            }
            
            if ((getSecondCounterHeatingTask() >= ReadSmartEeprom16(SEEP_ADDR_HEATING_TIME_CONSTANT_SEC)) && (getSecondCounterHeatingTask() != UINT32_MAX)){
                // Temperature not rised a set temperature in a set time
                setSecondCounterHeatingTask(0);
                hot_water_heating_mode_data.initialHeatingBufferTemp = heatingBufferTemperature;
                //TurnOnHeatingElementHeatingBuffer();
                hot_water_heating_mode_data.HeatingElementOn = true;
                
                hot_water_heating_mode_data.state = HOT_WATER_HEATING_RUNNING_ON_HEATING_WITH_ELEMENT_ON;
                break;
            }
            
            break;
        }
        
        // 3
        case HOT_WATER_HEATING_RUNNING_ON_HEATING_WITH_ELEMENT_ON:{
            
            if ((getHeatpumpCompressorFrequency() == 0) && (isDefrostingActive() == false)){
                // Compressor is not running and is also not in defrosting
                //TurnOffHeatingElementHeatingBuffer();
                hot_water_heating_mode_data.HeatingElementOn = false;
                hot_water_heating_mode_data.initialHeatingBufferTemp = TEMPERATURE_ALARM_VALUE;
                setSecondCounterHeatingTask(UINT32_MAX);
                
                if (regulateOnTempSensorInBufferHotWaterHeating) {
                    // ChangeHeatpumpSetting(ADDRESS_CONSTANT_TEMPERATURE_OPERATION_CYCLE, 240);
                    setActiveModeControllerPumpOffDueToDipSwitch1(true);
                }

                hot_water_heating_mode_data.state = HOT_WATER_HEATING_IDLE_HEATING;
                break;
            }
            
            if (heatingBufferTemperature >= hot_water_heating_mode_data.initialHeatingBufferTemp + ReadSmartEeprom16(SEEP_ADDR_HEATING_RISE_TEMP_IN_GIVEN_TIME)){
                // Temperature rised a set temperature in a set time
                setSecondCounterHeatingTask(0);
                hot_water_heating_mode_data.initialHeatingBufferTemp = heatingBufferTemperature;
                break;
            }
            
            if ((getSecondCounterHeatingTask() >= ReadSmartEeprom16(SEEP_ADDR_HEATING_TIME_CONSTANT_SEC)) && (getSecondCounterHeatingTask() != UINT32_MAX)){
                // Temperature not rised a set temperature in a set time
                setSecondCounterHeatingTask(0);
                hot_water_heating_mode_data.initialHeatingBufferTemp = heatingBufferTemperature;
                //TurnOffHeatingElementHeatingBuffer();
                hot_water_heating_mode_data.HeatingElementOn = false;
                
                hot_water_heating_mode_data.state = HOT_WATER_HEATING_RUNNING_ON_HEATING;
                break;
            }
            
            break;
        }
        /*        
         __    __    ______   .___________.   ____    __    ____  ___   .___________. _______ .______      
        |  |  |  |  /  __  \  |           |   \   \  /  \  /   / /   \  |           ||   ____||   _  \     
        |  |__|  | |  |  |  | `---|  |----`    \   \/    \/   / /  ^  \ `---|  |----`|  |__   |  |_)  |    
        |   __   | |  |  |  |     |  |          \            / /  /_\  \    |  |     |   __|  |      /     
        |  |  |  | |  `--'  |     |  |           \    /\    / /  _____  \   |  |     |  |____ |  |\  \----.
        |__|  |__|  \______/      |__|            \__/  \__/ /__/     \__\  |__|     |_______|| _| `._____|
        */                                                                                                  
        
        // 4
        case HOT_WATER_HEATING_INITIALIZE_HOT_WATER:{
            
            setSecondCounterHeatingTask(UINT32_MAX);
            setSecondCounterHotwaterTask(0);
            
            //TurnOffHeatingElementHeatingBuffer();
            //TurnOffHeatingElementHotWaterBuffer();

            //if (regulateOnTempSensorInBufferHotWaterHeating) {
                // ChangeHeatpumpSetting(ADDRESS_CONSTANT_TEMPERATURE_OPERATION_CYCLE, 1);
            setActiveModeControllerPumpOffDueToDipSwitch1(false);
            //}
            if(hot_water_heating_mode_data.heatingCurveSet) {
                WriteSmartEeprom16(SEEP_ADDR_HEATING_SETPOINT_CURVE_BACKUP, getHeatpumpHeatingSetpoint()*10);
            } else {
                WriteSmartEeprom16(SEEP_ADDR_HEATING_SETPOINT_CURVE_BACKUP, UINT16_MAX);
            }
  
            ChangeHeatpumpSetting(ADDRESS_RETURN_WATER_TEMPERATURE_COMPENSATION_VALUE, 2);
            ChangeHeatpumpSetting(ADDRESS_OUTLET_WATER_TEMPERATURE_COMPENSATION_VALUE, 2);            
            
            //WriteSmartEeprom8(SEEP_ADDR_HEATING_CURVE, getDataFromMemoryCallable(ADDRESS_HEATING_CURVE_SETTING));
            //WriteSmartEeprom8(SEEP_ADDR_COOLING_CURVE, getDataFromMemoryCallable(ADDRESS_COOLING_CURVE_SETTING));
            ChangeHeatpumpSetting(ADDRESS_HEATING_CURVE_SETTING, 0);
            ChangeHeatpumpSetting(ADDRESS_COOLING_CURVE_SETTING, 0);
            
            hot_water_heating_mode_data.HeatingElementOn = false;
            hot_water_heating_mode_data.HotwaterElementOn = false;
            
            hot_water_heating_mode_data.initialHeatingBufferTemp = TEMPERATURE_ALARM_VALUE;
            
            hot_water_heating_mode_data.hotwaterPassive = false;
            hot_water_heating_mode_data.setpointHotWaterOffset = ReadSmartEeprom16(SEEP_ADDR_HOT_WATER_SETPOINT_OFFSET_START);
            hot_water_heating_mode_data.state = HOT_WATER_HEATING_STATE_WAIT_FOR_MINIMAL_TIME_IN_HOT_WATER;
            break;
        }
        
        // 5
        case HOT_WATER_HEATING_STATE_WAIT_FOR_MINIMAL_TIME_IN_HOT_WATER:{
            
            adjustSetpointOffsetHotWater();
            
            if ((getSecondCounterHotwaterTask() >= ReadSmartEeprom16(SEEP_ADDR_HOT_WATER_MIN_TIME_IN_HOT_WATER_MODE)) && (getSecondCounterHotwaterTask() != UINT32_MAX)) {
                // Minimal time in hot water passed
                hot_water_heating_mode_data.state = HOT_WATER_HEATING_STATE_RUNNING_IN_HOT_WATER;
                break;
            }
            
            break;
        }
        
        // 6
        case HOT_WATER_HEATING_STATE_RUNNING_IN_HOT_WATER:{
            
            adjustSetpointOffsetHotWater();
            
            if ((getHeatpumpCompressorFrequency() == 0) && (isDefrostingActive() == false)){
                // Compressor is not running and is also not in defrosting, so go back to heating
                //TurnOffHeatingElementHotWaterBuffer();
                hot_water_heating_mode_data.HotwaterElementOn = false;
                setSecondCounterHotwaterTask(UINT32_MAX);
                
                hot_water_heating_mode_data.hotwaterPassive = false;
                hot_water_heating_mode_data.setpointHotWaterOffset = TEMPERATURE_ALARM_VALUE;
                hot_water_heating_mode_data.state = HOT_WATER_HEATING_INITIALIZE_HEATING;
                break;
            }
            
            if ((getSecondCounterHotwaterTask() >= ReadSmartEeprom16(SEEP_ADDR_HOT_WATER_RUNNING_TIME_BEFORE_TURNING_ON_HEATING_ELEMENT)) && (getSecondCounterHotwaterTask() != UINT32_MAX)){
                // 2 hours passed in hot water and not reached setpoint, turn on element, time counter not needed anymore
                //TurnOnHeatingElementHotWaterBuffer();
                hot_water_heating_mode_data.HotwaterElementOn = true;
                setSecondCounterHotwaterTask(UINT32_MAX);
                hot_water_heating_mode_data.state = HOT_WATER_HEATING_STATE_RUNNING_WITH_ELEMENT_ON_IN_HOT_WATER;
                break;
            }
            
            break;
        }
        
        // 7
        case HOT_WATER_HEATING_STATE_RUNNING_WITH_ELEMENT_ON_IN_HOT_WATER:{
            
            adjustSetpointOffsetHotWater();
            
            if ((getHeatpumpCompressorFrequency() == 0) && (isDefrostingActive() == false)){
                // Compressor is not running and is also not in defrosting, so go back to heating
                //TurnOffHeatingElementHotWaterBuffer();
                hot_water_heating_mode_data.HotwaterElementOn = false;
                setSecondCounterHotwaterTask(UINT32_MAX);
                
                hot_water_heating_mode_data.hotwaterPassive = false;
                hot_water_heating_mode_data.setpointHotWaterOffset = TEMPERATURE_ALARM_VALUE;
                hot_water_heating_mode_data.state = HOT_WATER_HEATING_INITIALIZE_HEATING;
                break;
            }
            
            if (GetThermostatContact() == true)
            {   // Thermostat contact has been made, turn on passive mode, reset timer and go to heating
                setSecondCounterHotwaterTask(0);
                
                hot_water_heating_mode_data.hotwaterPassive = true;
                hot_water_heating_mode_data.setpointHotWaterOffset = TEMPERATURE_ALARM_VALUE;
                hot_water_heating_mode_data.state = HOT_WATER_HEATING_INITIALIZE_HEATING;
                break;
            }
            
            break;
        }
        
        default:{
            break;
        }
    }
    
}


