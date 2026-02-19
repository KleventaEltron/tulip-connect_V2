/*******************************************************************************
  MPLAB Harmony Application Source File

  Company:
    Microchip Technology Inc.

  File Name:
    app_heatpump_comm.c

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

#include "app_heatpump_comm.h"
//#include "files\modbus\display_parameters.h"
#include "files\modbus\heatpump.h"
#include "files\modbus\shiftregisters.h"
#include "files\modbus\modbus.h"
#include "files\alarms.h"
#include "files\time_counters.h"
#include "files/logging.h"
#include "files/states.h"
#include "files\eeprom.h"


APP_HEATPUMP_COMM_DATA app_heatpump_commData;

static uint8_t TxBuffer[TX_BUFFER_DISPLAY_SIZE];
static uint8_t RxBuffer[RX_BUFFER_DISPLAY_SIZE];

static volatile uint32_t ResponseDelay = 0;
static volatile uint32_t CommunicationWindowSecondCounter = 0;
static volatile uint32_t CommunicationTimeOutCounter = 0;



static uint8_t LastSentModbusAddress = UINT8_MAX;




void StartReceivingDataFromHeatpump(void) {
    SetOutput(LED_RX_HEATPUMP, true);
    app_heatpump_commData.commStatus = HEATPUMP_COMM_STATUS_WAITING_FOR_DATA_FROM_HEATPUMP;
    // Start a single byte read operation
    SERCOM7_USART_Read(&RxBuffer[0], 1);
}





void StartTransmittingDataToHeatpump(uint8_t * txBufferHeatpump) {
    SetOutput(LED_TX_HEATPUMP, true);
    app_heatpump_commData.commStatus = HEATPUMP_COMM_STATUS_SENDING_DATA_TO_HEATPUMP;
    if (txBufferHeatpump[0] == 104) {
        SERCOM7_USART_Write(&TxBuffer[0], 16);
    } else {
        SERCOM7_USART_Write(&TxBuffer[0], 8);
    }
}





void APP_WriteCallback(uintptr_t context) {
    app_heatpump_commData.commStatus = HEATPUMP_COMM_STATUS_DATA_SENT_TO_HEATPUMP;
}





void APP_ReadCallback(uintptr_t context)
{
    if(SERCOM7_USART_ErrorGet() != USART_ERROR_NONE)
    {
        /* ErrorGet clears errors, set error flag to notify console */
    }
    else
    {
        switch (app_heatpump_commData.commStatus)
            {
                case HEATPUMP_COMM_STATUS_WAITING_FOR_DATA_FROM_HEATPUMP:
                {   // Eerste byte ontvangen, kijken wat dit is
                    if (RxBuffer[MODBUS_ADDRESS_INDEX] == LastSentModbusAddress)
                    {   // Als het het address is van de slave, vraag om de 7 andere bytes die hierna komen.
                        app_heatpump_commData.commStatus = HEATPUMP_COMM_STATUS_DEVICE_ADDRESS_RECEIVED;
                        SERCOM7_USART_Read(&RxBuffer[MODBUS_COMMAND_INDEX], 7);
                    }
                    else
                    {   // Wacht op volgende eerste byte
                        SERCOM7_USART_Read(&RxBuffer[MODBUS_ADDRESS_INDEX], 1);
                    }
                    break;
                }
                case HEATPUMP_COMM_STATUS_DEVICE_ADDRESS_RECEIVED:
                {
                    if (RxBuffer[MODBUS_COMMAND_INDEX] == MB_FC_WRITE_REG)
                    {   // Alle data ontvangen
                        app_heatpump_commData.commStatus = HEATPUMP_COMM_STATUS_DATA_RECEIVED_FROM_HEATPUMP;
                    }
                    else if (RxBuffer[MODBUS_COMMAND_INDEX] == MB_FC_READ_REGS)
                    {   // Vraag wat nog komt
                        app_heatpump_commData.commStatus = HEATPUMP_COMM_STATUS_FIRST_8_BYTES_RECEIVED;
                        SERCOM7_USART_Read(&RxBuffer[8], (RxBuffer[MODBUS_BYTES_RETURNED_INDEX] - 3)); 
                    }
                    else{}
                    
                    break;
                }
                case HEATPUMP_COMM_STATUS_FIRST_8_BYTES_RECEIVED:
                {
                    app_heatpump_commData.commStatus = HEATPUMP_COMM_STATUS_DATA_RECEIVED_FROM_HEATPUMP;
                    break;
                }
                default:
                {
                    break;
                }   
            }        
    }
}





