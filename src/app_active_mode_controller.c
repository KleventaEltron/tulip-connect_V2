
#include <stddef.h>                     
#include <stdbool.h>                    
#include <stdlib.h>                    
#include <string.h>
#include <stdio.h>
#include "definitions.h" 

#include "app_active_mode_controller.h"

#include "config/default/user.h"
#include "files/ntc.h"

#include "files/states.h"
#include "files/eeprom.h"
#include "files/time_counters.h"
#include "files/modbus/heatpump_parameters.h"

#include "files/heating_mode.h"
#include "files/hot_water_mode.h"
#include "files/cooling_mode.h"
#include "files/floor_heating_mode.h"
#include "files/hot_water_cooling_mode.h"
#include "files/hot_water_heating_mode.h"
#include "files/hot_water_floor_heating_mode.h"
#include "files/threeWayValve.h"
#include "files/sterilization.h"
#include "files/defrosting.h"
#include "files/modbus/display.h"
#include "files/eeprom.h"
#include "files/i2c/mac.h"

#include "files/circulation_pump.h"
#include "files/emergency_mode.h"



extern APP_ACTIVE_MODE_CONTROLLER_STATES app_active_mode_controllerState;
extern APP_ACTIVE_MODE_CONTROLLER_DATA app_active_mode_controllerData;

bool factorySettingResetInProgress = false;
 
 

