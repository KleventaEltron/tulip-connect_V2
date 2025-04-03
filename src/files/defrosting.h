#ifndef _DEFROSTING_H    /* Guard against multiple inclusion */
#define _DEFROSTING_H

/* Provide C++ Compatibility */
#ifdef __cplusplus
extern "C" {
#endif
    
#include <stddef.h>                    
#include <stdbool.h>                   
#include <stdlib.h>                   
#include <string.h>
#include <stdio.h>
#include "definitions.h" 
    
#include "states.h"    
    
    
bool getDefrostingElementOnState();
void CheckDefrosting(HOT_WATER_HEATING_MODE_STATES currentHotWaterHeatingModeState, STERILIZATION_MODE currentSterilizationModeState);

#ifdef __cplusplus
}
#endif

#endif /* _DEFROSTING_H */

/* *****************************************************************************
 End of File
 */


