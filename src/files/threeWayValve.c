#include <stddef.h>                    
#include <stdbool.h>                   
#include <stdlib.h>                   
#include <string.h>
#include <stdio.h>
#include "definitions.h" 

#include "threeWayValve.h"
#include "modbus/heatpump_parameters.h"
#include "time_counters.h"
#include "sterilization.h"
#include "states.h"
#include "eeprom.h"
#include "alarms.h"

bool neededValvePosition = VALVE_IS_ON_HEATING_CIRCUIT;

bool neededThreeWayValveState(RUNNING_MODES selectedRunningMode) {    
    bool valvePositionForActiveState = 0;
    
    if (getSterilisationMode() == ACTIVE) {
        valvePositionForActiveState = VALVE_IS_ON_HOT_WATER_CIRCUIT;
        return valvePositionForActiveState;
    }
    
    //switch(app_active_mode_controllerData.currentRunningMode)
    switch(selectedRunningMode)
    { 
        case HEATING:{
            valvePositionForActiveState = VALVE_IS_ON_HEATING_CIRCUIT;
            break;
        }
        
        
        case COOLING:{
            valvePositionForActiveState = VALVE_IS_ON_HEATING_CIRCUIT;
            break;
        }
        
        
        case FLOOR_HEATING: {
            valvePositionForActiveState = VALVE_IS_ON_HEATING_CIRCUIT;    
            break;
        }
            
        
        case HOT_WATER:{
            valvePositionForActiveState = VALVE_IS_ON_HOT_WATER_CIRCUIT;
            break;
        }
        
        
        case HOT_WATER_COOLING:{
            if ((getHotWaterCoolingModeData().state == HOT_WATER_COOLING_INITIALIZE_COOLING) || 
                    (getHotWaterCoolingModeData().state == HOT_WATER_COOLING_IDLE_COOLING) ||
                    (getHotWaterCoolingModeData().state == HOT_WATER_COOLING_MODE_RUNNING_ON_COOLING)) {
                // Valve must be on heating (also cooling) circuit
                valvePositionForActiveState = VALVE_IS_ON_HEATING_CIRCUIT;
                break;
            }
            
            if ((getHotWaterCoolingModeData().state == HOT_WATER_COOLING_INITIALIZE_HOT_WATER) || 
                    (getHotWaterCoolingModeData().state == HOT_WATER_COOLING_STATE_WAIT_FOR_MINIMAL_TIME_IN_HOT_WATER) ||
                    (getHotWaterCoolingModeData().state == HOT_WATER_COOLING_STATE_RUNNING_IN_HOT_WATER) ||
                    (getHotWaterCoolingModeData().state == HOT_WATER_COOLING_STATE_RUNNING_WITH_ELEMENT_ON_IN_HOT_WATER)) {
                // Valve must be on hot water circuit
                valvePositionForActiveState = VALVE_IS_ON_HOT_WATER_CIRCUIT;
                break;
            } 
            
            break;
        }
        
        
        case HOT_WATER_HEATING:{
            if ((getHotWaterHeatingModeData().state == HOT_WATER_HEATING_INITIALIZE_HEATING) || 
                    (getHotWaterHeatingModeData().state == HOT_WATER_HEATING_IDLE_HEATING) ||
                    (getHotWaterHeatingModeData().state == HOT_WATER_HEATING_RUNNING_ON_HEATING) ||
                    (getHotWaterHeatingModeData().state == HOT_WATER_HEATING_RUNNING_ON_HEATING_WITH_ELEMENT_ON)) {
                valvePositionForActiveState = VALVE_IS_ON_HEATING_CIRCUIT;
                break;
            }
            
            if ((getHotWaterHeatingModeData().state == HOT_WATER_HEATING_INITIALIZE_HOT_WATER) || 
                    (getHotWaterHeatingModeData().state == HOT_WATER_HEATING_STATE_WAIT_FOR_MINIMAL_TIME_IN_HOT_WATER) ||
                    (getHotWaterHeatingModeData().state == HOT_WATER_HEATING_STATE_RUNNING_IN_HOT_WATER) ||
                    (getHotWaterHeatingModeData().state == HOT_WATER_HEATING_STATE_RUNNING_WITH_ELEMENT_ON_IN_HOT_WATER)) {
                valvePositionForActiveState = VALVE_IS_ON_HOT_WATER_CIRCUIT;
                break;
            } 
            
            break;
        }
        
        
        case HOT_WATER_FLOOR_HEATING:{
            valvePositionForActiveState = VALVE_IS_ON_HEATING_CIRCUIT;
            // TODO: LATER OP TERUG KOMEN
            break;
        }
         
        default:{
            break;
        }
    }        
    
    return valvePositionForActiveState;
}





void switchThreeWayValve() {
    // Heatpump must be off, and water flow must be 0 before we are allowed to switch the valve
    if (getHeatpumpWaterFlow() != 0) {
        ChangeHeatpumpSetting(ADDRESS_ON_OFF, SET_HEATPUMP_OFF);
        setWaitingThreeWayValveSwitch(0);
        return;
    }
    
    if (neededValvePosition == VALVE_IS_ON_HEATING_CIRCUIT) {
        Switch3WayValveToHeating();
    } else if (neededValvePosition == VALVE_IS_ON_HOT_WATER_CIRCUIT) {
        Switch3WayValveToHotWater();
    } 
    setWaitingThreeWayValveSwitch(0);
    
    return;
}





bool validateThreeWayValveStateOkay(RUNNING_MODES currentRunningMode) {
    neededValvePosition = neededThreeWayValveState(currentRunningMode);
    
    // Delay for after either turning off the heatpump or switching the three way valve
    if (getWaitingThreeWayValveSwitch() >= 0 && getWaitingThreeWayValveSwitch() < 20) {
        return false;
    }
    setWaitingThreeWayValveSwitch(UINT32_MAX);
    
    if (GetAlarmStatus(ALARM_HEATPUMP_COMMUNICATION) == true) {
        // Heatpump communication alarm, no need for switching the 3-way valve
        return true;
    }
    
    // Check the valve position for the selected mode and switch it if needed
    if (getStatus3WayValve() != neededValvePosition) {   
        switchThreeWayValve(neededValvePosition);      
        return false;
    }
    // Heatpump must be on before we can may start any other action again
    if(UserParameters[ADDRESS_ON_OFF - START_ADDRESS_USER_PARAMETERS][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP] != SET_HEATPUMP_ON && !getActiveModeControllerPumpOffDueToDipSwitch1()){
        ChangeHeatpumpSetting(ADDRESS_ON_OFF, SET_HEATPUMP_ON);
        setWaitingThreeWayValveSwitch(0);
        return false;
    }
    // Reset system stuck counter
    //setSystemStuckProtectionCounter(0);    
    return true;
}



bool getNeededValvePosition() {
    return neededValvePosition;
}