void debugPI(void)
{
                    SYS_CONSOLE_PRINT("\n=== HEATPUMP PI DEBUG ===\n");
                
                SYS_CONSOLE_PRINT(" Time counter:         %lu s\n", getSecondCounterHeatpumpPowerRegulation());
                SYS_CONSOLE_PRINT(" Update interval:      %lu s\n", getHeatingModeData().visualUpdateInterval_sec);
                
                SYS_CONSOLE_PRINT("\n--- TEMPERATURE ---\n");
                
                SYS_CONSOLE_PRINT(" Heatpump Setpoint:    %u C\n",
                    getHeatpumpHeatingSetpoint());
                
                SYS_CONSOLE_PRINT(" Temp now:             %ld mC (%.2f C)\n",
                    getHeatingModeData().visualTempNow_mC,
                    getHeatingModeData().visualTempNow_mC / 1000.0f);

                SYS_CONSOLE_PRINT(" Temp previous:        %ld mC (%.2f C)\n",
                    getHeatingModeData().visualTempPrev_mC,
                    getHeatingModeData().visualTempPrev_mC / 1000.0f);

                SYS_CONSOLE_PRINT(" Delta T:              %ld mC (%.3f C)\n",
                    getHeatingModeData().visualdT_mC,
                    getHeatingModeData().visualdT_mC / 1000.0f);

                SYS_CONSOLE_PRINT(" Delta time:           %lu s\n",
                    getHeatingModeData().visualdt_sec);
                
                SYS_CONSOLE_PRINT(" Startup seconds:      %lu s\n",
                    getHeatingModeData().visualStartupSeconds);


                SYS_CONSOLE_PRINT("\n--- SLOPE ---\n");
                SYS_CONSOLE_PRINT(" Target slope:         %ld mC/min (%.3f C/100min)\n",
                    getHeatingModeData().visualSlopeTarget_mC_min,
                    getHeatingModeData().visualSlopeTarget_mC_min * 100 / 1000.0f);

                SYS_CONSOLE_PRINT(" Measured slope:       %ld mC/min (%.3f C/100min)\n",
                    getHeatingModeData().visualSlopeMeas_mC_min,
                    getHeatingModeData().visualSlopeMeas_mC_min * 100 / 1000.0f);

                SYS_CONSOLE_PRINT(" Error:                %ld mC/min\n",
                    getHeatingModeData().visualError_mC_min);


                SYS_CONSOLE_PRINT("\n--- PI OUTPUT ---\n");
                SYS_CONSOLE_PRINT(" P term:               %ld (x1000 Hz)\n",
                    getHeatingModeData().visualP_Hz_x1000);

                SYS_CONSOLE_PRINT(" I term:               %ld (x1000 Hz)\n",
                    getHeatingModeData().visualI_Hz_x1000);

                SYS_CONSOLE_PRINT(" Output:               %ld Hz\n",
                    getHeatingModeData().visualU_Hz);


                SYS_CONSOLE_PRINT("\n--- FREQUENCY ---\n");
                SYS_CONSOLE_PRINT(" Frequency before:     %u Hz\n",
                    getHeatingModeData().visualFreqBefore);

                SYS_CONSOLE_PRINT(" Frequency after:      %u Hz\n",
                    getHeatingModeData().visualFreqAfter);
                
                SYS_CONSOLE_PRINT(" Target frequency:     %u Hz\n",
                    getHeatpumpTargetFrequency());
                
                SYS_CONSOLE_PRINT(" Running frequency:    %u Hz\n",
                    getHeatpumpCompressorFrequency(0));

                SYS_CONSOLE_PRINT("\n--- EVENTS ---\n");
                SYS_CONSOLE_PRINT(" Flags raw:            0x%02X\n",
                    getHeatingModeData().visualEventFlags);

                SYS_CONSOLE_PRINT("  Time reached:        %s\n",
                    (getHeatingModeData().visualEventFlags & 0x01) ? "YES" : "NO");

                SYS_CONSOLE_PRINT("  Step reached (0.2C): %s\n",
                    (getHeatingModeData().visualEventFlags & 0x02) ? "YES" : "NO");

                SYS_CONSOLE_PRINT("  Temp dropped:        %s\n",
                    (getHeatingModeData().visualEventFlags & 0x04) ? "YES" : "NO");
}
 
 void printDebugInfo() {
    //printCustomEepromParameters();
    if (DebugDipSwitch() == true) {
        /*
        SYS_CONSOLE_PRINT("\r\nINFO:\n", getActiveModeToString(app_active_mode_controllerData.currentRunningMode));
        SYS_CONSOLE_PRINT(" Active mode:          %s\n", getActiveModeToString(app_active_mode_controllerData.currentRunningMode));
        SYS_CONSOLE_PRINT(" 3-way valve mode:     %s\n", getThreeWayValveState(getStatus3WayValve()));
        SYS_CONSOLE_PRINT(" 3-way needed state:   %s\n\n", getThreeWayValveState(getNeededValvePosition()));
        SYS_CONSOLE_PRINT(" Heatpump state:       %s\n", (UserParameters[ADDRESS_ON_OFF - START_ADDRESS_USER_PARAMETERS][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP] ? "True" : "False"));
        SYS_CONSOLE_PRINT(" Display pump on:      %s\n", (ReadSmartEeprom8(SEEP_ADDR_DISPLAY_PUMP_ON) ? "True" : "False"));
        SYS_CONSOLE_PRINT(" Sterilisation active: %s\n\n", getSterilizationState(getSterilisationMode()));
        SYS_CONSOLE_PRINT(" Counters:\n"
                          "   Legionella:               %i\n"
                          "   3-Way switch:             %i\n"
                          "   Sys stuck protection:     %i\n"
                          "   Day Counter:              %i\n"
                          "   System On:                %i\n\n", getSecondCounterLegionella(), getWaitingThreeWayValveSwitch(), getSystemStuckProtectionCounter(),  ReadSmartEeprom16(SEEP_ADDR_DAY_COUNTER_STERILIZATION), getsystemOnCounter());
        SYS_CONSOLE_PRINT(" Pumpstate:            %s\n", getCirculationPumpStateToString());
        SYS_CONSOLE_PRINT(" Heatpump Setpoint:    %i\n", app_active_mode_controllerData.setPoint);
        SYS_CONSOLE_PRINT(" Buffer:               %d\n", GetNtcTemperature(NTC_HEATING_BUFFER));
        SYS_CONSOLE_PRINT(" Temp too low:         %d\n", getCircPumpData().temperatureTooLowForPumpToBeOn);
        SYS_CONSOLE_PRINT(" Counter:              %d\n", (int)getSecondCounterCirculationPumpTask());
        */
           
//        SYS_CONSOLE_PRINT("\r\nINFO:\n", getActiveModeToString(app_active_mode_controllerData.currentRunningMode));
//        SYS_CONSOLE_PRINT(" FW:                   %d-%d-%d\n", (int)((THIS_FIRMWARE_VERSION / 1000000)), (int)((THIS_FIRMWARE_VERSION / 1000) % 1000), (int)(THIS_FIRMWARE_VERSION % 1000));
//        SYS_CONSOLE_PRINT(" Return diff:          %i\n", getDataFromMemoryCallable(ADDRESS_AIR_CONDITIONER_RETURN_DIFFERENCE));
//        SYS_CONSOLE_PRINT(" Active mode:          %s\n", getActiveModeToString(app_active_mode_controllerData.currentRunningMode));
//        SYS_CONSOLE_PRINT(" Heatpump ON:          %s\n", (UserParameters[ADDRESS_ON_OFF - START_ADDRESS_USER_PARAMETERS][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP] ? "True" : "False"));
//        SYS_CONSOLE_PRINT(" Display pump on:      %s\n", (ReadSmartEeprom8(SEEP_ADDR_DISPLAY_PUMP_ON) ? "True" : "False"));
//        SYS_CONSOLE_PRINT(" Pump on Dip1:         %s\n", (!getActiveModeControllerPumpOffDueToDipSwitch1() ? "True" : "False"));
//        SYS_CONSOLE_PRINT(" Sys stuck protection: %i\n", getSystemStuckProtectionCounter());
//        SYS_CONSOLE_PRINT(" Sys on time:          %i\n\n", getsystemOnCounter());
        //SYS_CONSOLE_PRINT(" Hot W/Cooling Curve:  %i\n", getDataFromMemoryCallable(ADDRESS_HOT_WATER_COOLING_CURVE_SETTINGS));
        //SYS_CONSOLE_PRINT(" Cooling Curve:        %i\n", getDataFromMemoryCallable(ADDRESS_COOLING_CURVE_SETTING));
        //SYS_CONSOLE_PRINT(" Heating SETTY WETTY:        %i\n", (getHeatpumpHeatingSetpoint()*10));
//        SYS_CONSOLE_PRINT(" Heating Curve:        %i\n", getDataFromMemoryCallable(ADDRESS_HEATING_CURVE_SETTING));
//        SYS_CONSOLE_PRINT(" Cooling Curve:        %i\n\n", getDataFromMemoryCallable(ADDRESS_COOLING_CURVE_SETTING));
//        SYS_CONSOLE_PRINT(" Heating Curve seep:        %i\n", ReadSmartEeprom16(SEEP_ADDR_HEATING_CURVE));
//        SYS_CONSOLE_PRINT(" Cooling Curve seep:        %i\n\n", ReadSmartEeprom16(SEEP_ADDR_COOLING_CURVE));
//        SYS_CONSOLE_PRINT(" Heating seep temp:        %i\n", ReadSmartEeprom16(SEEP_ADDR_HEATING_SETPOINT_CURVE_BACKUP));
//        SYS_CONSOLE_PRINT(" Cooling seep temp:        %i\n\n", ReadSmartEeprom16(SEEP_ADDR_COOLING_SETPOINT_CURVE_BACKUP));
//        SYS_CONSOLE_PRINT(" Return Compensation:        %i\n", (int16_t)getDataFromMemoryCallable(ADDRESS_RETURN_WATER_TEMPERATURE_COMPENSATION_VALUE));
//        SYS_CONSOLE_PRINT(" Outlet Compensation:        %i\n\n", (int16_t)getDataFromMemoryCallable(ADDRESS_OUTLET_WATER_TEMPERATURE_COMPENSATION_VALUE));    
        
        //SYS_CONSOLE_PRINT(" Hot W Curve:          %i\n", getDataFromMemoryCallable(ADDRESS_HOT_WATER_CURVE_SETTING));
        //SYS_CONSOLE_PRINT(" UnderF Curve:         %i\n", getDataFromMemoryCallable(ADDRESS_FLOOR_HEATING_CURVE_SETTING));
        
        /* EVU and thermostat
        SYS_CONSOLE_PRINT("\r\nEVU:\n");
        SYS_CONSOLE_PRINT(" EVU enabled:           %s\n", (ReadSmartEeprom8(SEEP_ADDR_EVU_CONTACT_ENABLE) ? "True" : "False"));
        SYS_CONSOLE_PRINT(" EVU contact:           %s\n\n", (GetDigitalInput2() ? "True" : "False"));
        SYS_CONSOLE_PRINT(" Forced off:            %s\n", (app_active_mode_controllerData.heatpumpForcedOff ? "True" : "False"));
        SYS_CONSOLE_PRINT(" Display was on:        %s\n\n", (ReadSmartEeprom8(SEEP_ADDR_HEATPUMP_WAS_ON_BEFORE_FORCED_OFF) ? "True" : "False"));
        SYS_CONSOLE_PRINT(" Forced off counter:    %i\n", getWriteHeatpumpForcedOffCounter());
        SYS_CONSOLE_PRINT(" HP turning on counter: %i\n\n", getWaitingTurningHeatpumpOn());
        SYS_CONSOLE_PRINT(" Heatpump ON:           %s\n", (UserParameters[ADDRESS_ON_OFF - START_ADDRESS_USER_PARAMETERS][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP] ? "True" : "False"));
        SYS_CONSOLE_PRINT(" Display pump on:       %s\n\n", (ReadSmartEeprom8(SEEP_ADDR_DISPLAY_PUMP_ON) ? "True" : "False"));
        SYS_CONSOLE_PRINT(" SW HP on thermostat:   %s\n", (ReadSmartEeprom8(SEEP_ADDR_SWITCH_HEATPUMP_ON_OFF_WITH_THERMOSTAT) ? "True" : "False"));
        SYS_CONSOLE_PRINT(" Thermostat contact:    %s\n", (GetThermostatContact() ? "True" : "False"));
        */
        
        if(GetDip3()) {
            printHeadOfStringBuffer();
        }
        /*
        SYS_CONSOLE_PRINT("\r\nHEATPUMP:\n");
        SYS_CONSOLE_PRINT(" FW:                  %x:%x:%x:%x:%x:%x:%x:%x\n", eui64[0], eui64[1], eui64[2], eui64[3], eui64[4], eui64[5], eui64[6], eui64[7]);
        SYS_CONSOLE_PRINT("\r\nINFO:\n", getActiveModeToString(app_active_mode_controllerData.currentRunningMode));
        SYS_CONSOLE_PRINT(" SILENT MODE EEPROM:  %i\n", ReadSmartEeprom8(SEEP_ADDR_SILENT_MODE));
        SYS_CONSOLE_PRINT(" SILENT BOOST EEPROM: %i\n", ReadSmartEeprom8(SEEP_ADDR_BOOST_MODE));
        SYS_CONSOLE_PRINT(" FREQ MODE CONNECT:   %i\n", getDataFromMemoryCallable(ADDRESS_FREQUENCY_CONVERSION_MODE));
        SYS_CONSOLE_PRINT(" Active mode:         %s\n", getActiveModeToString(app_active_mode_controllerData.currentRunningMode));
        SYS_CONSOLE_PRINT(" Setpoint Heating:    %i\n", getHeatpumpHeatingSetpoint());
        SYS_CONSOLE_PRINT(" Setpoint Cooling:    %i\n", getHeatpumpCoolingSetpoint());
        SYS_CONSOLE_PRINT(" Heatpump mode:       %i\n", getHeatpumpRunningMode());
        SYS_CONSOLE_PRINT(" Heatpump ON:         %s\n", (UserParameters[ADDRESS_ON_OFF - START_ADDRESS_USER_PARAMETERS][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP] ? "True" : "False"));
        SYS_CONSOLE_PRINT(" Display pump on:     %s\n\n", (ReadSmartEeprom8(SEEP_ADDR_DISPLAY_PUMP_ON) ? "True" : "False"));
        SYS_CONSOLE_PRINT(" Heating Curve:       %i\n", getDataFromMemoryCallable(ADDRESS_HEATING_CURVE_SETTING));
        SYS_CONSOLE_PRINT(" Cooling Curve:       %i\n\n", getDataFromMemoryCallable(ADDRESS_COOLING_CURVE_SETTING));
        SYS_CONSOLE_PRINT(" Compressor master:   %i\n", getHeatpumpCompressorFrequency(MASTER_HEATPUMP_IN_CASCADE));
        SYS_CONSOLE_PRINT(" Compressor slave:    %i\n\n", getHeatpumpCompressorFrequency(SLAVE_HEATPUMP_1_IN_CASCADE));
        //SYS_CONSOLE_PRINT(" Waterflow:            %i\n", getHeatpumpWaterFlow());
        SYS_CONSOLE_PRINT(" Inlet temp. master:  %i\n", getHeatpumpReturnWaterTemperature(MASTER_HEATPUMP_IN_CASCADE));
        SYS_CONSOLE_PRINT(" Inlet temp. slave:   %i\n\n", getHeatpumpReturnWaterTemperature(SLAVE_HEATPUMP_1_IN_CASCADE));
        SYS_CONSOLE_PRINT(" Outlet temp. master: %i\n", RealTimeData1[ADDRESS_WATER_OUTLET_TEMPERATURE_T7 - START_ADDRESS_REAL_TIME_DATA_1][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP][MASTER_HEATPUMP_IN_CASCADE]);
        SYS_CONSOLE_PRINT(" Outlet temp. slave:  %i\n\n", RealTimeData1[ADDRESS_WATER_OUTLET_TEMPERATURE_T7 - START_ADDRESS_REAL_TIME_DATA_1][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP][SLAVE_HEATPUMP_1_IN_CASCADE]);
        SYS_CONSOLE_PRINT(" Compr. running?:     %x\n\n", getActiveCompressorsMask());
        SYS_CONSOLE_PRINT(" Defrosting active:   %x\n\n", getDefrostingActiveMask());
        */
        
        int16_t  heatingSetpoint;
    
        if (ReadSmartEeprom16(SEEP_ADDR_HEATING_CURVE) > 0 && ReadSmartEeprom16(SEEP_ADDR_HEATING_CURVE) != UINT16_MAX) {
            heatingSetpoint = getHeatpumpHeatingSetpoint() * 10; 
        } else {
            heatingSetpoint = getHeatingSetpoint(); 
        }
        
//        SYS_CONSOLE_PRINT("\r\nINFO: %s\n\n", getActiveModeToString(app_active_mode_controllerData.currentRunningMode));
//        
//        SYS_CONSOLE_PRINT(" Emergency Mode Heating:     %d\n", ReadSmartEeprom16(SEEP_ADDR_EMERGENCY_MODE_HEATING_ENABLED));
//        SYS_CONSOLE_PRINT(" Heating setpoint:           %d\n", (int16_t)heatingSetpoint);
//        SYS_CONSOLE_PRINT(" Delta:                      %d\n", (int16_t)getAirConditionerReturnDifference());
//        SYS_CONSOLE_PRINT(" Buffer:                     %d\n", GetNtcTemperature(NTC_HEATING_BUFFER));
//        SYS_CONSOLE_PRINT(" Heating element:            %d\n\n", getHeatingElementBoolFromEmergencyMode());
//        
//        SYS_CONSOLE_PRINT(" Emergency Mode Hotwater:    %d\n", ReadSmartEeprom16(SEEP_ADDR_EMERGENCY_MODE_HOTWATER_ENABLED));
//        SYS_CONSOLE_PRINT(" Hotwater setpoint:          %d\n", (int16_t)getHotwaterSetpoint());
//        SYS_CONSOLE_PRINT(" Delta:                      %d\n", (int16_t)getAirConditionerReturnDifference());
//        SYS_CONSOLE_PRINT(" Boiler:                     %d\n", GetNtcTemperature(NTC_HOT_WATER_BUFFER));
//        SYS_CONSOLE_PRINT(" Hot water element:          %d\n\n", getHotWaterElementBoolFromEmergencyMode());
        
        
        /*
        SYS_CONSOLE_PRINT("\r\nSTERILIZATION:\n");
        SYS_CONSOLE_PRINT(" Sterilisation active:       %s\n", getSterilizationState(getSterilisationMode()));
        SYS_CONSOLE_PRINT(" Emergency Mode Hotwater:    %d\n", ReadSmartEeprom16(SEEP_ADDR_EMERGENCY_MODE_HOTWATER_ENABLED));
        SYS_CONSOLE_PRINT(" ON HOLD:                    %d\n\n", ReadSmartEeprom16(SEEP_ADDR_STERILIZATION_ON_HOLD));
        
        //SYS_CONSOLE_PRINT(" Function  :           %i\n", UnitSystemParameterL[ADDRESS_HIGH_TEMPERATURE_STERILIZATION_FUNCTION - START_ADDRESS_UNIT_SYSTEM_PARAMETER_L][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP]);
        //SYS_CONSOLE_PRINT(" Interval days  :      %i\n", UnitSystemParameterL[ADDRESS_STERILIZATION_INTERVAL_DAYS - START_ADDRESS_UNIT_SYSTEM_PARAMETER_L][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP]);
        //SYS_CONSOLE_PRINT(" Start time  :         %i\n\n", UnitSystemParameterL[ADDRESS_STERILIZATION_START_TIME - START_ADDRESS_UNIT_SYSTEM_PARAMETER_L][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP]);
        
        SYS_CONSOLE_PRINT(" Current hours  :      %i\n", (uint8_t)(UserParameters[ADDRESS_DISPLAY_TIME - START_ADDRESS_USER_PARAMETERS][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP] >> 8));
        SYS_CONSOLE_PRINT(" Current minutes  :    %i\n", (uint8_t)UserParameters[ADDRESS_DISPLAY_TIME - START_ADDRESS_USER_PARAMETERS][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP]);
        SYS_CONSOLE_PRINT(" Day counter:          %i\n\n", ReadSmartEeprom16(SEEP_ADDR_DAY_COUNTER_STERILIZATION));
        
        SYS_CONSOLE_PRINT(" Time counter:         %i\n", getSecondCounterLegionella());
        SYS_CONSOLE_PRINT(" Temp reached time:    %i\n", getSterilizationReachedTemperatureTimeStamp());
        SYS_CONSOLE_PRINT(" Hotwater buffer:      %i\n", GetNtcTemperature(NTC_HOT_WATER_BUFFER));
        SYS_CONSOLE_PRINT(" Ster. temp.:          %i\n", UnitSystemParameterL[ADDRESS_STERILIZATION_TEMPERATURE_SETTING - START_ADDRESS_UNIT_SYSTEM_PARAMETER_L][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP]);
        SYS_CONSOLE_PRINT(" Ster. offset:         %i\n", getSterilizationTemperatureOffset());
        SYS_CONSOLE_PRINT(" Ster. element:        %s\n\n", getSterilizationElementOnState() ? "True" : "False");
        */
        
        if (getSterilisationMode() != OFF && getSterilisationMode() != ON_HOLD) {
            SYS_CONSOLE_PRINT("\r\nSTERILIZATION:\n");
            SYS_CONSOLE_PRINT(" Sterilisation active: %s\n", getSterilizationState(getSterilisationMode()));
            SYS_CONSOLE_PRINT(" Time counter:         %i\n", getSecondCounterLegionella());
            SYS_CONSOLE_PRINT(" Temp reached time:    %i\n", getSterilizationReachedTemperatureTimeStamp());
            SYS_CONSOLE_PRINT(" Hotwater buffer:      %i\n", GetNtcTemperature(NTC_HOT_WATER_BUFFER));
            SYS_CONSOLE_PRINT(" Ster. temp.:          %i\n", UnitSystemParameterL[ADDRESS_STERILIZATION_TEMPERATURE_SETTING - START_ADDRESS_UNIT_SYSTEM_PARAMETER_L][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP]);
            SYS_CONSOLE_PRINT(" Ster. offset:         %i\n", getSterilizationTemperatureOffset());
            SYS_CONSOLE_PRINT(" Ster. element:        %s\n", getSterilizationElementOnState() ? "True" : "False");
            SYS_CONSOLE_PRINT(" Day counter:          %i\n\n", ReadSmartEeprom16(SEEP_ADDR_DAY_COUNTER_STERILIZATION));
        }
        else {
            int16_t heatpumpMode = ReadSmartEeprom16(SEEP_ADDR_HEATPUMP_MODE);

            if (heatpumpMode == HEATING) {

//                SYS_CONSOLE_PRINT("\r\nHEATING:\n");
//                SYS_CONSOLE_PRINT(" State:                %s\n", getHeatingStateToString());
//                SYS_CONSOLE_PRINT(" Element ON:           %s\n", getStatusHeatingElementHeatingBuffer() ? "True" : "False");
//                SYS_CONSOLE_PRINT(" Buffer:               %i\n", GetNtcTemperature(NTC_HEATING_BUFFER));
//                SYS_CONSOLE_PRINT(" Initial buffer temp.: %i\n", getHeatingModeData().initialBufferTemp);
//                SYS_CONSOLE_PRINT(" Stepper setpoint:     %i\n", getHeatingModeData().stepperSetpoint);
//                SYS_CONSOLE_PRINT(" Heating setpoint:     %i\n", getHeatingSetpoint());
//                SYS_CONSOLE_PRINT(" Inlet temp. master:   %i\n", getHeatpumpReturnWaterTemperature(MASTER_HEATPUMP_IN_CASCADE));
//                SYS_CONSOLE_PRINT(" Ambient temp. master: %i\n", getExternalAmbientTemperature(MASTER_HEATPUMP_IN_CASCADE));
//                SYS_CONSOLE_PRINT(" Setpoint in HP:       %i\n", getHeatpumpHeatingSetpoint() * 10);
//                SYS_CONSOLE_PRINT(" Operating Cycle:      %i\n", getDataFromMemoryCallable(ADDRESS_CONSTANT_TEMPERATURE_OPERATION_CYCLE, MASTER_HEATPUMP_IN_CASCADE));
//                SYS_CONSOLE_PRINT(" Time counter:         %i\n\n", getSecondCounterHeatingTask());
                debugPI();
            }
            
            if (heatpumpMode == COOLING) {

                SYS_CONSOLE_PRINT("\r\nCooling:\n");
                SYS_CONSOLE_PRINT(" State:                %s\n\n", getCoolingStateToString());
                
                SYS_CONSOLE_PRINT(" Cooling setpoint:     %i\n", getCoolingSetpoint());
                SYS_CONSOLE_PRINT(" Cooling buffer:       %i\n", GetNtcTemperature(NTC_HEATING_BUFFER));
                SYS_CONSOLE_PRINT(" DIP 1 state:          %i\n", getCurrentDip1SwitchState());
            }
            
            if (heatpumpMode == HOT_WATER) {
                SYS_CONSOLE_PRINT("\r\nHOTWATER:\n");
                SYS_CONSOLE_PRINT(" State:                %s\n\n", getHotWaterStateToString());
                SYS_CONSOLE_PRINT(" Hotwater setpoint:    %i\n", getHotwaterSetpoint());
                SYS_CONSOLE_PRINT(" Hotwater buffer:      %i\n", GetNtcTemperature(NTC_HOT_WATER_BUFFER));
                SYS_CONSOLE_PRINT(" Offset setpoint:      %i\n", getHotWaterModeData().setpointHotWaterOffset);
                SYS_CONSOLE_PRINT(" Time counter:         %i\n", getSecondCounterHotwaterTask());
                SYS_CONSOLE_PRINT(" Hotwater element:     %s\n", getHotwaterElementBoolFromHotwaterMode() ? "True" : "False");              
            }

            if (heatpumpMode == HOT_WATER_HEATING) {
                SYS_CONSOLE_PRINT("\r\nHOTWATER AND HEATING:\n");
                SYS_CONSOLE_PRINT(" State:                %s\n\n", getHotwaterHeatingStateToString());
                
                SYS_CONSOLE_PRINT(" Heating setpoint:     %i\n", getHeatingSetpoint());
                SYS_CONSOLE_PRINT(" Heating buffer:       %i\n", GetNtcTemperature(NTC_HEATING_BUFFER));
                SYS_CONSOLE_PRINT(" Initial buffer temp.: %i\n", getHotWaterHeatingModeData().initialHeatingBufferTemp);
                SYS_CONSOLE_PRINT(" Inlet temp. master:   %i\n", getHeatpumpReturnWaterTemperature(MASTER_HEATPUMP_IN_CASCADE));
                SYS_CONSOLE_PRINT(" Ambient temp. master: %i\n", getExternalAmbientTemperature(MASTER_HEATPUMP_IN_CASCADE));
                SYS_CONSOLE_PRINT(" Time counter:         %i\n", getSecondCounterHeatingTask());
                SYS_CONSOLE_PRINT(" Heating element:      %s\n\n", getStatusHeatingElementHeatingBuffer() ? "True" : "False");

                SYS_CONSOLE_PRINT(" Hotwater setpoint:    %i\n", getHotwaterSetpoint());
                SYS_CONSOLE_PRINT(" Hotwater buffer:      %i\n", GetNtcTemperature(NTC_HOT_WATER_BUFFER));
                SYS_CONSOLE_PRINT(" Offset setpoint:      %i\n", getHotWaterHeatingModeData().setpointHotWaterOffset);
                SYS_CONSOLE_PRINT(" Time counter:         %i\n", getSecondCounterHotwaterTask());
                SYS_CONSOLE_PRINT(" Hotwater element:     %s\n", getStatusHeatingElementHotWaterBuffer() ? "True" : "False");
                SYS_CONSOLE_PRINT(" Hot water passive:    %s\n\n", getHotWaterHeatingModeData().hotwaterPassive ? "True" : "False");
                SYS_CONSOLE_PRINT(" Operating Cycle:      %i\n\n", getDataFromMemoryCallable(ADDRESS_CONSTANT_TEMPERATURE_OPERATION_CYCLE, MASTER_HEATPUMP_IN_CASCADE));
            }
            
            if (heatpumpMode == HOT_WATER_COOLING) {
                SYS_CONSOLE_PRINT("\r\nHOTWATER AND COOLING:\n");
                SYS_CONSOLE_PRINT(" State:                %s\n", getHotWaterCoolingStateToString());
                
                SYS_CONSOLE_PRINT(" Cooling setpoint:     %i\n", getCoolingSetpoint());
                SYS_CONSOLE_PRINT(" Cooling buffer:       %i\n\n", GetNtcTemperature(NTC_HEATING_BUFFER));

                SYS_CONSOLE_PRINT(" Hotwater setpoint:    %i\n", getHotwaterSetpoint());
                SYS_CONSOLE_PRINT(" Hotwater buffer:      %i\n", GetNtcTemperature(NTC_HOT_WATER_BUFFER));
                SYS_CONSOLE_PRINT(" Offset setpoint:      %i\n", getHotWaterCoolingModeData().setpointHotWaterOffset);
                SYS_CONSOLE_PRINT(" Time counter:         %i\n", getSecondCounterHotwaterTask());
                SYS_CONSOLE_PRINT(" Hotwater element:     %s\n", getStatusHeatingElementHotWaterBuffer() ? "True" : "False");
                SYS_CONSOLE_PRINT(" Hot water passive:    %s\n\n", getHotWaterCoolingModeData().hotwaterPassive ? "True" : "False");
                SYS_CONSOLE_PRINT(" DIP 1 state:          %i\n", getCurrentDip1SwitchState());
                SYS_CONSOLE_PRINT(" Operating Cycle:      %i\n\n", getDataFromMemoryCallable(ADDRESS_CONSTANT_TEMPERATURE_OPERATION_CYCLE, MASTER_HEATPUMP_IN_CASCADE));
            }
        }
        
    }
    return;
 }
 
 void TurnOnAuxiliaryHeatingSource(void)
 {
    if (ReadSmartEeprom16(SEEP_ADDR_HYBRID_SYSTEM_ENABLED) == true) {
        // Hybrid relay must be set
        HybridActiveRelaySet();
    } 
    else {
        // Hybrid relay must not be set
        HybridActiveRelayClear();
    }
    
    if ((ReadSmartEeprom16(SEEP_ADDR_HYBRID_SYSTEM_ENABLED) == false) || (ReadSmartEeprom16(SEEP_ADDR_HYBRID_SYSTEM_ENABLED_ON_HEATING_ELEMENT_RELAIS) == true)) {
        // Hybrid relais is not enabled OR hybrid is on heating element relay
        TurnOnHeatingElementHeatingBuffer();
    }
    else {
        TurnOffHeatingElementHeatingBuffer();
    }
 }
 
 void checkHeatingElementOrHybridStates() {
    
     // Heating element
    if ((getHeatingElementBoolFromHotwaterHeatingMode() == true) || 
            (getHeatingElementBoolFromHeatingMode() == true)     || 
            (getHeatingElementBoolFromEmergencyMode() == true)) {
        //TurnOnHeatingElementHeatingBuffer();
        TurnOnAuxiliaryHeatingSource();
    }
    else{
        TurnOffHeatingElementHeatingBuffer();
        HybridActiveRelayClear();
    }

    // Hot water element
    if ((getHotwaterElementBoolFromHotwaterHeatingMode() == true) ||
            (getHotwaterElementBoolFromHotwaterCoolingMode() == true) ||
            (getHotwaterElementBoolFromHotwaterMode() == true) ||
            (getDefrostingElementOnState() == true) ||
            (getSterilizationElementOnState() == true) ||
            (getHotWaterElementBoolFromEmergencyMode() == true)) {
        TurnOnHeatingElementHotWaterBuffer();
    } else {
        TurnOffHeatingElementHotWaterBuffer();
    }
     
    return;
 }
 
 
 
 void checkHeatpumpHeatingSetpoint() {
    if (getWriteNewSetPointHeatpumpCounter() < 10) {
        return;
    }
    
    if (app_active_mode_controllerData.setPointHeating != (getHeatpumpHeatingSetpoint() * 10)) {
            //&& (app_active_mode_controllerData.currentRunningMode == HEATING || app_active_mode_controllerData.currentRunningMode == HOT_WATER_HEATING)) {   
        // Setpoint in heatpump is not correct, send the correct one
        ChangeHeatpumpSetting(ADDRESS_HEATING_SET_TEMPERATURE, (app_active_mode_controllerData.setPointHeating / 10));
    }
    
    if (app_active_mode_controllerData.setPointCooling != (getHeatpumpCoolingSetpoint() * 10)
            && (app_active_mode_controllerData.currentRunningMode == COOLING || app_active_mode_controllerData.currentRunningMode == HOT_WATER_COOLING)) {   
        // Setpoint in heatpump is not correct, send the correct one
        ChangeHeatpumpSetting(ADDRESS_COOLING_SET_TEMPERATURE, (app_active_mode_controllerData.setPointCooling / 10));
    }
    
    setWriteNewSetPointHeatpumpCounter(0); 
 }
 
 
 
 void checkHeatpumpRunningMode() {
    if (getWriteHeatpumpRunningModeCounter() < 10) {
        return;
    }
    
    if (app_active_mode_controllerData.heatpumpRunningMode != getHeatpumpRunningMode()) {   
        // Heatpump is not on correct running mode, send correct one
        ChangeHeatpumpSetting(ADDRESS_SET_MODE, app_active_mode_controllerData.heatpumpRunningMode);
    }
    
    setWriteHeatpumpRunningModeCounter(0); 
 }
 
