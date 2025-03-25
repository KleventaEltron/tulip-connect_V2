#include <stddef.h>                    
#include <stdbool.h>                   
#include <stdlib.h>                   
#include <string.h>
#include <stdio.h>
#include "definitions.h"    

#include "hot_water_cooling_mode.h"
#include "states.h"

extern HOT_WATER_COOLING_MODE_DATA hot_water_cooling_mode_data;


void HOT_WATER_COOLING_MODE_Initialize ( void )
{
    hot_water_cooling_mode_data.state = HOT_WATER_COOLING_INITIALIZE;
    return;
}



void HOT_WATER_COOLING_MODE_Tasks ( void )
{    
    switch ( hot_water_cooling_mode_data.state )
    {
        case HOT_WATER_COOLING_INITIALIZE:{
            SYS_CONSOLE_PRINT("HOT_WATER_COOLING_INITIALIZE\r\n");
            hot_water_cooling_mode_data.state = HOT_WATER_COOLING_IDLE;
            break;
        }
        
        case HOT_WATER_COOLING_IDLE:{
            break;
        }
        
        default:{
            break;
        }
    }
    
}


