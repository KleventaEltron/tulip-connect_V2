#include <stddef.h>                    
#include <stdbool.h>                   
#include <stdlib.h>                   
#include <string.h>
#include <stdio.h>
#include "definitions.h" 

#include "states.h"

#include "modbus/heatpump_parameters.h"
#include "eeprom.h"

APP_ACTIVE_MODE_CONTROLLER_STATES app_active_mode_controllerState;
APP_ACTIVE_MODE_CONTROLLER_DATA app_active_mode_controllerData;


HEATING_MODE_DATA heating_mode_data;
HOT_WATER_MODE_DATA hot_water_mode_data;
COOLING_MODE_DATA cooling_mode_data;
FLOOR_HEATING_MODE_DATA floor_heating_mode_data;
HOT_WATER_COOLING_MODE_DATA hot_water_cooling_mode_data;
HOT_WATER_HEATING_MODE_DATA hot_water_heating_mode_data;
HOT_WATER_FLOOR_HEATING_MODE_DATA hot_water_floor_heating_mode_data;

CIRCULATION_PUMP_DATA circulation_pump_data;


void setActiveModeControllerHeatpumpSetpoint(int16_t newSetpoint) {
    app_active_mode_controllerData.setPoint = newSetpoint;
}


void resetActiveModeStates() {
    heating_mode_data.state = HEATING_INITIALIZE;
    hot_water_mode_data.state = HOT_WATER_INITIALIZE;
    cooling_mode_data.state = COOLING_INITIALIZE;
    floor_heating_mode_data.state = FLOOR_HEATING_INITIALIZE;
    hot_water_cooling_mode_data.state = HOT_WATER_COOLING_INITIALIZE;
    hot_water_heating_mode_data.state = HOT_WATER_HEATING_INITIALIZE_HEATING;
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
        
        case(RESERVED): {
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
    }
    return "-1, Unkown";
}



const char * getThreeWayValveState(int state) {
    switch (state)
    {
        case(0): {
            return "0, VALVE_IS_ON_HEATING_CIRCUIT";
            break;
        }
        
        case(1): {
            return "1, VALVE_IS_ON_HOT_WATER_CIRCUIT";
            break;
        }
        
        default: {
            return "-1, Unkown";
            break;
        }
    }

    return "-1, Unkown";
}

uint16_t getHeatpumpCompressorFrequency()
{
    return RealTimeData[ADDRESS_COMPRESSOR_OPERATING_FREQUENCY - START_ADDRESS_REAL_TIME_DATA][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP];
}

int16_t getHeatpumpReturnWaterTemperature()
{
    int16_t returnWaterTemperature = RealTimeData[ADDRESS_RETURN_WATER_TEMPERATURE_T6 - START_ADDRESS_REAL_TIME_DATA][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP];
    
    if (returnWaterTemperature != TEMPERATURE_ALARM_VALUE){
        // If temperature is not an alarm value, do times 10
        returnWaterTemperature *= 10;
    }
    
    return returnWaterTemperature;
}

CIRCULATION_PUMP_DATA getCircPumpData(){
    return circulation_pump_data;
}

HEATING_MODE_DATA getHeatingModeData(){
    return heating_mode_data;
}

HOT_WATER_HEATING_MODE_DATA getHotWaterHeatingModeData(){
    return hot_water_heating_mode_data;
}

int16_t getHeatingSetpoint()
{
    // Get Heating setpoint out of smart eeprom
    int16_t setpointHeating = ReadSmartEeprom16(SEEP_ADDR_HEATING_SETPOINT);
    
    if (setpointHeating != TEMPERATURE_ALARM_VALUE){
        setpointHeating *= 10;
    }
    
    return setpointHeating;
}

int16_t getHotwaterSetpoint()
{
    // Get hot water setpoint out of smart eeprom
    int16_t setpointHotwater = ReadSmartEeprom16(SEEP_ADDR_HOT_WATER_SETPOINT);
    
    if (setpointHotwater != TEMPERATURE_ALARM_VALUE){
        setpointHotwater *= 10;
    }
    
    return setpointHotwater;
}

int16_t getHotwaterDelta()
{
    int16_t delta = UnitSystemParameters[ADDRESS_HOT_WATER_RETURN_DIFFERENCE - START_ADDRESS_UNIT_SYSTEM_PARAMETERS][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP];
    
    if (delta != TEMPERATURE_ALARM_VALUE){
        // Is not alarm value, so do times 10
        delta *= 10;
    }
    
    return delta;
}