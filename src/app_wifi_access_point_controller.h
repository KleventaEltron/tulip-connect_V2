/*******************************************************************************
  MPLAB Harmony Application Header File

  Company:
    Microchip Technology Inc.

  File Name:
    app_wifi_access_point_controller.h

  Summary:
    This header file provides prototypes and definitions for the application.

  Description:
    This header file provides function prototypes and data type definitions for
    the application.  Some of these are required by the system (such as the
    "APP_WIFI_ACCESS_POINT_CONTROLLER_Initialize" and "APP_WIFI_ACCESS_POINT_CONTROLLER_Tasks" prototypes) and some of them are only used
    internally by the application (such as the "APP_WIFI_ACCESS_POINT_CONTROLLER_STATES" definition).  Both
    are defined here for convenience.
*******************************************************************************/

#ifndef _APP_WIFI_ACCESS_POINT_CONTROLLER_H
#define _APP_WIFI_ACCESS_POINT_CONTROLLER_H

// *****************************************************************************
// *****************************************************************************
// Section: Included Files
// *****************************************************************************
// *****************************************************************************

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include "configuration.h"
#include "definitions.h"

// DOM-IGNORE-BEGIN
#ifdef __cplusplus  // Provide C++ Compatibility

extern "C" {

#endif

    

typedef enum
{
    APP_WIFI_ACCESS_POINT_CONTROLLER_WAIT_FOR_BUTTON_PRESS=0,
    APP_WIFI_ACCESS_POINT_CONTROLLER_STATE_INIT,
    APP_WIFI_ACCESS_POINT_CONTROLLER_STATE_INIT_READY,
    APP_WIFI_ACCESS_POINT_CONTROLLER_WDRV_OPEN,
} APP_WIFI_ACCESS_POINT_CONTROLLER_STATES;




typedef struct
{
    APP_WIFI_ACCESS_POINT_CONTROLLER_STATES state;
    SYS_CONSOLE_HANDLE consoleHandle;
} APP_WIFI_ACCESS_POINT_CONTROLLER_DATA;



void APP_WIFI_ACCESS_POINT_CONTROLLER_Initialize ( void );



void APP_WIFI_ACCESS_POINT_CONTROLLER_Tasks( void );

//DOM-IGNORE-BEGIN
#ifdef __cplusplus
}
#endif
//DOM-IGNORE-END

#endif /* _APP_WIFI_ACCESS_POINT_CONTROLLER_H */

/*******************************************************************************
 End of File
 */

