
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
    uint16_t setPointHeating;
    uint16_t setPointCooling;
    uint16_t heatpumpRunningMode;
    bool heatpumpForcedOff;
    bool dip1SwitchCurrentState;
    bool dip1SwitchPreviousState;
    bool resetFactorySettings;
    bool factorySettingResetInProgress;
    bool pumpOffDueToDipSwitch1;
} APP_ACTIVE_MODE_CONTROLLER_DATA;    
    
void setActiveModeControllerHeatpumpSetpointHeating(int16_t newSetpoint);
void setActiveModeControllerHeatpumpSetpointCooling(int16_t newSetpoint);
void setActiveModeControllerHeatpumpRunningMode(uint16_t mode);
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
    int16_t stepperSetpoint;
    bool heatingCurveSet;
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
    HOT_WATER_STATE_RUNNING_IN_HOT_WATER,
    HOT_WATER_STATE_RUNNING_WITH_ELEMENT_ON_IN_HOT_WATER
} HOT_WATER_MODE_STATES;



typedef struct{
    HOT_WATER_MODE_STATES state;
    int16_t setpointHotWaterOffset;
    bool HotwaterElementOn;    
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
    COOLING_RUNNING
} COOLING_MODE_STATES;



typedef struct{
    bool coolingCurveSet;
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
    // Cooling modes:
    HOT_WATER_COOLING_INITIALIZE_COOLING = 0,
    HOT_WATER_COOLING_IDLE_COOLING,
    HOT_WATER_COOLING_MODE_RUNNING_ON_COOLING,
            
    // Hot water modes:
    HOT_WATER_COOLING_INITIALIZE_HOT_WATER,   
    HOT_WATER_COOLING_STATE_WAIT_FOR_MINIMAL_TIME_IN_HOT_WATER,
    HOT_WATER_COOLING_STATE_RUNNING_IN_HOT_WATER,
    HOT_WATER_COOLING_STATE_RUNNING_WITH_ELEMENT_ON_IN_HOT_WATER
} HOT_WATER_COOLING_MODE_STATES;



typedef struct{
    HOT_WATER_COOLING_MODE_STATES state;
    
    bool hotwaterPassive;
    int16_t setpointHotWaterOffset;
    bool HotwaterElementOn;
    bool coolingCurveSet;
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
    int16_t stepperSetpoint;
    bool HeatingElementOn;
    bool HotwaterElementOn;
    bool heatingCurveSet;
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



typedef struct{
    CIRCULATION_PUMP_STATES state;
    bool temperatureTooLowForPumpToBeOn;
    bool temperatureTooHighForPumpToBeOn;
} CIRCULATION_PUMP_DATA;

void resetActiveModeStates();
uint16_t getActiveStateFromActiveMode(RUNNING_MODES state);
const char * getActiveModeToString(RUNNING_MODES state);
//bool isDefrostingActive();
uint16_t getDefrostingActiveMask();
bool getResetFactorySettings();
void setResetFactorySettings();
bool getFactorySettingsResetInProgress();
uint16_t getActiveCompressorsMask();
uint16_t getHeatpumpCompressorFrequency(uint8_t whichHeatpump);
int16_t getHeatpumpHeatingSetpoint();
int16_t getHeatpumpCoolingSetpoint();
uint16_t getHeatpumpWaterFlow();
int16_t getHeatpumpRunningMode();
int16_t getHeatpumpOnOff();
int16_t getHeatpumpReturnWaterTemperature(uint8_t whichHeatpump);
const char * getThreeWayValveState(int state);
bool blockHotWaterBasedOnTimers(void);
bool checkIfDefrostingActive(void);

void setActiveModeControllerPumpOffDueToDipSwitch1 (bool target);
bool getActiveModeControllerPumpOffDueToDipSwitch1 ();

CIRCULATION_PUMP_DATA getCircPumpData();
HEATING_MODE_DATA getHeatingModeData();
COOLING_MODE_DATA getCoolingModeData();
HOT_WATER_MODE_DATA getHotWaterModeData();
HOT_WATER_HEATING_MODE_DATA getHotWaterHeatingModeData();
HOT_WATER_COOLING_MODE_DATA getHotWaterCoolingModeData();
int16_t getHeatingSetpoint();
int16_t getCoolingSetpoint();
int16_t getHotwaterSetpoint();
int16_t getHotwaterDelta();
uint16_t getCascadeSlaveStatus();

bool getCurrentDip1SwitchState();
bool getPreviousDip1SwitchState();
void setCurrentDip1SwitchState();
void setPreviousDip1SwitchState(bool currentState);

#ifdef __cplusplus
}
#endif

#endif /* _STATES_H */

/* *****************************************************************************
 End of File
 */
