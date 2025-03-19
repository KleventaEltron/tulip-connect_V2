
#ifndef _EXAMPLE_FILE_NAME_H    /* Guard against multiple inclusion */
#define _EXAMPLE_FILE_NAME_H

/* Provide C++ Compatibility */
#ifdef __cplusplus
extern "C" {
#endif


#define EXAMPLE_CONSTANT 0

typedef enum{
            HEATING=0,
            COOLING,
            FLOOR_HEATING,
            HOT_WATER,
            HOT_WATER_COOLING,
            HOT_WATER_HEATING,
            HOT_WATER_FLOOR_HEATING
} RUNNING_MODES;    
    
    
typedef enum
{
    APP_ACTIVE_MODE_CONTROLLER_STATE_INIT=0,
    APP_ACTIVE_MODE_CONTROLLER_STATE_SERVICE_TASKS,

} APP_ACTIVE_MODE_CONTROLLER_STATES;
    
typedef struct
{
    RUNNING_MODES previousRunningMode;
    RUNNING_MODES currentRunningMode;
} APP_ACTIVE_MODE_CONTROLLER_DATA;    
    
#ifdef __cplusplus
}
#endif

#endif /* _EXAMPLE_FILE_NAME_H */

/* *****************************************************************************
 End of File
 */