void checkHeatpumpTargetFrequency() {
    if (getWriteHeatpumpTargetFrequencyCounter() < 10) {
        return;
    }

    if (getHeatingModeData().compressorTargetFrequency != getHeatpumpTargetFrequency() && getHeatingModeData().compressorTargetFrequency != UINT8_MAX) {   
        // Target frequency is not correct
        ChangeHeatpumpSetting(ADDRESS_HEATING_TARGET_FREQUENCY_CONSTANT_B, getHeatingModeData().compressorTargetFrequency);
    }
    
    if (getHeatingModeData().compressorTargetFrequency != getHeatpumpMinimumFrequency() && getHeatingModeData().compressorTargetFrequency != UINT8_MAX) {   
        // Minimum frequency is not correct
        ChangeHeatpumpSetting(ADDRESS_HEATING_TARGET_FREQUENCY_LOWER_LIMIT, getHeatingModeData().compressorTargetFrequency);
    }

    setWriteHeatpumpTargetFrequencyCounter(0); 
}
 
 void checkHeatpumpForcedOff() {
    if (getWriteHeatpumpForcedOffCounter() < 10) {
        // 10 seconds not over, return
        return;
    }
    // 10 second over, reset counter and go further
    setWriteHeatpumpForcedOffCounter(0);
    
    if (app_active_mode_controllerData.heatpumpForcedOff == false){
        // No forced off, do nothing
        return;
    }
    
    if (getHeatpumpOnOff() == true){
        //Heatpump is on, but must be forced off
        ChangeHeatpumpSetting(ADDRESS_ON_OFF, SET_HEATPUMP_OFF);
    }
 }

 void checkIfSoftwareResetNeeded() {
     if (!getPowerFailStatus()){
         WriteSmartEeprom32(SEEP_ADDR_SECONDS_COUNTER_DAYS, getsystemOnCounter());
     }
     
    // If sterilization is running ACTIVELY or PASSIVLY we have to wait for it to finish
    if (getSterilisationMode() != OFF) {
        return;
    }
     
    // Reset was forced from web app
    if (ReadSmartEeprom8(SEEP_ADDR_SOFTWARE_RESET) == 1) {
        
        while(ReadSmartEeprom8(SEEP_ADDR_SOFTWARE_RESET) != 0) {
            WriteSmartEeprom8(SEEP_ADDR_SOFTWARE_RESET, 0);
        }
        
        SYS_RESET_SoftwareReset();
        
        return;
    } 
    
    // Wait till a day has passed before a reset is possible
    if (getsystemOnCounter() < SECONDS_IN_DAY) {
        return;
    }
    
    uint16_t dayCounter = (ReadSmartEeprom16(SEEP_ADDR_DAY_COUNTER_STERILIZATION) + 1);
    WriteSmartEeprom32(SEEP_ADDR_SECONDS_COUNTER_DAYS, 0);
    WriteSmartEeprom16(SEEP_ADDR_DAY_COUNTER_STERILIZATION, dayCounter);
    
    SYS_RESET_SoftwareReset();
    
    return;
 }
 
 
 
 
 void checkActivateSilentModeTimers() {
     
     if (getCheckSilentModeOnTimerCounter() < 10) {
         return;
     }
     
    /* Silent mode should always be acitve */
    if (ReadSmartEeprom8(SEEP_ADDR_SILENT_MODE) == true) {
        setCheckSilentModeOnTimerCounter(0);
        return;
    }     
     
    /* No need to set */
    if (ReadSmartEeprom8(SEEP_ADDR_USE_SILENT_MODE_TIMERS) == false) {
        setCheckSilentModeOnTimerCounter(0);
        return;
    }

    /* Timers enabled from here */

    uint16_t raw = UserParameters[ADDRESS_DISPLAY_TIME - START_ADDRESS_USER_PARAMETERS]
                                      [PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP];

    uint8_t hours   = (uint8_t)(raw >> 8);   // high byte
    uint8_t minutes = (uint8_t)(raw & 0xFF); // low byte

    uint16_t currentDisplayTimeAdjusted = (uint16_t)hours * 60u + (uint16_t)minutes;

    uint16_t start = ReadSmartEeprom16(SEEP_ADDR_START_TIME_SILENT_MODE);
    uint16_t end   = ReadSmartEeprom16(SEEP_ADDR_END_TIME_SILENT_MODE);
    
    SYS_CONSOLE_PRINT(" CURRENT TIME:           %i\n", (int16_t)currentDisplayTimeAdjusted);
    SYS_CONSOLE_PRINT(" START   TIME:           %i\n", (int16_t)start);
    SYS_CONSOLE_PRINT(" END     TIME:           %i\n", (int16_t)end);
    

    bool inWindow = false;

    if (start < end) {
        /* Same-day window: [start, end) */
        inWindow = (currentDisplayTimeAdjusted >= start) &&
                   (currentDisplayTimeAdjusted <  end);
    } else if (start > end) {
        /* Overnight window: [start, 24:00) U [0, end) */
        inWindow = (currentDisplayTimeAdjusted >= start) ||
                   (currentDisplayTimeAdjusted <  end);
    } else {
        /* start == end: treat as "no timed silent mode" (never in window) */
        inWindow = false;
    }
    SYS_CONSOLE_PRINT(" INW     TIME:           %i\n", (int16_t)inWindow);
    uint8_t currentMode = getDataFromMemoryCallable(ADDRESS_FREQUENCY_CONVERSION_MODE, MASTER_HEATPUMP_IN_CASCADE);

    if (inWindow) {
        /* Inside window: ensure silent mode = 2 */
        if (currentMode != 2) {
            ChangeHeatpumpSetting(ADDRESS_FREQUENCY_CONVERSION_MODE, 2);
        }
    } else {
        /* Outside window: ensure silent mode is not 2 */
        if (currentMode == 2) {
            ChangeHeatpumpSetting(ADDRESS_FREQUENCY_CONVERSION_MODE, 0);
        }
    }

    setCheckSilentModeOnTimerCounter(0);
    return;
 }
 
 
 
 
 bool checkForcedOffHeatpump()
 {
    // Checks every 10 seconds for the heatpumpForcedOff boolean and turns off
    // the heatpump if it is true
    checkHeatpumpForcedOff();
     
    if(((GetEvuContact() == true) && (ReadSmartEeprom8(SEEP_ADDR_EVU_CONTACT_ENABLE) == true)) ||
            ((GetThermostatContact() == false) && (ReadSmartEeprom8(SEEP_ADDR_SWITCH_HEATPUMP_ON_OFF_WITH_THERMOSTAT) == true))){
        // If EVU contact is made, with EVU enabled OR
        // Thermostat contact is made with Heatpump Switch on Thermostat setting on,
        // heatpump must be forced off
        app_active_mode_controllerData.heatpumpForcedOff = true;
        setWaitingTurningHeatpumpOn(UINT32_MAX);
        
        if (ReadSmartEeprom8(SEEP_ADDR_DISPLAY_PUMP_ON) == true){
            // Heatpump was on (display), save this and turn off on display
            WriteSmartEeprom8(SEEP_ADDR_HEATPUMP_WAS_ON_BEFORE_FORCED_OFF, true);
            WriteSmartEeprom8(SEEP_ADDR_DISPLAY_PUMP_ON, false);
        }
        
        
        if (ReadSmartEeprom8(SEEP_ADDR_HEATPUMP_WAS_ON_BEFORE_FORCED_OFF) == true){
            // Can do Circulation pump tasks, if heatpump was on before forced off
            CIRCULATION_PUMP_Tasks();
        }
        
        
        // return true: stops the active mode controller
        return true;
    }
    
    if (app_active_mode_controllerData.heatpumpForcedOff == true){
        // If HeatpumpForcedOff is true, and the EVU or thermostat doesn't block
        // the heatpump from going on, put the heatpump back on if it was on
        // before the blocking 
        
        if (ReadSmartEeprom8(SEEP_ADDR_HEATPUMP_WAS_ON_BEFORE_FORCED_OFF) == false){
            // Heatpump was off before, so keep the heatpump off and reset values
            app_active_mode_controllerData.heatpumpForcedOff = false;
            setWaitingTurningHeatpumpOn(UINT32_MAX);
            
            // return true: stops the active mode controller, because is off
            return true;
        }
        
        if (getWaitingTurningHeatpumpOn() >= 0 && getWaitingTurningHeatpumpOn() < 20) {
            // Wait 20 seconds here and check the heatpump state
            if (getHeatpumpOnOff() == SET_HEATPUMP_ON){
                // Heatpump has gone ON, reset states
                app_active_mode_controllerData.heatpumpForcedOff = false;
                setWaitingTurningHeatpumpOn(UINT32_MAX);
                
                WriteSmartEeprom8(SEEP_ADDR_HEATPUMP_WAS_ON_BEFORE_FORCED_OFF, false);
                WriteSmartEeprom8(SEEP_ADDR_DISPLAY_PUMP_ON, true);
                
                // return false: do active mode controller, because is on
                return false;
            }
            
            // Can do Circulation pump tasks
            CIRCULATION_PUMP_Tasks();
            
            // return true: stops the active mode controller
            return true;
        }
        
        // 20 seconds are over, turn ON the heatpump (again) and restart timer
        ChangeHeatpumpSetting(ADDRESS_ON_OFF, SET_HEATPUMP_ON);
        setWaitingTurningHeatpumpOn(0);
        
        // return true: stops the active mode controller
        return true;
    }
    
    if (ReadSmartEeprom8(SEEP_ADDR_DISPLAY_PUMP_ON) == false) {
        // Heatpump manually set to off on display
        CIRCULATION_PUMP_Initialize();
        
        // return true: stops the active mode controller
        return true;
    }
    
    // return false: do active mode controller
    return false;
 }
 
 
 
