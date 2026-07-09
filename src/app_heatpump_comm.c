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

//static uint8_t TxBuffer[TX_BUFFER_DISPLAY_SIZE];
//static uint8_t RxBuffer[RX_BUFFER_DISPLAY_SIZE];
static uint8_t TxBuffer[TX_BUFFER_HEATPUMP_SIZE];
static uint8_t RxBuffer[RX_BUFFER_HEATPUMP_SIZE];

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

    if (txBufferHeatpump[0] == 0x68) {
        //SYS_CONSOLE_PRINT("SENDING DLT645 FRAME TO PUMP\r\n"); 
        SERCOM7_USART_Write(&TxBuffer[0], 16);
        return;
    }  

    SERCOM7_USART_Write(&TxBuffer[0], 8);
    return;
}





void APP_WriteCallback(uintptr_t context) {
    app_heatpump_commData.commStatus = HEATPUMP_COMM_STATUS_DATA_SENT_TO_HEATPUMP;
}





void APP_ReadCallback(uintptr_t context)
{
    if (SERCOM7_USART_ErrorGet() != USART_ERROR_NONE)
    {
        DLT645_FrameClear();

        app_heatpump_commData.state = APP_HEATPUMP_COMM_STATE_INIT;
        app_heatpump_commData.commStatus = HEATPUMP_COMM_STATUS_IDLE;
        LastSentModbusAddress = UINT8_MAX;

        return;
    }

    switch (app_heatpump_commData.commStatus)
    {
        case HEATPUMP_COMM_STATUS_WAITING_FOR_DATA_FROM_HEATPUMP:
        {
            /* Detect protocol from first byte */
            if (RxBuffer[0] == 0x68U)
            {
                //SYS_CONSOLE_PRINT("RECEIVED POSSIBLE DLT645 FRAME FROM PUMP\r\n");

                /* We already have byte 0, now read bytes 1..9
                   so we can inspect second 0x68 and length byte */
                app_heatpump_commData.commStatus = HEATPUMP_COMM_STATUS_DLT645_HEADER_RECEIVED;
                SERCOM7_USART_Read(&RxBuffer[1], 9);
            }
            else if (RxBuffer[MODBUS_ADDRESS_INDEX] == LastSentModbusAddress)
            {
                /* Modbus slave address matched */
                app_heatpump_commData.commStatus = HEATPUMP_COMM_STATUS_DEVICE_ADDRESS_RECEIVED;
                SERCOM7_USART_Read(&RxBuffer[MODBUS_COMMAND_INDEX], 7);
            }
            else
            {
                /* Wait for next first byte */
                SERCOM7_USART_Read(&RxBuffer[MODBUS_ADDRESS_INDEX], 1);
            }
            break;
        }

        case HEATPUMP_COMM_STATUS_DEVICE_ADDRESS_RECEIVED:
        {
            if (RxBuffer[MODBUS_COMMAND_INDEX] == MB_FC_WRITE_REG)
            {
                app_heatpump_commData.commStatus = HEATPUMP_COMM_STATUS_DATA_RECEIVED_FROM_HEATPUMP;
            }
            else if (RxBuffer[MODBUS_COMMAND_INDEX] == MB_FC_READ_REGS)
            {
                app_heatpump_commData.commStatus = HEATPUMP_COMM_STATUS_FIRST_8_BYTES_RECEIVED;

                /* For Modbus read response:
                   total is addr + fc + bytecount + data + crc(2)
                   here first 8 bytes are already partly read */
                //SERCOM7_USART_Read(&RxBuffer[8], (RxBuffer[MODBUS_BYTES_RETURNED_INDEX] - 3));
                uint8_t byteCount = RxBuffer[MODBUS_BYTES_RETURNED_INDEX];

                if (byteCount < 3 || byteCount > (RX_BUFFER_HEATPUMP_SIZE - 5)) {
                    app_heatpump_commData.commStatus = HEATPUMP_COMM_STATUS_WAITING_FOR_DATA_FROM_HEATPUMP;
                    SERCOM7_USART_Read(&RxBuffer[0], 1);
                    break;
                }

                SERCOM7_USART_Read(&RxBuffer[8], byteCount - 3);
            }
            else
            {
                /* Unknown Modbus function, restart */
                app_heatpump_commData.commStatus = HEATPUMP_COMM_STATUS_WAITING_FOR_DATA_FROM_HEATPUMP;
                SERCOM7_USART_Read(&RxBuffer[0], 1);
            }
            break;
        }

        case HEATPUMP_COMM_STATUS_FIRST_8_BYTES_RECEIVED:
        {
            app_heatpump_commData.commStatus = HEATPUMP_COMM_STATUS_DATA_RECEIVED_FROM_HEATPUMP;
            break;
        }

        case HEATPUMP_COMM_STATUS_DLT645_HEADER_RECEIVED:
        {
            /* We now have bytes 0..9 */
            if (RxBuffer[0] == 0x68U && RxBuffer[7] == 0x68U)
            {
                uint8_t dataLen = RxBuffer[9];

                /*
                 * Full DLT645 frame length:
                 * bytes 0..7  = address/header
                 * byte 8      = control
                 * byte 9      = data length
                 * dataLen     = data bytes
                 * byte        = checksum
                 * byte        = 0x16 end
                 *
                 * Total = 12 + dataLen
                 */
                uint16_t frameLen = 12U + dataLen;

                if (frameLen > RX_BUFFER_HEATPUMP_SIZE)
                {
                    SYS_CONSOLE_PRINT("DLT645 frame too large: %u\r\n", frameLen);

                    DLT645_FrameClear();
                    app_heatpump_commData.commStatus =
                        HEATPUMP_COMM_STATUS_WAITING_FOR_DATA_FROM_HEATPUMP;

                    SERCOM7_USART_Read(&RxBuffer[0], 1);
                    break;
                }

                /*
                 * We already received bytes 0..9.
                 * Remaining bytes are:
                 * dataLen data bytes + checksum + end byte = dataLen + 2
                 */
                uint16_t remainingLen = dataLen + 2U;

                app_heatpump_commData.commStatus =
                    HEATPUMP_COMM_STATUS_DLT645_REST_RECEIVED;

                SERCOM7_USART_Read(&RxBuffer[10], remainingLen);
            }
            else
            {
                SYS_CONSOLE_PRINT("INVALID DLT645 HEADER\r\n");

                DLT645_FrameClear();
                app_heatpump_commData.commStatus =
                    HEATPUMP_COMM_STATUS_WAITING_FOR_DATA_FROM_HEATPUMP;

                SERCOM7_USART_Read(&RxBuffer[0], 1);
            }

            break;
        }

        case HEATPUMP_COMM_STATUS_DLT645_REST_RECEIVED:
        {
            uint8_t dataLen = RxBuffer[9];
            uint16_t frameLen = 12U + dataLen;

            if (frameLen > RX_BUFFER_HEATPUMP_SIZE)
            {
                SYS_CONSOLE_PRINT("DLT645 frame length invalid after receive: %u\r\n", frameLen);

                DLT645_FrameClear();
                app_heatpump_commData.commStatus =
                    HEATPUMP_COMM_STATUS_WAITING_FOR_DATA_FROM_HEATPUMP;

                SERCOM7_USART_Read(&RxBuffer[0], 1);
                break;
            }

            if (RxBuffer[frameLen - 1U] == 0x16U)
            {
                if (DLT645_FrameStore(RxBuffer, frameLen) == true)
                {
                    app_heatpump_commData.commStatus =
                        HEATPUMP_COMM_STATUS_DATA_RECEIVED_FROM_HEATPUMP;
                }
                else
                {
                    DLT645_FrameClear();
                    app_heatpump_commData.commStatus =
                        HEATPUMP_COMM_STATUS_WAITING_FOR_DATA_FROM_HEATPUMP;

                    SERCOM7_USART_Read(&RxBuffer[0], 1);
                }
            }
            else
            {
                DLT645_FrameClear();
                app_heatpump_commData.commStatus =
                    HEATPUMP_COMM_STATUS_WAITING_FOR_DATA_FROM_HEATPUMP;

                SERCOM7_USART_Read(&RxBuffer[0], 1);
            }

            break;
        }

        default:
        {
            break;
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
        memset(TxBuffer, 0, TX_BUFFER_HEATPUMP_SIZE);
        memset(RxBuffer, 0, RX_BUFFER_HEATPUMP_SIZE);

        app_heatpump_commData.state = APP_HEATPUMP_COMM_STATE_INIT;
        app_heatpump_commData.commStatus = HEATPUMP_COMM_STATUS_IDLE;
        LastSentModbusAddress = UINT8_MAX;
        //activeProtocol = HEATPUMP_PROTOCOL_NONE;

        DLT645_FrameClear();

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
            ResponseDelay = 0;
            FillTxBuffer(&TxBuffer[0]);    
            LastSentModbusAddress = TxBuffer[MODBUS_ADDRESS_INDEX];
            StartTransmittingDataToHeatpump(&TxBuffer[0]);
            app_heatpump_commData.state = APP_HEATPUMP_COMM_STATE_WAIT_FOR_DATA_SENT;
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
            else if (ResponseDelay >= 3)
            {
                SetOutput(LED_TX_HEATPUMP, false);

                app_heatpump_commData.state = APP_HEATPUMP_COMM_STATE_INIT;
                app_heatpump_commData.commStatus = HEATPUMP_COMM_STATUS_IDLE;
                LastSentModbusAddress = UINT8_MAX;
                //activeProtocol = HEATPUMP_PROTOCOL_NONE;
            }

            break;
        }
        // 4: Start met ontvangen data
        case APP_HEATPUMP_COMM_STATE_RECEIVE_DATA:
        {            
            ResponseDelay = 0;
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
            else if (ResponseDelay >= 3)
            {
                SetOutput(LED_RX_HEATPUMP, false);
                DLT645_FrameClear();

                app_heatpump_commData.state = APP_HEATPUMP_COMM_STATE_INIT;
                app_heatpump_commData.commStatus = HEATPUMP_COMM_STATUS_IDLE;
                LastSentModbusAddress = UINT8_MAX;
                //activeProtocol = HEATPUMP_PROTOCOL_NONE;
            }

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
            if (ResponseDelay >= 3)
            //if (ResponseDelay >= 2)
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
