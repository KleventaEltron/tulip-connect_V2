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
            // TODO: LATER OP TERUG KOMEN
            //valvePositionForActiveState = VALVE_IS_ON_HEATING_CIRCUIT;
            break;
        }
        
        
        case HOT_WATER_HEATING:{
            valvePositionForActiveState = VALVE_IS_ON_HOT_WATER_CIRCUIT;
            // TODO: LATER OP TERUG KOMEN
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
    if (UserParameters[ADDRESS_ON_OFF - START_ADDRESS_USER_PARAMETERS][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP] != SET_HEATPUMP_OFF || 
            RealTimeData[ADDRESS_WATER_FLOW - START_ADDRESS_REAL_TIME_DATA][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP] != 0) {
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
    // Check the valve position for the selected mode and switch it if needed
    if (getStatus3WayValve() != neededValvePosition) {   
        switchThreeWayValve(neededValvePosition);      
        return false;
    }
    // Heatpump must be on before we can may start any other action again
    if(UserParameters[ADDRESS_ON_OFF - START_ADDRESS_USER_PARAMETERS][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP] != SET_HEATPUMP_ON){
        ChangeHeatpumpSetting(ADDRESS_ON_OFF, SET_HEATPUMP_ON);
        setWaitingThreeWayValveSwitch(0);
        return false;
    }
    // Reset system stuck counter
    setSystemStuckProtectionCounter(0);    
    return true;
}



bool getNeededValvePosition() {
    return neededValvePosition;
}
