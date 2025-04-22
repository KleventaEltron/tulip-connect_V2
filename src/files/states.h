
#ifndef _STATES_H    /* Guard against multiple inclusion */
#define _STATES_H

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
    RESERVED,
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
    uint16_t setPoint;
} APP_ACTIVE_MODE_CONTROLLER_DATA;    
    
void setActiveModeControllerHeatpumpSetpoint(int16_t newSetpoint);
RUNNING_MODES getActiveStateValue();
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
    bool HeatingElementOn;
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
    // Heating modes:
    HOT_WATER_HEATING_INITIALIZE_HEATING=0,
    HOT_WATER_HEATING_IDLE_HEATING,
    HOT_WATER_HEATING_RUNNING_ON_HEATING,
    HOT_WATER_HEATING_RUNNING_ON_HEATING_WITH_ELEMENT_ON,
            
    // Hot water modes: 
    HOT_WATER_HEATING_INITIALIZE_HOT_WATER,   
    HOT_WATER_HEATING_STATE_WAIT_FOR_MINIMAL_TIME_IN_HOT_WATER,
    HOT_WATER_HEATING_STATE_RUNNING_IN_HOT_WATER,
    HOT_WATER_HEATING_STATE_RUNNING_WITH_ELEMENT_ON_IN_HOT_WATER
} HOT_WATER_HEATING_MODE_STATES;



typedef struct{
    HOT_WATER_HEATING_MODE_STATES state;
    
    int16_t initialHeatingBufferTemp;
    
    bool hotwaterPassive;
    int16_t setpointHotWaterOffset;
    bool HeatingElementOn;
    bool HotwaterElementOn;
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

/*   _____ _____ _____   _____ _    _ _            _______ _____ ____  _   _   _____  _    _ __  __ _____  
  / ____|_   _|  __ \ / ____| |  | | |        /\|__   __|_   _/ __ \| \ | | |  __ \| |  | |  \/  |  __ \ 
 | |      | | | |__) | |    | |  | | |       /  \  | |    | || |  | |  \| | | |__) | |  | | \  / | |__) |
 | |      | | |  _  /| |    | |  | | |      / /\ \ | |    | || |  | | . ` | |  ___/| |  | | |\/| |  ___/ 
 | |____ _| |_| | \ \| |____| |__| | |____ / ____ \| |   _| || |__| | |\  | | |    | |__| | |  | | |     
  \_____|_____|_|  \_\\_____|\____/|______/_/    \_\_|  |_____\____/|_| \_| |_|     \____/|_|  |_|_|     
*/

typedef enum{
    CIRCULATION_PUMP_INITIALIZE,
    CIRCULATION_PUMP_OFF,
    CIRCULATION_PUMP_ON,
    CIRCULATION_PUMP_LAG_TIME,
    CIRCULATION_PUMP_TOO_LONG_OFF
} CIRCULATION_PUMP_STATES; 


typedef enum
{
    HEATPUMP_COMM_STATUS_IDLE=0,
    HEATPUMP_COMM_STATUS_SENDING_DATA_TO_HEATPUMP,         
    HEATPUMP_COMM_STATUS_DATA_SENT_TO_HEATPUMP,          
    HEATPUMP_COMM_STATUS_WAITING_FOR_DATA_FROM_HEATPUMP, 
    HEATPUMP_COMM_STATUS_DEVICE_ADDRESS_RECEIVED,
    HEATPUMP_COMM_STATUS_FIRST_8_BYTES_RECEIVED,        
    HEATPUMP_COMM_STATUS_DATA_RECEIVED_FROM_HEATPUMP,
            
} HEATPUMP_COMMUNICATION_STATUS;
    

typedef enum
{
    APP_HEATPUMP_COMM_STATE_INIT=0,
    APP_HEATPUMP_COMM_STATE_SEND_DATA,
    APP_HEATPUMP_COMM_STATE_WAIT_FOR_DATA_SENT,        
    APP_HEATPUMP_COMM_STATE_RECEIVE_DATA,
    APP_HEATPUMP_COMM_STATE_WAIT_FOR_DATA_RECEIVED,      
    APP_HEATPUMP_COMM_STATE_CHECKSUM_CHECK,         
    APP_HEATPUMP_COMM_STATE_PARSE_DATA,        
    APP_HEATPUMP_COMM_STATE_DELAY,             
} APP_HEATPUMP_COMM_STATES;

typedef struct
{
    /* The application's current state */
    APP_HEATPUMP_COMM_STATES state;
    HEATPUMP_COMMUNICATION_STATUS commStatus;

    /* TODO: Define any additional data used by the application. */

} APP_HEATPUMP_COMM_DATA;





typedef struct{
    CIRCULATION_PUMP_STATES state;
    bool temperatureTooLowForPumpToBeOn;
} CIRCULATION_PUMP_DATA;

void resetActiveModeStates();
const char * getActiveModeToString(RUNNING_MODES state);
bool isDefrostingActive();
uint16_t getHeatpumpCompressorFrequency();
int16_t getHeatpumpSetpoint();
uint16_t getHeatpumpWaterFlow();
int16_t getHeatpumpReturnWaterTemperature();
const char * getThreeWayValveState(int state);
char * getHeatpumpStateToString();

CIRCULATION_PUMP_DATA getCircPumpData();
HEATING_MODE_DATA getHeatingModeData();
HOT_WATER_HEATING_MODE_DATA getHotWaterHeatingModeData();
int16_t getHeatingSetpoint();
int16_t getHotwaterSetpoint();
int16_t getHotwaterDelta();

#ifdef __cplusplus
}
#endif

#endif /* _STATES_H */

/* *****************************************************************************
 End of File
 */