void APP_HEATPUMP_COMM_Initialize ( void )
{
    SmartEepromInit();
    
    // Make sure arrays contain known values
    SetDataInArraysAtStartup();
    
    //DMAC_ChannelCallbackRegister(DMAC_CHANNEL_3, APP_ReadCallbackHeatpump, 0);
    //DMAC_ChannelCallbackRegister(DMAC_CHANNEL_0, APP_WriteCallbackHeatpump, 0);
    
    SERCOM7_USART_WriteCallbackRegister(APP_WriteCallback, 0);
    SERCOM7_USART_ReadCallbackRegister(APP_ReadCallback, 0);
    //TC4_TimerCallbackRegister(TC4_Callback_InterruptHandler, (uintptr_t)NULL);
    //TC4_TimerStart();

    InitShiftRegisters();
    SetOutput(LED_RX_HEATPUMP, false);
    SetOutput(LED_TX_HEATPUMP, false);
    
    memset(TxBuffer, 0, TX_BUFFER_HEATPUMP_SIZE);
    memset(RxBuffer, 0, RX_BUFFER_HEATPUMP_SIZE);
    
    // Change password setting at startup:
    //if (Setting.settingStatus == SETTING_SEND_STATUS_IDLE)
    //{
    //    ChangeHeatpumpSetting(ADDRESS_PARAMETER_PASSWORD_SETTING, 255); // Set parameters password to 255
    //}
    
    if (ReadSmartEeprom8(SEEP_ADDR_CONNECT_HAS_EVER_CONTACTED_HEATPUMP) == false)
    {   // Connect has never communicated with an heatpump before
        setDoFirstTimeHeatpumpCommunicationSettings(true);
    }
    
    CheckHeatpumpStaticSettings();
    
    app_heatpump_commData.commStatus = HEATPUMP_COMM_STATUS_IDLE;
    app_heatpump_commData.state = APP_HEATPUMP_COMM_STATE_INIT;
}





