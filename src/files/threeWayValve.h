#ifndef _THREE_WAY_VALVE_H    /* Guard against multiple inclusion */
#define _THREE_WAY_VALVE_H

/* Provide C++ Compatibility */
#ifdef __cplusplus
extern "C" {
#endif
    
#include "states.h"    
    
bool validateThreeWayValveStateOkay(RUNNING_MODES currentRunningMode);
bool getNeededValvePosition();
    
    
#ifdef __cplusplus
}
#endif

#endif /* _THREE_WAY_VALVE_H */

/* *****************************************************************************
 End of File
 */
