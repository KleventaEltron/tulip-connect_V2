#include "heatpump_pi_adapter.h"
#include <stddef.h>                    
#include <stdbool.h>                   
#include <stdlib.h>                   
#include <string.h>
#include <stdio.h>

#include "../config/default/definitions.h"
#include "eeprom.h"
#include "modbus/heatpump_parameters.h"
#include "ntc.h"

//#include "definitions.h"    
//#include "user.h"
//#include "eeprom.h"
//#include "time_counters.h"
//#include "ntc.h"
//#include "modbus\heatpump_parameters.h"
//#include "modbus/display.h"

// Always print using SYS_CONSOLE_PRINT (as requested)
//extern void SYS_CONSOLE_PRINT(const char *fmt, ...);

// ---------- YOUR PROJECT FUNCTIONS (declare or include headers) ----------
//extern uint32_t getSecondCounterHeatpumpPowerRegulation(void);
//extern void     setSecondCounterHeatpumpPowerRegulation(uint32_t v);

//extern int32_t  GetNtcTemperature(int ntc_id);
//extern int32_t  getCorrectHeatingSetpoint(void);

//extern int16_t  ReadSmartEeprom16(uint16_t addr);

// ---------- YOUR PROJECT CONSTANTS (FIX THESE ADDRESSES / IDs) ----------
//#ifndef NTC_HEATING_BUFFER
//#define NTC_HEATING_BUFFER 0
//#endif
//
//#ifndef SEEP_ADDR_MINIMUM_TARGET_COMPRESSOR_FREQUENCY
//#define SEEP_ADDR_MINIMUM_TARGET_COMPRESSOR_FREQUENCY   0x0000
//#endif
//
//#ifndef SEEP_ADDR_MAXIMUM_TARGET_COMPRESSOR_FREQUENCY
//#define SEEP_ADDR_MAXIMUM_TARGET_COMPRESSOR_FREQUENCY   0x0000
//#endif
//
//#ifndef SEEP_ADDR_HEATING_RISE_TEMP_IN_GIVEN_TIME
//#define SEEP_ADDR_HEATING_RISE_TEMP_IN_GIVEN_TIME       0x0000
//#endif
//
//#ifndef SEEP_ADDR_HEATING_TIME_CONSTANT_SEC
//#define SEEP_ADDR_HEATING_TIME_CONSTANT_SEC             0x0000
//#endif

#define MINF_ENTER_NEAR_mC   2000  // 2.0°C
#define MINF_EXIT_NEAR_mC    2500  // 2.5°C

// =====================================================================
// Single shared instance (so wrappers exist only once)
// =====================================================================
static HeatpumpPI g_pi;
static HeatpumpPI_Visuals g_vis;
static uint8_t g_inited = 0;

// =====================================================================
// Wrappers (ONLY HERE)
// =====================================================================
static uint32_t hp_get_sec_counter(void)
{
    return getSecondCounterHeatpumpPowerRegulation();
}

static void hp_set_sec_counter(uint32_t v)
{
    setSecondCounterHeatpumpPowerRegulation(v);
}

static int32_t hp_read_temp_mC(void)
{
    // Your existing code used: GetNtcTemperature(...) * 100 => mC
    // Keep consistent here.
    return GetNtcTemperature(NTC_HEATING_BUFFER) * 100;
}

static int32_t hp_read_setpoint_mC(void)
{
    return getCorrectHeatingSetpoint() * 100;
}

uint8_t s_nearMinActive = 0;
static uint16_t hp_read_min_freq_hz(void)
{
    uint16_t eepromMin = 30;
        //(uint16_t)ReadSmartEeprom16(SEEP_ADDR_MINIMUM_TARGET_COMPRESSOR_FREQUENCY);
    return eepromMin;

//    int32_t temp_mC = hp_read_temp_mC();
//    int32_t sp_mC   = hp_read_setpoint_mC();
//    int32_t dist_mC = sp_mC - temp_mC;
//    if (dist_mC < 0) dist_mC = -dist_mC;
//
//    if (!s_nearMinActive) {
//        if (dist_mC <= MINF_ENTER_NEAR_mC) s_nearMinActive = 1;
//    } else {
//        if (dist_mC >= MINF_EXIT_NEAR_mC) s_nearMinActive = 0;
//    }
//
//    if (s_nearMinActive) {
//        return eepromMin; // allow 30Hz near setpoint
//    }
//
//    // far away: minimum 40Hz
//    return (eepromMin < 40) ? 30 : eepromMin;
}

static uint16_t hp_read_max_freq_hz(void)
{
    uint16_t eepromMax = 100;
    return eepromMax;//(uint16_t)ReadSmartEeprom16(SEEP_ADDR_MAXIMUM_TARGET_COMPRESSOR_FREQUENCY);
}

