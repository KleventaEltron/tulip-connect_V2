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
HOT_WATER_COOLING_MODE_DATA hot_water_cooling_data;
HOT_WATER_HEATING_MODE_DATA hot_water_heating_data;
HOT_WATER_FLOOR_HEATING_MODE_DATA hot_water_floor_heating_data;


void resetActiveModeStates() {
    heating_mode_data.state = HEATING_INITIALIZE;
    hot_water_mode_data.state = HOT_WATER_INITIALIZE;
    cooling_mode_data.state = COOLING_INITIALIZE;
    floor_heating_mode_data.state = FLOOR_HEATING_INITIALIZE;
    hot_water_cooling_data.state = HOT_WATER_COOLING_INITIALIZE;
    hot_water_heating_data.state = HOT_WATER_HEATING_INITIALIZE;
    hot_water_floor_heating_data.state = HOT_WATER_FLOOR_HEATING_INITIALIZE;
    return;
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
    return 0;
}