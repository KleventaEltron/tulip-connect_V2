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

static int32_t clamp32(int32_t v, int32_t lo, int32_t hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

void powerControlHeatpump(void)
{
    // Bijhouden van I in tijd:
    static int32_t I_Hz_x1000  = 0;
    
    // Bijhouden wat laatste seconde was, zodat het maar elke seconde wordt gedaan.
    static uint32_t lastSecCounter = UINT32_MAX;  // gate: laatste verwerkte seconde-tellerwaarde
    
    static uint32_t startupSeconds = 0;              // telt op, NOOIT resetten
    heating_mode_data.visualStartupSeconds = startupSeconds;
    
    uint32_t secCounter = getSecondCounterHeatpumpPowerRegulation();
    
    if (secCounter == UINT32_MAX) {
        // Timer has no valid value
        setSecondCounterHeatpumpPowerRegulation(0);
        
        // Startfrequency on minimum
        heating_mode_data.compressorTargetFrequency = ReadSmartEeprom16(SEEP_ADDR_MINIMUM_TARGET_COMPRESSOR_FREQUENCY);
        
        // Previous temperature reset
        heating_mode_data.previousTemp_mC = INT32_MAX;
        I_Hz_x1000  = 0;
        
        lastSecCounter = 0;
        startupSeconds = 0;
        
        return;
    }
    
    if (secCounter == lastSecCounter) {
        return; // dezelfde seconde: niets doen
    }
    lastSecCounter = secCounter; // nieuwe seconde: door met de regeling
    
    startupSeconds++; // telt echte ?seconden sinds start? (zolang je functie blijft lopen)
    heating_mode_data.visualStartupSeconds = startupSeconds;
    
    if (startupSeconds < 180) {
        // Eerste 3 minuten nog niks doen, warmtepomp blijft dan ook op 45 Hz draaien.
        return; // dezelfde seconde: niets doen
    }
    
//    if ((secCounter % 60) != 0) {
//        // Eens per minuut doorlopen
//        return; // dezelfde seconde: niets doen
//    }
    
    int32_t riseTemp_mC = (int32_t)ReadSmartEeprom16(SEEP_ADDR_HEATING_RISE_TEMP_IN_GIVEN_TIME) * 100;  // bv 1000 Standaard 10 (1.0 graden)
    int32_t riseTime_min = (int32_t)ReadSmartEeprom16(SEEP_ADDR_HEATING_TIME_CONSTANT_SEC) / 60; // bv 100 Standaard 6000 (100 minuten)

    if (riseTemp_mC <= 0 || riseTime_min <= 0) {
        // Invalid settings
        return;
    }
    
    // doelhelling (mC/min)
    // Met onze instellingen zou dit (1000/100 = 10 zijn), dus 10 mC/min = 0,01 C/min
    int32_t slopeTarget_mC_min = (int32_t)((int64_t)riseTemp_mC / riseTime_min); // Wordt 10 (0.01 graad per minuut ivm milliCelcius)
    
    if (slopeTarget_mC_min <= 0) {
        // Invalid
        slopeTarget_mC_min = 1;
    }
    
    // update-interval (sec) om 0.2°C te halen: dt = 60 * step / slope
    // Bereken hoelang nodig is met huidige instellingen om 0,2 graden te stijgen, in ons geval is dit:
    // 60 * 200 / 10 = 12000 / 10 = 1200 seconden (20 minuten)
    #define TEMP_STEP_mC          200   // 0.2°C
    uint32_t updateInterval_sec = (uint32_t)((int64_t)60 * TEMP_STEP_mC / slopeTarget_mC_min);
    
    if (updateInterval_sec < 60) {
        updateInterval_sec = 60; // minimaal 1 min
    }
    
    // lees temperatuur
    int32_t tempNow_mC = GetNtcTemperature(NTC_HEATING_BUFFER) * 100;
    
    if (heating_mode_data.previousTemp_mC == INT32_MAX) {
        heating_mode_data.previousTemp_mC = tempNow_mC;
        
        heating_mode_data.visualTempNow_mC           = tempNow_mC;
        heating_mode_data.visualTempPrev_mC          = tempNow_mC;
        heating_mode_data.visualdT_mC                = 0;
        heating_mode_data.visualdt_sec               = 0;
        heating_mode_data.visualUpdateInterval_sec   = updateInterval_sec;
        heating_mode_data.visualSlopeTarget_mC_min   = slopeTarget_mC_min;
        heating_mode_data.visualEventFlags           = 0;
        heating_mode_data.visualSlopeMeas_mC_min     = 0;
        heating_mode_data.visualError_mC_min         = 0;
        heating_mode_data.visualP_Hz_x1000           = 0;
        heating_mode_data.visualI_Hz_x1000           = I_Hz_x1000;
        heating_mode_data.visualU_Hz                 = 0;
        heating_mode_data.visualFreqBefore           = heating_mode_data.compressorTargetFrequency;
        heating_mode_data.visualFreqAfter            = heating_mode_data.compressorTargetFrequency;
        heating_mode_data.visualStartupSeconds       = startupSeconds;
        
        return;
    }
    
    uint32_t dt_sec = getSecondCounterHeatpumpPowerRegulation();
    int32_t  previousTemp_mC = heating_mode_data.previousTemp_mC;
    int32_t dT_mC = tempNow_mC - previousTemp_mC;
    
    // wacht tot tijd voorbij is, OF 0.2°C gehaald is, OF daling
    #define TEMP_DROP_mC          400    // 0.2°C daling => eerder bijsturen
    uint8_t timeReached = (dt_sec >= updateInterval_sec);
    uint8_t stepReached = (dT_mC >= TEMP_STEP_mC);
    uint8_t tempDropped = (dT_mC <= -(int32_t)TEMP_DROP_mC);
    
    // --- VISUALS DIRECT NA HET METEN (ook als we straks returnen) ---
    heating_mode_data.visualTempNow_mC         = tempNow_mC;
    heating_mode_data.visualTempPrev_mC        = previousTemp_mC;
    heating_mode_data.visualdT_mC              = dT_mC;
    heating_mode_data.visualdt_sec             = dt_sec;
    heating_mode_data.visualUpdateInterval_sec = updateInterval_sec;
    heating_mode_data.visualSlopeTarget_mC_min = slopeTarget_mC_min;
    heating_mode_data.visualEventFlags =
        (timeReached ? 0x01 : 0) |
        (stepReached ? 0x02 : 0) |
        (tempDropped ? 0x04 : 0);

    // zolang we nog niet regelen, kunnen we slope/error alvast tonen (indien dt>0)
    if (dt_sec > 0) {
        int32_t slopeMeas_preview_mC_min = (int32_t)((int64_t)dT_mC * 60 / (int64_t)dt_sec);
        heating_mode_data.visualSlopeMeas_mC_min = slopeMeas_preview_mC_min;
        heating_mode_data.visualError_mC_min     = slopeTarget_mC_min - slopeMeas_preview_mC_min;
    } else {
        heating_mode_data.visualSlopeMeas_mC_min = 0;
        heating_mode_data.visualError_mC_min     = 0;
    }

    // PI outputs nog niet bekend als we nog niet regelen: zet ze alvast ?neutraal?
    heating_mode_data.visualP_Hz_x1000 = 0;
    heating_mode_data.visualI_Hz_x1000 = I_Hz_x1000;
    heating_mode_data.visualU_Hz       = 0;
    heating_mode_data.visualFreqBefore = heating_mode_data.compressorTargetFrequency;
    heating_mode_data.visualFreqAfter  = heating_mode_data.compressorTargetFrequency;
    // --- einde visuals na meten ---
    
    
    // wacht tot tijd voorbij is, OF 0.2°C gehaald is, OF daling
    if (!timeReached && !stepReached && !tempDropped) {
        return;
    }
    
    // beveiliging tegen dt=0 / hele kleine dt
    if (dt_sec < 60) {  // aan te passen, maar dit voorkomt extreem grote slopes
        return;
    }
    
    //debugPI();
    SYS_CONSOLE_PRINT("\n========== BIJSTUREN ==========\n");
    

    // gemeten helling (mC/min)
    int32_t slopeMeas_mC_min = (int32_t)((int64_t)dT_mC * 60 / (int64_t)dt_sec);

    // PI op helling-error
    int32_t error = slopeTarget_mC_min - slopeMeas_mC_min;

//    int32_t Kp = (int32_t)ReadSmartEeprom16(SEEP_ADDR_PI_KP_X1000); // Hz x1000 per (mC/min)
//    int32_t Ki = (int32_t)ReadSmartEeprom16(SEEP_ADDR_PI_KI_X1000); // Hz x1000 per (mC/min)/min
    int32_t Kp = 200; // Hz x1000 per (mC/min)
    int32_t Ki = 0; // Hz x1000 per (mC/min)/min

    int32_t P = (int32_t)((int64_t)Kp * error);
    I_Hz_x1000 += (int32_t)((int64_t)Ki * error * (int64_t)dt_sec / 60);

    #define INTEGRAL_CLAMP_X1000  20000 // +/- 20 Hz (x1000)
    I_Hz_x1000 = clamp32(I_Hz_x1000, -INTEGRAL_CLAMP_X1000, INTEGRAL_CLAMP_X1000);

    int32_t u_Hz = (P + I_Hz_x1000) / 1000;
    
    #define MAX_FREQ_STEP_HZ  3  // bv max 3 Hz per regelactie

    if (u_Hz >  MAX_FREQ_STEP_HZ) u_Hz =  MAX_FREQ_STEP_HZ;
    if (u_Hz < -MAX_FREQ_STEP_HZ) u_Hz = -MAX_FREQ_STEP_HZ;

    // frequentie toepassen + clamp
    uint16_t freqBefore = heating_mode_data.compressorTargetFrequency;
    int32_t newFreq = (int32_t)heating_mode_data.compressorTargetFrequency + u_Hz;

    int32_t minF = (int32_t)ReadSmartEeprom16(SEEP_ADDR_MINIMUM_TARGET_COMPRESSOR_FREQUENCY);
    int32_t maxF = (int32_t)ReadSmartEeprom16(SEEP_ADDR_MAXIMUM_TARGET_COMPRESSOR_FREQUENCY);

    if (newFreq < minF) newFreq = minF;
    if (newFreq > maxF) newFreq = maxF;

    heating_mode_data.compressorTargetFrequency = (uint16_t)newFreq;
    
    // --- VISUALS NA REGELACTIE (overschrijven met echte PI waarden) ---
    heating_mode_data.visualSlopeMeas_mC_min = slopeMeas_mC_min;
    heating_mode_data.visualError_mC_min     = error;
    heating_mode_data.visualP_Hz_x1000       = P;
    heating_mode_data.visualI_Hz_x1000       = I_Hz_x1000;
    heating_mode_data.visualU_Hz             = u_Hz;
    heating_mode_data.visualFreqBefore       = freqBefore;
    heating_mode_data.visualFreqAfter        = (uint16_t)newFreq;

    // reset interval
    heating_mode_data.previousTemp_mC = tempNow_mC;
    setSecondCounterHeatpumpPowerRegulation(0);
    lastSecCounter = 0;
}

void resetPowerControlVariables() 
{
    setSecondCounterHeatpumpPowerRegulation(UINT32_MAX);
    
    heating_mode_data.compressorTargetFrequency = UINT8_MAX; 
    heating_mode_data.previousTemp_mC = INT32_MAX;
    heating_mode_data.previousDt_sec = INT32_MAX;
    
    heating_mode_data.visualTempNow_mC  = 0;
    heating_mode_data.visualTempPrev_mC = 0;
    heating_mode_data.visualdT_mC       = 0;
    heating_mode_data.visualdt_sec      = 0;

    heating_mode_data.visualUpdateInterval_sec = 0;
    heating_mode_data.visualEventFlags         = 0;

    heating_mode_data.visualSlopeTarget_mC_min = 0;
    heating_mode_data.visualSlopeMeas_mC_min   = 0;
    heating_mode_data.visualError_mC_min       = 0;

    heating_mode_data.visualP_Hz_x1000 = 0;
    heating_mode_data.visualI_Hz_x1000 = 0;
    heating_mode_data.visualU_Hz       = 0;

    heating_mode_data.visualFreqBefore = heating_mode_data.compressorTargetFrequency;
    heating_mode_data.visualFreqAfter  = heating_mode_data.compressorTargetFrequency;
    
    heating_mode_data.visualStartupSeconds = 0;
}

void HEATING_MODE_Initialize ( void )
{
    setSecondCounterHeatingTask(UINT32_MAX);
    heating_mode_data.initialBufferTemp = TEMPERATURE_ALARM_VALUE;
    //TurnOffHeatingElementHeatingBuffer();
    heating_mode_data.HeatingElementOn = false;
    heating_mode_data.stepperSetpoint = TEMPERATURE_ALARM_VALUE;
    
    heating_mode_data.heatingCurveSet = false;
    
    resetPowerControlVariables();
    
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
        
        case(HEATING_RUNNING_WITH_CIRCULATION_PUMP_OFF): {
            return "4, Running with circulation pump off";
            break;
        }
        
        case(HEATING_RUNNING_WITH_ELEMENT_ON_AND_CIRCULATION_PUMP_OFF): {
            return "5, Running with element on and circulation pump off";
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
    
    switch ( heating_mode_data.state )
    {
        // 0
        case HEATING_INITIALIZE:{
            
            //TurnOffHeatingElementHeatingBuffer();
            heating_mode_data.HeatingElementOn = false;
            heating_mode_data.initialBufferTemp = TEMPERATURE_ALARM_VALUE; 
            heating_mode_data.stepperSetpoint = TEMPERATURE_ALARM_VALUE;
            setSecondCounterHeatingTask(UINT32_MAX);
            resetPowerControlVariables();
            
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
        
        // 1
        case HEATING_IDLE:{                
            if (getActiveCompressorsMask() != 0){
                // Compressor is running
                setSecondCounterHeatingTask(0);
                resetPowerControlVariables();
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
        
        // 2
        case HEATING_RUNNING:{
            
            if ((getActiveCompressorsMask() == 0) && (getDefrostingActiveMask() == 0)){
                // Compressor is not running and is also not in defrosting
                //TurnOffHeatingElementHeatingBuffer();
                heating_mode_data.HeatingElementOn = false;
                heating_mode_data.initialBufferTemp = TEMPERATURE_ALARM_VALUE;
                setSecondCounterHeatingTask(UINT32_MAX);
                resetPowerControlVariables();
                
                if(regulateOnTempSensorInBufferHeating && !checkIfDefrostingActive()) {
                    //ChangeHeatpumpSetting(ADDRESS_CONSTANT_TEMPERATURE_OPERATION_CYCLE, 240);
                    setActiveModeControllerPumpOffDueToDipSwitch1(true);
                }

                heating_mode_data.state = HEATING_IDLE;
                break;
            }
            
            if (heatingBufferTemperature < heating_mode_data.initialBufferTemp) {
                // Adjust the reference temp to the lowest measured temperature 
                heating_mode_data.initialBufferTemp = heatingBufferTemperature;
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
                //heating_mode_data.initialBufferTemp = TEMPERATURE_ALARM_VALUE;
                heating_mode_data.HeatingElementOn = true;
                
                if (checkIfBufferIsWithinSetpointMinusDelta(heatingBufferTemperature) == true) {
                    // Buffer temp is within setpoint - delta, must rise 1 degree within the given time next phase
                    heating_mode_data.initialBufferTemp = heatingBufferTemperature;
                }
                else {
                    // Buffer temp is not within setpoint - delta must rise untill the setpoint - delta next phase
                    heating_mode_data.initialBufferTemp = TEMPERATURE_ALARM_VALUE;
                }
                
                heating_mode_data.state = HEATING_RUNNING_WITH_ELEMENT_ON;
                break;
            }
            
            powerControlHeatpump();
            
            break;
        }
        
        // 3
        case HEATING_RUNNING_WITH_ELEMENT_ON:{
            
            if ((getActiveCompressorsMask() == 0) && (getDefrostingActiveMask() == 0)){
                // Compressor is not running and is also not in defrosting
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
            
            if (getSecondCounterHeatingTask() >= ReadSmartEeprom16(SEEP_ADDR_HEATING_MODE_MAX_TIME_WITH_ELEMENT_ON)){
                // Temperature not rised a set temperature in a set time
                setSecondCounterHeatingTask(0);
                heating_mode_data.HeatingElementOn = false;
                
                if (checkIfBufferIsWithinSetpointMinusDelta(heatingBufferTemperature) == true) {
                    // Buffer temp is within setpoint - delta, must rise 1 degree within the given time next phase
                    heating_mode_data.initialBufferTemp = heatingBufferTemperature;
                }
                else {
                    // Buffer temp is not within setpoint - delta must rise untill the setpoint - delta next phase
                    heating_mode_data.initialBufferTemp = TEMPERATURE_ALARM_VALUE;
                }
                
                heating_mode_data.state = HEATING_RUNNING_WITH_CIRCULATION_PUMP_OFF;
                break;
            }
            
            if (heating_mode_data.initialBufferTemp != TEMPERATURE_ALARM_VALUE) {
                // Has a valid value, so must rise 1 degree now
                if (heatingBufferTemperature < heating_mode_data.initialBufferTemp) {
                    // Adjust the reference temp to the lowest measured temperature 
                    heating_mode_data.initialBufferTemp = heatingBufferTemperature;
                }
                
                if (heatingBufferTemperature >= heating_mode_data.initialBufferTemp + ReadSmartEeprom16(SEEP_ADDR_HEATING_RISE_TEMP_IN_GIVEN_TIME)){
                    // Temperature rised a set temperature in a set time
                    setSecondCounterHeatingTask(0);
                    heating_mode_data.HeatingElementOn = false;
                    heating_mode_data.initialBufferTemp = heatingBufferTemperature;
                    //heating_mode_data.stepperSetpoint = getHeatingSetpoint();

                    heating_mode_data.state = HEATING_RUNNING;
                    break;
                }
            }
            else {
                // Has not a valid value, must rise untill the setpoint-delta
                if (checkIfBufferIsWithinSetpointMinusDelta(heatingBufferTemperature) == true) {
                    // Buffer temp is within setpoint - delta, go back to running
                    setSecondCounterHeatingTask(0);
                    heating_mode_data.HeatingElementOn = false;
                    heating_mode_data.initialBufferTemp = heatingBufferTemperature;
                    //heating_mode_data.stepperSetpoint = getHeatingSetpoint();

                    heating_mode_data.state = HEATING_RUNNING;
                    break;
                }
            }
            
            break;
        }
        
        // 4
        case HEATING_RUNNING_WITH_CIRCULATION_PUMP_OFF:{
            
            if ((getActiveCompressorsMask() == 0) && (getDefrostingActiveMask() == 0)){
                // Compressor is not running and is also not in defrosting
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
            
            if (getSecondCounterHeatingTask() >= ReadSmartEeprom16(SEEP_ADDR_HEATING_MODE_MAX_TIME_WITH_CIRCULATION_PUMP_OFF)){
                // Temperature not rised a set temperature in a set time
                setSecondCounterHeatingTask(0);
                heating_mode_data.HeatingElementOn = true;
                
                if (checkIfBufferIsWithinSetpointMinusDelta(heatingBufferTemperature) == true) {
                    // Buffer temp is within setpoint - delta, must rise 1 degree within the given time next phase
                    heating_mode_data.initialBufferTemp = heatingBufferTemperature;
                }
                else {
                    // Buffer temp is not within setpoint - delta must rise untill the setpoint - delta next phase
                    heating_mode_data.initialBufferTemp = TEMPERATURE_ALARM_VALUE;
                }
                
                heating_mode_data.state = HEATING_RUNNING_WITH_ELEMENT_ON_AND_CIRCULATION_PUMP_OFF;
                break;
            }
            
            if (heating_mode_data.initialBufferTemp != TEMPERATURE_ALARM_VALUE) {
                // Has a valid value, so must rise 1 degree now
                if (heatingBufferTemperature < heating_mode_data.initialBufferTemp) {
                    // Adjust the reference temp to the lowest measured temperature 
                    heating_mode_data.initialBufferTemp = heatingBufferTemperature;
                }
                
                if (heatingBufferTemperature >= heating_mode_data.initialBufferTemp + ReadSmartEeprom16(SEEP_ADDR_HEATING_RISE_TEMP_IN_GIVEN_TIME)){
                    // Temperature rised a set temperature in a set time
                    setSecondCounterHeatingTask(0);
                    heating_mode_data.HeatingElementOn = false;
                    heating_mode_data.initialBufferTemp = heatingBufferTemperature;
                    //heating_mode_data.stepperSetpoint = getHeatingSetpoint();

                    heating_mode_data.state = HEATING_RUNNING;
                    break;
                }
            }
            else {
                // Has not a valid value, must rise untill the setpoint-delta
                if (checkIfBufferIsWithinSetpointMinusDelta(heatingBufferTemperature) == true) {
                    // Buffer temp is within setpoint - delta, go back to running
                    setSecondCounterHeatingTask(0);
                    heating_mode_data.HeatingElementOn = false;
                    heating_mode_data.initialBufferTemp = heatingBufferTemperature;
                    //heating_mode_data.stepperSetpoint = getHeatingSetpoint();

                    heating_mode_data.state = HEATING_RUNNING;
                    break;
                }
            }

            break;
        }
        
        // 5
        case HEATING_RUNNING_WITH_ELEMENT_ON_AND_CIRCULATION_PUMP_OFF:{
            
            if ((getActiveCompressorsMask() == 0) && (getDefrostingActiveMask() == 0)){
                // Compressor is not running and is also not in defrosting
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
            
            if (getSecondCounterHeatingTask() >= ReadSmartEeprom16(SEEP_ADDR_HEATING_MODE_MAX_TIME_WITH_ELEMENT_ON_AND_CIRCULATION_PUMP_OFF)){
                // Temperature not rised a set temperature in a set time
                setSecondCounterHeatingTask(0);
                heating_mode_data.HeatingElementOn = false;
                heating_mode_data.initialBufferTemp = heatingBufferTemperature;
                
                heating_mode_data.state = HEATING_RUNNING;
                break;
            }
            
            if (heating_mode_data.initialBufferTemp != TEMPERATURE_ALARM_VALUE) {
                // Has a valid value, so must rise 1 degree now
                if (heatingBufferTemperature < heating_mode_data.initialBufferTemp) {
                    // Adjust the reference temp to the lowest measured temperature 
                    heating_mode_data.initialBufferTemp = heatingBufferTemperature;
                }
                
                if (heatingBufferTemperature >= heating_mode_data.initialBufferTemp + ReadSmartEeprom16(SEEP_ADDR_HEATING_RISE_TEMP_IN_GIVEN_TIME)){
                    // Temperature rised a set temperature in a set time
                    setSecondCounterHeatingTask(0);
                    heating_mode_data.HeatingElementOn = false;
                    heating_mode_data.initialBufferTemp = heatingBufferTemperature;
                    //heating_mode_data.stepperSetpoint = getHeatingSetpoint();

                    heating_mode_data.state = HEATING_RUNNING;
                    break;
                }
            }
            else {
                // Has not a valid value, must rise untill the setpoint-delta
                if (checkIfBufferIsWithinSetpointMinusDelta(heatingBufferTemperature) == true) {
                    // Buffer temp is within setpoint - delta, go back to running
                    setSecondCounterHeatingTask(0);
                    heating_mode_data.HeatingElementOn = false;
                    heating_mode_data.initialBufferTemp = heatingBufferTemperature;
                    //heating_mode_data.stepperSetpoint = getHeatingSetpoint();

                    heating_mode_data.state = HEATING_RUNNING;
                    break;
                }
            }

            break;
        }
        
        // Others:
        default:{
            HEATING_MODE_Initialize();
            break;
        }
    }
    
}


