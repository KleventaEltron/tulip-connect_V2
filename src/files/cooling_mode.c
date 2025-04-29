#include <stddef.h>                    
#include <stdbool.h>                   
#include <stdlib.h>                   
#include <string.h>
#include <stdio.h>
#include "definitions.h"    

#include "cooling_mode.h"
#include "states.h"

#include "eeprom.h"
#include "ntc.h"
#include "modbus\heatpump_parameters.h"

extern COOLING_MODE_DATA cooling_mode_data;


void COOLING_MODE_Initialize ( void )
{
    cooling_mode_data.state = COOLING_INITIALIZE;
    return;
}

const char * getCoolingStateToString()
{
    switch (cooling_mode_data.state)
    {
        case(COOLING_INITIALIZE): {
            return "0, Init";
            break;
        }
        
        case(COOLING_IDLE): {
            return "1, Idle";
            break;
        }
        
        case(COOLING_RUNNING): {
            return "2, Running";
            break;
        }
        
        default:{
            return "-1, Unkown";
            break;
        }
        return "-1, Unkown";
    }
}



void COOLING_MODE_Tasks ( void )
{    
    setActiveModeControllerHeatpumpSetpoint(getHeatpumpHeatingSetpoint() * 10);
    
    setActiveModeControllerHeatpumpRunningMode(SET_MODE_COOLING);
    
    switch ( cooling_mode_data.state )
    {
        case COOLING_INITIALIZE:{
            //SYS_CONSOLE_PRINT("COOLING_INITIALIZE\r\n");
            cooling_mode_data.state = COOLING_IDLE;
            break;
        }
        
        case COOLING_IDLE:{
            
            if (getHeatpumpCompressorFrequency() != 0){
                // Compressor is running
                cooling_mode_data.state = COOLING_RUNNING;
                break;
            }
            
            break;
        }
        
        case COOLING_RUNNING:{
            
            if ((getHeatpumpCompressorFrequency() == 0) && (isDefrostingActive() == false)){
                // Compressor is not running and is also not in defrosting
                cooling_mode_data.state = COOLING_IDLE;
                break;
            }
            
            break;
        }
        
        default:{
            COOLING_MODE_Initialize();
            break;
        }
    }
    
}


