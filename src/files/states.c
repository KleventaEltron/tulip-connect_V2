#include <stddef.h>                    
#include <stdbool.h>                   
#include <stdlib.h>                   
#include <string.h>
#include <stdio.h>
#include "definitions.h" 

#include "states.h"

#include "modbus/heatpump_parameters.h"
#include "eeprom.h"
#include "time_counters.h"
#include "../config/default/user.h"
#include "ntc.h"
#include "modbus/display.h"



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
static volatile bool doFirstTimeHeatpumpCommunicationSettings = false;

bool getDoFirstTimeHeatpumpCommunicationSettings() {
    return doFirstTimeHeatpumpCommunicationSettings;
}

void setDoFirstTimeHeatpumpCommunicationSettings(bool value) {
    doFirstTimeHeatpumpCommunicationSettings = value;
}


void setActiveModeControllerPumpOffDueToDipSwitch1(bool target) {
    SYS_CONSOLE_PRINT("*** NEW HEATPUMP ON TARGET: %s ***\n", (target ? "True" : "False"));
    
    //WriteSmartEeprom8(SEEP_ADDR_DISPLAY_PUMP_ON, !target);
    ChangeHeatpumpSetting(ADDRESS_ON_OFF, !target);
    app_active_mode_controllerData.pumpOffDueToDipSwitch1 = target;
}

bool getActiveModeControllerPumpOffDueToDipSwitch1() {
    return app_active_mode_controllerData.pumpOffDueToDipSwitch1;
}


bool checkIfDefrostingActive(void) {
    uint16_t runningStatusOne = getDataFromMemoryCallable(ADDRESS_RUNNING_STATUS_1, MASTER_HEATPUMP_IN_CASCADE);
    return (runningStatusOne & (1u << 1)) != 0;
}


void setActiveModeControllerHeatpumpSetpointHeating(int16_t newSetpoint) {
    app_active_mode_controllerData.setPointHeating = newSetpoint;
}

void setActiveModeControllerHeatpumpSetpointCooling(int16_t newSetpoint) {
    app_active_mode_controllerData.setPointCooling = newSetpoint;
}

void setActiveModeControllerHeatpumpRunningMode(uint16_t mode) {
    app_active_mode_controllerData.heatpumpRunningMode = mode;
}

bool getResetFactorySettings() {
    return app_active_mode_controllerData.resetFactorySettings;
}

void setResetFactorySettings() {
    app_active_mode_controllerData.resetFactorySettings = true;
}

bool getFactorySettingsResetInProgress() {
    return app_active_mode_controllerData.factorySettingResetInProgress;
}

bool getCurrentDip1SwitchState() {
    return app_active_mode_controllerData.dip1SwitchCurrentState;
}

bool getPreviousDip1SwitchState() {
    return app_active_mode_controllerData.dip1SwitchPreviousState;
}

void setCurrentDip1SwitchState() {
    app_active_mode_controllerData.dip1SwitchCurrentState = GetDip1();
}

void setPreviousDip1SwitchState(bool currentState) {
    app_active_mode_controllerData.dip1SwitchPreviousState = currentState;
}

