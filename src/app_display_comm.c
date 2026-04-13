/*******************************************************************************
  MPLAB Harmony Application Source File

  Company:
    Microchip Technology Inc.

  File Name:
    app_display_comm.c

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
#include <stddef.h>                     // Defines NULL
#include <stdbool.h>                    // Defines true
#include <stdlib.h>                     // Defines EXIT_FAILURE
#include <string.h>
#include "definitions.h"                // SYS function prototypes

#include "app_display_comm.h"
#include "files\modbus\display.h"
#include "files\modbus\shiftregisters.h"
#include "files\modbus\modbus.h"
#include "files\alarms.h"
#include "files\time_counters.h"
#include "files\logging.h"

// *****************************************************************************
// *****************************************************************************
// Section: Global Data Definitions
// *****************************************************************************
// *****************************************************************************

// *****************************************************************************

APP_DISPLAY_COMM_DATA app_display_commData;

static uint8_t TxBuffer[TX_BUFFER_DISPLAY_SIZE];
static uint8_t RxBuffer[RX_BUFFER_DISPLAY_SIZE];

static volatile uint32_t ResponseDelay = 0;
static volatile uint32_t CommunicationWindowSecondCounter = 0;
static volatile uint32_t CommunicationTimeOutCounter = 0;


// *****************************************************************************
// *****************************************************************************
// Section: Application Callback Functions
// *****************************************************************************
// *****************************************************************************

//static void APP_WriteCallbackDisplay(DMAC_TRANSFER_EVENT event, uintptr_t contextHandle)
void APP_WriteCallbackDisplay(uintptr_t context)
{
    //if (event == DMAC_TRANSFER_EVENT_COMPLETE)
    //{
        app_display_commData.commStatus = DISPLAY_COMM_STATUS_DATA_SENT_TO_DISPLAY;
    //}
}


static bool IsExactFFFrame(const uint8_t *buf)
{
    static const uint8_t frame[8] = {0xFF, 0x03, 0x00, 0x00, 0x00, 0x1E, 0xD0, 0x1C};

    if (buf == NULL)
    {
        return false;
    }

    return (memcmp(buf, frame, 8) == 0);
}


//static void APP_ReadCallbackDisplay(DMAC_TRANSFER_EVENT event, uintptr_t contextHandle)
void APP_ReadCallbackDisplay(uintptr_t context)
{
    //if (event == DMAC_TRANSFER_EVENT_COMPLETE)
    //{
        if(SERCOM1_USART_ErrorGet() == USART_ERROR_NONE)
        {   // ErrorGet clears errors, set error flag to notify console
            //errorStatus = true;
            
            switch (app_display_commData.commStatus)
            {
                case DISPLAY_COMM_STATUS_WAITING_FOR_DATA_FROM_DISPLAY:
                {   // Eerste byte ontvangen, kijken wat dit is
                    if (RxBuffer[0] == THIS_DEVICE_ADDRESS || RxBuffer[0] <= 16)
                    {   // Als het het address is van de modbus slave, vraag om de 7 andere bytes die hierna komen.
                        app_display_commData.commStatus = DISPLAY_COMM_STATUS_DEVICE_ADDRESS_RECEIVED;
                        SERCOM1_USART_Read(&RxBuffer[1], 7);
                        /*DMAC_ChannelTransfer(DMAC_CHANNEL_2, \
                            (const void *)&SERCOM1_REGS->USART_INT.SERCOM_DATA, \
                            &RxBuffer[1], 7);*/
                    }
                    else if (RxBuffer[0] == POWER_CONSUMPTION_FRAME)
                    {
                        POWER_CONSUMPTION_RESPONSE_TO_DISPLAY = true;
                        POWER_CONSUMPTION_TO_HEATPUMP = true;
                        app_display_commData.commStatus = DISPLAY_COMM_STATUS_104_RECEIVED;
                        SERCOM1_USART_Read(&RxBuffer[1], 15);

                        //SYS_CONSOLE_PRINT("RECEIVED DLT645 FRAME FROM DISPLAY\r\n");
                    }
                    else if (RxBuffer[0] == 0xFF)
                    {   
                        app_display_commData.commStatus = DISPLAY_COMM_STATUS_255_RECEIVED;
                        
                        SERCOM1_USART_Read(&RxBuffer[1], 7);
                    }
                    else
                    {   
                        SERCOM1_USART_Read(&RxBuffer[0], 1);
                    }
                    break;
                }
                case DISPLAY_COMM_STATUS_DEVICE_ADDRESS_RECEIVED:
                {
                    app_display_commData.commStatus = DISPLAY_COMM_STATUS_DATA_RECEIVED_FROM_DISPLAY;
                    break;
                }
                case DISPLAY_COMM_STATUS_104_RECEIVED:
                {
                    app_display_commData.commStatus = DISPLAY_COMM_STATUS_DATA_RECEIVED_FROM_DISPLAY;
                                            // Print full frame (16 bytes total)
//                    SYS_CONSOLE_PRINT("DLT645 RX: ");
//                    for (uint8_t i = 0; i < 16; i++)
//                    {
//                        SYS_CONSOLE_PRINT("%02X ", RxBuffer[i]);
//                     }
//                     SYS_CONSOLE_PRINT("\r\n");
                    break;
                }
                case DISPLAY_COMM_STATUS_255_RECEIVED:
                {
                    app_display_commData.commStatus = DISPLAY_COMM_STATUS_UNKNOWN_DATA_RECEIVED_FROM_DISPLAY;
//                    if (IsExactFFFrame(RxBuffer) == true)
//                    {
//                        //SYS_CONSOLE_PRINT("VALID FF FRAME RECEIVED\r\n");
//                        //PAUSE_FRAME_TRIGGERED = true;
//                        app_display_commData.commStatus = DISPLAY_COMM_STATUS_DATA_RECEIVED_FROM_DISPLAY;
//                    }
//                    else
//                    {
//                        //SYS_CONSOLE_PRINT("INVALID FF FRAME\r\n");
//                        app_display_commData.commStatus =  DISPLAY_COMM_STATUS_WAITING_FOR_DATA_FROM_DISPLAY;
//                        SERCOM1_USART_Read(&RxBuffer[0], 1);
//                    }
                    break;                    

                }
                default:
                {
                    break;
                }   
            }
        }
        else{}
    //}
}
/*
static void TC1_Callback_InterruptHandler(TC_TIMER_STATUS status, uintptr_t context)
{
    CommunicationWindowSecondCounter++;
    
    if (ResponseDelay < UINT32_MAX)
        ResponseDelay++;    
    
    if (CommunicationWindowSecondCounter < UINT32_MAX)
        CommunicationWindowSecondCounter++;
    
    if (CommunicationTimeOutCounter < UINT32_MAX)
        CommunicationTimeOutCounter++;
}
*/
// *****************************************************************************
// *****************************************************************************
// Section: Application Local Functions
// *****************************************************************************
// *****************************************************************************

