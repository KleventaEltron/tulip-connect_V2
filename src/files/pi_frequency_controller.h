
#ifndef _PI_FREQUENCY_CONTROLLER_H   /* Guard against multiple inclusion */
#define _PI_FREQUENCY_CONTROLLER_H
    
#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// =============================
// Visuals/debug output (optional)
// =============================
typedef struct
{
    // timing
    uint32_t startupSeconds;
    uint32_t dt_sec;
    uint32_t controlPeriod_sec;

    // temperatures (mC = milli-Celsius)
    int32_t  tempNow_mC;
    int32_t  tempPrev_mC;
    int32_t  dT_mC;

    int32_t  setpoint_mC;
    int32_t  deltaToSet_mC;

    // ramp tracking
    int32_t  slopeTarget_mC_min;
    int32_t  Tref_mC;
    int32_t  eRamp_mC;
    int32_t  eHold_mC;

    // window slope estimate
    int32_t  slopeMeas_mC_min;

    // blend
    int32_t  wHold_1000;     // 0..1000 (0=ramp, 1000=hold)

    // control error actually used by PI (mC)
    int32_t  eActive_mC;

    // controller gains used (blended)
    int32_t  Kp_x1000_per_mC;
    int32_t  Ki_x1000_per_mC_min;

    // PI terms
    int32_t  P_Hz_x1000;
    int32_t  I_Hz_x1000;

    // output step applied this update (Hz)
    int16_t  dHz;
    uint16_t freqBefore;
    uint16_t freqAfter;

    // limiters
    int16_t  maxUp_Hz;
    int16_t  maxDown_Hz;

    // integral management
    uint32_t intHoldoff_sec;

    // event flags:
    // 0x01 = control update executed (period reached)
    // 0x04 = coolingFast detected (used by no-drop guard)
    uint8_t  eventFlags;
    
    int32_t slopeLong_mC_min;   // 5-min slope
    uint8_t fallingPersist;     // consecutive falling detections
    uint8_t fallingConfirmed;   // 0/1
    uint8_t forceUpActive;      // 0/1 (when we override dHz)
    
    uint16_t maintFloor_Hz;         // learned minimum
    uint16_t maintFloorDecayTicks;  // counter for slow decay    
    
    uint8_t noChangePersist;
    uint8_t overrideActive;

} HeatpumpPI_Visuals;

// =============================
// Callback interface (provided by adapter)
// =============================
typedef struct
{
    // seconds counter since last reset, or UINT32_MAX if invalid
    uint32_t (*get_sec_counter)(void);
    void     (*set_sec_counter)(uint32_t v);

    // temperatures in milli-Celsius (mC)
    int32_t  (*read_temp_mC)(void);
    int32_t  (*read_setpoint_mC)(void);

    // frequency constraints
    uint16_t (*read_min_freq_hz)(void);
    uint16_t (*read_max_freq_hz)(void);

    
    // ramp settings: riseTemp_mC and riseTime_min (return false if invalid)
    bool     (*read_ramp_settings)(int32_t *riseTemp_mC, int32_t *riseTime_min);

    // optional visuals storage (can be NULL)
    HeatpumpPI_Visuals *visuals;

} HeatpumpPI_Config;

// =============================
// Tunables
// =============================
typedef struct
{
    // enable/disable internal prints (always SYS_CONSOLE_PRINT)
    uint8_t  debug_prints_enabled;

    // warmup behavior
    uint32_t warmup_sec;          // default 180
    uint32_t control_period_sec;  // default 20

    // lag handling
    uint32_t deadtime_sec;                // default 240
    uint32_t int_holdoff_after_up_sec;    // default 240

    // integral clamp (Hz*1000)
    int32_t  i_clamp_x1000;       // default 20000 (+/-20Hz)

    // smoothness (per control update)
    int16_t  max_step_up_hz;      // default 1
    int16_t  max_step_dn_hz;      // default 2

    // blending ramp -> hold based on deltaToSet = setpoint - temp (mC)
    int32_t  blend_start_mC;      // default 4000 (4.0°C below setpoint => ramp)
    int32_t  blend_end_mC;        // default 800  (0.8°C below setpoint => hold)

    // no-drop guard near setpoint
    int32_t  hold_hyst_mC;        // default 200 (0.2°C)
    int32_t  max_neg_slope_mC_min;// default 5   (0.005°C/min)

    // gains (Hz*1000 per mC) and (Hz*1000 per (mC*min))
    int32_t  kp_ramp_x1000_per_mC;      // default 8
    int32_t  ki_ramp_x1000_per_mC_min;  // default 1
    int32_t  kp_hold_x1000_per_mC;      // default 10
    int32_t  ki_hold_x1000_per_mC_min;  // default 2

} HeatpumpPI_Params;

// =============================
// Controller state
// =============================
typedef struct
{
    HeatpumpPI_Config cfg;
    HeatpumpPI_Params p;

    // output
    uint16_t output_target_hz;

    // internal states
    int32_t  I_Hz_x1000;
    int32_t  fracAccu_x1000;

    uint32_t lastSecCounter;
    uint32_t startupSeconds;

    uint8_t  rampStarted;
    uint32_t rampStartSeconds;
    int32_t  rampStartTemp_mC;

    uint32_t intHoldoff_sec;

    // last window start temp (for slope estimate)
    int32_t  prevTemp_mC;
    
        // ---- long-term trend estimation ----
    int32_t  trendBuf_mC[16];
    uint8_t  trendCount;
    uint8_t  trendIdx;
    uint8_t  fallingPersist; // consecutive falling detections
    uint8_t  rampHighPersist;
    
    uint16_t maintFloor_Hz;         // learned minimum
    uint16_t maintFloorDecayTicks;  // counter for slow decay
    
    uint8_t noChangePersist;   // consecutive windows with no floor change
    uint8_t upCooldown;

    uint8_t overrideActive;
    uint8_t overridePersist;

} HeatpumpPI;

// =============================
// API
// =============================
void HeatpumpPI_DefaultParams(HeatpumpPI_Params *p);
void HeatpumpPI_Init(HeatpumpPI *pi, const HeatpumpPI_Config *cfg);
void HeatpumpPI_Reset(HeatpumpPI *pi);
void HeatpumpPI_Update(HeatpumpPI *pi);

#ifdef __cplusplus
}
#endif

#endif


/* *****************************************************************************
 End of File
 */
