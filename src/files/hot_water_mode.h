
#ifndef HOT_WATER_MODE_H   
#define HOT_WATER_MODE_H


#ifdef __cplusplus
extern "C" {
#endif

bool getHotwaterElementBoolFromHotwaterMode();
const char * getHotWaterStateToString();
       
void HOT_WATER_MODE_Initialize ( void );
void HOT_WATER_MODE_Tasks ( void );
   
    
    

#ifdef __cplusplus
}
#endif

#endif 
