

#ifndef HOT_WATER_HEATING_MODE_H   
#define HOT_WATER_HEATING_MODE_H


#ifdef __cplusplus
extern "C" {
#endif
    
#include <stddef.h>                    
#include <stdbool.h>                   
#include <stdlib.h>                   
#include <string.h>
#include <stdio.h>
#include "definitions.h" 
       
void HOT_WATER_HEATING_MODE_Initialize ( void );
void HOT_WATER_HEATING_MODE_Tasks ( void );

bool getHeatingElementBoolFromHotwaterHeatingMode();
bool getHotwaterElementBoolFromHotwaterHeatingMode();
   
    
#ifdef __cplusplus
}
#endif

#endif 
