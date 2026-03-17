

#ifndef _HEATPUMP_PI_ADAPTER_H    /* Guard against multiple inclusion */
#define _HEATPUMP_PI_ADAPTER_H


#pragma once
#include <stdint.h>
#include "pi_frequency_controller.h"

#ifdef __cplusplus
extern "C" {
#endif

// Initialize once (safe to call multiple times)
void HPPI_InitOnce(void);

// Reset PI state (recommended when switching between your 4 controllers/modes)
void HPPI_Reset(void);


void HPPI_Clear(void);

// Call frequently from the currently active controller.
// Returns the updated compressor target frequency (Hz).
void HPPI_UpdateAndGetTargetHz(void);

// Optional: get latest visuals snapshot (may be NULL if disabled in adapter)
const HeatpumpPI_Visuals* HPPI_GetVisuals(void);

// Optional: access the PI object if you want to tweak params (deadtime, gains, etc.)
HeatpumpPI* HPPI_GetController(void);

uint16_t HPPI_GetCompressorTargetFrequency(void);

#ifdef __cplusplus
}
#endif

#endif