//void StartReceivingDataFromDisplay(void)
void StartReceivingDataFromDisplay(void)
{
    SetOutput(LED_RX_DISPLAY, true);
    
    app_display_commData.commStatus = DISPLAY_COMM_STATUS_WAITING_FOR_DATA_FROM_DISPLAY;
    
    SERCOM1_USART_Read(&RxBuffer[0], 1);
    /*
    DMAC_ChannelTransfer(DMAC_CHANNEL_2, \
        (const void *)&SERCOM1_REGS->USART_INT.SERCOM_DATA, \
        &RxBuffer[0], 1);*/
}

//void StartTransmittingDataToDisplay(uint8_t sizeOfBuffer)
void StartTransmittingDataToDisplay(uint8_t sizeOfBuffer)
{
    SetOutput(LED_TX_DISPLAY, true);

    app_display_commData.commStatus = DISPLAY_COMM_STATUS_SENDING_DATA_TO_DISPLAY;

//        if (POWER_CONSUMPTION_FRAME_TRIGGERED)
//        {
//            uint8_t powerResponse[] = {
//                0xFE, 0xFE, 0xFE, 0xFE,
//                0x68,
//                0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
//                0x68,
//                0x91,
//                0x23,
//                0x33, 0x34, 0x42, 0x35,
//                0x3B, 0x56, 0x67, 0x56, 0x73, 0x56, 0xA7, 0x55,
//                0x33, 0xA7, 0x53, 0x33, 0x83, 0x5C, 0x33, 0x57,
//                0x64, 0x34, 0x4A, 0x74, 0x33, 0x8A, 0x68, 0x33,
//                0x7C, 0x87, 0x33, 0x93, 0x78, 0x33, 0x33,
//                0xF9,
//                0x16
//            };
//    
//            SERCOM1_USART_Write(powerResponse, sizeof(powerResponse));
//    
//            POWER_CONSUMPTION_FRAME_TRIGGERED = false; // reset flag
//    
//            return;
//        }

    // Default behavior (Modbus etc.)
    SERCOM1_USART_Write(&TxBuffer[0], sizeOfBuffer);
}
//void StartTransmittingDataToDisplay(uint8_t sizeOfBuffer)
//{
//    SetOutput(LED_TX_DISPLAY, true);
//    
//    app_display_commData.commStatus = DISPLAY_COMM_STATUS_SENDING_DATA_TO_DISPLAY;
//    
//    SERCOM1_USART_Write(&TxBuffer[0], sizeOfBuffer);
//}