void APP_HEATPUMP_COMM_Tasks ( void )
{
    //static uint8_t startupSettingsCounter = 0;
    
    if (CommunicationWindowSecondCounter >= RESET_AFTER_NO_COMMUNICATION_SECONDS)
    {   // Tijd lang geen communicatie geweest
        CommunicationWindowSecondCounter = 0;
        //ManualToInitCounter++;
        
        //while(true); // Laatste redmiddel, zorg ervoor dat watchdog ingaat en print gereset wordt
        app_heatpump_commData.state = APP_HEATPUMP_COMM_STATE_INIT;
        
        SERCOM7_USART_Initialize();
            
        SERCOM7_USART_WriteCallbackRegister(APP_WriteCallback, 0);
        SERCOM7_USART_ReadCallbackRegister(APP_ReadCallback, 0);
    } 
    
    if (CommunicationTimeOutCounter > RESET_AFTER_NO_COMMUNICATION_SECONDS)
    {
        SetOrClearAlarm(ALARM_HEATPUMP_COMMUNICATION, SET_ALARM);
    }
    else
    {
        SetOrClearAlarm(ALARM_HEATPUMP_COMMUNICATION, CLEAR_ALARM);
    }
    
    if (HeatpumpCommunicationTimerExpired() == true)
    {
        if (ResponseDelay < UINT32_MAX) {
            ResponseDelay++;    
        }
    
        if (CommunicationWindowSecondCounter < UINT32_MAX) {
            CommunicationWindowSecondCounter++;
        }

        if (CommunicationTimeOutCounter < UINT32_MAX) {
            CommunicationTimeOutCounter++;
        }
        
        // Every 30 seconds the static settings are checked
        CheckHeatpumpStaticSettings();
        //FillBufferWithStartupSettings(doFirstTimeHeatpumpCommunicationSettings);
    }
    
    /* Check the application's current state. */
    switch ( app_heatpump_commData.state )
    {
        // 1: Init
        case APP_HEATPUMP_COMM_STATE_INIT:
        {
            //if(setLoggingLock()){
                memset(TxBuffer, 0, TX_BUFFER_HEATPUMP_SIZE);
                memset(RxBuffer, 0, RX_BUFFER_HEATPUMP_SIZE);
            
                 //LedStatus_Set();
            
                SetOutput(LED_RX_HEATPUMP, false);
                SetOutput(LED_TX_HEATPUMP, false);
                
                LastSentModbusAddress = UINT8_MAX;
            
                //startupSettingsCounter = FillBufferWithStartupSettings(doFirstTimeHeatpumpCommunicationSettings);
                
                //while(!releaseLoggingLock());
                app_heatpump_commData.commStatus = DISPLAY_COMM_STATUS_IDLE;
                app_heatpump_commData.state = APP_HEATPUMP_COMM_STATE_SEND_DATA;
            //}	
            break;
        }
        // 2: State start sending data
        case APP_HEATPUMP_COMM_STATE_SEND_DATA:
        {            
            //memcpy(&TxBuffer[0], testArray, 8);
            
            //if(setLoggingLock()){
            // Fill buffer with setting to send or with reading data request
                FillTxBuffer(&TxBuffer[0]);
                
                LastSentModbusAddress = TxBuffer[MODBUS_ADDRESS_INDEX];
                
                StartTransmittingDataToHeatpump(&TxBuffer[0]);
                //while(!releaseLoggingLock());
                app_heatpump_commData.state = APP_HEATPUMP_COMM_STATE_WAIT_FOR_DATA_SENT;
            //}
            
            break;
        }
        // 3: State wachten tot data verzonden is
        case APP_HEATPUMP_COMM_STATE_WAIT_FOR_DATA_SENT:
        {          
            if (app_heatpump_commData.commStatus == HEATPUMP_COMM_STATUS_DATA_SENT_TO_HEATPUMP)
            {
                SetOutput(LED_TX_HEATPUMP, false);
                app_heatpump_commData.state = APP_HEATPUMP_COMM_STATE_RECEIVE_DATA;
            }
            else{}
            break;
        }
        // 4: Start met ontvangen data
        case APP_HEATPUMP_COMM_STATE_RECEIVE_DATA:
        {            
            StartReceivingDataFromHeatpump();
            app_heatpump_commData.state = APP_HEATPUMP_COMM_STATE_WAIT_FOR_DATA_RECEIVED;
            break;
        }
        // 5: Wachten tot data ontvangen is
        case APP_HEATPUMP_COMM_STATE_WAIT_FOR_DATA_RECEIVED:
        {            
            if (app_heatpump_commData.commStatus == HEATPUMP_COMM_STATUS_DATA_RECEIVED_FROM_HEATPUMP)
            {
                SetOutput(LED_RX_HEATPUMP, false);
                
                app_heatpump_commData.state = APP_HEATPUMP_COMM_STATE_CHECKSUM_CHECK;
            }
            else{}
            break;
        }
        // 6: Checksum check
        case APP_HEATPUMP_COMM_STATE_CHECKSUM_CHECK:
        {            
            if (ChecksumCheck(&RxBuffer[0], CalculateModbusBufferSize(&RxBuffer[0])) == true)
            {
                app_heatpump_commData.state = APP_HEATPUMP_COMM_STATE_PARSE_DATA;
            }
            else
            {
                app_heatpump_commData.state = APP_HEATPUMP_COMM_STATE_INIT;
            }
            break;
        }
        // 7: Parse data
        case APP_HEATPUMP_COMM_STATE_PARSE_DATA:
        {            
            //if(setLoggingLock()){
                ParseHeatpumpData(&TxBuffer[0], &RxBuffer[0]);
                
                ResponseDelay = 0;
                CommunicationWindowSecondCounter = 0;
                CommunicationTimeOutCounter = 0;
                
                //if ((doFirstTimeHeatpumpCommunicationSettings == true) && (startupSettingsCounter == UINT8_MAX))
                if ((getDoFirstTimeHeatpumpCommunicationSettings()))
                {
                    uint16_t cascadeMask = getCascadeSlaveStatus();
                    if (cascadeMask != UINT16_MAX) {
                        FillBufferWithStartupSettings(getDoFirstTimeHeatpumpCommunicationSettings());
                        setDoFirstTimeHeatpumpCommunicationSettings(false);
                        WriteSmartEeprom8(SEEP_ADDR_CONNECT_HAS_EVER_CONTACTED_HEATPUMP, true);
                    }
                }
                //while(!releaseLoggingLock()){}

                app_heatpump_commData.state = APP_HEATPUMP_COMM_STATE_DELAY;
            //}
            
            break;
        }
        // 8: Delay
        case APP_HEATPUMP_COMM_STATE_DELAY:
        {            
            if (ResponseDelay >= 5)
            {
                app_heatpump_commData.state = APP_HEATPUMP_COMM_STATE_INIT;
            }
            
            break;
        }
        /* TODO: implement your application state machine.*/

        /* The default state should never be executed. */
        default:
        {
            app_heatpump_commData.state = APP_HEATPUMP_COMM_STATE_INIT;
            /* TODO: Handle error in application's state machine. */
            break;
        }
    }
}


/*******************************************************************************
 End of File
 */
