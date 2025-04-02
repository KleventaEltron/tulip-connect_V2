#ifndef _CIRCULATION_PUMP_H    /* Guard against multiple inclusion */
#define _CIRCULATION_PUMP_H

#include <stddef.h>                    
#include <stdbool.h>                   
#include <stdlib.h>                   
#include <string.h>
#include <stdio.h>
#include "definitions.h" 
       
void CIRCULATION_PUMP_Initialize();
void CIRCULATION_PUMP_Tasks();
const char * getCirculationPumpStateToString();

#endif 