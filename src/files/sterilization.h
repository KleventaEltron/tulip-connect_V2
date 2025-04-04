#ifndef _STERILIZATION_H    /* Guard against multiple inclusion */
#define _STERILIZATION_H

/* Provide C++ Compatibility */
#ifdef __cplusplus
extern "C" {
#endif
    
#include "states.h"    
    
bool goToActiveSterilization();
bool sterilisationIsActivelyRunning();
STERILIZATION_MODE getSterilisationMode();
void setSterilisationMode(STERILIZATION_MODE newMode);
void checkNeedForSterilization();
const char * getSterilizationState(STERILIZATION_MODE state);
bool getSterilizationElementOnState();
    
#ifdef __cplusplus
}
#endif

#endif /* _STERILIZATION_H */

/* *****************************************************************************
 End of File
 */


