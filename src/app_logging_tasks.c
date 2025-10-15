
#include <stddef.h>                    
#include <stdbool.h>                   
#include <stdlib.h>                   
#include <string.h>
#include <stdio.h>
#include <xc.h>
#include "definitions.h"               
//#include "third_party/../time.h"

#include "app_logging_tasks.h"
#include "files/time_counters.h"
#include "files/logging.h"
#include "files/delay.h"

#include "configuration.h"
#include "system_config.h"
#include "system_definitions.h"
#include "tcpip/tcpip.h"
#include "files/credentials.h"
#include "files/modbus/heatpump_parameters.h"

#define LOGGING_TIMEOUT_MS 200000
#define TCPIP_STACK_INDEX_0 0

//int stackResetCount = 0;
//int TcpIPStackResetCount = 0;  

APP_LOGGING_TASKS_DATA app_logging_tasksData;

TCPIP_NET_HANDLE        netH;
SYS_STATUS              tcpipStat;
IPV4_ADDR               ipAddr;
    
bool onlyDoSettings = false;
bool firstStartAfterBoot = true;

void TC2_Callback_InterruptHandler(TC_TIMER_STATUS status, uintptr_t context)
{           
    increaseTcpIpResetCounter();
    while(!releaseLoggingLock());
    TCPIP_STACK_Deinitialize(sysObj.tcpip);
    app_logging_tasksData.state = APP_LOGGING_TASKS_CLOSE_CONNECTION;
}
    


void APP_LOGGING_TASKS_Initialize ( void )
{
    initializeMutex();
             
    memset(&app_logging_tasksData, 0, sizeof (app_logging_tasksData));
    TC2_TimerCallbackRegister(TC2_Callback_InterruptHandler, (uintptr_t)NULL);
 
    /* Place the App state machine in its initial state. */
    app_logging_tasksData.state = APP_LOGGING_TASKS_STATE_INIT;
}



