#include <stddef.h>                    
#include <stdbool.h>                   
#include <stdlib.h>                   
#include <string.h>
#include <stdio.h>
#include "definitions.h"    

#include "floor_heating_mode.h"
#include "states.h"

extern FLOOR_HEATING_MODE_DATA floor_heating_mode_data;


void FLOOR_HEATING_MODE_Initialize ( void )
{
    floor_heating_mode_data.state = FLOOR_HEATING_INITIALIZE;
    return;
}



void FLOOR_HEATING_MODE_Tasks ( void )
{    
    switch ( floor_heating_mode_data.state )
    {
        case FLOOR_HEATING_INITIALIZE:{
            floor_heating_mode_data.state = FLOOR_HEATING_IDLE;
            break;
        }
        
        case FLOOR_HEATING_IDLE:{
            break;
        }
        
        default:{
            break;
        }
    }
    
}


