
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
#include "files/network_manager.h"
#include "files/delay.h"

#include "configuration.h"
#include "system_config.h"
#include "system_definitions.h"
#include "tcpip/tcpip.h"
#include "files/credentials.h"
#include "files/modbus/heatpump_parameters.h"
#include "files/wincSoftAp.h"

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

static volatile bool loggingTimedOut;
static bool loggingLockHeld;
static bool retryPending,retryOnlySettings,cycleSuccess,overviewPending,freshConnection;
static uint32_t retryAt,retryDelay,phaseSince,attemptSince,overviewRevision;
static unsigned retryFailures;
static APP_LOGGING_TASKS_STATES observedState,resumeAfterTls=APP_LOGGING_TASKS_STATE_INIT;
static uint32_t loggingMilliseconds(void) {
    uint64_t n=SYS_TIME_Counter64Get();uint32_t hz=SYS_TIME_FrequencyGet();
    return hz?(uint32_t)((n/hz)*1000u+(n%hz)*1000u/hz):0;
}

static void releaseOwnedLoggingLock(void)
{
    if (loggingLockHeld && releaseLoggingLock()) loggingLockHeld = false;
}

static void finishLoggingAttempt(bool success) {
    TC2_TimerStop();loggingTimedOut=false;closeSocket();releaseOwnedLoggingLock();
    if(success) {
        retryFailures=0;retryPending=false;
        SYS_CONSOLE_MESSAGE("Logging transaction completed successfully\r\n");
    } else if(!retryPending) {
        if(retryFailures<5)retryFailures++;
        retryDelay=5000u<<(retryFailures-1u);if(retryDelay>60000u)retryDelay=60000u;
        retryAt=loggingMilliseconds();retryOnlySettings=onlyDoSettings;retryPending=true;
        SYS_CONSOLE_PRINT("Logging recovery: retry in %lu ms (failure %u)\r\n",(unsigned long)retryDelay,retryFailures);
    }
    resumeAfterTls=APP_LOGGING_TASKS_STATE_INIT;onlyDoSettings=false;cycleSuccess=freshConnection=false;
    app_logging_tasksData.state=APP_LOGGING_TASKS_IDLE;
}
static void loggingNetworkDown(void) {
    if(app_logging_tasksData.state!=APP_LOGGING_TASKS_IDLE&&app_logging_tasksData.state!=APP_LOGGING_TASKS_STATE_INIT)
        finishLoggingAttempt(false);
    else {TC2_TimerStop();loggingTimedOut=false;closeSocket();releaseOwnedLoggingLock();}
}

void TC2_Callback_InterruptHandler(TC_TIMER_STATUS status, uintptr_t context)
{
    /* ISR only signals. Socket/mutex/stack work belongs in the main loop. */
    loggingTimedOut = true;
}

void APP_LOGGING_TASKS_Initialize ( void )
{
    initializeMutex();
    loggingTimedOut = false;
    loggingLockHeld = false;
    retryPending=cycleSuccess=overviewPending=false;retryFailures=0;retryDelay=0;
    observedState=APP_LOGGING_TASKS_STATE_INIT;phaseSince=loggingMilliseconds();
             
    memset(&app_logging_tasksData, 0, sizeof (app_logging_tasksData));
    TC2_TimerCallbackRegister(TC2_Callback_InterruptHandler, (uintptr_t)NULL);
 
    /* Place the App state machine in its initial state. */
    app_logging_tasksData.state = APP_LOGGING_TASKS_STATE_INIT;
}



