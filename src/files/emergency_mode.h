#ifndef _EMERGENCY_MODE_H    /* Guard against multiple inclusion */
#define _EMERGENCY_MODE_H

/* Provide C++ Compatibility */
#ifdef __cplusplus
extern "C" {
#endif
    
//#include "states.h"    
    
//bool goToActiveSterilization();
void EmergencyModeTasks();
bool getHeatingElementBoolFromEmergencyMode();
bool getHotWaterElementBoolFromEmergencyMode();
    
#ifdef __cplusplus
}
#endif

#endif /* _STERILIZATION_H */

/* *****************************************************************************
 End of File
 */