void APP_LOGGING_TASKS_Tasks ( void )
{
    /* Check the application's current state. */
    switch ( app_logging_tasksData.state )
    {
        
        
        /* Application's initial state. */
        case APP_LOGGING_TASKS_STATE_INIT:
        { 
            //TC2_TimerStart();
            app_logging_tasksData.state = APP_LOGGING_TASKS_IDLE;
            break;
        }
        
        
        case APP_LOGGING_TASKS_IDLE:
        {
            if (LoggingTimerExpiredSettingsInterval() && !onlyDoSettings) {
                SYS_CONSOLE_PRINT("***** ONLY DO SETTINGS ***** \r\n");
                app_logging_tasksData.state = APP_LOGGING_TASKS_WAIT_FOR_LOGGING_UNLOCK;
                onlyDoSettings = true;
            }            
            /* Wait for next logging timer trigger */
            if (LoggingTimerExpired()) {
                onlyDoSettings = false;
                app_logging_tasksData.state = APP_LOGGING_TASKS_WAIT_FOR_LOGGING_UNLOCK;
            }
            
            break;
        }
        
        
        case APP_LOGGING_TASKS_WAIT_FOR_LOGGING_UNLOCK:{
            
            if(!setLoggingLock()){   
                break;
            }
                            
            // Get handle to the first network interface
            TCPIP_NET_HANDLE netH = TCPIP_STACK_NetHandleGet("eth0");
            
            if (netH == NULL) {
                //SYS_CONSOLE_PRINT("Error: Network interface not found! Setting up new interface\n");
                onlyDoSettings = false;
                app_logging_tasksData.state = APP_LOGGING_TASKS_IDLE;
                if(!setupNewTcpipStack()){
                    while(!releaseLoggingLock());
                    break;
                }
                while(!releaseLoggingLock());
                break;
            }    
            
            if(!isEthernetCableConnected(netH)) {
                SYS_CONSOLE_PRINT("Ethernet cable is DISCONNECTED!\n");
                TCPIP_STACK_Deinitialize(sysObj.tcpip);
                increaseTcpIpResetCounter();
                onlyDoSettings = false;
                app_logging_tasksData.state = APP_LOGGING_TASKS_IDLE;
                while(!releaseLoggingLock());
                break;
            }
                 
            // Check if the network is up
            if (!TCPIP_STACK_NetIsUp(netH)) {      
                while(!releaseLoggingLock());
                break;
            } 
            
            app_logging_tasksData.state = APP_LOGGING_TASKS_WAIT_FOR_TCPIP_INIT;  
            break;
        }
        
        
        case APP_LOGGING_TASKS_WAIT_FOR_TCPIP_INIT:
        {  
            TC2_TimerStart();
            tcpipStat = TCPIP_STACK_Status(sysObj.tcpip);
            if(tcpipStat == SYS_STATUS_READY)
            {
                TCPIP_SNTP_ConnectionInitiate();
                app_logging_tasksData.state = APP_LOGGING_TASKS_WAIT_FOR_IP;
            }
            break;
        }
        
        
        
        case APP_LOGGING_TASKS_WAIT_FOR_IP:
        {       
            for (int i = 0; i < TCPIP_STACK_NumberOfNetworksGet(); i++) {
                netH = TCPIP_STACK_IndexToNet(i);
                if (!TCPIP_STACK_NetIsReady(netH)) {
                    return; // interface not ready yet!
                }
                ipAddr.Val = TCPIP_STACK_NetAddress(netH);
                //SYS_CONSOLE_PRINT("IP Address obtained: %d.%d.%d.%d \r\n", ipAddr.v[0], ipAddr.v[1], ipAddr.v[2], ipAddr.v[3]);
            }      
            app_logging_tasksData.state = APP_LOGGING_TASKS_WAIT_FOR_SNTP;
            break;
        }
        
        
        
        case APP_LOGGING_TASKS_WAIT_FOR_SNTP:
        {
            if (getCurrentUtcTimestamp()) {
                app_logging_tasksData.state = APP_LOGGING_TASKS_PARSE_STRING_TO_IP;
            }
            
        }
        
        
        
        case APP_LOGGING_TASKS_PARSE_STRING_TO_IP:
        {
            TCPIP_DNS_RESULT result;
            //SYS_CONSOLE_PRINT("Using DNS to Resolve '%s'\r\n", HOST);
            result = TCPIP_DNS_Resolve(HOST, TCPIP_DNS_TYPE_A);
                
            SYS_ASSERT(result != TCPIP_DNS_RES_NAME_IS_IPADDRESS, "DNS Result is TCPIP_DNS_RES_NAME_IS_IPADDRESS, which should not happen since we already checked");
            if (result >= 0) {
                app_logging_tasksData.state = APP_LOGGING_TASKS_WAIT_ON_DNS;
            } else {
                SYS_CONSOLE_PRINT("DNS Query returned %d Aborting\r\n", result);
                //while(!releaseLoggingLock());
                app_logging_tasksData.state = APP_LOGGING_TASKS_IDLE;
            }
            break;
        }
        
        
        
        case APP_LOGGING_TASKS_WAIT_ON_DNS:
        {
            if (!waitOnDNS()) {
                break;
            }
            app_logging_tasksData.state = APP_LOGGING_TASKS_START_CONNECTION;
            break;
        }
            
            
        
        case APP_LOGGING_TASKS_START_CONNECTION:
        {
            SYS_CONSOLE_PRINT("***** LOGGING LOCK SET ***** \r\n");
            if (!startSocketConnection()) {
                while(!releaseLoggingLock());
                app_logging_tasksData.state = APP_LOGGING_TASKS_IDLE;
            } else {
                app_logging_tasksData.state = APP_LOGGING_TASKS_WAIT_FOR_CONNECTION; 
            }   
                
            break;
        }
        
        
        
        case APP_LOGGING_TASKS_WAIT_FOR_CONNECTION:
        {
            //SSL_NEGOTIATION_STATES SLL_STATE;
            switch(waitForConnection()) {
                case SOCKET_NOT_CONNECTED:
                    break;
                case SSL_CREATE_CONNECTION_FAILED:
                    app_logging_tasksData.state = APP_LOGGING_TASKS_CLOSE_CONNECTION;
                    break;
                case SSL_CONNECTION_SUCCES:
                    app_logging_tasksData.state = APP_LOGGING_TASKS_WAIT_FOR_SSL_CONNECT;
                    break;
                default: /* Zou nooit gecalled moeten worden */
                    break;
            }
            break;
        }
        
        
        
        case APP_LOGGING_TASKS_WAIT_FOR_SSL_CONNECT:
        {
            switch (waitForSslConnection()) {
                case SSL_BUSY_NEGOTIATING:
                    break;
                    
                case SSL_SOCKET_NOT_SECURE:
                    app_logging_tasksData.state = APP_LOGGING_TASKS_CLOSE_CONNECTION;
                    break;
                    
                case SSL_NEGOTIATION_SUCCES:
                    if (onlyDoSettings) {
                        SYS_CONSOLE_PRINT("***** SKIPPING INTIAL SETTINGS LOG ***** \r\n");
                        app_logging_tasksData.state = APP_LOGGING_TASKS_REQUEST_UPDATE_SETTINGS;
                    } else {
                        app_logging_tasksData.state = APP_LOGGING_TASKS_SEND_REQUEST_SSL;
                    }
                    break;
            }
            break;
        }
        
        
        
        case APP_LOGGING_TASKS_SEND_REQUEST_SSL:
        {
            if(!loggingRequestBuilder(POST)){
            //if(!getUpdateFileFromServer()) {
                app_logging_tasksData.state = APP_LOGGING_TASKS_CLOSE_CONNECTION;
                break;
            }
            
            app_logging_tasksData.state = APP_LOGGING_TASKS_WAIT_RECEIVE_SOCKET_READY;
            break;
        }
        
        
        
        case APP_LOGGING_TASKS_WAIT_RECEIVE_SOCKET_READY: {
            
            SSL_SOCKET_STATES sslState = socketReady();
            if (sslState == SSL_SOCKET_NOT_READY) { break; } 
            if (sslState == SSL_SOCKET_WAS_DISCONNECTED) {
                app_logging_tasksData.state = APP_LOGGING_TASKS_CLOSE_CONNECTION;
                break;
            } 
            
            app_logging_tasksData.state = APP_LOGGING_TASKS_READ_RECEIVE_BUFFER;   
            break;
        }
        
        case APP_LOGGING_TASKS_READ_RECEIVE_BUFFER:
        {
            if (readServerResponseDone()) {
                SYS_CONSOLE_PRINT("Done receiving data from server, start parsing!\r\n");
                app_logging_tasksData.state = APP_LOGGING_TASKS_CHECK_FOR_UPDATE_SETTINGS;            
            }
            
            break;
        }
        
        
        case APP_LOGGING_TASKS_CHECK_FOR_UPDATE_SETTINGS:
        {
            //if(!readNetworkBufferBodySslResponse()) {
            //    break;
            //}
            if(!areUpdateSettingsAvailable()) {
            //if(!readNetworkBufferBodySslResponse()) {
                SYS_CONSOLE_PRINT("No update settings avialable, closing connection!\r\n");
                if (getSettingChangedInDisplay()) {
                    app_logging_tasksData.state = APP_LOGGING_TASKS_REQUEST_UPDATE_SETTINGS;
                } else {
                    app_logging_tasksData.state = APP_LOGGING_TASKS_CLOSE_CONNECTION; 
                }
                break;
            }                  
            
            SYS_CONSOLE_PRINT("Retrieving update settings!\r\n");
    
            
            app_logging_tasksData.state = APP_LOGGING_TASKS_REQUEST_UPDATE_SETTINGS;
            break;
        }
        
        
    
        case APP_LOGGING_TASKS_REQUEST_UPDATE_SETTINGS:
        {
            //if(!loggingRequestBuilder(POST)){
            if(!getUpdateSettingsFromServer()) {
                app_logging_tasksData.state = APP_LOGGING_TASKS_CLOSE_CONNECTION;
                break;
            }
            
            app_logging_tasksData.state = APP_LOGGING_TASKS_WAIT_RECEIVE_SOCKET_READY_UPDATE_SETTINGS;   
            break;
        }
        
        
        
        case APP_LOGGING_TASKS_WAIT_RECEIVE_SOCKET_READY_UPDATE_SETTINGS: {
            
            SSL_SOCKET_STATES sslState = socketReady();
            if (sslState == SSL_SOCKET_NOT_READY) { break; } 
            if (sslState == SSL_SOCKET_WAS_DISCONNECTED) {
                app_logging_tasksData.state = APP_LOGGING_TASKS_CLOSE_CONNECTION;
                break;
            } 
            
            app_logging_tasksData.state = APP_LOGGING_TASKS_READ_RECEIVE_BUFFER_UPDATE_SETTINGS;   
            break;
        }        
        
        
        case APP_LOGGING_TASKS_READ_RECEIVE_BUFFER_UPDATE_SETTINGS:
        {
            if (readServerResponseDone()) {
                SYS_CONSOLE_PRINT("Done receiving data from server, start parsing!\r\n");
                app_logging_tasksData.state = APP_LOGGING_TASKS_PARSE_UPDATE_SETTINGS;            
            }
            
            break;     
        }        
        
        
        
        case APP_LOGGING_TASKS_PARSE_UPDATE_SETTINGS:
        {
            if (parse_modbus_settings()) {
                SYS_CONSOLE_PRINT("Done receiving data from server, SETTING PARSED!\r\n");
                resetLoggingTimers();
                setNewLogRequired(true);
                app_logging_tasksData.state = APP_LOGGING_TASKS_WAIT_FOR_NEW_SETTINGS_CHANGED;      
                break;
            }
            
            if (firstStartAfterBoot || getSettingChangedInDisplay()) {
                SYS_CONSOLE_PRINT("NOTHING BULL STILL GOING ON BECAUSE FIRST LOG OR DISPLAY CHANGE!\r\n");
                firstStartAfterBoot = false;
                setSettingChangedInDisplay(false);
                app_logging_tasksData.state = APP_LOGGING_TASKS_WAIT_FOR_NEW_SETTINGS_CHANGED;
            } else {
                SYS_CONSOLE_PRINT("COULD NOT PARSE (OR THERE WERE NO) NEW SETTINGS! QUITTING\r\n");
                app_logging_tasksData.state = APP_LOGGING_TASKS_CLOSE_CONNECTION;
            }
            break; 
        }
        
        
        
        case APP_LOGGING_TASKS_WAIT_FOR_NEW_SETTINGS_CHANGED:
        {
            if(getSettingsQueuedAmount() > 0) { break; }
            
            SYS_CONSOLE_PRINT("SETTING QUEUE EMPTY, PROCEEDING!\r\n");
            
            setSecondCounterDelayAfterChangingSettings(0);
            
            app_logging_tasksData.state = APP_LOGGING_TASKS_SEND_NEW_SETTINGS_OVERVIEW;
            break;
        }
        
        
        
        case APP_LOGGING_TASKS_SEND_NEW_SETTINGS_OVERVIEW:
        {
            // Needed because the Heatpump needs some time to actually store the settings
            // Otherwise we would send back outdated settings
            if (getSecondCounterDelayAfterChangingSettings() < 50) {
                break;
            }
                
            if(!sendUpdatedSettingsList()) {
                setSecondCounterDelayAfterChangingSettings(UINT32_MAX);
                app_logging_tasksData.state = APP_LOGGING_TASKS_CLOSE_CONNECTION;
            }
            
            setSecondCounterDelayAfterChangingSettings(UINT32_MAX);
            app_logging_tasksData.state = APP_LOGGING_TASKS_WAIT_CONFIRM_CHANGED_SETTINGS;
            break;
        }
        

        case APP_LOGGING_TASKS_WAIT_CONFIRM_CHANGED_SETTINGS: {
            
            SSL_SOCKET_STATES sslState = socketReady();
            if (sslState == SSL_SOCKET_NOT_READY) { break; } 
            if (sslState == SSL_SOCKET_WAS_DISCONNECTED) {
                app_logging_tasksData.state = APP_LOGGING_TASKS_CLOSE_CONNECTION;
                break;
            } 
            
            app_logging_tasksData.state = APP_LOGGING_TASKS_READ_RECEIVE_BUFFER_SETTINGS_UPDATED;   
            break;
        }              

        
        case APP_LOGGING_TASKS_READ_RECEIVE_BUFFER_SETTINGS_UPDATED:
        {
            if (readServerResponseUpdatedSettingsDone()) {
                SYS_CONSOLE_PRINT("SERVER RESPONSE SETTINGS RECEIVED!\r\n");
                app_logging_tasksData.state = APP_LOGGING_TASKS_CLOSE_CONNECTION;            
            }

            break;     
        }            
                
        
        case APP_LOGGING_TASKS_CLOSE_CONNECTION:
        {
            SYS_CONSOLE_PRINT("******** CLOSING CONNECTION **********!\r\n");
            TC2_TimerStop();
            closeSocket();
            while(!releaseLoggingLock());
            onlyDoSettings = false;
            app_logging_tasksData.state = APP_LOGGING_TASKS_IDLE;
            break;
        }
            
        /* The default state should never be executed. */
        default:
        {
            app_logging_tasksData.state = APP_LOGGING_TASKS_IDLE;
            break;
        }
    }
}


/*******************************************************************************
 End of File
 */