// *****************************************************************************
// *****************************************************************************
// Section: Application Initialization and State Machine Functions
// *****************************************************************************
// *****************************************************************************

/*******************************************************************************
  Function:
    void APP_DISPLAY_COMM_Initialize ( void )

  Remarks:
    See prototype in app_display_comm.h.
 */

void APP_DISPLAY_COMM_Initialize ( void )
{
    // DMAC_ChannelCallbackRegister(DMAC_CHANNEL_2, APP_ReadCallbackDisplay, 0);
    // DMAC_ChannelCallbackRegister(DMAC_CHANNEL_1, APP_WriteCallbackDisplay, 0);
    
    SERCOM1_USART_WriteCallbackRegister(APP_WriteCallbackDisplay, 0);
    SERCOM1_USART_ReadCallbackRegister(APP_ReadCallbackDisplay, 0);
    
    //TC1_TimerCallbackRegister(TC1_Callback_InterruptHandler, (uintptr_t)NULL);
    //TC1_TimerStart();
    
    InitShiftRegisters();
    SetOutput(LED_RX_DISPLAY, false);
    SetOutput(LED_TX_DISPLAY, false);
   
    memset(TxBuffer, 0, TX_BUFFER_DISPLAY_SIZE);
    memset(RxBuffer, 0, RX_BUFFER_DISPLAY_SIZE);
    
    app_display_commData.commStatus = DISPLAY_COMM_STATUS_IDLE;
    app_display_commData.state = APP_DISPLAY_COMM_STATE_INIT;
}

/******************************************************************************
  Function:
    void APP_DISPLAY_COMM_Tasks ( void )

  Remarks:
    See prototype in app_display_comm.h.
 */

