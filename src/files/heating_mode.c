#include <stddef.h>                    
#include <stdbool.h>                   
#include <stdlib.h>                   
#include <string.h>
#include <stdio.h>
#include "definitions.h"    

#include "heating_mode.h"
#include "states.h"

extern HEATING_MODE_DATA heating_mode_data;


void HEATING_MODE_Initialize ( void )
{
    heating_mode_data.state = HEATING_INITIALIZE;
    return;
}



void HEATING_MODE_Tasks ( void )
{    
    switch ( heating_mode_data.state )
    {
        case HEATING_INITIALIZE:{
            SYS_CONSOLE_PRINT("HEATING_INITIALIZE\r\n");
            heating_mode_data.state = HEATING_IDLE;
            break;
        }
        
        case HEATING_IDLE:{
            break;
        }
        
        default:{
            break;
        }
    }
    
}


