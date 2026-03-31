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

#include "heatpump_pi_adapter.h"
#include "pi_frequency_controller.h"

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
        
        case(HOT_WATER_HEATING_RUNNING_ON_HEATING_WITH_CIRCULATION_PUMP_OFF): {
            return "4, Running heating with circulation pump off";
            break;
        }
        
        case(HOT_WATER_HEATING_RUNNING_ON_HEATING_WITH_ELEMENT_ON_AND_CIRCULATION_PUMP_OFF): {
            return "5, Running heating with element on and circulation pump off";
            break;
        }
        
        case(HOT_WATER_HEATING_INITIALIZE_HOT_WATER): {
            return "6, Init hotwater";
            break;
        }
        
        case(HOT_WATER_HEATING_STATE_WAIT_FOR_MINIMAL_TIME_IN_HOT_WATER): {
            return "7, Minimal time in hot water";
            break;
        }
        
        case(HOT_WATER_HEATING_STATE_RUNNING_IN_HOT_WATER): {
            return "8, Running hotwater";
            break;
        }
        
        case(HOT_WATER_HEATING_STATE_RUNNING_WITH_ELEMENT_ON_IN_HOT_WATER): {
            return "9, Running hotwater with element on";
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
    
    if ((getHeatpumpReturnWaterTemperature(MASTER_HEATPUMP_IN_CASCADE) >= (getHotwaterSetpoint() + hot_water_heating_mode_data.setpointHotWaterOffset - 20)) && (hot_water_heating_mode_data.setpointHotWaterOffset != 0) && (hot_water_heating_mode_data.setpointHotWaterOffset != TEMPERATURE_ALARM_VALUE)){   
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

    if (checkIfDefrostingActive()) {
        if(getActiveModeControllerPumpOffDueToDipSwitch1()) {
            setActiveModeControllerPumpOffDueToDipSwitch1(false);
        }
        changeSettingHotWaterHeating = false;
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
        heatingSetpoint = getHeatpumpHeatingSetpoint() * 10;
    } else {
        heatingSetpoint = getHeatingSetpoint();
    }   
    
    
    if (heatingBufferTemperature <= (heatingSetpoint - (getDataFromMemoryCallable(ADDRESS_AIR_CONDITIONER_RETURN_DIFFERENCE, MASTER_HEATPUMP_IN_CASCADE) * 10)) 
            // && getDataFromMemoryCallable(ADDRESS_CONSTANT_TEMPERATURE_OPERATION_CYCLE) != 1
            && getActiveModeControllerPumpOffDueToDipSwitch1()
            && changeSettingHotWaterHeating) {
        // ChangeHeatpumpSetting(ADDRESS_CONSTANT_TEMPERATURE_OPERATION_CYCLE, 1);
        setActiveModeControllerPumpOffDueToDipSwitch1(false);
        changeSettingHotWaterHeating = false;
        return;
    }  

    if (heatingBufferTemperature >= (heatingSetpoint + 10)
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
                    (getDataFromMemoryCallable(ADDRESS_RETURN_WATER_TEMPERATURE_COMPENSATION_VALUE, MASTER_HEATPUMP_IN_CASCADE) != 2 
                    || getDataFromMemoryCallable(ADDRESS_OUTLET_WATER_TEMPERATURE_COMPENSATION_VALUE, MASTER_HEATPUMP_IN_CASCADE) != 2)) { 
                ChangeHeatpumpSetting(ADDRESS_RETURN_WATER_TEMPERATURE_COMPENSATION_VALUE, 2);
                ChangeHeatpumpSetting(ADDRESS_OUTLET_WATER_TEMPERATURE_COMPENSATION_VALUE, 2);
                changeCompensationsHotWaterHeating = false;
            }

            if (getActiveCompressorsMask() == 0){
                return heatingSetpoint;
            }

            if (heatingBufferTemperature >= (heatingSetpoint + 10)) {
                return heatingSetpoint;
            }
            
            if((heatingSetpoint - getHeatpumpReturnWaterTemperature(MASTER_HEATPUMP_IN_CASCADE)) < 30  && changeCompensationsHotWaterHeating) {
                ChangeHeatpumpSetting(ADDRESS_RETURN_WATER_TEMPERATURE_COMPENSATION_VALUE, (getDataFromMemoryCallable(ADDRESS_RETURN_WATER_TEMPERATURE_COMPENSATION_VALUE, MASTER_HEATPUMP_IN_CASCADE) - 1));
                ChangeHeatpumpSetting(ADDRESS_OUTLET_WATER_TEMPERATURE_COMPENSATION_VALUE, (getDataFromMemoryCallable(ADDRESS_OUTLET_WATER_TEMPERATURE_COMPENSATION_VALUE, MASTER_HEATPUMP_IN_CASCADE) - 1));            
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

        if (heatingBufferTemperature >= (heatingSetpoint + 10)) {
            // Hot water buffer tempereature is equal or higher than actual setpoint, so reset offset to 0
            hot_water_heating_mode_data.stepperSetpoint = heatingSetpoint;
            return heatingSetpoint;
        }    

        if (getActiveCompressorsMask() == 0) {
            hot_water_heating_mode_data.stepperSetpoint = heatingSetpoint;
            return heatingSetpoint;
        }
        
        if ((heatingSetpoint - getHeatpumpReturnWaterTemperature(MASTER_HEATPUMP_IN_CASCADE)) <= 20 ) {
            if (getHeatpumpReturnWaterTemperature(MASTER_HEATPUMP_IN_CASCADE) >= (hot_water_heating_mode_data.stepperSetpoint - 20)) {
                hot_water_heating_mode_data.stepperSetpoint += 20;
            }
        }

        /*
        if ((heatingSetpoint - getHeatpumpReturnWaterTemperature(MASTER_HEATPUMP_IN_CASCADE)) > 50) {
            hot_water_heating_mode_data.stepperSetpoint = getHeatpumpReturnWaterTemperature(MASTER_HEATPUMP_IN_CASCADE) + 20;
            return hot_water_heating_mode_data.stepperSetpoint;
        }


        if (getHeatpumpReturnWaterTemperature(MASTER_HEATPUMP_IN_CASCADE) >= (hot_water_heating_mode_data.stepperSetpoint - 20) && (hot_water_heating_mode_data.stepperSetpoint - getHeatpumpReturnWaterTemperature(MASTER_HEATPUMP_IN_CASCADE)) <= 20) {
            hot_water_heating_mode_data.stepperSetpoint = getHeatpumpReturnWaterTemperature(MASTER_HEATPUMP_IN_CASCADE) + 20;
            return hot_water_heating_mode_data.stepperSetpoint;
            //heatingSetpoint += 20;
        }
        */
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

void BlockCirculationPumpIfBufferTempIsTooLow(int16_t bufferTemperature) 
{
    int16_t heatingSetpoint = getHeatpumpHeatingSetpoint() * 10;
    int16_t heatingDelta = getAirConditionerReturnDifference();
    
    // Blokkeer als Buffer temp < setpoint - delta. Pas aanzetten nadat warmtepomp 2 minuten draait.
    // Jelle vragen wat nou het juiste heating setpoint is
    if (bufferTemperature == TEMPERATURE_ALARM_VALUE || heatingDelta == TEMPERATURE_ALARM_VALUE || heatingSetpoint == UINT16_MAX || heatingSetpoint == TEMPERATURE_ALARM_VALUE) {
        // No valid temperatures, no need for blocking
        hot_water_heating_mode_data.blockCirculationPumpLongerBecauseTempTooLow = false;
        setSecondCounterBlockCirculationPumpAtHeatingStart(UINT32_MAX);
        return;
    }
    
    if (bufferTemperature <= (heatingSetpoint - heatingDelta)) {
        // Buffer temp is equal or lower than setpoint - delta, block the pump from running
        hot_water_heating_mode_data.blockCirculationPumpLongerBecauseTempTooLow = true;
        setSecondCounterBlockCirculationPumpAtHeatingStart(0);
        return;
    }
    else {
        // Buffer temp is within setpoint - delta, circulation pump can run
        hot_water_heating_mode_data.blockCirculationPumpLongerBecauseTempTooLow = false;
        setSecondCounterBlockCirculationPumpAtHeatingStart(UINT32_MAX);
        return;
    }
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
    
    hot_water_heating_mode_data.blockCirculationPumpAtHeatingStart = false;
    hot_water_heating_mode_data.blockCirculationPumpLongerBecauseTempTooLow = false;
    setSecondCounterBlockCirculationPumpAtHeatingStart(UINT32_MAX);
    
    HPPI_Clear();
    
    hot_water_heating_mode_data.state = HOT_WATER_HEATING_INITIALIZE_HEATING;
    return;
}



void HOT_WATER_HEATING_MODE_Tasks ( void )
{   
    int16_t heatingBufferTemperature = GetNtcTemperature(NTC_HEATING_BUFFER);
    
    int16_t hotwaterBufferTemperature = GetNtcTemperature(NTC_HOT_WATER_BUFFER);
    int16_t hotwaterSetpoint = getHotwaterSetpoint();
    int16_t hotwaterDelta = getHotwaterDelta();
    
    if (ReadSmartEeprom16(SEEP_ADDR_HEATING_CURVE) > 0 && ReadSmartEeprom16(SEEP_ADDR_HEATING_CURVE) != UINT16_MAX) {
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
            if( blockHotWaterBasedOnTimers() == false ) { 
                hot_water_heating_mode_data.state = HOT_WATER_HEATING_INITIALIZE_HOT_WATER;
                return;
            }
        }
    }
    
    if ((areWeOnHotWaterModeInHotWaterAndHeatingMode() == false) && (hot_water_heating_mode_data.hotwaterPassive == false) && (hotwaterBufferTemperature < (hotwaterSetpoint - hotwaterDelta)) && (getSterilisationMode() != PASSIVE)) {
        // Not in hot water state yet
        // Not on one of the hot water states or doing passive hot water
        // Hot water buffer is lower than setpoint - delta
        if( blockHotWaterBasedOnTimers() == false ) { 
            hot_water_heating_mode_data.state = HOT_WATER_HEATING_INITIALIZE_HOT_WATER;
            return;
        }
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
            HPPI_Clear();
                        
            if (regulateOnTempSensorInBufferHotWaterHeating && !checkIfDefrostingActive()) {
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
            hot_water_heating_mode_data.stepperSetpoint = TEMPERATURE_ALARM_VALUE;
            
            hot_water_heating_mode_data.blockCirculationPumpAtHeatingStart = true;
            hot_water_heating_mode_data.blockCirculationPumpLongerBecauseTempTooLow = false;
            setSecondCounterBlockCirculationPumpAtHeatingStart(0);
            
            hot_water_heating_mode_data.state = HOT_WATER_HEATING_IDLE_HEATING;
            break;
        }
        
        // 1
        case HOT_WATER_HEATING_IDLE_HEATING:{
            
            if (getActiveCompressorsMask() != 0){
                // Compressor is running
                setSecondCounterHeatingTask(0);
                hot_water_heating_mode_data.initialHeatingBufferTemp = heatingBufferTemperature;
                HPPI_Reset();
                
                if (regulateOnTempSensorInBufferHotWaterHeating) {
                    //ChangeHeatpumpSetting(ADDRESS_CONSTANT_TEMPERATURE_OPERATION_CYCLE, 1);
                    setActiveModeControllerPumpOffDueToDipSwitch1(false);
                }
                
                hot_water_heating_mode_data.stepperSetpoint = getHeatingSetpoint();
                
                if (hot_water_heating_mode_data.blockCirculationPumpAtHeatingStart == true) {
                    // Within 30 seconds compressor goes running, check if circulation pump still needs to be blocked
                    hot_water_heating_mode_data.blockCirculationPumpAtHeatingStart = false;
                    BlockCirculationPumpIfBufferTempIsTooLow(heatingBufferTemperature);
                }
                
                setSecondCounterBlockCirculationPumpAtHeatingStart(0); // Reset timer
                hot_water_heating_mode_data.state = HOT_WATER_HEATING_RUNNING_ON_HEATING;
                break;
            }
            
            if (hot_water_heating_mode_data.blockCirculationPumpAtHeatingStart == true) {
                // Just coming back from hot water mode/ sterilization and circulation pump is blocked
                if (getSecondCounterBlockCirculationPumpAtHeatingStart() >= ReadSmartEeprom16(SEEP_ADDR_CIRCULATION_PUMP_INIT_OFF_TIME_WHEN_SWITCHING_TO_HEATING)) {
                    // 30 seconds past, check if circulation pump still needs to be blocked
                    hot_water_heating_mode_data.blockCirculationPumpAtHeatingStart = false;
                    BlockCirculationPumpIfBufferTempIsTooLow(heatingBufferTemperature);
                }
            }
            
            if (hot_water_heating_mode_data.blockCirculationPumpLongerBecauseTempTooLow == true) {
                // Block the circulation pump temporary
                if (getSecondCounterBlockCirculationPumpAtHeatingStart() >= ReadSmartEeprom16(SEEP_ADDR_CIRCULATION_PUMP_MAX_OFF_TIME_WHEN_SWITCHING_TO_HEATING)) {
                    // Time passed X seconds, don't block the circulation pump anymore
                    hot_water_heating_mode_data.blockCirculationPumpAtHeatingStart = false;
                    hot_water_heating_mode_data.blockCirculationPumpLongerBecauseTempTooLow = false;
                    setSecondCounterBlockCirculationPumpAtHeatingStart(UINT32_MAX);
                }
            }
            
            break;
        }
        
        // 2
        case HOT_WATER_HEATING_RUNNING_ON_HEATING:{
            
            HPPI_UpdateAndGetTargetHz();   
            
            if ((getActiveCompressorsMask() == 0) && (getDefrostingActiveMask() == 0)){
                // Compressor is not running and is also not in defrosting
                //TurnOffHeatingElementHeatingBuffer();
                hot_water_heating_mode_data.HeatingElementOn = false;
                hot_water_heating_mode_data.initialHeatingBufferTemp = TEMPERATURE_ALARM_VALUE;
                setSecondCounterHeatingTask(UINT32_MAX);
                HPPI_Clear();
                
                if (regulateOnTempSensorInBufferHotWaterHeating && !checkIfDefrostingActive()) {
                    // ChangeHeatpumpSetting(ADDRESS_CONSTANT_TEMPERATURE_OPERATION_CYCLE, 240);
                    setActiveModeControllerPumpOffDueToDipSwitch1(true);
                }
                
                hot_water_heating_mode_data.blockCirculationPumpAtHeatingStart = false;
                hot_water_heating_mode_data.blockCirculationPumpLongerBecauseTempTooLow = false;
                setSecondCounterBlockCirculationPumpAtHeatingStart(UINT32_MAX);
                
                hot_water_heating_mode_data.state = HOT_WATER_HEATING_IDLE_HEATING;
                break;
            }
            
            if (hot_water_heating_mode_data.blockCirculationPumpLongerBecauseTempTooLow == true) {
                // Block the circulation pump temporary
                if (getSecondCounterHeatingTask() >= ReadSmartEeprom16(SEEP_ADDR_CIRCULATION_PUMP_OFF_TIME_WHEN_HEATPUMP_RUNNING)) {
                    //
                    hot_water_heating_mode_data.blockCirculationPumpAtHeatingStart = false;
                    hot_water_heating_mode_data.blockCirculationPumpLongerBecauseTempTooLow = false;
                    setSecondCounterBlockCirculationPumpAtHeatingStart(UINT32_MAX);
                }
            }
            
            if (heatingBufferTemperature < hot_water_heating_mode_data.initialHeatingBufferTemp) {
                // Adjust the reference temp to the lowest measured temperature 
                hot_water_heating_mode_data.initialHeatingBufferTemp = heatingBufferTemperature;
            }
            
            if (heatingBufferTemperature >= hot_water_heating_mode_data.initialHeatingBufferTemp + ReadSmartEeprom16(SEEP_ADDR_HEATING_RISE_TEMP_IN_GIVEN_TIME)){
                // Temperature rised a set temperature in a set time
                setSecondCounterHeatingTask(0);
                hot_water_heating_mode_data.initialHeatingBufferTemp = heatingBufferTemperature;
                break;
            }
            
            if (getSecondCounterHeatingTask() >= ReadSmartEeprom16(SEEP_ADDR_HEATING_TIME_CONSTANT_SEC)){
                // Temperature not rised a set temperature in a set time
                setSecondCounterHeatingTask(0);
                //heating_mode_data.initialBufferTemp = TEMPERATURE_ALARM_VALUE;
                hot_water_heating_mode_data.HeatingElementOn = true;
                
                if (checkIfBufferIsWithinSetpointMinusDelta(heatingBufferTemperature) == true) {
                    // Buffer temp is within setpoint - delta, must rise 1 degree within the given time next phase
                    hot_water_heating_mode_data.initialHeatingBufferTemp = heatingBufferTemperature;
                }
                else {
                    // Buffer temp is not within setpoint - delta must rise untill the setpoint - delta next phase
                    hot_water_heating_mode_data.initialHeatingBufferTemp = TEMPERATURE_ALARM_VALUE;
                }
                
                hot_water_heating_mode_data.blockCirculationPumpAtHeatingStart = false;
                hot_water_heating_mode_data.blockCirculationPumpLongerBecauseTempTooLow = false;
                setSecondCounterBlockCirculationPumpAtHeatingStart(UINT32_MAX);
                
                hot_water_heating_mode_data.state = HOT_WATER_HEATING_RUNNING_ON_HEATING_WITH_ELEMENT_ON;
                break;
            }
            
            break;
        }
        
        // 3
        case HOT_WATER_HEATING_RUNNING_ON_HEATING_WITH_ELEMENT_ON:{
            HPPI_UpdateAndGetTargetHz();   
            if ((getActiveCompressorsMask() == 0) && (getDefrostingActiveMask() == 0)){
                // Compressor is not running and is also not in defrosting
                //TurnOffHeatingElementHeatingBuffer();
                hot_water_heating_mode_data.HeatingElementOn = false;
                hot_water_heating_mode_data.initialHeatingBufferTemp = TEMPERATURE_ALARM_VALUE;
                setSecondCounterHeatingTask(UINT32_MAX);
                HPPI_Clear();
                
                if (regulateOnTempSensorInBufferHotWaterHeating && !checkIfDefrostingActive()) {
                    // ChangeHeatpumpSetting(ADDRESS_CONSTANT_TEMPERATURE_OPERATION_CYCLE, 240);
                    setActiveModeControllerPumpOffDueToDipSwitch1(true);
                }

                hot_water_heating_mode_data.state = HOT_WATER_HEATING_IDLE_HEATING;
                break;
            }
            
            if (getSecondCounterHeatingTask() >= ReadSmartEeprom16(SEEP_ADDR_HEATING_MODE_MAX_TIME_WITH_ELEMENT_ON)){
                // Temperature not rised a set temperature in a set time
                setSecondCounterHeatingTask(0);
                hot_water_heating_mode_data.HeatingElementOn = false;
                
                if (checkIfBufferIsWithinSetpointMinusDelta(heatingBufferTemperature) == true) {
                    // Buffer temp is within setpoint - delta, must rise 1 degree within the given time next phase
                    hot_water_heating_mode_data.initialHeatingBufferTemp = heatingBufferTemperature;
                }
                else {
                    // Buffer temp is not within setpoint - delta must rise untill the setpoint - delta next phase
                    hot_water_heating_mode_data.initialHeatingBufferTemp = TEMPERATURE_ALARM_VALUE;
                }
                
                hot_water_heating_mode_data.state = HOT_WATER_HEATING_RUNNING_ON_HEATING_WITH_CIRCULATION_PUMP_OFF;
                break;
            }
            
            if (hot_water_heating_mode_data.initialHeatingBufferTemp != TEMPERATURE_ALARM_VALUE) {
                // Has a valid value, so must rise 1 degree now
                if (heatingBufferTemperature < hot_water_heating_mode_data.initialHeatingBufferTemp) {
                    // Adjust the reference temp to the lowest measured temperature 
                    hot_water_heating_mode_data.initialHeatingBufferTemp = heatingBufferTemperature;
                }
                
                if (heatingBufferTemperature >= hot_water_heating_mode_data.initialHeatingBufferTemp + ReadSmartEeprom16(SEEP_ADDR_HEATING_RISE_TEMP_IN_GIVEN_TIME)){
                    // Temperature rised a set temperature in a set time
                    setSecondCounterHeatingTask(0);
                    hot_water_heating_mode_data.HeatingElementOn = false;
                    hot_water_heating_mode_data.initialHeatingBufferTemp = heatingBufferTemperature;
                    //heating_mode_data.stepperSetpoint = getHeatingSetpoint();

                    hot_water_heating_mode_data.state = HOT_WATER_HEATING_RUNNING_ON_HEATING;
                    break;
                }
            }
            else {
                // Has not a valid value, must rise untill the setpoint-delta
                if (checkIfBufferIsWithinSetpointMinusDelta(heatingBufferTemperature) == true) {
                    // Buffer temp is within setpoint - delta, go back to running
                    setSecondCounterHeatingTask(0);
                    hot_water_heating_mode_data.HeatingElementOn = false;
                    hot_water_heating_mode_data.initialHeatingBufferTemp = heatingBufferTemperature;
                    //heating_mode_data.stepperSetpoint = getHeatingSetpoint();

                    hot_water_heating_mode_data.state = HOT_WATER_HEATING_RUNNING_ON_HEATING;
                    break;
                }
            }
            
            break;
        }
        
        // 4
        case HOT_WATER_HEATING_RUNNING_ON_HEATING_WITH_CIRCULATION_PUMP_OFF:{
            HPPI_UpdateAndGetTargetHz();   
            if ((getActiveCompressorsMask() == 0) && (getDefrostingActiveMask() == 0)){
                // Compressor is not running and is also not in defrosting
                hot_water_heating_mode_data.HeatingElementOn = false;
                hot_water_heating_mode_data.initialHeatingBufferTemp = TEMPERATURE_ALARM_VALUE;
                setSecondCounterHeatingTask(UINT32_MAX);

                if(regulateOnTempSensorInBufferHotWaterHeating && !checkIfDefrostingActive()) {
                    //ChangeHeatpumpSetting(ADDRESS_CONSTANT_TEMPERATURE_OPERATION_CYCLE, 240);
                    setActiveModeControllerPumpOffDueToDipSwitch1(true);
                }
                
                hot_water_heating_mode_data.state = HOT_WATER_HEATING_IDLE_HEATING;
                break;
            }
            
            if (getSecondCounterHeatingTask() >= ReadSmartEeprom16(SEEP_ADDR_HEATING_MODE_MAX_TIME_WITH_CIRCULATION_PUMP_OFF)){
                // Temperature not rised a set temperature in a set time
                setSecondCounterHeatingTask(0);
                hot_water_heating_mode_data.HeatingElementOn = true;
                
                if (checkIfBufferIsWithinSetpointMinusDelta(heatingBufferTemperature) == true) {
                    // Buffer temp is within setpoint - delta, must rise 1 degree within the given time next phase
                    hot_water_heating_mode_data.initialHeatingBufferTemp = heatingBufferTemperature;
                }
                else {
                    // Buffer temp is not within setpoint - delta must rise untill the setpoint - delta next phase
                    hot_water_heating_mode_data.initialHeatingBufferTemp = TEMPERATURE_ALARM_VALUE;
                }
                
                hot_water_heating_mode_data.state = HOT_WATER_HEATING_RUNNING_ON_HEATING_WITH_ELEMENT_ON_AND_CIRCULATION_PUMP_OFF;
                break;
            }
            
            if (hot_water_heating_mode_data.initialHeatingBufferTemp != TEMPERATURE_ALARM_VALUE) {
                // Has a valid value, so must rise 1 degree now
                if (heatingBufferTemperature < hot_water_heating_mode_data.initialHeatingBufferTemp) {
                    // Adjust the reference temp to the lowest measured temperature 
                    hot_water_heating_mode_data.initialHeatingBufferTemp = heatingBufferTemperature;
                }
                
                if (heatingBufferTemperature >= hot_water_heating_mode_data.initialHeatingBufferTemp + ReadSmartEeprom16(SEEP_ADDR_HEATING_RISE_TEMP_IN_GIVEN_TIME)){
                    // Temperature rised a set temperature in a set time
                    setSecondCounterHeatingTask(0);
                    hot_water_heating_mode_data.HeatingElementOn = false;
                    hot_water_heating_mode_data.initialHeatingBufferTemp = heatingBufferTemperature;
                    //heating_mode_data.stepperSetpoint = getHeatingSetpoint();

                    hot_water_heating_mode_data.state = HOT_WATER_HEATING_RUNNING_ON_HEATING;
                    break;
                }
            }
            else {
                // Has not a valid value, must rise untill the setpoint-delta
                if (checkIfBufferIsWithinSetpointMinusDelta(heatingBufferTemperature) == true) {
                    // Buffer temp is within setpoint - delta, go back to running
                    setSecondCounterHeatingTask(0);
                    hot_water_heating_mode_data.HeatingElementOn = false;
                    hot_water_heating_mode_data.initialHeatingBufferTemp = heatingBufferTemperature;
                    //heating_mode_data.stepperSetpoint = getHeatingSetpoint();

                    hot_water_heating_mode_data.state = HOT_WATER_HEATING_RUNNING_ON_HEATING;
                    break;
                }
            }
            
            break;
        }
        
        // 5
        case HOT_WATER_HEATING_RUNNING_ON_HEATING_WITH_ELEMENT_ON_AND_CIRCULATION_PUMP_OFF:{
            HPPI_UpdateAndGetTargetHz();   
            if ((getActiveCompressorsMask() == 0) && (getDefrostingActiveMask() == 0)){
                // Compressor is not running and is also not in defrosting
                hot_water_heating_mode_data.HeatingElementOn = false;
                hot_water_heating_mode_data.initialHeatingBufferTemp = TEMPERATURE_ALARM_VALUE;
                setSecondCounterHeatingTask(UINT32_MAX);

                if(regulateOnTempSensorInBufferHotWaterHeating && !checkIfDefrostingActive()) {
                    //ChangeHeatpumpSetting(ADDRESS_CONSTANT_TEMPERATURE_OPERATION_CYCLE, 240);
                    setActiveModeControllerPumpOffDueToDipSwitch1(true);
                }
                
                hot_water_heating_mode_data.state = HOT_WATER_HEATING_IDLE_HEATING;
                break;
            }
            
            if (getSecondCounterHeatingTask() >= ReadSmartEeprom16(SEEP_ADDR_HEATING_MODE_MAX_TIME_WITH_ELEMENT_ON_AND_CIRCULATION_PUMP_OFF)){
                // Temperature not rised a set temperature in a set time
                setSecondCounterHeatingTask(0);
                hot_water_heating_mode_data.HeatingElementOn = false;
                hot_water_heating_mode_data.initialHeatingBufferTemp = heatingBufferTemperature;
                
                hot_water_heating_mode_data.state = HOT_WATER_HEATING_RUNNING_ON_HEATING;
                break;
            }
            
            if (hot_water_heating_mode_data.initialHeatingBufferTemp != TEMPERATURE_ALARM_VALUE) {
                // Has a valid value, so must rise 1 degree now
                if (heatingBufferTemperature < hot_water_heating_mode_data.initialHeatingBufferTemp) {
                    // Adjust the reference temp to the lowest measured temperature 
                    hot_water_heating_mode_data.initialHeatingBufferTemp = heatingBufferTemperature;
                }
                
                if (heatingBufferTemperature >= hot_water_heating_mode_data.initialHeatingBufferTemp + ReadSmartEeprom16(SEEP_ADDR_HEATING_RISE_TEMP_IN_GIVEN_TIME)){
                    // Temperature rised a set temperature in a set time
                    setSecondCounterHeatingTask(0);
                    hot_water_heating_mode_data.HeatingElementOn = false;
                    hot_water_heating_mode_data.initialHeatingBufferTemp = heatingBufferTemperature;
                    //heating_mode_data.stepperSetpoint = getHeatingSetpoint();

                    hot_water_heating_mode_data.state = HOT_WATER_HEATING_RUNNING_ON_HEATING;
                    break;
                }
            }
            else {
                // Has not a valid value, must rise untill the setpoint-delta
                if (checkIfBufferIsWithinSetpointMinusDelta(heatingBufferTemperature) == true) {
                    // Buffer temp is within setpoint - delta, go back to running
                    setSecondCounterHeatingTask(0);
                    hot_water_heating_mode_data.HeatingElementOn = false;
                    hot_water_heating_mode_data.initialHeatingBufferTemp = heatingBufferTemperature;
                    //heating_mode_data.stepperSetpoint = getHeatingSetpoint();

                    hot_water_heating_mode_data.state = HOT_WATER_HEATING_RUNNING_ON_HEATING;
                    break;
                }
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
        
        // 6
        case HOT_WATER_HEATING_INITIALIZE_HOT_WATER:{
            
            setSecondCounterHeatingTask(UINT32_MAX);
            setSecondCounterHotwaterTask(0);
            hot_water_heating_mode_data.blockCirculationPumpAtHeatingStart = false;
            hot_water_heating_mode_data.blockCirculationPumpLongerBecauseTempTooLow = false;
            setSecondCounterBlockCirculationPumpAtHeatingStart(UINT32_MAX);
            HPPI_Clear();
            
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
        
        // 7
        case HOT_WATER_HEATING_STATE_WAIT_FOR_MINIMAL_TIME_IN_HOT_WATER:{
            
            adjustSetpointOffsetHotWater();
            
            if ((getSecondCounterHotwaterTask() >= ReadSmartEeprom16(SEEP_ADDR_HOT_WATER_MIN_TIME_IN_HOT_WATER_MODE)) && (getSecondCounterHotwaterTask() != UINT32_MAX)) {
                // Minimal time in hot water passed
                hot_water_heating_mode_data.state = HOT_WATER_HEATING_STATE_RUNNING_IN_HOT_WATER;
                break;
            }
            
            break;
        }
        
        // 8
        case HOT_WATER_HEATING_STATE_RUNNING_IN_HOT_WATER:{
            
            adjustSetpointOffsetHotWater();
            
            if ((getActiveCompressorsMask() == 0) && (getDefrostingActiveMask() == 0)){
                // Compressor is not running and is also not in defrosting, so go back to heating
                //TurnOffHeatingElementHotWaterBuffer();
                hot_water_heating_mode_data.HotwaterElementOn = false;
                setSecondCounterHotwaterTask(UINT32_MAX);
                HPPI_Clear();
                
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
        
        // 9
        case HOT_WATER_HEATING_STATE_RUNNING_WITH_ELEMENT_ON_IN_HOT_WATER:{
            
            adjustSetpointOffsetHotWater();
            
            if ((getActiveCompressorsMask() == 0) && (getDefrostingActiveMask() == 0)){
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
            // Unknown state, back to initialize
            HOT_WATER_HEATING_MODE_Initialize();
            break;
        }
    }
    
}