void callActiveModeTaskHandler() {
    switch(app_active_mode_controllerData.currentRunningMode)
    {
        
        case HEATING:{
            HEATING_MODE_Tasks();
            break;
        }
        
        
        case COOLING:{
            COOLING_MODE_Tasks();
            break;
        }
        
        
        case FLOOR_HEATING: {
            WriteSmartEeprom16(SEEP_ADDR_HEATPUMP_MODE, HEATING);
            //FLOOR_HEATING_MODE_Tasks();
            break;
        }
            
        
        case HOT_WATER:{
            HOT_WATER_MODE_Tasks();
            break;
        }
        
        
        case HOT_WATER_COOLING:{
            HOT_WATER_COOLING_MODE_Tasks();
            break;
        }
        
        
        case HOT_WATER_HEATING:{
            HOT_WATER_HEATING_MODE_Tasks();
            break;
        }
        
        
        case HOT_WATER_FLOOR_HEATING:{
            WriteSmartEeprom16(SEEP_ADDR_HEATPUMP_MODE, HOT_WATER_HEATING);
            //HOT_WATER_FLOOR_HEATING_MODE_Tasks();
            break;
        }
        
              
        default:{
            APP_ACTIVE_MODE_CONTROLLER_Initialize();
            break;
        }
    }
    return;
}




