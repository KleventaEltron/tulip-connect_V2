/*******************************************************************************
  MPLAB Harmony Application Source File

  Company:
    Microchip Technology Inc.

  File Name:
    app_wifi_access_point_controller.c

  Summary:
    This file contains the source code for the MPLAB Harmony application.

  Description:
    This file contains the source code for the MPLAB Harmony application.  It
    implements the logic of the application's state machine and it may call
    API routines of other MPLAB Harmony modules in the system, such as drivers,
    system services, and middleware.  However, it does not call any of the
    system interfaces (such as the "Initialize" and "Tasks" functions) of any of
    the modules in the system or make any assumptions about when those functions
    are called.  That is the responsibility of the configuration-specific system
    files.
 *******************************************************************************/

// *****************************************************************************
// *****************************************************************************
// Section: Included Files
// *****************************************************************************
// *****************************************************************************

#include "app_wifi_access_point_controller.h"
#include "wdrv_winc_client_api.h"
#include "files/wincSoftAp.h"

APP_WIFI_ACCESS_POINT_CONTROLLER_DATA app_wifi_access_point_controllerData;

static DRV_HANDLE wdrvHandle;

volatile uint32_t btnPressTime = 0;
volatile bool btnIsDown = false;

void EIC_EXTINT_11_Callback(uintptr_t context)
{
    uint32_t now = SYS_TIME_CounterGet();
    uint8_t level = NotInputBtn1_Get(); // read PC27

    if (level == 0) // button pressed (active-low)
    {
        btnPressTime = now;
        btnIsDown = true;
    }
    else // released
    {
        if (btnIsDown)
        {
            uint32_t dt = now - btnPressTime;

            if (dt > SYS_TIME_MSToCount(2000))
            {
                SYS_CONSOLE_PRINT("LONG PRESS\r\n");
                setActivateWifiAp(!getActivateWifiAp());
            }
            else
            {
                SYS_CONSOLE_PRINT("SHORT PRESS\r\n");
                setActivateWifiAp(false);
            }
        }

        btnIsDown = false;
    }
}




void APP_WIFI_ACCESS_POINT_CONTROLLER_Initialize ( void )
{
    EIC_CallbackRegister(EIC_PIN_11, EIC_EXTINT_11_Callback, 0);
    app_wifi_access_point_controllerData.state = APP_WIFI_ACCESS_POINT_CONTROLLER_WAIT_FOR_BUTTON_PRESS;
}



void APP_WIFI_ACCESS_POINT_CONTROLLER_Tasks ( void )
{

    /* Check the application's current state. */
    switch ( app_wifi_access_point_controllerData.state )
    {
        case APP_WIFI_ACCESS_POINT_CONTROLLER_WAIT_FOR_BUTTON_PRESS:
        {
            if (getActivateWifiAp()) {
                app_wifi_access_point_controllerData.state = APP_WIFI_ACCESS_POINT_CONTROLLER_STATE_INIT;
            }
            break;
        }  
        
        case APP_WIFI_ACCESS_POINT_CONTROLLER_STATE_INIT:
        {
            /* Get handles to both the USB console instances */
            app_wifi_access_point_controllerData.consoleHandle = SYS_CONSOLE_HandleGet(SYS_CONSOLE_INDEX_0);

            if (SYS_STATUS_READY == WDRV_WINC_Status(sysObj.drvWifiWinc))
            {
                app_wifi_access_point_controllerData.state = APP_WIFI_ACCESS_POINT_CONTROLLER_STATE_INIT_READY;
            }

            break;
        }

        case APP_WIFI_ACCESS_POINT_CONTROLLER_STATE_INIT_READY:
        {
            wdrvHandle = WDRV_WINC_Open(0, 0);

            if (DRV_HANDLE_INVALID != wdrvHandle)
            {
                APP_ExampleInitialize(wdrvHandle);
                app_wifi_access_point_controllerData.state = APP_WIFI_ACCESS_POINT_CONTROLLER_WDRV_OPEN;
            }
            break;
        }

        case APP_WIFI_ACCESS_POINT_CONTROLLER_WDRV_OPEN:
        {
            if (!getActivateWifiAp()) {
                app_wifi_access_point_controllerData.state = APP_WIFI_ACCESS_POINT_CONTROLLER_WAIT_FOR_BUTTON_PRESS;
                break;
            }
            
            APP_ExampleTasks(wdrvHandle);
            break;
        }

        default:
        {
            /* TODO: Handle error in application's state machine. */
            break;
        }
    }
}


/*******************************************************************************
 End of File
 */