void APP_DISPLAY_COMM_Tasks ( void )
{
    if (CommunicationWindowSecondCounter >= RESET_AFTER_NO_COMMUNICATION_SECONDS)
    {   // Tijd lang geen communicatie geweest
        CommunicationWindowSecondCounter = 0;
        
        SERCOM1_USART_Initialize();
        SERCOM1_USART_WriteCallbackRegister(APP_WriteCallbackDisplay, 0);
        SERCOM1_USART_ReadCallbackRegister(APP_ReadCallbackDisplay, 0);
        
        app_display_commData.state = APP_DISPLAY_COMM_STATE_RECEIVE_DATA;
    }  
    
    if (CommunicationTimeOutCounter > RESET_AFTER_NO_COMMUNICATION_SECONDS) {
        SetOrClearAlarm(ALARM_DISPLAY_COMMUNICATION, SET_ALARM);
    
    } else {
        SetOrClearAlarm(ALARM_DISPLAY_COMMUNICATION, CLEAR_ALARM);
    }
    
    if (DisplayCommunicationTimerExpired() == true)
    {
        CommunicationWindowSecondCounter++;
    
        if (ResponseDelay < UINT32_MAX)
            ResponseDelay++;    

        if (CommunicationWindowSecondCounter < UINT32_MAX)
            CommunicationWindowSecondCounter++;

        if (CommunicationTimeOutCounter < UINT32_MAX)
            CommunicationTimeOutCounter++;
    }
    
    /* Check the application's current state. */
    switch ( app_display_commData.state )
    {
        // 1: State INIT
        case APP_DISPLAY_COMM_STATE_INIT:
        {
            memset(TxBuffer, 0, TX_BUFFER_DISPLAY_SIZE);
            memset(RxBuffer, 0, RX_BUFFER_DISPLAY_SIZE);
            
            SetOutput(LED_RX_DISPLAY, false);
            SetOutput(LED_TX_DISPLAY, false);
            
            app_display_commData.commStatus = DISPLAY_COMM_STATUS_IDLE;
            app_display_commData.state = APP_DISPLAY_COMM_STATE_RECEIVE_DATA;
            break;
        }
        // 2: State start receive display data
        case APP_DISPLAY_COMM_STATE_RECEIVE_DATA:
        {
            StartReceivingDataFromDisplay();
            //CommunicationWindowSecondCounter = 0;
            
            app_display_commData.state = APP_DISPLAY_COMM_STATE_WAIT_FOR_DATA_RECEIVED;
            break;
        }
        // 3: State wait for data
        case APP_DISPLAY_COMM_STATE_WAIT_FOR_DATA_RECEIVED:
        {
            if (app_display_commData.commStatus == DISPLAY_COMM_STATUS_DATA_RECEIVED_FROM_DISPLAY)
            {
                SetOutput(LED_RX_DISPLAY, false);
                app_display_commData.state = APP_DISPLAY_COMM_STATE_CHECKSUM_CHECK;
            }
            else if (app_display_commData.commStatus == DISPLAY_COMM_STATUS_UNKNOWN_DATA_RECEIVED_FROM_DISPLAY)
            {
                app_display_commData.state = APP_DISPLAY_COMM_STATE_INIT;
            }
            else{}
            break;
        }
        // 4: State check checksum
        case APP_DISPLAY_COMM_STATE_CHECKSUM_CHECK:
        { 
        //            if (RxBuffer[0] == 0x68) 
        //            {
        //                //SYS_CONSOLE_PRINT("DLT645 RX: CHECKSUM \r\n");
        //                POWER_CONSUMPTION_FRAME_TRIGGERED = true;
        //                app_display_commData.state = APP_DISPLAY_COMM_STATE_PARSE_DATA;
        //            } 
            if (ChecksumCheck(&RxBuffer[0], 8) == true)
            {
                app_display_commData.state = APP_DISPLAY_COMM_STATE_PARSE_DATA;
            } 
            else
            {
                app_display_commData.state = APP_DISPLAY_COMM_STATE_INIT;
            }
            break;
        }
        // 5: State parse data
        case APP_DISPLAY_COMM_STATE_PARSE_DATA:
        {
            if (RxBuffer[0] != 0x68) {
                ParseDisplayData(&RxBuffer[0]);
            }  

            ResponseDelay = 0;
            app_display_commData.state = APP_DISPLAY_COMM_STATE_DELAY;

            break;
        }
        // 6: State delay
        case APP_DISPLAY_COMM_STATE_DELAY:
        {
            if (ResponseDelay >= 3)
            {   // Wacht 500 ms
                app_display_commData.state = APP_DISPLAY_COMM_STATE_PREPARING_DATA_TO_SENT;
            }
            break;
        }
        // 7: State Preparing data to be sent
        case APP_DISPLAY_COMM_STATE_PREPARING_DATA_TO_SENT:
        {

            GetDataFromHeatpump();
            app_display_commData.state = APP_DISPLAY_COMM_STATE_SEND_DATA;
            break;
        }
        // 8: Start sending data to display
        case APP_DISPLAY_COMM_STATE_SEND_DATA:
        {            
            StartTransmittingDataToDisplay(FillTransmitBuffer(&TxBuffer[0], &RxBuffer[0]));
            
            app_display_commData.state = APP_DISPLAY_COMM_STATE_WAIT_FOR_DATA_SENT;
            break;
        }
        // 9: Start wait for data to be sent to display
        case APP_DISPLAY_COMM_STATE_WAIT_FOR_DATA_SENT:
        {
            if (app_display_commData.commStatus == DISPLAY_COMM_STATUS_DATA_SENT_TO_DISPLAY)
            {   
                SetOutput(LED_TX_DISPLAY, false);
                CommunicationWindowSecondCounter = 0;
                CommunicationTimeOutCounter = 0;
                app_display_commData.state = APP_DISPLAY_COMM_STATE_INIT;
            }
            else{}
            break;
        }
        /* TODO: implement your application state machine.*/


        /* The default state should never be executed. */
        default:
        {
            app_display_commData.state = APP_DISPLAY_COMM_STATE_INIT;
            break;
        }
    }
}