void APP_ACTIVE_MODE_CONTROLLER_Initialize ( void )
{
    // Get the previous system counter
    int32_t storedSystemCounter = ReadSmartEeprom32(SEEP_ADDR_SECONDS_COUNTER_DAYS);
    if(storedSystemCounter == UINT32_MAX) {
        storedSystemCounter = 0;
    }
    setSystemOnCounter(storedSystemCounter);
    
    // Get the previous heatpump mode
    // If the value in the eeprom is invalid we reset it it to default heating mode
    int16_t heatpumpMode = ReadSmartEeprom16(SEEP_ADDR_HEATPUMP_MODE);
    if(heatpumpMode == RESERVED || heatpumpMode > 7 || heatpumpMode < 0) {
        WriteSmartEeprom16(SEEP_ADDR_HEATPUMP_MODE, HEATING);
        heatpumpMode = HEATING;
    }
    
    app_active_mode_controllerData.currentRunningMode = heatpumpMode;
    app_active_mode_controllerData.previousRunningMode  = heatpumpMode;
    app_active_mode_controllerData.setPointHeating = 0;
    app_active_mode_controllerData.setPointCooling = 0;
    app_active_mode_controllerData.heatpumpRunningMode = 0;
    app_active_mode_controllerData.dip1SwitchCurrentState = GetDip1();
    app_active_mode_controllerData.dip1SwitchPreviousState = GetDip1();
    app_active_mode_controllerData.resetFactorySettings = false;
    app_active_mode_controllerData.heatpumpForcedOff = false;
    
    // Reset the system stuck protection counter
    //setSystemStuckProtectionCounter(0);
    
    // Start the counter for checking and writing the correct setpoint
    setWriteNewSetPointHeatpumpCounter(0); 
    setWriteHeatpumpRunningModeCounter(0); 
    setCheckSilentModeOnTimerCounter(0);
    setWriteHeatpumpTargetFrequencyCounter(0);
    
    // Initialize every active mode
    HEATING_MODE_Initialize();
    HOT_WATER_MODE_Initialize();
    COOLING_MODE_Initialize();
    FLOOR_HEATING_MODE_Initialize();
    HOT_WATER_COOLING_MODE_Initialize();
    HOT_WATER_HEATING_MODE_Initialize();
    HOT_WATER_FLOOR_HEATING_MODE_Initialize();
    
    CIRCULATION_PUMP_Initialize();
    
    CoolingActiveRelayClear();
    
    app_active_mode_controllerState = APP_ACTIVE_MODE_CONTROLLER_STATE_INIT;
}