void resetActiveModeStates() {
    heating_mode_data.state = HEATING_INITIALIZE;
    hot_water_mode_data.state = HOT_WATER_INITIALIZE;
    cooling_mode_data.state = COOLING_INITIALIZE;
    floor_heating_mode_data.state = FLOOR_HEATING_INITIALIZE;
    hot_water_cooling_mode_data.state = HOT_WATER_COOLING_INITIALIZE_COOLING;
    hot_water_heating_mode_data.state = HOT_WATER_HEATING_INITIALIZE_HEATING;
    hot_water_floor_heating_mode_data.state = HOT_WATER_FLOOR_HEATING_INITIALIZE;
    
    // Reset Heating mode data
    setSecondCounterHeatingTask(UINT32_MAX);
    heating_mode_data.initialBufferTemp = TEMPERATURE_ALARM_VALUE;
    heating_mode_data.HeatingElementOn = false;
    
    // Reset Hotwater and Heating mode data
    setSecondCounterHeatingTask(UINT32_MAX);
    setSecondCounterHotwaterTask(UINT32_MAX);
    
    hot_water_heating_mode_data.HeatingElementOn = false;
    hot_water_heating_mode_data.HotwaterElementOn = false;
    
    hot_water_heating_mode_data.initialHeatingBufferTemp = TEMPERATURE_ALARM_VALUE;
    hot_water_heating_mode_data.hotwaterPassive = false;
    hot_water_heating_mode_data.setpointHotWaterOffset = TEMPERATURE_ALARM_VALUE;
            
    if(app_active_mode_controllerData.currentRunningMode == COOLING 
            || app_active_mode_controllerData.currentRunningMode == HOT_WATER_COOLING) {
        if (ReadSmartEeprom8(SEEP_ADDR_COOLING_CONTACT_ENABLE) == true) {
            CoolingActiveRelaySet();
        }
    } else {
        CoolingActiveRelayClear();
    }
    
    return;
}

RUNNING_MODES getActiveStateValue() {
    return app_active_mode_controllerData.currentRunningMode;
}


