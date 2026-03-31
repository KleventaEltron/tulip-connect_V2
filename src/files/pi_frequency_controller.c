
#include "pi_frequency_controller.h"

#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>

#include "states.h"

#include "../config/default/definitions.h"

// ---------- small helpers ----------
static int32_t clamp32_i(int32_t v, int32_t lo, int32_t hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static uint16_t clamp16_u(uint16_t v, uint16_t lo, uint16_t hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static int32_t estimate_slope_mC_min(int32_t tempNow_mC, int32_t tempPrev_mC, uint32_t dt_sec)
{
    if (tempPrev_mC == INT32_MAX || dt_sec == 0) return 0;
    int32_t dT = tempNow_mC - tempPrev_mC;
    return (int32_t)((int64_t)dT * 60 / (int64_t)dt_sec);
}

// ---------- long-term trend ring buffer ----------
static void trend_reset(HeatpumpPI *pi)
{
    pi->trendCount = 0;
    pi->trendIdx   = 0;

    for (uint8_t i = 0; i < 16; i++) pi->trendBuf_mC[i] = INT32_MAX;
}

static void trend_push(HeatpumpPI *pi, int32_t temp_mC)
{
    pi->trendBuf_mC[pi->trendIdx] = temp_mC;
    pi->trendIdx = (uint8_t)((pi->trendIdx + 1u) % 16u);

    if (pi->trendCount < 16u) pi->trendCount++;
}

// slope over N samples back (mC/min), using control period spacing
static int32_t trend_slope_mC_min(const HeatpumpPI *pi, uint8_t samplesBack, uint32_t spacing_sec)
{
    if (spacing_sec == 0) return 0;
    if (pi->trendCount < (samplesBack + 1u)) return 0;

    uint8_t newestIdx = (uint8_t)((pi->trendIdx + 16u - 1u) % 16u);
    uint8_t olderIdx  = (uint8_t)((pi->trendIdx + 16u - 1u - samplesBack) % 16u);

    int32_t newest = pi->trendBuf_mC[newestIdx];
    int32_t older  = pi->trendBuf_mC[olderIdx];
    if (newest == INT32_MAX || older == INT32_MAX) return 0;

    int32_t dT_mC = newest - older;
    uint32_t dt_sec = spacing_sec * (uint32_t)samplesBack;

    return (int32_t)((int64_t)dT_mC * 60 / (int64_t)dt_sec);
}

// choose samplesBack so that ~5 minutes is covered for the current control period
static uint8_t samples_back_for_5min(uint32_t control_period_sec)
{
    if (control_period_sec == 0) return 10;

    // round(300 / control_period_sec)
    uint32_t n = (300u + (control_period_sec / 2u)) / control_period_sec;

    if (n < 2u)  n = 2u;
    if (n > 15u) n = 15u; 
    return (uint8_t)n;
}

// ============================================================
// API
// ============================================================
void HeatpumpPI_DefaultParams(HeatpumpPI_Params *p)
{
    if (!p) return;
    memset(p, 0, sizeof(*p));
    p->debug_prints_enabled = 0;
    p->warmup_sec          = 180;
    p->control_period_sec  = 30;

    p->deadtime_sec = 240;
    p->int_holdoff_after_up_sec = 240;
    p->i_clamp_x1000 = 20000;
    p->max_step_up_hz = 1;
    p->max_step_dn_hz = 2;

    p->blend_start_mC = 4000;
    p->blend_end_mC   = 800;
    p->hold_hyst_mC   = 200;
    p->max_neg_slope_mC_min = 5;

    p->kp_ramp_x1000_per_mC = 8;
    p->ki_ramp_x1000_per_mC_min = 1;
    p->kp_hold_x1000_per_mC = 10;
    p->ki_hold_x1000_per_mC_min = 2;
}

void HeatpumpPI_Init(HeatpumpPI *pi, const HeatpumpPI_Config *cfg)
{
    if (!pi || !cfg) return;

    memset(pi, 0, sizeof(*pi));
    pi->cfg = *cfg;

    HeatpumpPI_DefaultParams(&pi->p);

    HeatpumpPI_Reset(pi);
}

void HeatpumpPI_Reset(HeatpumpPI *pi)
{
    if (!pi) return;

    if (pi->cfg.set_sec_counter) pi->cfg.set_sec_counter(0);

    pi->I_Hz_x1000     = 0;
    pi->fracAccu_x1000 = 0;

    pi->lastSecCounter = UINT32_MAX;
    pi->startupSeconds = 0;

    pi->rampStarted = 0;

    pi->intHoldoff_sec = 0;

    pi->prevTemp_mC = INT32_MAX;

    pi->fallingPersist = 0;
    pi->maintFloor_Hz = 0;
    pi->maintFloorDecayTicks = 0;
    
    pi->noChangePersist = 0;


    trend_reset(pi);

    uint16_t hardMin = (pi->cfg.read_min_freq_hz) ? pi->cfg.read_min_freq_hz() : 0;
    pi->maintFloor_Hz = hardMin;
    pi->output_target_hz = hardMin;

    if (pi->cfg.visuals) {
        *pi->cfg.visuals = (HeatpumpPI_Visuals){0};
        pi->cfg.visuals->freqBefore = pi->output_target_hz;
        pi->cfg.visuals->freqAfter  = pi->output_target_hz;
        pi->cfg.visuals->controlPeriod_sec = pi->p.control_period_sec;
    }
}

void HeatpumpPI_Update(HeatpumpPI *pi)
{
    if (!pi) return;

    // Required callbacks
    if (!pi->cfg.get_sec_counter || !pi->cfg.set_sec_counter ||
        !pi->cfg.read_temp_mC || !pi->cfg.read_setpoint_mC ||
        !pi->cfg.read_min_freq_hz || !pi->cfg.read_max_freq_hz) {
        return;
    }

    HeatpumpPI_Visuals *v = pi->cfg.visuals;

    // Once-per-second gate
    uint32_t secCounter = pi->cfg.get_sec_counter();
    if (secCounter == UINT32_MAX) {
        HeatpumpPI_Reset(pi);
        return;
    }
    if (secCounter == pi->lastSecCounter) return;

    pi->lastSecCounter = secCounter;
    pi->startupSeconds++;

    // Read process values
    int32_t tempNow_mC    = pi->cfg.read_temp_mC();
    int32_t setpoint_mC   = pi->cfg.read_setpoint_mC();
    int32_t deltaToSet_mC = setpoint_mC - tempNow_mC;

    // Base visuals always updated
    if (v) {
        v->startupSeconds    = pi->startupSeconds;
        v->tempNow_mC        = tempNow_mC;
        v->setpoint_mC       = setpoint_mC;
        v->deltaToSet_mC     = deltaToSet_mC;
        v->controlPeriod_sec = pi->p.control_period_sec;
        v->eventFlags        = 0;
        v->dt_sec            = secCounter; // during warmup this is time since reset
        v->Kp_x1000_per_mC = 0;
        v->Ki_x1000_per_mC_min = 0;
        v->P_Hz_x1000 = 0;
        v->I_Hz_x1000 = 0;
        v->wHold_1000 = 1000;
        v->Tref_mC = 0;
        v->eRamp_mC = 0;
        v->eHold_mC = deltaToSet_mC;
        v->eActive_mC = 0;
        v->maxUp_Hz = 0;
        v->maxDown_Hz = 0;
        v->intHoldoff_sec = 0;
        v->forceUpActive = 0;
    }

    // Warmup: hold minimum frequency, but keep buffers fresh
    if (pi->startupSeconds < pi->p.warmup_sec) {
        uint16_t hardMin = pi->cfg.read_min_freq_hz();
        pi->maintFloor_Hz = hardMin;
        pi->output_target_hz = hardMin;

        if (pi->prevTemp_mC == INT32_MAX) pi->prevTemp_mC = tempNow_mC;

        if (v) {
            v->tempPrev_mC = pi->prevTemp_mC;
            v->dT_mC = tempNow_mC - pi->prevTemp_mC;
            v->slopeMeas_mC_min = estimate_slope_mC_min(tempNow_mC, pi->prevTemp_mC, (secCounter == 0 ? 1u : secCounter));
            v->dHz = 0;
            v->freqBefore = pi->output_target_hz;
            v->freqAfter  = pi->output_target_hz;
            v->maintFloor_Hz = pi->maintFloor_Hz;
            v->maintFloorDecayTicks = pi->maintFloorDecayTicks;
        }
        return;
    }

    // First time after warmup: initialize trend buffer and reset the internal second counter
    if (!pi->rampStarted) {
        pi->rampStarted = 1;
        pi->prevTemp_mC = tempNow_mC;
        trend_reset(pi);
        trend_push(pi, tempNow_mC);

        uint16_t hardMin = pi->cfg.read_min_freq_hz();
        pi->maintFloor_Hz = hardMin;
        pi->output_target_hz = hardMin;

        // reset persistence counters so we start clean
        pi->fallingPersist = 0;
        pi->rampHighPersist = 0;
        pi->maintFloorDecayTicks = 0;
        pi->noChangePersist = 0;
        pi->upCooldown = 0;

        // NEW: override state
        pi->overrideActive = 0;
        pi->overridePersist = 0;

        pi->cfg.set_sec_counter(0);
        pi->lastSecCounter = 0;

        if (v) {
            v->tempPrev_mC = tempNow_mC;
            v->dT_mC = 0;
            v->slopeMeas_mC_min = 0;
            v->dHz = 0;
            v->freqBefore = pi->output_target_hz;
            v->freqAfter  = pi->output_target_hz;
            v->maintFloor_Hz = pi->maintFloor_Hz;
            v->maintFloorDecayTicks = pi->maintFloorDecayTicks;
        }
        return;
    }

    uint32_t dt_sec = pi->cfg.get_sec_counter();
    if (v) v->dt_sec = dt_sec;

    if (dt_sec < pi->p.control_period_sec) {
        // preview slope over the current window
        if (v) {
            v->tempPrev_mC = pi->prevTemp_mC;
            v->dT_mC = (pi->prevTemp_mC == INT32_MAX) ? 0 : (tempNow_mC - pi->prevTemp_mC);
            v->slopeMeas_mC_min = estimate_slope_mC_min(tempNow_mC, pi->prevTemp_mC, dt_sec);
            v->dHz = 0;
            v->freqBefore = pi->output_target_hz;
            v->freqAfter  = pi->output_target_hz;
            v->maintFloor_Hz = pi->maintFloor_Hz;
            v->maintFloorDecayTicks = pi->maintFloorDecayTicks;
        }
        return;
    }

    // ------------------------------------------------------------
    // CONTROL EXECUTES HERE (once per control period)
    // ------------------------------------------------------------
    if (v) v->eventFlags |= 0x01;

    // --------- EXTERNAL OVERRIDE DETECTION (burst, defrost, etc.) ----------
    // Detect ONLY when actual Hz is significantly above our commanded Hz,
    // so if we command >60 ourselves, it will NOT trigger.
    const uint16_t OVR_ENTER_DELTA_HZ = 10u; // enter override if act > cmd + 10
    const uint16_t OVR_EXIT_DELTA_HZ  = 6u;  // exit override when act <= cmd + 6
    const uint8_t  OVR_PERSIST_WINDOWS = 1u; // require 1 full control window of mismatch

    uint16_t cmdHz = pi->output_target_hz;
    uint16_t actHz = getHeatpumpCompressorFrequency(0);

    bool overEnter = (actHz > (uint16_t)(cmdHz + OVR_ENTER_DELTA_HZ));
    bool underExit = (actHz <= (uint16_t)(cmdHz + OVR_EXIT_DELTA_HZ));

    if (!pi->overrideActive) {
        if (overEnter) {
            if (pi->overridePersist < 255u) pi->overridePersist++;
            if (pi->overridePersist >= OVR_PERSIST_WINDOWS) {
                pi->overrideActive = 1u;
                pi->overridePersist = 0u;
            }
        } else {
            pi->overridePersist = 0u;
        }
    } else {
        // already in override; wait until it settles back
        if (underExit) {
            // override ended -> reset trend + counters so slope isn't poisoned
            pi->overrideActive = 0u;
            pi->overridePersist = 0u;

            trend_reset(pi);
            trend_push(pi, tempNow_mC);

            pi->fallingPersist = 0u;
            pi->rampHighPersist = 0u;
            pi->noChangePersist = 0u;
            pi->upCooldown = 2u; // small cooldown to avoid reacting instantly
        }
    }

    // While override is active:
    //  - do NOT learn (no trend_push)
    //  - do NOT adjust maintFloor_Hz
    //  - keep output command unchanged
    if (pi->overrideActive) {
        if (v) {
            // Mark it for visibility
            v->eventFlags |= 0x20;         // arbitrary "override" flag
            v->forceUpActive = 1u;         // optional: re-use field
            v->maintFloor_Hz = pi->maintFloor_Hz;
            v->freqBefore = pi->output_target_hz;
            v->freqAfter  = pi->output_target_hz;
            v->dHz        = 0;
        }

        // Reset the period counter for next window (keep cadence)
        pi->cfg.set_sec_counter(0);
        pi->lastSecCounter = 0;
        return;
    }

    // Update trend buffer once per control execution
    trend_push(pi, tempNow_mC);

    uint8_t back = samples_back_for_5min(pi->p.control_period_sec);
    int32_t slopeLong_mC_min = trend_slope_mC_min(pi, back, pi->p.control_period_sec);

    // Maintain-floor learning goal band:
    //   0 .. +100 mC/min  (0 .. +0.10°C/min)
    const int32_t SLOPE_LOW_mC_MIN   = 0;
    const int32_t SLOPE_HIGH_mC_MIN  = 100;
    const int32_t SLOPE_HYST_mC_MIN  = 10;   // +/-0.01°C/min

    // Only push floor up if we are meaningfully below setpoint
    const int32_t BELOW_SP_ENABLE_mC = 200;  // 0.2°C below setpoint

    // After an increase, wait N windows before allowing another increase
    const uint8_t UP_COOLDOWN_WINDOWS = 2u;

    uint16_t hardMin = pi->cfg.read_min_freq_hz();
    uint16_t maxF    = pi->cfg.read_max_freq_hz();
    if (maxF < hardMin) maxF = hardMin;

    uint16_t floorCap = (maxF > 1u) ? (uint16_t)(maxF - 1u) : maxF;

    if (pi->maintFloor_Hz < hardMin)  pi->maintFloor_Hz = hardMin;
    if (pi->maintFloor_Hz > floorCap) pi->maintFloor_Hz = floorCap;

    bool meaningfullyBelow = (deltaToSet_mC > BELOW_SP_ENABLE_mC);

    bool slopeTooLow  = (slopeLong_mC_min < (SLOPE_LOW_mC_MIN  - SLOPE_HYST_mC_MIN));
    bool slopeTooHigh = (slopeLong_mC_min > (SLOPE_HIGH_mC_MIN + SLOPE_HYST_mC_MIN));

    // Decrement cooldown once per control execution
    if (pi->upCooldown > 0u) pi->upCooldown--;

    // Persistence on low
    if (meaningfullyBelow && slopeTooLow) {
        if (pi->fallingPersist < 255u) pi->fallingPersist++;
    } else {
        pi->fallingPersist = 0;
    }
    bool lowConfirmed = (pi->fallingPersist >= 2u);

    // Persistence on high
    if (meaningfullyBelow && slopeTooHigh) {
        if (pi->rampHighPersist < 255u) pi->rampHighPersist++;
    } else {
        pi->rampHighPersist = 0;
    }
    bool highConfirmed = (pi->rampHighPersist >= 2u);

    // ------------------------------------------------------------
    // FLOOR UPDATE (steady approach + stagnation decay + slower up)
    // ------------------------------------------------------------
    bool floorChanged = false;

    // +1 Hz only if low confirmed AND cooldown expired
    if (lowConfirmed && (pi->upCooldown == 0u)) {
        if (pi->maintFloor_Hz < floorCap) {
            pi->maintFloor_Hz += 1u;
            floorChanged = true;
        }
        pi->maintFloorDecayTicks = 0;
        pi->noChangePersist = 0;
        pi->upCooldown = UP_COOLDOWN_WINDOWS;
    }
    // -1 Hz if high confirmed
    else if (highConfirmed) {
        if (pi->maintFloor_Hz > hardMin) {
            pi->maintFloor_Hz -= 1u;
            floorChanged = true;
        }
        pi->maintFloorDecayTicks = 0;
        pi->noChangePersist = 0;
    }
    // Hold, but if held for 20 windows -> decay by 1 Hz
    else {
        pi->maintFloorDecayTicks = 0;

        if (meaningfullyBelow) {
            if (pi->noChangePersist < 255u) pi->noChangePersist++;

            if (pi->noChangePersist >= 20u) {
                pi->noChangePersist = 0;
                if (pi->maintFloor_Hz > hardMin) {
                    pi->maintFloor_Hz -= 1u;
                    floorChanged = true;
                }
            }
        } else {
            pi->noChangePersist = 0;
        }
    }

    (void)floorChanged;

    // Final clamp
    if (pi->maintFloor_Hz < hardMin)  pi->maintFloor_Hz = hardMin;
    if (pi->maintFloor_Hz > floorCap) pi->maintFloor_Hz = floorCap;

    // Output command is the floor
    uint16_t freqBefore = pi->output_target_hz;
    uint16_t newFreq    = pi->maintFloor_Hz;

    newFreq = clamp16_u(newFreq, hardMin, maxF);
    pi->output_target_hz = newFreq;

    // Update prev temp
    int32_t prevTempBefore = pi->prevTemp_mC;
    pi->prevTemp_mC = tempNow_mC;

    // Reset counter for next window
    pi->cfg.set_sec_counter(0);
    pi->lastSecCounter = 0;

    // Fill visuals
    if (v) {
        int32_t dT_mC = (prevTempBefore == INT32_MAX) ? 0 : (tempNow_mC - prevTempBefore);
        int32_t slopeMeas_mC_min = estimate_slope_mC_min(tempNow_mC, prevTempBefore, dt_sec);

        v->tempPrev_mC = (prevTempBefore == INT32_MAX) ? tempNow_mC : prevTempBefore;
        v->dT_mC       = dT_mC;
        v->slopeMeas_mC_min = slopeMeas_mC_min;

        v->slopeLong_mC_min = slopeLong_mC_min;
        v->fallingPersist   = pi->fallingPersist;
        v->fallingConfirmed = lowConfirmed ? 1u : 0u;
        v->forceUpActive    = highConfirmed ? 1u : 0u;

        v->maintFloor_Hz = pi->maintFloor_Hz;
        v->maintFloorDecayTicks = pi->maintFloorDecayTicks;
        v->noChangePersist = pi->noChangePersist;
        v->overrideActive = pi->overrideActive;


        v->freqBefore = freqBefore;
        v->freqAfter  = newFreq;
        v->dHz        = (int16_t)((int32_t)newFreq - (int32_t)freqBefore);

        // Not used anymore
        v->slopeTarget_mC_min = 0;
        v->Tref_mC = 0;
        v->eRamp_mC = 0;
        v->eActive_mC = 0;
        v->maxUp_Hz = 0;
        v->maxDown_Hz = 0;

        // Optional visibility of override: pack in eventFlags bit 0x20 already
    }

    if (pi->p.debug_prints_enabled) {
        SYS_CONSOLE_PRINT(
            "[HPPI][FLOOR] slope5=%ld mC/min, dSet=%ld mC, lowP=%u, highP=%u, cool=%u, floor=%u, cmd=%u, act=%u, ovr=%u\n",
            (long)slopeLong_mC_min, (long)deltaToSet_mC,
            (unsigned)pi->fallingPersist, (unsigned)pi->rampHighPersist,
            (unsigned)pi->upCooldown,
            (unsigned)pi->maintFloor_Hz, (unsigned)pi->output_target_hz,
            (unsigned)actHz, (unsigned)pi->overrideActive
        );
    }
}

