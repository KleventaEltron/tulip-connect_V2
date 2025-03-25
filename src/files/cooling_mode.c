#include <stddef.h>                    
#include <stdbool.h>                   
#include <stdlib.h>                   
#include <string.h>
#include <stdio.h>
#include "definitions.h"    

#include "cooling_mode.h"
#include "states.h"

extern COOLING_MODE_DATA cooling_mode_data;


void COOLING_MODE_Initialize ( void )
{
    cooling_mode_data.state = COOLING_INITIALIZE;
    return;
}



void COOLING_MODE_Tasks ( void )
{    
    switch ( cooling_mode_data.state )
    {
        case COOLING_INITIALIZE:{
            SYS_CONSOLE_PRINT("COOLING_INITIALIZE\r\n");
            cooling_mode_data.state = COOLING_IDLE;
            break;
        }
        
        case COOLING_IDLE:{
            break;
        }
        
        default:{
            break;
        }
    }
    
}