uint16_t getActiveStateFromActiveMode(RUNNING_MODES state){
    
    uint16_t activeModeActiveState = 0;
    switch (state)
    {
        case(COOLING): {
            activeModeActiveState = cooling_mode_data.state;
            break;
        }
        
        case(HEATING): {
            activeModeActiveState = heating_mode_data.state;
            break;
        }
        
        case(HOT_WATER): {
            activeModeActiveState = hot_water_mode_data.state;
            break;
        }
        
        case(FLOOR_HEATING): {
            activeModeActiveState = floor_heating_mode_data.state;
            break;
        }
        
        case(HOT_WATER_COOLING): {
            activeModeActiveState = hot_water_cooling_mode_data.state;
            break;
        }
        
        case(HOT_WATER_HEATING): {
            activeModeActiveState = hot_water_heating_mode_data.state;
            break;
        }
        
        case(RESERVED): {
            activeModeActiveState = 0;
            break;
        }
        
        case(HOT_WATER_FLOOR_HEATING): {
            activeModeActiveState = hot_water_floor_heating_mode_data.state ;
            break;
        }
        
        default:{
            activeModeActiveState = 0;
            break;
        }
    }
    return activeModeActiveState;
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

uint16_t getActiveCompressorsMask()
{
    uint16_t mask = 0;

    for (uint8_t i = 0; i < MAX_AMOUNT_HEATPUMPS_IN_CASCADE; i++) {
        // For all heatpumps
        uint16_t freq = getHeatpumpCompressorFrequency(i);
        
        if ((freq != 0) && (freq != UINT16_MAX)) {
            // 
            mask |= (uint16_t)(1 << i);
        }
    }

    return mask;
}

uint16_t getHeatpumpCompressorFrequency(uint8_t whichHeatpump)
{
    return RealTimeData1[ADDRESS_COMPRESSOR_OPERATING_FREQUENCY - START_ADDRESS_REAL_TIME_DATA_1][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP][whichHeatpump];
}

int16_t getHeatpumpHeatingSetpoint()
{
    return UserParameters[ADDRESS_HEATING_SET_TEMPERATURE - START_ADDRESS_USER_PARAMETERS][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP];
}

int16_t getHeatpumpCoolingSetpoint()
{
    return UserParameters[ADDRESS_COOLING_SET_TEMPERATURE - START_ADDRESS_USER_PARAMETERS][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP];
}

uint16_t getHeatpumpWaterFlow()
{
    return RealTimeData1[ADDRESS_WATER_FLOW - START_ADDRESS_REAL_TIME_DATA_1][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP][MASTER_HEATPUMP_IN_CASCADE];
}

int16_t getHeatpumpRunningMode()
{
    return UserParameters[ADDRESS_SET_MODE - START_ADDRESS_USER_PARAMETERS][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP];
}

int16_t getHeatpumpOnOff()
{
    return UserParameters[ADDRESS_ON_OFF - START_ADDRESS_USER_PARAMETERS][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP];
}


int16_t getHeatpumpReturnWaterTemperature(uint8_t whichHeatpump)
{
    int16_t returnWaterTemperature = RealTimeData1[ADDRESS_RETURN_WATER_TEMPERATURE_T6 - START_ADDRESS_REAL_TIME_DATA_1][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP][whichHeatpump];
    
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

COOLING_MODE_DATA getCoolingModeData(){
    return cooling_mode_data;
}

HOT_WATER_MODE_DATA getHotWaterModeData(){
    return hot_water_mode_data;
}

HOT_WATER_HEATING_MODE_DATA getHotWaterHeatingModeData(){
    return hot_water_heating_mode_data;
}

HOT_WATER_COOLING_MODE_DATA getHotWaterCoolingModeData(){
    return hot_water_cooling_mode_data;
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

int16_t getCoolingSetpoint()
{
    // Get Heating setpoint out of smart eeprom
    int16_t setpointCooling = ReadSmartEeprom16(SEEP_ADDR_COOLING_SETPOINT);
    //int16_t setpointCooling = UserParameters[ADDRESS_COOLING_SET_TEMPERATURE - START_ADDRESS_USER_PARAMETERS][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP];
    
    if (setpointCooling != TEMPERATURE_ALARM_VALUE){
        setpointCooling *= 10;
    }
    
    return setpointCooling;
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

int16_t getAirConditionerReturnDifference()
{
    int16_t delta = UnitSystemParameters[ADDRESS_AIR_CONDITIONER_RETURN_DIFFERENCE - START_ADDRESS_UNIT_SYSTEM_PARAMETERS][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP];
    
    if (delta != TEMPERATURE_ALARM_VALUE){
        // Is not alarm value, so do times 10
        delta *= 10;
    }
    
    return delta;
}

int16_t getExternalAmbientTemperature(uint8_t whichHeatpump)
{
    int16_t temperature = RealTimeData1[ADDRESS_EXTERNAL_AMBIENT_TEMPERATURE_T1 - START_ADDRESS_REAL_TIME_DATA_1][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP][whichHeatpump];
    
    if (temperature != TEMPERATURE_ALARM_VALUE){
        // Is not alarm value, so do times 10
        temperature *= 10;
    }
    
    return temperature;
}

bool blockHotWaterBasedOnTimers(void) 
{
    if (ReadSmartEeprom8(SEEP_ADDR_BLOCK_HOTWATER) == false) {
        return false;
    }

    // Get current time from heatpump
    uint16_t raw = UserParameters[ADDRESS_DISPLAY_TIME - START_ADDRESS_USER_PARAMETERS]
                                  [PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP];

    uint8_t hours   = (uint8_t)(raw >> 8);    
    uint8_t minutes = (uint8_t)(raw & 0xFF);  

    uint16_t currentDisplayTimeAdjusted = (uint16_t)hours * 60u + (uint16_t)minutes;

    uint16_t start = ReadSmartEeprom16(SEEP_ADDR_START_TIME_BLOCK_HOTWATER);
    uint16_t end   = ReadSmartEeprom16(SEEP_ADDR_END_TIME_BLOCK_HOTWATER);

    bool in_window;

    if (start <= end) {
        // voor bijv start 10:30 tot eind 20:00
        in_window = (currentDisplayTimeAdjusted >= start &&
                     currentDisplayTimeAdjusted <  end);
    } else {
        // voor bijv start 20:00 tot eind 06:30
        in_window = (currentDisplayTimeAdjusted >= start ||
                     currentDisplayTimeAdjusted <  end);
    }

    if (in_window) {
        return false;
    }

    return true;
}

uint16_t getCascadeSlaveStatus()
{
    return RealTimeData1[ADDRESS_CASCADE_SLAVES_ONLINE_1 - START_ADDRESS_REAL_TIME_DATA_1][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP][MASTER_HEATPUMP_IN_CASCADE];
}