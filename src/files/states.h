
#ifndef _EXAMPLE_FILE_NAME_H    /* Guard against multiple inclusion */
#define _EXAMPLE_FILE_NAME_H

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

#define EXAMPLE_CONSTANT 0

typedef enum{
    OFF=0,
    PASSIVE,
    ACTIVE,
} STERILIZATION_MODE;    
    
    
    
typedef enum{
    COOLING=0,
    HEATING,
    HOT_WATER,
    FLOOR_HEATING,
    HOT_WATER_COOLING,
    HOT_WATER_HEATING,
    RESERVE,
    HOT_WATER_FLOOR_HEATING,
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
    


/*********
,--.  ,--.,------.  ,---. ,--------.,--.,--.  ,--. ,----.       ,--.   ,--. ,-----. ,------.  ,------. 
|  '--'  ||  .---' /  O  \'--.  .--'|  ||  ,'.|  |'  .-./       |   `.'   |'  .-.  '|  .-.  \ |  .---' 
|  .--.  ||  `--, |  .-.  |  |  |   |  ||  |' '  ||  | .---.    |  |'.'|  ||  | |  ||  |  \  :|  `--,  
|  |  |  ||  `---.|  | |  |  |  |   |  ||  | `   |'  '--'  |    |  |   |  |'  '-'  '|  '--'  /|  `---. 
`--'  `--'`------'`--' `--'  `--'   `--'`--'  `--' `------'     `--'   `--' `-----' `-------' `------' 
 ************/

typedef enum{
    HEATING_INITIALIZE,
    HEATING_IDLE,
    HEATING_RUNNING,
    HEATING_RUNNING_WITH_ELEMENT_ON
} HEATING_MODE_STATES;



typedef struct{
    HEATING_MODE_STATES state;
    int16_t initialBufferTemp;
} HEATING_MODE_DATA;



/***********
 ,--.  ,--. ,-----. ,--------.    ,--.   ,--.  ,---. ,--------.,------.,------.     ,--.   ,--. ,-----. ,------.  ,------. 
|  '--'  |'  .-.  ''--.  .--'    |  |   |  | /  O  \'--.  .--'|  .---'|  .--. '    |   `.'   |'  .-.  '|  .-.  \ |  .---' 
|  .--.  ||  | |  |   |  |       |  |.'.|  ||  .-.  |  |  |   |  `--, |  '--'.'    |  |'.'|  ||  | |  ||  |  \  :|  `--,  
|  |  |  |'  '-'  '   |  |       |   ,'.   ||  | |  |  |  |   |  `---.|  |\  \     |  |   |  |'  '-'  '|  '--'  /|  `---. 
`--'  `--' `-----'    `--'       '--'   '--'`--' `--'  `--'   `------'`--' '--'    `--'   `--' `-----' `-------' `------' 
 *******/


typedef enum{
    HOT_WATER_INITIALIZE,
    HOT_WATER_IDLE,
    HOT_WATER_MODE
} HOT_WATER_MODE_STATES;



typedef struct{
    HOT_WATER_MODE_STATES state;
} HOT_WATER_MODE_DATA;



/*
 ,-----. ,-----.  ,-----. ,--.   ,--.,--.  ,--. ,----.       ,--.   ,--. ,-----. ,------.  ,------. 
'  .--./'  .-.  ''  .-.  '|  |   |  ||  ,'.|  |'  .-./       |   `.'   |'  .-.  '|  .-.  \ |  .---' 
|  |    |  | |  ||  | |  ||  |   |  ||  |' '  ||  | .---.    |  |'.'|  ||  | |  ||  |  \  :|  `--,  
'  '--'\'  '-'  ''  '-'  '|  '--.|  ||  | `   |'  '--'  |    |  |   |  |'  '-'  '|  '--'  /|  `---. 
 `-----' `-----'  `-----' `-----'`--'`--'  `--' `------'     `--'   `--' `-----' `-------' `------'                                  
 */

typedef enum{
    COOLING_INITIALIZE,
    COOLING_IDLE,
    COOLING_MODE
} COOLING_MODE_STATES;



typedef struct{
    COOLING_MODE_STATES state;
} COOLING_MODE_DATA;



/*
,------.,--.    ,-----.  ,-----. ,------.     ,--.  ,--.,------.  ,---. ,--------.,--.,--.  ,--. ,----.       ,--.   ,--. ,-----. ,------.  ,------. 
|  .---'|  |   '  .-.  ''  .-.  '|  .--. '    |  '--'  ||  .---' /  O  \'--.  .--'|  ||  ,'.|  |'  .-./       |   `.'   |'  .-.  '|  .-.  \ |  .---' 
|  `--, |  |   |  | |  ||  | |  ||  '--'.'    |  .--.  ||  `--, |  .-.  |  |  |   |  ||  |' '  ||  | .---.    |  |'.'|  ||  | |  ||  |  \  :|  `--,  
|  |`   |  '--.'  '-'  ''  '-'  '|  |\  \     |  |  |  ||  `---.|  | |  |  |  |   |  ||  | `   |'  '--'  |    |  |   |  |'  '-'  '|  '--'  /|  `---. 
`--'    `-----' `-----'  `-----' `--' '--'    `--'  `--'`------'`--' `--'  `--'   `--'`--'  `--' `------'     `--'   `--' `-----' `-------' `------'  
 */

typedef enum{
    FLOOR_HEATING_INITIALIZE,
    FLOOR_HEATING_IDLE,
    FLOOT_HEATING_MODE
} FLOOR_HEATING_MODE_STATES;



typedef struct{
    FLOOR_HEATING_MODE_STATES state;
} FLOOR_HEATING_MODE_DATA;




/*
,--.  ,--. ,-----. ,--------.    ,--.   ,--.  ,---. ,--------.,------.,------.         |  |         ,-----. ,-----.  ,-----. ,--.   ,--.,--.  ,--. ,----.       ,--.   ,--. ,-----. ,------.  ,------. 
|  '--'  |'  .-.  ''--.  .--'    |  |   |  | /  O  \'--.  .--'|  .---'|  .--. '    ,---|  |---.    '  .--./'  .-.  ''  .-.  '|  |   |  ||  ,'.|  |'  .-./       |   `.'   |'  .-.  '|  .-.  \ |  .---' 
|  .--.  ||  | |  |   |  |       |  |.'.|  ||  .-.  |  |  |   |  `--, |  '--'.'    '---|  |---'    |  |    |  | |  ||  | |  ||  |   |  ||  |' '  ||  | .---.    |  |'.'|  ||  | |  ||  |  \  :|  `--,  
|  |  |  |'  '-'  '   |  |       |   ,'.   ||  | |  |  |  |   |  `---.|  |\  \         |  |        '  '--'\'  '-'  ''  '-'  '|  '--.|  ||  | `   |'  '--'  |    |  |   |  |'  '-'  '|  '--'  /|  `---. 
`--'  `--' `-----'    `--'       '--'   '--'`--' `--'  `--'   `------'`--' '--'        `--'         `-----' `-----'  `-----' `-----'`--'`--'  `--' `------'     `--'   `--' `-----' `-------' `------'  
 */

typedef enum{
    HOT_WATER_COOLING_INITIALIZE,
    HOT_WATER_COOLING_IDLE,
    HOT_WATER_COOLING_MODE
} HOT_WATER_COOLING_MODE_STATES;



typedef struct{
    HOT_WATER_COOLING_MODE_STATES state;
} HOT_WATER_COOLING_MODE_DATA;





/*
,--.  ,--. ,-----. ,--------.    ,--.   ,--.  ,---. ,--------.,------.,------.         |  |        ,--.  ,--.,------.  ,---. ,--------.,--.,--.  ,--. ,----.       ,--.   ,--. ,-----. ,------.  ,------. 
|  '--'  |'  .-.  ''--.  .--'    |  |   |  | /  O  \'--.  .--'|  .---'|  .--. '    ,---|  |---.    |  '--'  ||  .---' /  O  \'--.  .--'|  ||  ,'.|  |'  .-./       |   `.'   |'  .-.  '|  .-.  \ |  .---' 
|  .--.  ||  | |  |   |  |       |  |.'.|  ||  .-.  |  |  |   |  `--, |  '--'.'    '---|  |---'    |  .--.  ||  `--, |  .-.  |  |  |   |  ||  |' '  ||  | .---.    |  |'.'|  ||  | |  ||  |  \  :|  `--,  
|  |  |  |'  '-'  '   |  |       |   ,'.   ||  | |  |  |  |   |  `---.|  |\  \         |  |        |  |  |  ||  `---.|  | |  |  |  |   |  ||  | `   |'  '--'  |    |  |   |  |'  '-'  '|  '--'  /|  `---. 
`--'  `--' `-----'    `--'       '--'   '--'`--' `--'  `--'   `------'`--' '--'        `--'        `--'  `--'`------'`--' `--'  `--'   `--'`--'  `--' `------'     `--'   `--' `-----' `-------' `------' 
 */

typedef enum{
    HOT_WATER_HEATING_INITIALIZE,
    HOT_WATER_HEATING_IDLE,
    HOT_WATER_HEATING_MODE
} HOT_WATER_HEATING_MODE_STATES;



typedef struct{
    HOT_WATER_HEATING_MODE_STATES state;
} HOT_WATER_HEATING_MODE_DATA;







/*
,--.  ,--. ,-----. ,--------.    ,--.   ,--.  ,---. ,--------.,------.,------.         |  |        ,------.,--.    ,-----.  ,-----. ,------.     ,--.   ,--. ,-----. ,------.  ,------. 
|  '--'  |'  .-.  ''--.  .--'    |  |   |  | /  O  \'--.  .--'|  .---'|  .--. '    ,---|  |---.    |  .---'|  |   '  .-.  ''  .-.  '|  .--. '    |   `.'   |'  .-.  '|  .-.  \ |  .---' 
|  .--.  ||  | |  |   |  |       |  |.'.|  ||  .-.  |  |  |   |  `--, |  '--'.'    '---|  |---'    |  `--, |  |   |  | |  ||  | |  ||  '--'.'    |  |'.'|  ||  | |  ||  |  \  :|  `--,  
|  |  |  |'  '-'  '   |  |       |   ,'.   ||  | |  |  |  |   |  `---.|  |\  \         |  |        |  |`   |  '--.'  '-'  ''  '-'  '|  |\  \     |  |   |  |'  '-'  '|  '--'  /|  `---. 
`--'  `--' `-----'    `--'       '--'   '--'`--' `--'  `--'   `------'`--' '--'        `--'        `--'    `-----' `-----'  `-----' `--' '--'    `--'   `--' `-----' `-------' `------' 
 */

typedef enum{
    HOT_WATER_FLOOR_HEATING_INITIALIZE,
    HOT_WATER_FLOOR_HEATING_IDLE,
    HOT_WATER_FLOOR_HEATING_MODE
} HOT_WATER_FLOOR_HEATING_MODE_STATES;



typedef struct{
    HOT_WATER_FLOOR_HEATING_MODE_STATES state;
} HOT_WATER_FLOOR_HEATING_MODE_DATA;



void resetActiveModeStates();
const char * getActiveModeToString(RUNNING_MODES state);
bool isDefrostingActive();
uint16_t getHeatpumpCompressorFrequency();


#ifdef __cplusplus
}
#endif

#endif /* _EXAMPLE_FILE_NAME_H */

/* *****************************************************************************
 End of File
 */
