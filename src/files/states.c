#include <stddef.h>                    
#include <stdbool.h>                   
#include <stdlib.h>                   
#include <string.h>
#include <stdio.h>
#include "definitions.h" 

#include "states.h"

#include "modbus/heatpump_parameters.h"

APP_ACTIVE_MODE_CONTROLLER_STATES app_active_mode_controllerState;
APP_ACTIVE_MODE_CONTROLLER_DATA app_active_mode_controllerData;


HEATING_MODE_DATA heating_mode_data;
HOT_WATER_MODE_DATA hot_water_mode_data;
COOLING_MODE_DATA cooling_mode_data;
FLOOR_HEATING_MODE_DATA floor_heating_mode_data;
HOT_WATER_COOLING_MODE_DATA hot_water_cooling_mode_data;
HOT_WATER_HEATING_MODE_DATA hot_water_heating_mode_data;
HOT_WATER_FLOOR_HEATING_MODE_DATA hot_water_floor_heating_mode_data;


void resetActiveModeStates() {
    heating_mode_data.state = HEATING_INITIALIZE;
    hot_water_mode_data.state = HOT_WATER_INITIALIZE;
    cooling_mode_data.state = COOLING_INITIALIZE;
    floor_heating_mode_data.state = FLOOR_HEATING_INITIALIZE;
    hot_water_cooling_mode_data.state = HOT_WATER_COOLING_INITIALIZE;
    hot_water_heating_mode_data.state = HOT_WATER_HEATING_INITIALIZE;
    hot_water_floor_heating_mode_data.state = HOT_WATER_FLOOR_HEATING_INITIALIZE;
    return;
}

const char * getActiveModeToString(RUNNING_MODES state){
    switch (state)
    {
        case(COOLING): {
            return "0, Cooling";
            break;
        }
        
        case(HEATING): {
            return "1, Heating";
            break;
        }
        
        case(HOT_WATER): {
            return "2, Hot Water";
            break;
        }
        
        case(FLOOR_HEATING): {
            return "3, Floor Heating";
            break;
        }
        
        case(HOT_WATER_COOLING): {
            return "4, Hot Water & Cooling";
            break;
        }
        
        case(HOT_WATER_HEATING): {
            return "5, Hot Water & Heating";
            break;
        }
        
        case(RESERVE): {
            return "6, Reserved";
            break;
        }
        
        case(HOT_WATER_FLOOR_HEATING): {
            return "7, Hot Water & Floor Heating";
            break;
        }
        
        default:{
            return "-1, Unkown";
            break;
        }
        return "-1, Unkown";
    }
}

bool isDefrostingActive()
{
    if (RealTimeDataStatussen[ADDRESS_RUNNING_STATUS_1 - START_ADDRESS_REAL_TIME_DATA_STATUSSEN][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP] & (1 << RUNNING_STATUS_1_SYSTEM_DEFROST_BIT)){
        // Defrosting bit high and thus active
        return true;
    }
    else{
        return false;
    }
}

uint16_t getHeatpumpCompressorFrequency()
{
    return RealTimeData[ADDRESS_COMPRESSOR_OPERATING_FREQUENCY - START_ADDRESS_REAL_TIME_DATA][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP];
}