void APP_ACTIVE_MODE_CONTROLLER_Tasks ( void )
{    
    /*
     *
     * Reset the connect to factory settings, including the heatpump itself
     *
     */
    if (getResetFactorySettings()) {
        app_active_mode_controllerData.resetFactorySettings = false;
        
        restoreEepromValuesToDefault();
        
//        APP_HEATPUMP_COMM_Initialize();
//        APP_DISPLAY_COMM_Initialize();
        APP_IN_OUTPUTS_Initialize();
        APP_I2C_TASKS_Initialize();
        APP_LOGGING_TASKS_Initialize();
//        APP_SD_CARD_TASKS_Initialize();
        APP_ACTIVE_MODE_CONTROLLER_Initialize();

        setDoFirstTimeHeatpumpCommunicationSettings(true);
        
        return;
    }
   
    /*
     * 
     * Functions as a watchdog timer, if it is not constantly reset the system is stuck
     * 
     */ 
    //if (getSystemStuckProtectionCounter() >= SYS_STUCK_TIMER_MAX_LIMIT) {
    //    SYS_RESET_SoftwareReset();
    //}    
 
        
    /*
     *
     *  Checks if the system should return to the bootloader
     *
     */
    checkIfSoftwareResetNeeded();   
    
    
    /*
     *
     *  Update all counters
     *
     */
    UpdateCounters(); 
    
    
    
    /*
     *
     * Get the most recent selected active mode from the display 
     * 
     */
    app_active_mode_controllerData.currentRunningMode = ReadSmartEeprom16(SEEP_ADDR_HEATPUMP_MODE);
    
    
    /*
     *
     * Prints debug info if needed and checks if sterilization is needed
     *
     */
    if(HeatingHotWaterTimerExpired()) {
        //CoolingActiveRelayToggle();
        // If DIP switch 4 set, print debug info
        printDebugInfo();  
        // Get Dip1 state
        setCurrentDip1SwitchState();     
        
        // Only do sterilization in HOT_WATER states
        if (app_active_mode_controllerData.currentRunningMode == HOT_WATER
                || app_active_mode_controllerData.currentRunningMode == HOT_WATER_COOLING
                || app_active_mode_controllerData.currentRunningMode == HOT_WATER_HEATING) {
            // Sterilization was either on passive mode or off, but has to be set to ACTIVE mode
            checkNeedForSterilization();
        }
        
        // Every 10 seconds the setpoint in the heatpump is checked
        checkHeatpumpHeatingSetpoint();
        // Every 10 seconds the running mode of the heatpump is checked
        checkHeatpumpRunningMode();
        // Every 10 seconds the P54 of the heatpump is checked
        checkHeatpumpTargetFrequency();
    }
    
    
    // Wait 30 seconds to receive data from heatpump before doing anything
    if(getsystemOnCounter() < 30) { return; }
    
    
    // Check if silent mode needs to be activated based on Time of day
    checkActivateSilentModeTimers();       
    
    
    
    /*
     * 
     * Check Forced off heatpump, can be:
     * - User turned off heatpump on display
     * - EVU is ON and D2 is ON (Circulation pump can be on)
     * - Setting is ON that heatpump goes on and off together with thermostat contact
     *
     */
    if (checkForcedOffHeatpump() == true){
        // Heatpump is forced off, don't go further
        TurnOffHeatingElementHeatingBuffer();
        TurnOffHeatingElementHotWaterBuffer();
        HybridActiveRelayClear();
        
        // Guard against system reset, because it is not actually stuck
        //setSystemStuckProtectionCounter(0);
        return;
    }
    
    
    /*
     * 
     * If the 3-way valve is not properly set to the right mode we need to return
     * until the switch has happened successfully
     *
     */
    if(!validateThreeWayValveStateOkay(app_active_mode_controllerData.currentRunningMode)) {
        return;
    }
    
    
    /*
     * 
     * Check if the heating and/or hot water heating elements must be on or off
     * 
     *
     */
    checkHeatingElementOrHybridStates();
    
    
    /* 
     * 
     * In the hot water states, including sterilization, defrosting must be checked.
     * If the boiler temperature drops to far, the element must be turned on.
     *
     */
    CheckDefrosting(getHotWaterHeatingModeData().state, getSterilisationMode());
    
    
    
    /*
     *
     * circulatiepomp tasks
     * 
     */
    CIRCULATION_PUMP_Tasks();



    /*
     *
     * Emergency mode
     * 
     */
    EmergencyModeTasks();     
   
    
    
    /*
     *
     * If sterilization is actively running we must return until it is either
     * finished or it switches to passive mode
     *
     */
    if(sterilisationIsActivelyRunning()) {
        return;
    }      
    
    
    
    
    switch ( app_active_mode_controllerState )
    {
        /* Application's initial state. */
        case APP_ACTIVE_MODE_CONTROLLER_STATE_INIT: {
            app_active_mode_controllerState = APP_ACTIVE_MODE_CONTROLLER_STATE_SERVICE_TASKS;
            break;
        }

        
        case APP_ACTIVE_MODE_CONTROLLER_STATE_SERVICE_TASKS: {
            
            // Check if the active mode was switched
            if (app_active_mode_controllerData.currentRunningMode != app_active_mode_controllerData.previousRunningMode) {     
                
                //storeHeatingCoolingCurveToEeprom();
                
                SYS_CONSOLE_PRINT("Switching to mode %s\r\n", getActiveModeToString(app_active_mode_controllerData.currentRunningMode));
                resetActiveModeStates();
                app_active_mode_controllerData.previousRunningMode = app_active_mode_controllerData.currentRunningMode;
            }
            
            callActiveModeTaskHandler();
            break;
        }


        default: {
            APP_ACTIVE_MODE_CONTROLLER_Initialize();
            break;
        }
        
    }
}


/*******************************************************************************
 End of File
 */
