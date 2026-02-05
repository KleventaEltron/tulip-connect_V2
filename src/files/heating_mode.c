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

// vind segment-index i zodat adc tussen adc[i] en adc[i+1] valt
//static int find_ntc_segment(int16_t adc)
//{
//    const int N_REAL = 29; // 0..28 geldig (29 punten => 28 segmenten)
//    if (adc >= ntc_10k_tabel[0][0]) return 0;
//    if (adc <= ntc_10k_tabel[0][N_REAL - 1]) return N_REAL - 2;
//
//    for (int i = 0; i < N_REAL - 1; i++)
//    {
//        int16_t a0 = ntc_10k_tabel[0][i];
//        int16_t a1 = ntc_10k_tabel[0][i + 1];
//        if (adc <= a0 && adc >= a1) return i;
//    }
//    return N_REAL - 2;
//}

// target_rise_temp100: °C×100 per 3 min (0,03°C => 3)
//static int16_t target_dADC_from_verschil(int16_t adc_now, int16_t target_rise_temp100)
//{
//    int seg = ntc_find_segment_from_adc(adc_now);
//
//    uint8_t dA_5C = ntc_verschil[seg];
//    if (dA_5C == 255) dA_5C = 1; // safety
//
//    // target_dADC = target_temp100 * dA_5C / 500  (want 5°C = 500 in temp100)
//    int32_t num = (int32_t)target_rise_temp100 * (int32_t)dA_5C;
//
//    // afronden naar dichtstbijzijnde
//    int32_t t = (num + 250) / 500;
//
//    if (t < 1) t = 1;
//    if (t > 2000) t = 2000;
//    return (int16_t)t;
//}

int16_t ntc_get_target_dadc(uint16_t adc, uint16_t target_temp100)
{
    uint8_t counts_5C = ntc_get_counts_per_5C_from_adc(adc);

    // 5°C = 500 in temp100
    int32_t num = (int32_t)target_temp100 * counts_5C;

    int32_t result = (num + 250) / 500;   // afronden

    if (result < 1){
        //
        result = 1;
    }
    
    if (result > 2000) {
        // 
        result = 2000;
    }

    return (int16_t)result;
}

void powerControlHeatpump(void)
{
    uint16_t currentAdcValue  = GetAdcValue(NTC_HEATING_BUFFER);
    uint16_t previousAdcValue = heating_mode_data.previousAdcValue; 
    heating_mode_data.previousAdcValue = currentAdcValue;
    
    // als 3 minuten (INGESTELDE TIJD) niet verstreken, hier returnend, en kijken wat hierboven staat wat dan hieronder moet
    
    // Bij aanvang is er natuurlijk geen previous ADC value, dit moet worden dedetecteerd en opgevangen zoals ongeveer hieronder?
    
//    if (heating_mode_data.previousValid == false) {
//        // No target known
//        heating_mode_data.previousValid = true;
//        heating_mode_data.previousAdc = currentAdcValueBufferTemp;
//        //return heating_mode_data.targetFrequency;
//        return;
//    }
    
    //int16_t adc_now  = adc_buffer;
   
    // opwarmen => ADC daalt => dADC positief
    int16_t deltaAdc = (int16_t)(previousAdcValue - currentAdcValue);

    int16_t deltaAdcTarget = ntc_get_target_dadc(currentAdcValue, ReadSmartEeprom16(SEEP_ADDR_TARGET_RISE_TEMP_100));

    // ratio promille: 1000 * meas / target
    int32_t ratioPermille = (int32_t)1000 * (int32_t)deltaAdc / (int32_t)deltaAdcTarget;

    uint16_t minimumTargetFreq = ReadSmartEeprom16(SEEP_ADDR_MINIMUM_TARGET_COMPRESSOR_FREQUENCY);
    uint16_t maximumTargetFreq = ReadSmartEeprom16(SEEP_ADDR_MAXIMUM_TARGET_COMPRESSOR_FREQUENCY);
    
    if (ratioPermille < 700) {
        // Ratio is lower than big permille
        heating_mode_data.targetFrequency += ReadSmartEeprom16(SEEP_ADDR_BIG_INCREASE_STEP_HZ);
    } else if (ratioPermille < 850) {
        // Ratio is lower than small permille
        heating_mode_data.targetFrequency += ReadSmartEeprom16(SEEP_ADDR_SMALL_INCREASE_STEP_HZ);
    } else if (ratioPermille <= 1150) {
        // Do nothing
        
    } else if (ratioPermille <= 1300) {
        // Ratio is lower than 
        heating_mode_data.targetFrequency -= ReadSmartEeprom16(SEEP_ADDR_BIG_DECREASE_STEP_HZ);
    } else {
        // else
        heating_mode_data.targetFrequency -= ReadSmartEeprom16(SEEP_ADDR_SMALL_DECREASE_STEP_HZ);
    }
    
    if (heating_mode_data.targetFrequency < minimumTargetFreq) {
        // Minimum clamp
        heating_mode_data.targetFrequency = minimumTargetFreq;
    }
    
    if (heating_mode_data.targetFrequency > maximumTargetFreq) {
        // Maximum clamp
        heating_mode_data.targetFrequency = maximumTargetFreq;
    }
    
    //return heating_mode_data.targetFrequency;
    return;
}