static bool hp_read_ramp_settings(int32_t *riseTemp_mC, int32_t *riseTime_min)
{
    // Your existing scheme:
    // riseTemp from EEPROM *100 (mC)
    // riseTime from EEPROM seconds /60 (min)
    int32_t t_mC  = (int32_t)ReadSmartEeprom16(SEEP_ADDR_HEATING_RISE_TEMP_IN_GIVEN_TIME) * 100;
    int32_t t_min = (int32_t)ReadSmartEeprom16(SEEP_ADDR_HEATING_TIME_CONSTANT_SEC) / 60;

    if (t_mC <= 0 || t_min <= 0) return false;

    *riseTemp_mC  = t_mC;
    *riseTime_min = t_min;
    return true;
}

// =====================================================================
// Adapter API
// =====================================================================
void HPPI_InitOnce(void)
{
    if (g_inited) return;
    g_inited = 1;

    HeatpumpPI_Config cfg = {
        .get_sec_counter    = hp_get_sec_counter,
        .set_sec_counter    = hp_set_sec_counter,
        .read_temp_mC       = hp_read_temp_mC,
        .read_setpoint_mC   = hp_read_setpoint_mC,
        .read_min_freq_hz   = hp_read_min_freq_hz,
        .read_max_freq_hz   = hp_read_max_freq_hz,
        .read_ramp_settings = hp_read_ramp_settings,
        .visuals            = &g_vis,
    };

    HeatpumpPI_Init(&g_pi, &cfg);

    // Optional: tune parameters here globally for all controllers.
    // g_pi.p.debug_prints_enabled = 1;
    // g_pi.p.deadtime_sec = 300;
    // g_pi.p.control_period_sec = 30;

    SYS_CONSOLE_PRINT("[HPPI] Initialized.\n");
}

void HPPI_Reset(void)
{
    HPPI_InitOnce();
    HeatpumpPI_Reset(&g_pi);
    
    if (getDataFromMemoryCallable(FREQ_INCREASE_CURVE_SELECTION, MASTER_HEATPUMP_IN_CASCADE) == 1) {
        return;
    }
    
    if (ReadSmartEeprom16(SEEP_ADDR_ENABLE_FREQUENCY_CONTROLLER_FUNCTION) == false) {
        return;
    }
    
    ChangeHeatpumpSetting(ADDRESS_DC_FAN_INITIAL_FREQUENCY, 25);
    ChangeHeatpumpSetting(ADDRESS_HEATING_TARGET_FREQUENCY_CONSTANT_B, 30);
    ChangeHeatpumpSetting(ADDRESS_HEATING_TARGET_FREQUENCY_UPPER_LIMIT, 50);
    ChangeHeatpumpSetting(ADDRESS_HEATING_TARGET_FREQUENCY_LOWER_LIMIT, 30);     
    
    SYS_CONSOLE_PRINT("[HPPI] Reset.\n");
}


void HPPI_Clear(void)
{
    HPPI_InitOnce();
    HeatpumpPI_Reset(&g_pi);
    
    ChangeHeatpumpSetting(ADDRESS_DC_FAN_INITIAL_FREQUENCY, ReadSmartEeprom16(SEEP_ADDR_INITIAL_FAN_SPEED));
    ChangeHeatpumpSetting(ADDRESS_HEATING_TARGET_FREQUENCY_CONSTANT_B, ReadSmartEeprom16(SEEP_ADDR_MAXIMUM_TARGET_COMPRESSOR_FREQUENCY_CONSTANT_B));
    ChangeHeatpumpSetting(ADDRESS_HEATING_TARGET_FREQUENCY_UPPER_LIMIT, ReadSmartEeprom16(SEEP_ADDR_MAXIMUM_TARGET_COMPRESSOR_FREQUENCY));
    ChangeHeatpumpSetting(ADDRESS_HEATING_TARGET_FREQUENCY_LOWER_LIMIT, ReadSmartEeprom16(SEEP_ADDR_MINIMUM_TARGET_COMPRESSOR_FREQUENCY));
    
    SYS_CONSOLE_PRINT("[HPPI] Cleared.\n");
}


void HPPI_UpdateAndGetTargetHz(void)
{
    if (getDataFromMemoryCallable(FREQ_INCREASE_CURVE_SELECTION, MASTER_HEATPUMP_IN_CASCADE) == 1) {
        return;
    }
    
    if (ReadSmartEeprom16(SEEP_ADDR_ENABLE_FREQUENCY_CONTROLLER_FUNCTION) == false) {
        return;
    }
    
    HPPI_InitOnce();
    HeatpumpPI_Update(&g_pi);
}

uint16_t HPPI_GetCompressorTargetFrequency(void) {
    return g_pi.output_target_hz;
}

const HeatpumpPI_Visuals* HPPI_GetVisuals(void)
{
    HPPI_InitOnce();
    return &g_vis;
}

HeatpumpPI* HPPI_GetController(void)
{
    HPPI_InitOnce();
    return &g_pi;
}