void APP_LOGGING_TASKS_Tasks ( void )
{
    if (loggingTimedOut)
    {
        loggingNetworkDown();
        return;
    }
    if (app_logging_tasksData.state != APP_LOGGING_TASKS_STATE_INIT &&
        app_logging_tasksData.state != APP_LOGGING_TASKS_IDLE &&
        app_logging_tasksData.state != APP_LOGGING_TASKS_WAIT_FOR_LOGGING_UNLOCK &&
        !NETWORK_MANAGER_IsReady())
    {
        loggingNetworkDown();
        return;
    }
    {
        uint32_t now=loggingMilliseconds(),limit=30000u;bool waitingResponse=false;
        if(observedState!=app_logging_tasksData.state) {observedState=app_logging_tasksData.state;phaseSince=now;}
        switch(app_logging_tasksData.state) {
            case APP_LOGGING_TASKS_STATE_INIT:
            case APP_LOGGING_TASKS_IDLE:break;
            default:
                if(loggingLockHeld&&(uint32_t)(now-attemptSince)>=LOGGING_TIMEOUT_MS) {
                    SYS_CONSOLE_MESSAGE("Logging overall deadline expired\r\n");finishLoggingAttempt(false);return;
                }
                switch(app_logging_tasksData.state) {
                    case APP_LOGGING_TASKS_WAIT_FOR_NEW_SETTINGS_CHANGED:
                    case APP_LOGGING_TASKS_SEND_NEW_SETTINGS_OVERVIEW:limit=120000u;break;
                    case APP_LOGGING_TASKS_WAIT_ON_DNS:limit=10000u;break;
                    case APP_LOGGING_TASKS_WAIT_RECEIVE_SOCKET_READY:
                    case APP_LOGGING_TASKS_READ_RECEIVE_BUFFER:
                    case APP_LOGGING_TASKS_WAIT_RECEIVE_SOCKET_READY_UPDATE_SETTINGS:
                    case APP_LOGGING_TASKS_READ_RECEIVE_BUFFER_UPDATE_SETTINGS:
                    case APP_LOGGING_TASKS_WAIT_CONFIRM_CHANGED_SETTINGS:
                    case APP_LOGGING_TASKS_READ_RECEIVE_BUFFER_SETTINGS_UPDATED:waitingResponse=true;break;
                    default:break;
                }
                loggingNetworkTasks();
                if(loggingIoFailed()) {SYS_CONSOLE_MESSAGE("Logging I/O failed; closing and retrying\r\n");finishLoggingAttempt(false);return;}
                if(loggingTransmitBusy())return;
                if((waitingResponse?(uint32_t)(now-loggingLastProgress()):(uint32_t)(now-phaseSince))>=(waitingResponse?15000u:limit)) {
                    SYS_CONSOLE_PRINT("Logging phase timeout: state=%u\r\n",(unsigned)app_logging_tasksData.state);
                    finishLoggingAttempt(false);return;
                }
                break;
        }
    }
    /* Check the application's current state. */
    switch ( app_logging_tasksData.state )
    {
        
        
        /* Application's initial state. */
        case APP_LOGGING_TASKS_STATE_INIT:
        { 
            if (!NETWORK_MANAGER_RegisterDownCallback(loggingNetworkDown)) break;
            app_logging_tasksData.state = APP_LOGGING_TASKS_IDLE;
            break;
        }
        
        
        case APP_LOGGING_TASKS_IDLE:
        {
            if(!NETWORK_MANAGER_IsReady())break;
            if(retryPending) {
                if((uint32_t)(loggingMilliseconds()-retryAt)<retryDelay)break;
                onlyDoSettings=retryOnlySettings;
                if(LoggingTimerExpired())onlyDoSettings=false;
                retryPending=false;app_logging_tasksData.state=APP_LOGGING_TASKS_WAIT_FOR_LOGGING_UNLOCK;break;
            }

//            if (getActivateWifiAp()) {
//                break;
//            }
            
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
            
            if (!NETWORK_MANAGER_IsReady()) break;
            if (!setLoggingLock()) break;
            loggingLockHeld = true;attemptSince=loggingMilliseconds();cycleSuccess=false;
            app_logging_tasksData.state = APP_LOGGING_TASKS_WAIT_FOR_TCPIP_INIT;  
            break;
        }
        
        
        case APP_LOGGING_TASKS_WAIT_FOR_TCPIP_INIT:
        {  
            TC2_TimerStop();
            TC2_Timer32bitCounterSet(0);
            loggingTimedOut = false;
            TC2_TimerStart();
            tcpipStat = TCPIP_STACK_Status(sysObj.tcpip);
            if(tcpipStat == SYS_STATUS_READY)
            {
                if(!getCurrentUtcTimestamp())TCPIP_SNTP_ConnectionInitiate();
                app_logging_tasksData.state = APP_LOGGING_TASKS_WAIT_FOR_IP;
            }
            break;
        }
        
        
        
        case APP_LOGGING_TASKS_WAIT_FOR_IP:
        {       
            /* The shared manager owns interface selection/readiness. Do not
             * require inactive secondary interfaces to have an address. */
            if(!NETWORK_MANAGER_IsReady())break;
            app_logging_tasksData.state = APP_LOGGING_TASKS_WAIT_FOR_SNTP;
            break;
        }
        
        
        
        case APP_LOGGING_TASKS_WAIT_FOR_SNTP:
        {
            if (getCurrentUtcTimestamp()) {
                app_logging_tasksData.state = APP_LOGGING_TASKS_PARSE_STRING_TO_IP;
            }
            break;
        }
        
        
        
        case APP_LOGGING_TASKS_PARSE_STRING_TO_IP:
        {
            TCPIP_DNS_RESULT result;
            //SYS_CONSOLE_PRINT("Using DNS to Resolve '%s'\r\n", HOST);
            result = retryFailures?TCPIP_DNS_Send_Query(HOST,TCPIP_DNS_TYPE_A):TCPIP_DNS_Resolve(HOST, TCPIP_DNS_TYPE_A);
                
            SYS_ASSERT(result != TCPIP_DNS_RES_NAME_IS_IPADDRESS, "DNS Result is TCPIP_DNS_RES_NAME_IS_IPADDRESS, which should not happen since we already checked");
            if (result >= 0) {
                app_logging_tasksData.state = APP_LOGGING_TASKS_WAIT_ON_DNS;
            } else {
                SYS_CONSOLE_PRINT("DNS Query returned %d Aborting\r\n", result);
                app_logging_tasksData.state = APP_LOGGING_TASKS_CLOSE_CONNECTION;
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
                app_logging_tasksData.state = APP_LOGGING_TASKS_CLOSE_CONNECTION;
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
                    freshConnection=true;
                    if(resumeAfterTls!=APP_LOGGING_TASKS_STATE_INIT) {
                        app_logging_tasksData.state=resumeAfterTls;resumeAfterTls=APP_LOGGING_TASKS_STATE_INIT;break;
                    }
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
            freshConnection=false;
            if(!loggingRequestBuilder(POST)){
            //if(!getUpdateFileFromServer()) {
                app_logging_tasksData.state = APP_LOGGING_TASKS_CLOSE_CONNECTION;
                break;
            }
            resetRxBuffer();
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
                if(loggingIoFailed()) {app_logging_tasksData.state=APP_LOGGING_TASKS_CLOSE_CONNECTION;break;}
            //if(!readNetworkBufferBodySslResponse()) {
                SYS_CONSOLE_PRINT("No update settings avialable, closing connection!\r\n");
                if (getSettingChangedInDisplay() || overviewPending || firstStartAfterBoot) {
                    app_logging_tasksData.state = APP_LOGGING_TASKS_REQUEST_UPDATE_SETTINGS;
                } else {
                    cycleSuccess=true;app_logging_tasksData.state = APP_LOGGING_TASKS_CLOSE_CONNECTION;
                }
                break;
            }                  
            
            SYS_CONSOLE_PRINT("Retrieving update settings!\r\n");
    
            
            app_logging_tasksData.state = APP_LOGGING_TASKS_REQUEST_UPDATE_SETTINGS;
            break;
        }
        
        
    
        case APP_LOGGING_TASKS_REQUEST_UPDATE_SETTINGS:
        {
            if(!freshConnection&&!loggingConnectionReusable()) {
                closeSocket();resumeAfterTls=APP_LOGGING_TASKS_REQUEST_UPDATE_SETTINGS;
                app_logging_tasksData.state=APP_LOGGING_TASKS_START_CONNECTION;break;
            }
            //if(!loggingRequestBuilder(POST)){
            freshConnection=false;
            if(!getUpdateSettingsFromServer()) {
                app_logging_tasksData.state = APP_LOGGING_TASKS_CLOSE_CONNECTION;
                break;
            }
            resetRxBuffer();
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
                //SYS_CONSOLE_PRINT("Done receiving data from server, start parsing!\r\n");
                app_logging_tasksData.state = APP_LOGGING_TASKS_PARSE_UPDATE_SETTINGS;            
            }
            
            break;     
        }        
        
        
        
        case APP_LOGGING_TASKS_PARSE_UPDATE_SETTINGS:
        {
            if (parse_modbus_settings()) {
                //SYS_CONSOLE_PRINT("Done receiving data from server, SETTING PARSED!\r\n");
                overviewPending=true;
                resetLoggingTimers();
                setNewLogRequired(true);
                setSecondCounterDelayAfterChangingSettings(0);
                SYS_CONSOLE_PRINT("NEW SETTINGS FOUND, PROCEEDING\r\n");
                app_logging_tasksData.state = APP_LOGGING_TASKS_WAIT_FOR_NEW_SETTINGS_CHANGED;      
                break;
            }
            
            if(loggingIoFailed()) {app_logging_tasksData.state=APP_LOGGING_TASKS_CLOSE_CONNECTION;break;}
            if (firstStartAfterBoot || getSettingChangedInDisplay() || overviewPending) {
                SYS_CONSOLE_PRINT("Settings overview required; waiting for queued changes\r\n");
                setSecondCounterDelayAfterChangingSettings(0);
                app_logging_tasksData.state = APP_LOGGING_TASKS_WAIT_FOR_NEW_SETTINGS_CHANGED;
            } else {
                cycleSuccess=true;
                SYS_CONSOLE_PRINT("No new settings; logging cycle complete\r\n");
                app_logging_tasksData.state = APP_LOGGING_TASKS_CLOSE_CONNECTION;
            }
            break; 
        }
        
        
        
        case APP_LOGGING_TASKS_WAIT_FOR_NEW_SETTINGS_CHANGED:
        {
            //if(getSettingsQueuedAmount() > 0) { break; }        
            
            if (getSecondCounterDelayAfterChangingSettings() < 100) {
                break;
            }            
            
            SYS_CONSOLE_PRINT("QUEUE SIZE >> %i\r\n", getSettingsQueuedAmount());
            
            setSecondCounterDelayAfterChangingSettings(0);
            
            app_logging_tasksData.state = APP_LOGGING_TASKS_SEND_NEW_SETTINGS_OVERVIEW;
            break;
        }
        
        
        
        case APP_LOGGING_TASKS_SEND_NEW_SETTINGS_OVERVIEW:
        {
            // Needed because the Heatpump needs some time to actually store the settings
            // Otherwise we would send back outdated settings
            if (getSecondCounterDelayAfterChangingSettings() < 100) {
                break;
            }
            
            if(!freshConnection&&!loggingConnectionReusable()) {
                closeSocket();resumeAfterTls=APP_LOGGING_TASKS_SEND_NEW_SETTINGS_OVERVIEW;
                app_logging_tasksData.state=APP_LOGGING_TASKS_START_CONNECTION;break;
            }
            freshConnection=false;
            setSecondCounterDelayAfterChangingSettings(UINT32_MAX);
                
            overviewRevision=loggingDisplayRevision();
            if(!sendUpdatedSettingsList()) {
                SYS_CONSOLE_PRINT("!!! COULD NOT SEND UPDATE LIST !!!\r\n");
                app_logging_tasksData.state = APP_LOGGING_TASKS_CLOSE_CONNECTION;break;
            }
            resetRxBuffer();
            SYS_CONSOLE_PRINT("+++ WAIT SETTINGS CONFIRMED +++\r\n");
            app_logging_tasksData.state = APP_LOGGING_TASKS_WAIT_CONFIRM_CHANGED_SETTINGS;
            break;
        }
        

        case APP_LOGGING_TASKS_WAIT_CONFIRM_CHANGED_SETTINGS: {
            
            SSL_SOCKET_STATES sslState = socketReady();
            if (sslState == SSL_SOCKET_NOT_READY) { break; } 
            if (sslState == SSL_SOCKET_WAS_DISCONNECTED) {
                SYS_CONSOLE_PRINT("!!! SOCKET WAS DISCONNECTED !!!\r\n");
                app_logging_tasksData.state = APP_LOGGING_TASKS_CLOSE_CONNECTION;
                break;
            } 
            SYS_CONSOLE_PRINT("+++ SETTINGS SEND BACK CONFIRMED +++\r\n");
            app_logging_tasksData.state = APP_LOGGING_TASKS_READ_RECEIVE_BUFFER_SETTINGS_UPDATED;   
            break;
        }              

        
        case APP_LOGGING_TASKS_READ_RECEIVE_BUFFER_SETTINGS_UPDATED:
        {
            if (readServerResponseUpdatedSettingsDone()) {
                cycleSuccess=true;firstStartAfterBoot=false;overviewPending=false;
                if(loggingDisplayRevision()==overviewRevision)setSettingChangedInDisplay(false);
                SYS_CONSOLE_PRINT("SERVER RESPONSE SETTINGS RECEIVED!\r\n");
                app_logging_tasksData.state = APP_LOGGING_TASKS_CLOSE_CONNECTION;            
            }

            break;     
        }            
                
        
        case APP_LOGGING_TASKS_CLOSE_CONNECTION:
        {
            SYS_CONSOLE_PRINT("******** CLOSING CONNECTION **********!\r\n");
            finishLoggingAttempt(cycleSuccess);
            break;
        }
            
        /* The default state should never be executed. */
        default:
        {
            loggingNetworkDown();
            break;
        }
    }
}


/*******************************************************************************
 End of File
 */