void HEATING_MODE_Initialize ( void )
{
    setSecondCounterHeatingTask(UINT32_MAX);
    heating_mode_data.initialBufferTemp = TEMPERATURE_ALARM_VALUE;
    //TurnOffHeatingElementHeatingBuffer();
    heating_mode_data.HeatingElementOn = false;
    heating_mode_data.stepperSetpoint = TEMPERATURE_ALARM_VALUE;
    
    heating_mode_data.heatingCurveSet = false;
    
    heating_mode_data.targetFrequency = UINT8_MAX;
    heating_mode_data.previousAdcValue = UINT16_MAX;
    //heating_mode_data.previousValid = false;
    
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
    
    powerControlHeatpump();
    
    switch ( heating_mode_data.state )
    {
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
            
            if (heatingBufferTemperature >= heating_mode_data.initialBufferTemp + ReadSmartEeprom16(SEEP_ADDR_HEATING_RISE_TEMP_IN_GIVEN_TIME)){
                // Temperature rised a set temperature in a set time
                setSecondCounterHeatingTask(0);
                heating_mode_data.initialBufferTemp = heatingBufferTemperature;
                break;
            }
            
            if (getSecondCounterHeatingTask() >= ReadSmartEeprom16(SEEP_ADDR_HEATING_TIME_CONSTANT_SEC)){
                // Temperature not rised a set temperature in a set time
                setSecondCounterHeatingTask(0);
                heating_mode_data.initialBufferTemp = heatingBufferTemperature;
                //TurnOnHeatingElementHeatingBuffer();
                heating_mode_data.HeatingElementOn = true;
                
                heating_mode_data.state = HEATING_RUNNING_WITH_ELEMENT_ON;
                break;
            }
            
            break;
        }
        
        case HEATING_RUNNING_WITH_ELEMENT_ON:{
            
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
            
            if (heatingBufferTemperature >= heating_mode_data.initialBufferTemp + ReadSmartEeprom16(SEEP_ADDR_HEATING_RISE_TEMP_IN_GIVEN_TIME)){
                // Temperature rised a set temperature in a set time
                setSecondCounterHeatingTask(0);
                heating_mode_data.initialBufferTemp = heatingBufferTemperature;
                break;
            }
            
            if (getSecondCounterHeatingTask() >= ReadSmartEeprom16(SEEP_ADDR_HEATING_TIME_CONSTANT_SEC)){
                // Temperature not rised a set temperature in a set time
                setSecondCounterHeatingTask(0);
                heating_mode_data.initialBufferTemp = heatingBufferTemperature;
                //TurnOffHeatingElementHeatingBuffer();
                heating_mode_data.HeatingElementOn = false;
                
                heating_mode_data.state = HEATING_RUNNING;
                break;
            }
            
            break;
        }
        
        
        
        default:{
            HEATING_MODE_Initialize();
            break;
        }
    }
    
}


