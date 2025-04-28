
#ifndef COOLING_MODE_H   
#define COOLING_MODE_H


#ifdef __cplusplus
extern "C" {
#endif
    
#include <stddef.h>                    
#include <stdbool.h>                   
#include <stdlib.h>                   
#include <string.h>
#include <stdio.h>
#include "definitions.h" 

       
void COOLING_MODE_Initialize ( void );
void COOLING_MODE_Tasks ( void );
   
const char * getCoolingStateToString();    
    

#ifdef __cplusplus
}
#endif

#endif 
