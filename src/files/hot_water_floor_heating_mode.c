#include <stddef.h>                    
#include <stdbool.h>                   
#include <stdlib.h>                   
#include <string.h>
#include <stdio.h>
#include "definitions.h"    

#include "hot_water_floor_heating_mode.h"
#include "states.h"

extern HOT_WATER_FLOOR_HEATING_MODE_DATA hot_water_floor_heating_mode_data;


void HOT_WATER_FLOOR_HEATING_MODE_Initialize ( void )
{
    hot_water_floor_heating_mode_data.state = HOT_WATER_FLOOR_HEATING_INITIALIZE;
    return;
}



void HOT_WATER_FLOOR_HEATING_MODE_Tasks ( void )
{    
    switch ( hot_water_floor_heating_mode_data.state )
    {
        case HOT_WATER_FLOOR_HEATING_INITIALIZE:{
            SYS_CONSOLE_PRINT("HOT_WATER_FLOOR_HEATING_INITIALIZE\r\n");
            hot_water_floor_heating_mode_data.state = HOT_WATER_FLOOR_HEATING_IDLE;
            break;
        }
        
        case HOT_WATER_FLOOR_HEATING_IDLE:{
            break;
        }
        
        default:{
            break;
        }
    }
    
}


