#include <xc.h>
#include <string.h>
#include <stdbool.h>                   
#include "definitions.h"
#include "configuration.h"

#include "logging.h"
#include "network_manager.h"
#include "logging_http.h"
#include "modbus/heatpump_parameters.h"
#include "i2c/mac.h"
#include "alarms.h"
#include "eeprom.h"
#include "ntc.h"
#include "../config/default/user.h"
#include "../app_sd_card_tasks.h"
#include "sdcard.h"

#include "third_party/../time.h"
#include "modbus/display.h"
#include "osal/osal.h"
#include "tcpip/tcpip.h"
#include "credentials.h"
#include "states.h"
#include <zlib.h>
#include "cJSON.h"
#include <stdarg.h>
#include <errno.h>
//#include "delay.h"

/* Used in sending and receiving data to and from the server */
#define TRUE true
#define FALSE false
#define UNDEFINED_UINT16_T -32768
#define UNDEFINED_INT16_T 32767 
#define UNDEFINED_STRING "UNDEFINED"
#define UNDEFINED_CHAR "U"

//#define READ_BUFFER_SIZE 512
#define READ_CHUNK_SIZE 512
#define READ_BUFFER_SIZE 4096

#define MAX_LOG_DEVICES   17u
#define DEVICE_JSON_MAX  8192u   // start here; adjust if needed
#define CASCADE_BIT_IS_SET(mask, bit)   (((mask) & (1u << (bit))) != 0u)


static char devObjSendBuffer[DEVICE_JSON_MAX];
static char settingsNetworkBuffer[4096];
//extern const TCPIP_NETWORK_CONFIG __attribute__((unused))  TCPIP_HOSTS_CONFIGURATION[];
//extern const size_t TCPIP_HOSTS_CONFIGURATION_SIZE;
//extern APP_HEATING_AND_HOT_WATER_DATA app_Data;

NET_PRES_SKT_HANDLE_T   log_socket = INVALID_SOCKET;
IP_MULTI_ADDRESS        address;
static LOG_HTTP_TX loggingTx;
static LOG_HTTP_RESPONSE loggingResponse;
static bool loggingFault,loggingRxComplete;
static uint32_t loggingProgress;
static char loggingChunk[512];
static uint32_t loggingNow(void) {
    uint64_t ticks=SYS_TIME_Counter64Get();uint32_t hz=SYS_TIME_FrequencyGet();
    return hz?(uint32_t)((ticks/hz)*1000u+(ticks%hz)*1000u/hz):0;
}
bool loggingTransmitBusy(void) { return loggingTx.state!=LOG_TX_IDLE&&loggingTx.state!=LOG_TX_DONE&&loggingTx.state!=LOG_TX_FAILED; }
bool loggingIoFailed(void) { return loggingFault||loggingTx.state==LOG_TX_FAILED; }
uint32_t loggingLastProgress(void) { return loggingProgress; }
static int loggingWrite(void *user,const uint8_t *data,size_t length) {
    uint16_t ready;(void)user;
    if(log_socket==INVALID_SOCKET||!NET_PRES_SocketIsConnected(log_socket))return -1;
    ready=NET_PRES_SocketWriteIsReady(log_socket,(uint16_t)length,0);
    if(ready<(uint16_t)length)return 0;
    return (int)NET_PRES_SocketWrite(log_socket,data,(uint16_t)length);
}
void loggingNetworkTasks(void) {
    uint32_t now=loggingNow(),before=loggingTx.progress;LOG_HTTP_TxTasks(&loggingTx,loggingWrite,NULL,now);
    if(loggingTx.progress!=before)loggingProgress=loggingTx.progress;
}
static bool beginLoggingRequest(size_t length,LOG_HTTP_PRODUCER producer) {
    loggingFault=false;loggingProgress=loggingNow();
    return LOG_HTTP_TxBegin(&loggingTx,settingsNetworkBuffer,length,producer,NULL,loggingProgress);
}


DEVICE_TYPE DEVICE_TYPE_REQUESTED = -1; 
char NTP_TIME_BUFFER[40];

        
static size_t totalBytesReceived = 0;
static char readBuffer[READ_BUFFER_SIZE + 1];
//static uint8_t readBuffer[READ_BUFFER_SIZE + 1];
//static size_t expectedContentLength = 0;

bool settingChangedInDisplay = false;
static uint32_t displayChangeRevision;
bool newLogRequired = false;

void setSettingChangedInDisplay(bool value) {
    if(value)displayChangeRevision++;
    settingChangedInDisplay = value;
}

uint32_t loggingDisplayRevision(void) {return displayChangeRevision;}

bool getSettingChangedInDisplay() {
    return settingChangedInDisplay;
}

void setNewLogRequired(bool value) {
    newLogRequired = value;
}

bool getNewLogRequired() {
    return newLogRequired;
}

OSAL_MUTEX_DECLARE(loggingMutex);


void initializeMutex() {
    //SYS_CONSOLE_PRINT("**** APP STATE BEFORE ANYTHING >> %i\r\n", app_Data.appState);
    if (OSAL_MUTEX_Create(&loggingMutex) != OSAL_RESULT_TRUE) {}
}


static bool appendf(char* dst, size_t dstSize, const char* fmt, ...)
{
    size_t used = strnlen(dst, dstSize);
    if (used >= dstSize) return false;

    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(dst + used, dstSize - used, fmt, ap);
    va_end(ap);

    if (n < 0) return false;
    if ((size_t)n >= (dstSize - used)) {
        // truncated
        dst[dstSize - 1] = '\0';
        return false;
    }
    return true;
}


bool setLoggingLock() {
    if (OSAL_MUTEX_Lock(&loggingMutex, OSAL_WAIT_FOREVER ) == OSAL_RESULT_TRUE) {
        //SYS_DEBUG_PRINT(SYS_ERROR_ERROR, "***** Locked *****\r\n");
        return true;
    } 
    return false;
}



bool releaseLoggingLock() {
    if (OSAL_MUTEX_Unlock(&loggingMutex) == OSAL_RESULT_TRUE) {
        //SYS_DEBUG_PRINT(SYS_ERROR_ERROR, "***** unlock *****\r\n");
        return true;
    } 
    return false;
}



bool setLoggingLockNoPrint() {
    if (OSAL_MUTEX_Lock(&loggingMutex, OSAL_WAIT_FOREVER ) == OSAL_RESULT_TRUE) {
        return true;
    } 
    return false;
}



bool releaseLoggingLockNoPrint() {
    if (OSAL_MUTEX_Unlock(&loggingMutex) == OSAL_RESULT_TRUE) {
        return true;
    } 
    return false;
}



bool isEthernetCableConnected(TCPIP_NET_HANDLE netH) {
    if (netH != NULL && !TCPIP_STACK_NetIsLinked(netH)){
        return false;
    }
    return true;
}

uint8_t getTcpIpResetCounter() {
    return (uint8_t)NETWORK_MANAGER_GetStatus().stackRestarts;
}



char * getLoggingStateToString(APP_LOGGING_TASKS_STATES logState) {
    switch(logState){
        case(APP_LOGGING_TASKS_STATE_INIT): return "0, Init"; break;        
        case(APP_LOGGING_TASKS_IDLE): return "1, Idle"; break;   
        case(APP_LOGGING_TASKS_WAIT_FOR_TCPIP_INIT): return "2, TCPIP Init"; break;    
        case(APP_LOGGING_TASKS_WAIT_FOR_IP): return "3, Wait IP"; break;    
        case(APP_LOGGING_TASKS_WAIT_FOR_SNTP): return "4, Wait SNTP"; break;    
        case(APP_LOGGING_TASKS_PARSE_STRING_TO_IP): return "5, Parse IP"; break;      
        case(APP_LOGGING_TASKS_WAIT_ON_DNS):  return "6, Wait DNS"; break;    
        case(APP_LOGGING_TASKS_START_CONNECTION): return "7, Start Conn"; break;             
        case(APP_LOGGING_TASKS_WAIT_FOR_CONNECTION): return "8, Wait Conn"; break;         
        case(APP_LOGGING_TASKS_WAIT_FOR_SSL_CONNECT): return "9, Wait SSL"; break;       
        case(APP_LOGGING_TASKS_SEND_REQUEST_SSL): return "10, Send Request"; break;   
        case(APP_LOGGING_TASKS_WAIT_RECEIVE_SOCKET_READY): return "11, Wait Response H"; break;     
        case(APP_LOGGING_TASKS_CHECK_FOR_UPDATE_SETTINGS): return "11, Wait Response B"; break;           
        case(APP_LOGGING_TASKS_CLOSE_CONNECTION): return "12, Close Conn"; break;     
        case(APP_LOGGING_TASKS_WAIT_FOR_LOGGING_UNLOCK): return "13, Wait Unlock"; break;    
        default: return "14, Unkown"; break;
    }
}


void setHardwareId( char device[], char* hardwareId, int modbusIndex ) {
    switch (DEVICE_TYPE_REQUESTED){
        case TULIP_PRINT:{
            snprintf(hardwareId, 50, "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
                    eui64[0], eui64[1], eui64[2], eui64[3], eui64[4], eui64[5], eui64[6], eui64[7]);       
            break;
        }
        
        case HEATPUMP:{
            snprintf(hardwareId, 50, "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X-W-%i",
                    eui64[0], eui64[1], eui64[2], eui64[3], eui64[4], eui64[5], eui64[6], eui64[7], modbusIndex);
            break;
        }
        
        case BATTERY:{
            snprintf(hardwareId, 50, "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X-B-%i",
                    eui64[0], eui64[1], eui64[2], eui64[3], eui64[4], eui64[5], eui64[6], eui64[7], modbusIndex);            
            break;
        }
        
        case SMART_METER:{
            snprintf(hardwareId, 50, "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X-SM-%i",
                    eui64[0], eui64[1], eui64[2], eui64[3], eui64[4], eui64[5], eui64[6], eui64[7], modbusIndex);            
            break;
        }
        
        case INVERTER:{
            snprintf(hardwareId, 50, "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X-I-%i",
                    eui64[0], eui64[1], eui64[2], eui64[3], eui64[4], eui64[5], eui64[6], eui64[7], modbusIndex);            
            break;
        }
        
        case HEATPUMP_BOILER:{
            snprintf(hardwareId, 50, "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X-HB-%i",
                    eui64[0], eui64[1], eui64[2], eui64[3], eui64[4], eui64[5], eui64[6], eui64[7], modbusIndex);            
            break;
        }
        
        /* ZOU NOOIT GECALLED MOETEN WORDEN */
        default:{
            break;
        }
    }
    return;
}



bool getCurrentUtcTimestamp ( void ) {
    uint32_t utcSeconds = TCPIP_SNTP_UTCSecondsGet();
    if(utcSeconds > 0) {
        utcSeconds += 7200; // +2 uur voor converteren tussen tijdzones
        time_t utc_time_t = utcSeconds;
        struct tm *utc_time = gmtime(&utc_time_t);
        strftime(NTP_TIME_BUFFER, sizeof(NTP_TIME_BUFFER), "%Y-%m-%d %H:%M:%S", utc_time);
        //SYS_DEBUG_PRINT(SYS_ERROR_ERROR, "LOG TIMESTAMP %s\r\n", NTP_TIME_BUFFER);
        
        if (sizeof (NTP_TIME_BUFFER) > 0) {
            return true;
        }
        return false;
    }
    return false;
}



char parameter[100];
bool setLogValue_STRING(char* rb, size_t rbSize, const char* name, const char* value)
{
    return appendf(rb, rbSize, "\"%s\": \"%s\",", name, value);
}

bool setLogValue_BOOLEAN(char* rb, size_t rbSize, const char* name, bool value)
{
    return appendf(rb, rbSize, "\"%s\": %d,", name, value ? 1 : 0);
}

bool setLogValue_NUMBER(char* rb, size_t rbSize, const char* name, int value)
{
    return appendf(rb, rbSize, "\"%s\": %i,", name, value);
}


bool setLoggingDataPerDeviceType(char* requestBuilder, size_t requestBuilderSize, char device[], int index) {
    char hardwareId[50];
   
    /* SET THE DEVICE TYPE IN THE HTTP HEADER */
    if (!appendf(requestBuilder, requestBuilderSize, "{\"type\": \"%s\",", device)) return false;
    memset(parameter, 0, sizeof (parameter));
    
    /* SET THE HARDWARE ID IN THE HTTP HEADER */
    setHardwareId( device, hardwareId, index );
    if (!appendf(requestBuilder, requestBuilderSize, "\"hardware_id\": \"%s\",", hardwareId)) return false;
    memset(parameter, 0, sizeof (parameter));
        
    /* ADD THE REQUEST BODY TO THE HTTP */
    if (!appendf(requestBuilder, requestBuilderSize, "\"values\": {")) return false;
    switch (DEVICE_TYPE_REQUESTED){
        
        case TULIP_PRINT:{
            if (!setLogValue_STRING(requestBuilder, requestBuilderSize, HardwareID, hardwareId)) return false;          
            if (!setLogValue_STRING(requestBuilder, requestBuilderSize, KlantID, UNDEFINED_STRING)) return false;
            
            /* Software versie */
            if (!appendf(requestBuilder, requestBuilderSize, "\"%s\": \"%d-%d-%d\",", SoftwareVersie,
                    (int)((THIS_FIRMWARE_VERSION / 1000000)),
                    (int)((THIS_FIRMWARE_VERSION / 1000) % 1000),
                    (int)(THIS_FIRMWARE_VERSION % 1000))) return false;
            
            /* Bootloader software versie */
            uint32_t BOOTLOADER_FIRMWARE_VERSION = ReadSmartEeprom32(BOOTLOADER_SOFTWARE_VERSION);
            if (!appendf(requestBuilder, requestBuilderSize, "\"%s\": \"%d-%d-%d\",", BootloaderSoftwareVersie,
                    (int)((BOOTLOADER_FIRMWARE_VERSION / 1000000)),
                    (int)((BOOTLOADER_FIRMWARE_VERSION / 1000) % 1000),
                    (int)(BOOTLOADER_FIRMWARE_VERSION % 1000))) return false;
                
            //setLogValue_STRING(requestBuilder, BootloaderSoftwareVersie, UNDEFINED_STRING);
            if (!setLogValue_STRING(requestBuilder, requestBuilderSize, RTCCDatumtijd, NTP_TIME_BUFFER)) return false;    
            if (!setLogValue_STRING(requestBuilder, requestBuilderSize, NTPDatumtijd, NTP_TIME_BUFFER)) return false;   
            if (!setLogValue_NUMBER(requestBuilder, requestBuilderSize, Lognummer, UNDEFINED_INT16_T)) return false;
            
            if (!setLogValue_NUMBER(requestBuilder, requestBuilderSize, ConnectTemp1, GetNtcTemperature(NTC_HEATING_BUFFER))) return false;
            if (!setLogValue_NUMBER(requestBuilder, requestBuilderSize, ConnectTemp2, GetNtcTemperature(NTC_HOT_WATER_BUFFER))) return false;
            if (!setLogValue_NUMBER(requestBuilder, requestBuilderSize, ConnectTemp3, GetNtcTemperature(NTC_RESERVED_1))) return false;
            if (!setLogValue_NUMBER(requestBuilder, requestBuilderSize, ConnectTemp4, GetNtcTemperature(NTC_RESERVED_2))) return false;
            
            if (!setLogValue_BOOLEAN(requestBuilder, requestBuilderSize, ConnectInput1, GetThermostatContact())) return false;
            if (!setLogValue_BOOLEAN(requestBuilder, requestBuilderSize, ConnectInput2, GetDigitalInput2())) return false;
            if (!setLogValue_BOOLEAN(requestBuilder, requestBuilderSize, ConnectInput3, GetDigitalInput3())) return false;
            
            if (!setLogValue_BOOLEAN(requestBuilder, requestBuilderSize, ConnectRelay1, RelayPotfree1_Get())) return false;
            if (!setLogValue_BOOLEAN(requestBuilder, requestBuilderSize, ConnectRelay2, RelayPotfree2_Get())) return false;
            if (!setLogValue_BOOLEAN(requestBuilder, requestBuilderSize, ConnectRelay3, RelayPotfree3_Get())) return false;
            if (!setLogValue_BOOLEAN(requestBuilder, requestBuilderSize, BufferElekElement, getStatusHeatingElementHeatingBuffer())) return false;
            if (!setLogValue_BOOLEAN(requestBuilder, requestBuilderSize, TapwaterElekElement, getStatusHeatingElementHotWaterBuffer())) return false;
            
            if (!setLogValue_BOOLEAN(requestBuilder, requestBuilderSize, Reserved230vRelay, getReserved230VRelay())) return false;
            if (!setLogValue_BOOLEAN(requestBuilder, requestBuilderSize, ThreeWayValveRelay, getStatus3WayValve())) return false;
            
            if (!setLogValue_BOOLEAN(requestBuilder, requestBuilderSize, CirculatorPump, getStatusCirculationPump())) return false;
            if (!setLogValue_BOOLEAN(requestBuilder, requestBuilderSize, SwitchInput, getInputSlideSwitch())) return false;
            
            if (!setLogValue_BOOLEAN(requestBuilder, requestBuilderSize, VerbondenMetServer, TRUE)) return false;
            if (!setLogValue_BOOLEAN(requestBuilder, requestBuilderSize, DIPswitch1, GetDip1())) return false;
            if (!setLogValue_BOOLEAN(requestBuilder, requestBuilderSize, DIPswitch2, GetDip2())) return false;
            if (!setLogValue_BOOLEAN(requestBuilder, requestBuilderSize, DIPswitch3, GetDip3())) return false;
            if (!setLogValue_BOOLEAN(requestBuilder, requestBuilderSize, DIPswitch4, GetDip4())) return false;
            
            //setLogValue_BOOLEAN(requestBuilder, ConfigModusActief, FALSE);
            //setLogValue_STRING(requestBuilder, WifiStatus, UNDEFINED_CHAR);   
            //setLogValue_NUMBER(requestBuilder, WifiInfo, UNDEFINED_INT16_T);
            
            if (!setLogValue_BOOLEAN(requestBuilder, requestBuilderSize, SDcardAanwezig, SD_CARD_MOUNT_FLAG)) return false;
            if (!setLogValue_NUMBER(requestBuilder, requestBuilderSize, SDstatus, getSdCardState())) return false;          
            
            //setLogValue_STRING(requestBuilder, RS485commMetDisplayOK, UNDEFINED_CHAR);
            //setLogValue_STRING(requestBuilder, RS485ommMetWarmtepompOK, UNDEFINED_CHAR);
            //setLogValue_NUMBER(requestBuilder, EthernetStatus, UNDEFINED_INT16_T);  
            //setLogValue_NUMBER(requestBuilder, USBstatus, UNDEFINED_INT16_T);  
            
            if (!setLogValue_BOOLEAN(requestBuilder, requestBuilderSize, PowerFailStatus, getPowerFailStatus())) return false;
            if (!setLogValue_BOOLEAN(requestBuilder, requestBuilderSize, SupercapacitorFaultStatus, getSupercapFaultStatus())) return false;
            if (!setLogValue_BOOLEAN(requestBuilder, requestBuilderSize, Systemgoodindicator, getSystemGoodIndicator())) return false;
            if (!setLogValue_BOOLEAN(requestBuilder, requestBuilderSize, SupercapacitorPowerGoodIndicator, getSupercapacitorPowerGoodIndicator())) return false;
            
            if (!setLogValue_NUMBER(requestBuilder, requestBuilderSize, Alarmbytes, GetAlarm(0))) return false; 
            
            if (!setLogValue_BOOLEAN(requestBuilder, requestBuilderSize, emergencyModeHeating, ReadSmartEeprom16(SEEP_ADDR_EMERGENCY_MODE_HEATING_ENABLED))) return false;
            if (!setLogValue_BOOLEAN(requestBuilder, requestBuilderSize, emergencyModeHotwater, ReadSmartEeprom16(SEEP_ADDR_EMERGENCY_MODE_HOTWATER_ENABLED))) return false;            
            if (!setLogValue_NUMBER(requestBuilder, requestBuilderSize, circPumpOffTempLow, ReadSmartEeprom16(SEEP_ADDR_PUMP_OFF_TEMP_TOO_LOW))) return false; 
            if (!setLogValue_NUMBER(requestBuilder, requestBuilderSize, circPumpOnTempLow, ReadSmartEeprom16(SEEP_ADDR_PUMP_ON_TEMP_AFTER_TOO_LOW_TEMP))) return false; 
            if (!setLogValue_NUMBER(requestBuilder, requestBuilderSize, circPumpOffTempHigh, ReadSmartEeprom16(SEEP_ADDR_PUMP_OFF_TEMP_TOO_HIGH))) return false; 
            if (!setLogValue_NUMBER(requestBuilder, requestBuilderSize, circPumpOnTempHigh, ReadSmartEeprom16(SEEP_ADDR_PUMP_ON_TEMP_AFTER_TOO_HIGH_TEMP))) return false; 
            
            if (!setLogValue_NUMBER(requestBuilder, requestBuilderSize, circPumpOnTemp, ReadSmartEeprom16(SEEP_ADDR_CIRCULATION_PUMP_ON_TEMPERATURE))) return false; 
            if (!setLogValue_NUMBER(requestBuilder, requestBuilderSize, circPumpControlOnAmbTemp, ReadSmartEeprom16(SEEP_ADDR_CIRCULATION_PUMP_CONTROL_AT_AMBIENT_TEMPERATURE))) return false; 
            
            requestBuilder[strlen(requestBuilder)-1] = '\0';
            break;
        }
        
        
        
        case HEATPUMP:{
            index -= 1;
            if (!setLogValue_NUMBER(requestBuilder, requestBuilderSize, "WL1",  getDataFromMemoryCallable(ADDRESS_RUNNING_STATUS_1, index))) return false;
            if (!setLogValue_NUMBER(requestBuilder, requestBuilderSize, "WL2",  getDataFromMemoryCallable(ADDRESS_RUNNING_STATUS_2, index))) return false;
            if (!setLogValue_NUMBER(requestBuilder, requestBuilderSize, "WL3",  getDataFromMemoryCallable(ADDRESS_FAULT_STATE_1, index))) return false;
            if (!setLogValue_NUMBER(requestBuilder, requestBuilderSize, "WL4",  getDataFromMemoryCallable(ADDRESS_FAULT_STATE_2, index))) return false;
            if (!setLogValue_NUMBER(requestBuilder, requestBuilderSize, "WL5",  getDataFromMemoryCallable(ADDRESS_FAULT_STATE_3, index))) return false;
            if (!setLogValue_NUMBER(requestBuilder, requestBuilderSize, "WL6",  getDataFromMemoryCallable(ADDRESS_SYSTEM_1_FAULT_STATE_1, index))) return false;
            if (!setLogValue_NUMBER(requestBuilder, requestBuilderSize, "WL7",  getDataFromMemoryCallable(ADDRESS_SYSTEM_1_FAULT_STATE_2, index))) return false;
            if (!setLogValue_NUMBER(requestBuilder, requestBuilderSize, "WL8",  getDataFromMemoryCallable(ADDRESS_SYSTEM_1_DRIVE_FAULT_STATE_1, index))) return false;
            if (!setLogValue_NUMBER(requestBuilder, requestBuilderSize, "WL9",  getDataFromMemoryCallable(ADDRESS_SYSTEM_1_DRIVE_FAULT_STATE_2, index))) return false;
            if (!setLogValue_NUMBER(requestBuilder, requestBuilderSize, "WL10", getDataFromMemoryCallable(ADDRESS_SYSTEM_1_DRIVE_FAULT_STATE_3, index))) return false;

            if (!setLogValue_NUMBER(requestBuilder, requestBuilderSize, "WL11", getDataFromMemoryCallable(ADDRESS_RELAY_OUTPUT_STATUS_1, index))) return false;
            if (!setLogValue_NUMBER(requestBuilder, requestBuilderSize, "WL12", getDataFromMemoryCallable(ADDRESS_RELAY_OUTPUT_STATUS_2, index))) return false;
            if (!setLogValue_NUMBER(requestBuilder, requestBuilderSize, "WL13", getDataFromMemoryCallable(ADDRESS_RELAY_OUTPUT_STATUS_3, index))) return false;
            if (!setLogValue_NUMBER(requestBuilder, requestBuilderSize, "WL14", getDataFromMemoryCallable(ADDRESS_RELAY_OUTPUT_STATUS_4, index))) return false;
            if (!setLogValue_NUMBER(requestBuilder, requestBuilderSize, "WL15", getDataFromMemoryCallable(ADDRESS_SWITCH_PORT_STATE_1, index))) return false;
            if (!setLogValue_NUMBER(requestBuilder, requestBuilderSize, "WL16", getDataFromMemoryCallable(ADDRESS_SWITCH_PORT_STATE_2, index))) return false;
            if (!setLogValue_NUMBER(requestBuilder, requestBuilderSize, "WL17", getDataFromMemoryCallable(ADDRESS_SWITCH_PORT_STATE_3, index))) return false;
            if (!setLogValue_NUMBER(requestBuilder, requestBuilderSize, "WL18", getDataFromMemoryCallable(ADDRESS_SWITCH_PORT_STATE_4, index))) return false;
            if (!setLogValue_NUMBER(requestBuilder, requestBuilderSize, "WL19", getDataFromMemoryCallable(ADDRESS_CURRENT_UNIT_TOOLING_NO, index))) return false;
            if (!setLogValue_NUMBER(requestBuilder, requestBuilderSize, "WL20", (int16_t)getDataFromMemoryCallable(ADDRESS_COMPRESSOR_1_TARGET_FREQUENCY, index))) return false;

            if (!setLogValue_NUMBER(requestBuilder, requestBuilderSize, "WL21", (int16_t)getDataFromMemoryCallable(ADDRESS_COMPRESSOR_OPERATING_FREQUENCY, index))) return false;
            if (!setLogValue_NUMBER(requestBuilder, requestBuilderSize, "WL22", (int16_t)getDataFromMemoryCallable(ADDRESS_FAN_OPERATING_FREQUENCY_ROTATIONAL_SPEED, index))) return false;
            if (!setLogValue_NUMBER(requestBuilder, requestBuilderSize, "WL23", (int16_t)getDataFromMemoryCallable(ADDRESS_ELECTRONIC_EXPANSION_VALVE_STEPS_COUNT, index))) return false;
            if (!setLogValue_NUMBER(requestBuilder, requestBuilderSize, "WL24", (int16_t)getDataFromMemoryCallable(ADDRESS_NUMBER_OF_EVI_VALVE_STEPS, index))) return false;
            if (!setLogValue_NUMBER(requestBuilder, requestBuilderSize, "WL25", (int16_t)getDataFromMemoryCallable(ADDRESS_COMPRESSOR_2_TARGET_FREQUENCY, index))) return false;
            if (!setLogValue_NUMBER(requestBuilder, requestBuilderSize, "WL26", (int16_t)getDataFromMemoryCallable(ADDRESS_SYSTEM_2_COMPRESSOR_RUNNING, index))) return false;
            if (!setLogValue_NUMBER(requestBuilder, requestBuilderSize, "WL27", (int16_t)getDataFromMemoryCallable(ADDRESS_COMPRESSOR_PHASE_CURRENT, index))) return false;
            if (!setLogValue_NUMBER(requestBuilder, requestBuilderSize, "WL28", (int16_t)getDataFromMemoryCallable(ADDRESS_COMPRESSOR_IPM_TEMPERATURE, index))) return false;
            if (!setLogValue_NUMBER(requestBuilder, requestBuilderSize, "WL29", (int16_t)getDataFromMemoryCallable(ADDRESS_HIGH_PRESSURE_SATURATION_TEMPERATURE, index))) return false;
            if (!setLogValue_NUMBER(requestBuilder, requestBuilderSize, "WL30", (int16_t)getDataFromMemoryCallable(ADDRESS_LOW_PRESSURE_SATURATION_TEMPERATURE, index))) return false;

            if (!setLogValue_NUMBER(requestBuilder, requestBuilderSize, "WL31", (int16_t)getDataFromMemoryCallable(ADDRESS_EXTERNAL_AMBIENT_TEMPERATURE_T1, index))) return false;
            if (!setLogValue_NUMBER(requestBuilder, requestBuilderSize, "WL32", (int16_t)getDataFromMemoryCallable(ADDRESS_EXTERNAL_COIL_TUBE_FIN_T2, index))) return false;
            if (!setLogValue_NUMBER(requestBuilder, requestBuilderSize, "WL33", (int16_t)getDataFromMemoryCallable(ADDRESS_INTERNAL_COIL_TUBE_PLATE_REPLACEMENT_T3, index))) return false;
            if (!setLogValue_NUMBER(requestBuilder, requestBuilderSize, "WL34", (int16_t)getDataFromMemoryCallable(ADDRESS_RETURN_AIR_TEMPERATURE_T4, index))) return false;
            if (!setLogValue_NUMBER(requestBuilder, requestBuilderSize, "WL35", (int16_t)getDataFromMemoryCallable(ADDRESS_EXHAUST_TEMPERATURE_T5, index))) return false;
            if (!setLogValue_NUMBER(requestBuilder, requestBuilderSize, "WL36", (int16_t)getDataFromMemoryCallable(ADDRESS_RETURN_WATER_TEMPERATURE_T6, index))) return false;
            if (!setLogValue_NUMBER(requestBuilder, requestBuilderSize, "WL37", (int16_t)getDataFromMemoryCallable(ADDRESS_WATER_OUTLET_TEMPERATURE_T7, index))) return false;
            if (!setLogValue_NUMBER(requestBuilder, requestBuilderSize, "WL38", (int16_t)getDataFromMemoryCallable(ADDRESS_INDOOR_UNIT_PHASE_A_VOLTAGE, index))) return false;
            if (!setLogValue_NUMBER(requestBuilder, requestBuilderSize, "WL39", (int16_t)getDataFromMemoryCallable(ADDRESS_INDOOR_UNIT_PHASE_B_VOLTAGE, index))) return false;
            if (!setLogValue_NUMBER(requestBuilder, requestBuilderSize, "WL40", (int16_t)getDataFromMemoryCallable(ADDRESS_INDOOR_UNIT_PHASE_C_VOLTAGE, index))) return false;

            if (!setLogValue_NUMBER(requestBuilder, requestBuilderSize, "WL41", (int16_t)getDataFromMemoryCallable(ADDRESS_FLUORINE_OUTLET_TEMPERATURE_OF_PLATE_HEAT_EXCHANGER, index))) return false;
            if (!setLogValue_NUMBER(requestBuilder, requestBuilderSize, "WL42", (int16_t)getDataFromMemoryCallable(CURRENT_COP, index))) return false;
            if (!setLogValue_NUMBER(requestBuilder, requestBuilderSize, "WL43", (int16_t)getDataFromMemoryCallable(ADDRESS_PUMP_SPEED_PWM, index))) return false;
            if (!setLogValue_NUMBER(requestBuilder, requestBuilderSize, "WL44", (int16_t)getDataFromMemoryCallable(ADDRESS_WATER_FLOW, index))) return false;
            if (!setLogValue_NUMBER(requestBuilder, requestBuilderSize, "WL45", (int16_t)getDataFromMemoryCallable(ADDRESS_INDOOR_UNIT_PHASE_A_CURRENT, index))) return false;

            if (!setLogValue_NUMBER(requestBuilder, requestBuilderSize, "WL46", (int16_t)getDataFromMemoryCallable(ADDRESS_INDOOR_UNIT_PHASE_B_CURRENT, index))) return false;
            if (!setLogValue_NUMBER(requestBuilder, requestBuilderSize, "WL47", (int16_t)getDataFromMemoryCallable(ADDRESS_INDOOR_UNIT_PHASE_C_CURRENT, index))) return false;
            if (!setLogValue_NUMBER(requestBuilder, requestBuilderSize, "WL48", (int16_t)getDataFromMemoryCallable(ADDRESS_HIGH_INPUT_POWER_OF_INDOOR_UNIT, index))) return false;
            if (!setLogValue_NUMBER(requestBuilder, requestBuilderSize, "WL49", (int16_t)getDataFromMemoryCallable(ADDRESS_LOW_INPUT_POWER_OF_INDOOR_UNIT, index))) return false;
            if (!setLogValue_NUMBER(requestBuilder, requestBuilderSize, "WL50", (int16_t)getDataFromMemoryCallable(ADDRESS_HIGH_CUMULATIVE_ELECTRICITY_CONSUMPTION_OF_INDOOR_UNIT, index))) return false;

            if (!setLogValue_NUMBER(requestBuilder, requestBuilderSize, "WL51", (int16_t)getDataFromMemoryCallable(ADDRESS_LOW_CUMULATIVE_ELECTRICITY_CONSUMPTION_OF_INDOOR_UNIT, index))) return false;
            if (!setLogValue_NUMBER(requestBuilder, requestBuilderSize, "WL52", (int16_t)getDataFromMemoryCallable(OUTPUT_POWER_HEATING_HIGH_16BIT, index))) return false;
            if (!setLogValue_NUMBER(requestBuilder, requestBuilderSize, "WL53", (int16_t)getDataFromMemoryCallable(OUTPUT_POWER_HEATING_LOW_16BIT, index))) return false;
            if (!setLogValue_NUMBER(requestBuilder, requestBuilderSize, "WL54", (int16_t)getDataFromMemoryCallable(ADDRESS_WATER_INLET_TEMPERATURE, index))) return false;
            if (!setLogValue_NUMBER(requestBuilder, requestBuilderSize, "WL55", (int16_t)getDataFromMemoryCallable(ADDRESS_WATER_OUTLET_TEMPERATURE, index))) return false;
            if (!setLogValue_NUMBER(requestBuilder, requestBuilderSize, "WL56", (int16_t)getDataFromMemoryCallable(CURRENT_EER, index))) return false;
            if (!setLogValue_NUMBER(requestBuilder, requestBuilderSize, "WL57", (int16_t)getActiveStateFromActiveMode(getActiveStateValue()))) return false;

            if (!setLogValue_NUMBER(requestBuilder, requestBuilderSize, "WL58", (int16_t)getActiveStateValue())) return false;
            if (!setLogValue_NUMBER(requestBuilder, requestBuilderSize, "WL59", getActiveTemperatureSettingHeatpump())) return false;
            if (!setLogValue_NUMBER(requestBuilder, requestBuilderSize, "WL60", ReadSmartEeprom16(SEEP_ADDR_HEATING_SETPOINT))) return false;
            if (!setLogValue_NUMBER(requestBuilder, requestBuilderSize, "WL61", ReadSmartEeprom16(SEEP_ADDR_HOT_WATER_SETPOINT))) return false;
            if (!setLogValue_NUMBER(requestBuilder, requestBuilderSize, "WL62", UnitSystemParameterL[ADDRESS_STERILIZATION_TEMPERATURE_SETTING - START_ADDRESS_UNIT_SYSTEM_PARAMETER_L][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP])) return false;
            if (!setLogValue_NUMBER(requestBuilder, requestBuilderSize, "WL63", ReadSmartEeprom16(SEEP_ADDR_COOLING_SETPOINT))) return false;
            //if (!setLogValue_NUMBER(requestBuilder, requestBuilderSize, "WL63", UserParameters[ADDRESS_COOLING_SET_TEMPERATURE - START_ADDRESS_USER_PARAMETERS][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP])) return false;
            
            // LATER TERUG ZETTEN!!!
            if (!setLogValue_NUMBER(requestBuilder, requestBuilderSize, "WL64", (int16_t)getDataFromMemoryCallable(OUTPUT_POWER_COOLING_HIGH_16BIT, index))) return false;
            if (!setLogValue_NUMBER(requestBuilder, requestBuilderSize, "WL65", (int16_t)getDataFromMemoryCallable(OUTPUT_POWER_COOLING_LOW_16BIT, index))) return false;
            if (!setLogValue_NUMBER(requestBuilder, requestBuilderSize, "WL66", (int16_t)getDataFromMemoryCallable(COMPRESSOR_CUMMULATIVE_RUNNING_TIME_HIGH_BIT, index))) return false;
            if (!setLogValue_NUMBER(requestBuilder, requestBuilderSize, "WL67", (int16_t)getDataFromMemoryCallable(COMPRESSOR_CUMMULATIVE_RUNNING_TIME_LOW_BIT, index))) return false;
            if (!setLogValue_NUMBER(requestBuilder, requestBuilderSize, "WL68", (int16_t)getDataFromMemoryCallable(ADDRESS_CASCADE_SLAVES_ONLINE_1, index))) return false;          
            
            /*
             * Vervangen waardes
            setLogValue_NUMBER(requestBuilder, "WL58", (int16_t)getDataFromMemoryCallable(ADDRESS_SET_THE_UPPER_LIMIT_OF_FLOOR_HEATING_HEATING_HEATING_TEMPERATURE));
            setLogValue_NUMBER(requestBuilder, "WL59", (int16_t)getDataFromMemoryCallable(ADDRESS_SET_THE_LOWER_LIMIT_OF_FLOOR_HEATING_HEATING_HEATING_TEMPERATURE));
            setLogValue_NUMBER(requestBuilder, "WL60", (int16_t)getDataFromMemoryCallable(ADDRESS_SET_THE_UPPER_LIMIT_OF_HOT_WATER_TEMPERATURE));
            setLogValue_NUMBER(requestBuilder, "WL61", (int16_t)getDataFromMemoryCallable(ADDRESS_SET_THE_LOWER_LIMIT_OF_HOT_WATER_TEMPERATURE));
            setLogValue_NUMBER(requestBuilder, "WL62", (int16_t)getDataFromMemoryCallable(ADDRESS_SET_THE_UPPER_LIMIT_OF_REFRIGERATING_TEMPERATURE));
            setLogValue_NUMBER(requestBuilder, "WL63", (int16_t)getDataFromMemoryCallable(ADDRESS_SET_THE_LOWER_LIMIT_OF_REFRIGERATING_TEMPERATURE));       
            */     
            size_t L = strlen(requestBuilder);
            if (L > 0 && requestBuilder[L-1] == ',') requestBuilder[L-1] = '\0';
            break;
        }
        
        
        case BATTERY:{
                       
            break;
        }
        
        case SMART_METER:{
                     
            break;
        }
        
        case INVERTER:{
                       
            break;
        }
        
        case HEATPUMP_BOILER:{
                      
            break;
        }
        
        /* ZOU NOOIT GECALLED MOETEN WORDEN */
        default:{
            break;
        }
    }
    if (!appendf(requestBuilder, requestBuilderSize, "}}")) return false;
    return true;
}



void parseDeviceType (DEVICE_TYPE dvt, char* device) {
    DEVICE_TYPE_REQUESTED = dvt;
    switch (dvt) {
        case TULIP_PRINT:{
            snprintf(device, 20, "tulip_print");
            break;
        }
        
        case HEATPUMP:{
            snprintf(device, 20, "heatpump");
            break;
        }
        
        case BATTERY:{
            break;
        }
        
        case SMART_METER:{
            break;
        }
        
        case INVERTER:{
            break;
        }
        
        case HEATPUMP_BOILER:{
            break;
        }
        
        /* ZOU NOOIT GECALLED MOETEN WORDEN */
        default:{
            break;
        }
    }    
    return;
}


void parseRequestType (REQUEST_TYPE rqt, char* request) {
    switch(rqt) {
        case GET:{
            snprintf(request, 7, "GET");
            break;
        }    
        case POST:{
            snprintf(request, 7, "POST");
            break;
        }
        case PUT:{
            snprintf(request, 7, "PUT");
            break;
        } 
        case PATCH:{
            snprintf(request, 7, "PATCH");
            break;
        }  
        case DELETE:{
            snprintf(request, 7, "DELETE");
            break;
        }
        default:{
            break;
        }        
    }
    return;
}



void parsePath (char* path, REQUEST_TYPE rqt) {
    if (rqt == GET) { snprintf(path, 20, "%s", GET_PATH); }
    else if (rqt == POST) { snprintf(path, 20, "%s", POST_PATH); }
    return;  
}



static void SYS_CONSOLE_PRINT_CHUNKED(const char *label, const char *buf, size_t len)
{
    const size_t CHUNK = 400; // small enough to not overflow console ring buffer

    SYS_CONSOLE_PRINT("\r\n===== %s (%u bytes) =====\r\n", label, (unsigned)len);

    for (size_t off = 0; off < len; off += CHUNK) {
        size_t n = len - off;
        if (n > CHUNK) n = CHUNK;

        SYS_CONSOLE_PRINT("%04u: %.*s\r\n", (unsigned)off, (int)n, buf + off);

        // Give the console time to flush (important!)
        for (int i = 0; i < 10; i++) {
            SYS_CONSOLE_Tasks(sysObj.sysConsole0);
        }
    }

    SYS_CONSOLE_PRINT("===== END %s =====\r\n", label);
}


static void debugPrintSocketData(const uint8_t *data, size_t len)
{
    for (size_t i = 0; i < len; i++)
    {
        SYS_CONSOLE_PRINT("%c", data[i]);
    }
}



static void formatCascadeMaskBinary(uint16_t mask, char* out, size_t outSize)
{
    // Needs at least 20 bytes: "0000 0000 0000 0000\0"
    if (outSize < 20) {
        if (outSize > 0) out[0] = '\0';
        return;
    }

    int pos = 0;
    for (int bit = 15; bit >= 0; bit--)
    {
        out[pos++] = (mask & (1u << bit)) ? '1' : '0';

        if (bit % 4 == 0 && bit != 0)
            out[pos++] = ' ';
    }
    out[pos] = '\0';
}














static unsigned realtimeStage,realtimeDevice;
static uint16_t realtimeMask;
static int nextRealtime(void *user,const uint8_t **data,size_t *length) {
    char device[20];int n;(void)user;
    switch(realtimeStage) {
        case 0:
            n=snprintf(loggingChunk,sizeof(loggingChunk),"{\"date_time_utc\":\"%s\",\"checksum\":\"xxxyyy\",\"devices\":[",NTP_TIME_BUFFER);
            if(n<0||(size_t)n>=sizeof(loggingChunk))return -1;
            *data=(const uint8_t*)loggingChunk;*length=(size_t)n;realtimeStage=1;return 1;
        case 1:
            memset(devObjSendBuffer,0,sizeof(devObjSendBuffer));memset(device,0,sizeof(device));
            parseDeviceType(realtimeDevice?HEATPUMP:TULIP_PRINT,device);
            if(!setLoggingDataPerDeviceType(devObjSendBuffer,sizeof(devObjSendBuffer),device,(int)realtimeDevice))return -1;
            *data=(const uint8_t*)devObjSendBuffer;*length=strlen(devObjSendBuffer);
            if(realtimeDevice==0)realtimeDevice=1;
            else {
                unsigned bit=realtimeDevice;
                while(bit<16&&!CASCADE_BIT_IS_SET(realtimeMask,bit))bit++;
                realtimeDevice=bit<16?bit+1:0;
            }
            realtimeStage=realtimeDevice?2:3;return 1;
        case 2:*data=(const uint8_t*)",";*length=1;realtimeStage=1;return 1;
        case 3:*data=(const uint8_t*)"]}";*length=2;realtimeStage=4;return 1;
        default:return 0;
    }
}
bool TULIP_REQUEST_TESTER(char request[],char path[]) {
    int n;if(loggingTransmitBusy())return false;
    n=snprintf(settingsNetworkBuffer,sizeof(settingsNetworkBuffer),"%s /%s HTTP/1.1\r\nHost: %s\r\nAccept: application/json\r\nAccept-Encoding: identity\r\nContent-Type: application/json\r\nAuthorization: Bearer %s\r\nConnection: keep-alive\r\nTransfer-Encoding: chunked\r\n\r\n",request,path,HOST,TOKEN);
    realtimeStage=0;realtimeDevice=0;realtimeMask=getCascadeSlaveStatus();DEVICE_TYPE_REQUESTED=-1;
    return n>0&&(size_t)n<sizeof(settingsNetworkBuffer)&&beginLoggingRequest((size_t)n,nextRealtime);
}





void getRealTimeDataStatussen ( char requestBuilder[] ) {
    char device[20];

    if (!appendf(requestBuilder, DEVICE_JSON_MAX, "{\"date_time_utc\": \"%s\",", NTP_TIME_BUFFER)) return;
    if (!appendf(requestBuilder, DEVICE_JSON_MAX, "\"checksum\": \"xxxyyy\",")) return;
    if (!appendf(requestBuilder, DEVICE_JSON_MAX, "\"devices\": [")) return;

    /* LOG TULIP PRINT DATA */
    parseDeviceType(TULIP_PRINT, device);
    //setLoggingDataPerDeviceType(requestBuilder, DEVICE_JSON_MAX, device, 0);
    memset(device, 0, sizeof(device));

    /* ALS GEEN COMMUNICATIE ERROR MET DE WARMTE POMP LOG OOK DEZE WAARDES */
    if (!GetAlarm(ALARM_HEATPUMP_COMMUNICATION)) {
        if (!appendf(requestBuilder, DEVICE_JSON_MAX, ",")) return;
        parseDeviceType(HEATPUMP, device);
        //setLoggingDataPerDeviceType(requestBuilder, DEVICE_JSON_MAX, device, 1);
    }

    (void)appendf(requestBuilder, DEVICE_JSON_MAX, "]}");
    return;
}



//void loggingRequestBuilder ( REQUEST_TYPE rqt, DEVICE_TYPE dvt) {
bool loggingRequestBuilder ( REQUEST_TYPE rqt ) {
    char request[7];
    char path[20];
    
    getCurrentUtcTimestamp();
    parseRequestType(rqt, request);
    parsePath(path, rqt);
                
    return TULIP_REQUEST_TESTER(request, path);
}


/*
bool readNetworkBufferHeaderSslResponse ( void ) {
    int bytesRead;
    char* headerEnd;
    bytesRead = NET_PRES_SocketRead(socket, (uint8_t*)readBuffer, READ_BUFFER_SIZE);
    if (bytesRead <= 0) return false;

    readBuffer[bytesRead] = '\0';
    headerEnd = strstr(readBuffer, "\r\n\r\n");

    if (headerEnd) {
        size_t headerLen = headerEnd + 4 - readBuffer;
        size_t bodyLen = bytesRead - headerLen;
        if (bodyLen > 0) {
            //ProcessBinaryChunkBody((uint8_t*)(readBuffer + headerLen), bodyLen);
            totalBytesReceived += bodyLen;
        }
        
        char* contentLengthStr = strstr(readBuffer, "Content-Length:");
        if (contentLengthStr) {
            contentLengthStr += strlen("Content-Length:");
            while (*contentLengthStr == ' ') contentLengthStr++;  // skip spaces
            expectedContentLength = atoi(contentLengthStr);
            SYS_CONSOLE_PRINT("Content-Length parsed: %u bytes\n", (unsigned int)expectedContentLength);
        } else {
            SYS_CONSOLE_PRINT("No Content-Length header found. Using timeout fallback.\n");
            expectedContentLength = 0;
        }        
        //appState = APP_STATE_READ_BODY;
        return true;
    }    
    
    return false;    
}
*/


/*
 * Read out the server response from the network buffer.
 */
bool readNetworkBufferBodySslResponse ( void ) {
    //char networkBuffer[2048];
    int bytesRead;
    
    bytesRead = NET_PRES_SocketRead(log_socket, (uint8_t*)readBuffer, READ_BUFFER_SIZE);
       
    if (bytesRead > 0) {
        SYS_CONSOLE_PRINT("Received %d bytes from the server\r\n", bytesRead);
        
        for(int i = 0; i < bytesRead; i++) {
            SYS_CONSOLE_PRINT("%c", readBuffer[i]);
        }
        
        totalBytesReceived += bytesRead;
        return false;
    }    
    
    //if (expectedContentLength > 0 && totalBytesReceived >= expectedContentLength) {
    //    SYS_CONSOLE_PRINT("Received full content-length (%u bytes). Done.\n", (unsigned int)expectedContentLength);
    //    return true;
    //}    
    
    //return true;
    if (!NET_PRES_SocketIsConnected(log_socket)) {
        return true;
    }
    
    for(int i = 0; i < bytesRead; i++) {
        SYS_CONSOLE_PRINT("%c", readBuffer[i]);
    }
    
    // return false;
    return true;    
    
    //UNCOMMENT VOOR HELE RESPONSE
    //for(int i = 0; i < res; i++) {
    //    SYS_CONSOLE_PRINT("%c", networkBuffer[i]);
    //}
}



/*
 * Starts the TCP connection the the server on a given PORT
 */
static void reportLoggingSocketFailure(NET_PRES_SKT_ERROR_T error)
{
    TCPIP_STACK_HEAP_HANDLE heap = TCPIP_STACK_HeapHandleGet(TCPIP_STACK_HEAP_TYPE_INTERNAL_HEAP, 0);
    TCP_SOCKET_INFO info;
    unsigned occupied = 0;
    int socket;
    for (socket = 0; socket < TCPIP_TCP_MAX_SOCKETS; socket++)
    {
        if (TCPIP_TCP_SocketInfoGet((TCP_SOCKET)socket, &info))
        {
            occupied++;
            SYS_CONSOLE_PRINT("Logging TCP socket %d: state=%d tx=%u rx=%u pending=%u\r\n",
                socket, (int)info.state, (unsigned)info.txSize,
                (unsigned)info.rxSize, (unsigned)info.txPending);
        }
    }
    SYS_CONSOLE_PRINT("Could not create logging socket: NET_PRES error=%d TCP sockets=%u/%u heap free=%lu total=%lu largest=%lu\r\n",
        (int)error, occupied, (unsigned)TCPIP_TCP_MAX_SOCKETS,
        (unsigned long)(heap != NULL ? TCPIP_STACK_HEAP_FreeSize(heap) : 0),
        (unsigned long)(heap != NULL ? TCPIP_STACK_HEAP_Size(heap) : 0),
        (unsigned long)(heap != NULL ? TCPIP_STACK_HEAP_MaxSize(heap) : 0));
}

bool startSocketConnection ( void )
{
    NET_PRES_SKT_ERROR_T error = NET_PRES_SKT_OK;
    TCP_OPTION_LINGER_DATA closeOptions = {0};

    /* Never overwrite the only handle to an existing logging connection. */
    if (log_socket != INVALID_SOCKET)
    {
        SYS_CONSOLE_MESSAGE("Logging socket already owned; closing before retry\r\n");
        closeSocket();
    }
    log_socket = NET_PRES_SocketOpen(0, NET_PRES_SKT_UNENCRYPTED_STREAM_CLIENT,
        IP_ADDRESS_TYPE_IPV4, PORT, (NET_PRES_ADDRESS *)&address, &error);
    if (log_socket == INVALID_SOCKET)
    {
        reportLoggingSocketFailure(error);
        return false;
    }

    /* Logging is request/response. Once complete or aborted, do not retain its
     * large TCP buffers waiting for FIN/ACK. This affects only this socket;
     * normal writes/TLS remain unchanged until closeSocket is called. */
    closeOptions.lingerEnable = false;
    closeOptions.gracefulEnable = false;
    if (!NET_PRES_SocketOptionsSet(log_socket,
            (NET_PRES_SKT_OPTION_TYPE)TCP_OPTION_LINGER, &closeOptions))
    {
        SYS_CONSOLE_MESSAGE("Could not configure logging socket cleanup - aborting\r\n");
        closeSocket();
        return false;
    }
    NET_PRES_SocketWasReset(log_socket);
    return true;
}



/*
 * Wait for the connection to either be established successfully or fail.
 * In case of failing the process is aborted.
 */
SSL_NEGOTIATION_STATES waitForConnection ( void ) {
    if (!NET_PRES_SocketIsConnected(log_socket)) {
        return SOCKET_NOT_CONNECTED;
    }
            
    SYS_CONSOLE_MESSAGE("Connection Opened: Starting SSL Negotiation\r\n");
    if (!NET_PRES_SocketEncryptSocket(log_socket)) {
        SYS_CONSOLE_MESSAGE("SSL Create Connection Failed - Aborting the process!\r\n");
        return SSL_CREATE_CONNECTION_FAILED;
    } else {
        return SSL_NEGOTIATION_SUCCES;
    }    
}



/*
 * Parse the DNS for the server
 */
bool waitOnDNS ( void ) {
    TCPIP_DNS_RESULT result = TCPIP_DNS_IsResolved(HOST, &address, IP_ADDRESS_TYPE_IPV4);
    if (result != TCPIP_DNS_RES_OK) {
        return false;
    }
    
    //SYS_CONSOLE_PRINT("DNS Resolved IPv4 Address: %d.%d.%d.%d for host '%s'\r\n",
    //    address.v4Add.v[0], address.v4Add.v[1], address.v4Add.v[2], address.v4Add.v[3], HOST);
    return true;
}



/*
 * Wait for the SSL negotiation process to be completed.
 * If it fails we abort the process. 
 */
SSL_NEGOTIATION_STATES waitForSslConnection ( void ) {
    if (NET_PRES_SocketIsNegotiatingEncryption(log_socket)) {
        return SSL_BUSY_NEGOTIATING;
    }
            
    if (!NET_PRES_SocketIsSecure(log_socket)) {
        SYS_CONSOLE_MESSAGE("SSL Connection Negotiation Failed - Aborting\r\n");
        return SSL_SOCKET_NOT_SECURE;
    }

    SYS_CONSOLE_MESSAGE("SSL Connection Opened: Starting Clear Text Communication\r\n");
    return SSL_NEGOTIATION_SUCCES;
}



/*
 * Wait for the socket to be available for requests.
 */
SSL_SOCKET_STATES socketReady(void) {
    if(log_socket==INVALID_SOCKET)return SSL_SOCKET_WAS_DISCONNECTED;
    if(NET_PRES_SocketReadIsReady(log_socket)>0)return SSL_SOCKET_READY;
    if(!NET_PRES_SocketIsConnected(log_socket)||NET_PRES_SocketWasReset(log_socket)||NET_PRES_SocketWasDisconnected(log_socket))return SSL_SOCKET_WAS_DISCONNECTED;
    return SSL_SOCKET_NOT_READY;
}



/*
 * Close an open socket
 */
void closeSocket(void)
{
    LOG_HTTP_TxReset(&loggingTx);loggingFault=false;resetRxBuffer();
    memset(settingsNetworkBuffer,0,sizeof(settingsNetworkBuffer));memset(loggingChunk,0,sizeof(loggingChunk));
    if (log_socket != INVALID_SOCKET) {
        NET_PRES_SocketClose(log_socket);
        log_socket = INVALID_SOCKET;
    }
}



bool getUpdateSettingsFromServer(void) {
    int n;char hardware[50];if(loggingTransmitBusy())return false;
    snprintf(hardware,sizeof(hardware),"%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X-W-1",eui64[0],eui64[1],eui64[2],eui64[3],eui64[4],eui64[5],eui64[6],eui64[7]);
    n=snprintf(settingsNetworkBuffer,sizeof(settingsNetworkBuffer),"GET /api/v1/settings?hardware_id=%s&type=heatpump HTTP/1.1\r\nHost: %s\r\nAccept: application/json\r\nAuthorization: Bearer %s\r\nAccept-Encoding: identity\r\nConnection: keep-alive\r\nContent-Length: 0\r\n\r\n",hardware,HOST,TOKEN);
    return n>0&&(size_t)n<sizeof(settingsNetworkBuffer)&&beginLoggingRequest((size_t)n,NULL);
}



static cJSON *loggingParseJson(const char *text,size_t length) {
    const char *end; cJSON *root;size_t i,depth=0,tokens=0;bool quoted=false,escaped=false;
    if(!text||!length||memchr(text,0,length)) {loggingFault=true;return NULL;}
    for(i=0;i<length;i++) {
        char ch=text[i];
        if(quoted) {
            if(escaped) {
                if(ch=='u'&&i+4<length&&!memcmp(text+i+1,"0000",4)) {loggingFault=true;return NULL;}
                escaped=false;
            }
            else if(ch=='\\')escaped=true;
            else if(ch=='"')quoted=false;
            else if((unsigned char)ch<32) {loggingFault=true;return NULL;}
        } else if(ch=='"') {quoted=true;if(++tokens>512) {loggingFault=true;return NULL;}}
        else if(ch=='{'||ch=='[') {if(++depth>12) {loggingFault=true;return NULL;}}
        else if(ch=='}'||ch==']') {if(!depth) {loggingFault=true;return NULL;}depth--;}
    }
    if(quoted||depth) {loggingFault=true;return NULL;}
    root=cJSON_ParseWithLengthOpts(text,length,&end,false);
    if(!root) {loggingFault=true;return NULL;}
    while(end<text+length&&(*end==' '||*end=='\r'||*end=='\n'||*end=='\t'))end++;
    if(end!=text+length||!cJSON_IsObject(root)) {cJSON_Delete(root);loggingFault=true;return NULL;}
    return root;
}
bool parse_update_settings_from_json(const char *json_str, size_t len) {
    cJSON *root = loggingParseJson(json_str,len);
    if (!root) {
        SYS_CONSOLE_PRINT("Failed to parse JSON\n");
        loggingFault=true;return false;
    }

    cJSON *upd = cJSON_GetObjectItemCaseSensitive(root, "update_settings");
    if (cJSON_IsBool(upd)) {
        SYS_CONSOLE_PRINT("update_settings: %s\n", cJSON_IsTrue(upd) ? "true" : "false");
        bool updateSettings = cJSON_IsTrue(upd);
        cJSON_Delete(root);
        return updateSettings;
    } else {
        loggingFault=true;SYS_CONSOLE_PRINT("update_settings field missing\n");
        cJSON_Delete(root);
        return false;
    }

    cJSON_Delete(root);
    return false;
}


int bytesRead = 0;
size_t buffer_len;

void resetRxBuffer(void) { buffer_len=0;readBuffer[0]=0;loggingRxComplete=false;memset(&loggingResponse,0,sizeof(loggingResponse)); }
bool loggingConnectionReusable(void) { return log_socket!=INVALID_SOCKET&&loggingRxComplete&&loggingResponse.reusable&&NET_PRES_SocketIsConnected(log_socket); }

bool areUpdateSettingsAvailable(void) { if(loggingResponse.status==204&&buffer_len==0)return false;return parse_update_settings_from_json(readBuffer,buffer_len); }




bool readServerResponseDone(void) {
    int ready,count;bool eof;LOG_HTTP_RESULT result;
    if(loggingFault)return false;
    if(loggingRxComplete)return true;
    if(log_socket==INVALID_SOCKET) {loggingFault=true;return false;}
    ready=NET_PRES_SocketReadIsReady(log_socket);
    if(ready>0) {
        size_t room=READ_BUFFER_SIZE-buffer_len,size=READ_CHUNK_SIZE;
        if(!room) {loggingFault=true;return false;}
        if(size>room)size=room;
        if(size>(size_t)ready)size=(size_t)ready;
        count=(int)NET_PRES_SocketRead(log_socket,(uint8_t*)readBuffer+buffer_len,(uint16_t)size);
        if(count<0||(size_t)count>size) {loggingFault=true;return false;}
        if(count>0) {buffer_len+=(size_t)count;readBuffer[buffer_len]=0;loggingProgress=loggingNow();}
    }
    eof=ready<=0&&(!NET_PRES_SocketIsConnected(log_socket)||NET_PRES_SocketWasDisconnected(log_socket));
    if(NET_PRES_SocketWasReset(log_socket)) {loggingFault=true;return false;}
    result=LOG_HTTP_Response(readBuffer,buffer_len,eof,&loggingResponse);
    if(result==LOG_HTTP_BAD) {loggingFault=true;return false;}
    if(result==LOG_HTTP_MORE)return false;
    buffer_len=loggingResponse.bodyLength;loggingRxComplete=true;
    if(loggingResponse.status<200||loggingResponse.status>=300) {
        SYS_CONSOLE_PRINT("Logging HTTP failure: %u\r\n",loggingResponse.status);loggingFault=true;return false;
    }
    return true;
}

void processModbusSettingsFromServer (uint16_t address, uint16_t value) {
    switch(address) {
        /* DONE */
        case ADDRESS_SET_MODE: {
            WriteSmartEeprom16(SEEP_ADDR_HEATPUMP_MODE, value);
            break;
        }
        /* DONE */
        case ADDRESS_COOLING_SET_TEMPERATURE: {
            WriteSmartEeprom16(SEEP_ADDR_COOLING_SETPOINT, value);
            ChangeHeatpumpSetting(address, value);
            break;
        }
        /* DONE */
        case ADDRESS_HEATING_SET_TEMPERATURE: {
            WriteSmartEeprom16(SEEP_ADDR_HEATING_SETPOINT, value);
            ChangeHeatpumpSetting(address, value);
            break;
        }
        /* DONE */
        case ADDRESS_HOT_WATER_SET_TEMPERATURE: {
            WriteSmartEeprom16(SEEP_ADDR_HOT_WATER_SETPOINT, value);
            break;
        }
        /* DONE */
        case ADDRESS_ON_OFF:{
            WriteSmartEeprom8(SEEP_ADDR_DISPLAY_PUMP_ON, value);
            ChangeHeatpumpSetting(address, value);
            break;
        }
        
        case ADDRESS_TULIP_CONNECT_DIGITAL_INPUT_TWO: {
            // IF VALUE IS:
            // 0 == CHANGEOVER CONTACT DISABLED
            // 1 == CHANGEOVER CONTACT ENABLED
            WriteSmartEeprom8(SEEP_ADDR_DIGITAL_INPUT_TWO, value);
            WriteSmartEeprom16(SEEP_ADDR_CHANGEOVER_CONTACT_ENABLE, value);
            break;
        }

        /* DONE */
        /*  CUSTOM MODBUS ADRESSEN VANAF 0xC000  */
        case ADDRESS_TULIP_CONNECT_DIGITAL_INPUT_THREE: { 
            // IF VALUE IS:
            // 0 == EVU DISABLED
            // 1 == EVU ENABLED
            WriteSmartEeprom8(SEEP_ADDR_EVU_CONTACT_ENABLE, value);
            break;
        }
        
        /* DONE */
        case ADDRESS_TULIP_CONNECT_RELAIS_OUTPUT_ONE: {
            // IF VALUE IS:
            // 0 == COOLING CONTACT DISABLED
            // 1 == COOLING CONTACT ENABLED
            WriteSmartEeprom8(SEEP_ADDR_COOLING_CONTACT_ENABLE, value);
            break;
        }
        
        case ADDRESS_TULIP_CONNECT_RELAIS_OUTPUT_THREE: {
            // IF VALUE IS:
            // 0 == HYBRID SYSTEM DISABLED
            // 1 == HYBRID SYSTEM ENABLED
            WriteSmartEeprom16(SEEP_ADDR_HYBRID_SYSTEM_ENABLED, value);
            break;
        }        
        
        /* DONE */
        case ADDRESS_TULIP_CONNECT_SWITCH_HEATPUMP_ON_OFF_WITH_THERMOSTAT: { 
            WriteSmartEeprom8(SEEP_ADDR_SWITCH_HEATPUMP_ON_OFF_WITH_THERMOSTAT, value);
            break;
        }  
        
        /* DONE */
        case ADDRESS_TULIP_CONNECT_SILENT_MODE: {
            WriteSmartEeprom8(SEEP_ADDR_SILENT_MODE, value);
            if (value == 0 && ReadSmartEeprom8(SEEP_ADDR_BOOST_MODE) == 0) {
                ChangeHeatpumpSetting(ADDRESS_FREQUENCY_CONVERSION_MODE, 0);
            } else {
                ChangeHeatpumpSetting(ADDRESS_FREQUENCY_CONVERSION_MODE, 2);
            }
            break;     
        }
        
        /* DONE */
        case ADDRESS_TULIP_CONNECT_BOOST_MODE: {
            WriteSmartEeprom8(SEEP_ADDR_BOOST_MODE, value);
            if (value == 0 && ReadSmartEeprom8(SEEP_ADDR_SILENT_MODE) == 0) {
                ChangeHeatpumpSetting(ADDRESS_FREQUENCY_CONVERSION_MODE, 0);
            } else {
                ChangeHeatpumpSetting(ADDRESS_FREQUENCY_CONVERSION_MODE, 1);
            }
            break;               
        }             
        
        /* DONE */
        case ADDRESS_TULIP_CONNECT_START_TIME_SILENT_MODE: {
            WriteSmartEeprom16(SEEP_ADDR_START_TIME_SILENT_MODE, value);
            break;               
        }
        
        /* DONE */
        case ADDRESS_TULIP_CONNECT_END_TIME_SILENT_MODE: {
            WriteSmartEeprom16(SEEP_ADDR_END_TIME_SILENT_MODE, value);
            break;               
        }     
          
        /* DONE */
        case ADDRESS_TULIP_CONNECT_USE_SILENT_MODE_TIMERS: {
            WriteSmartEeprom8(SEEP_ADDR_USE_SILENT_MODE_TIMERS, value);
            break;               
        }               
        
        /* DONE */
        case ADDRESS_TULIP_CONNECT_BLOCK_HOTWATER: {
            WriteSmartEeprom8(SEEP_ADDR_BLOCK_HOTWATER, value);
            break;               
        }        
        
        /* DONE */
        case ADDRESS_TULIP_CONNECT_START_TIME_BLOCK_HOTWATER: {
            WriteSmartEeprom16(SEEP_ADDR_END_TIME_BLOCK_HOTWATER, value);
            break;               
        }         
        
        /* DONE */
        case ADDRESS_TULIP_CONNECT_END_TIME_BLOCK_HOTWATER: {
            WriteSmartEeprom16(SEEP_ADDR_START_TIME_BLOCK_HOTWATER, value);
            break;               
        }        
        
        /* ONDERSTAAND ZIJN ECHTE WAARDES */
        case ADDRESS_TULIP_CONNECT_SOFTWARE_RESET: {
            /* LET OP! DIT RESET DE CONNECT DIRECT NA HET OPHALEN VAN DE SETTINGS */
            WriteSmartEeprom8(SEEP_ADDR_SOFTWARE_RESET, value);
            break;    
        }        
        case ADDRESS_TULIP_CONNECT_MINIMUM_TIME_IN_HOTWATER: {
            /* Waarde wordt opgestuurd in minuten maar wij regelen op seconden dus *60! */
            WriteSmartEeprom16(SEEP_ADDR_HOT_WATER_MIN_TIME_IN_HOT_WATER_MODE, (value * 60));
            break;               
        }        
        case ADDRESS_TULIP_CONNECT_HOTWATER_ELEMENT_ON_AFTER_TIME: {
            /* Waarde wordt opgestuurd in minuten maar wij regelen op seconden dus *60! */
            WriteSmartEeprom16(SEEP_ADDR_HOT_WATER_RUNNING_TIME_BEFORE_TURNING_ON_HEATING_ELEMENT, (value * 60));
            break;               
        }               
        case ADDRESS_TULIP_CONNECT_HOTWATER_ELEMENT_MAX_ON_TIME: {
            /* Waarde wordt opgestuurd in minuten maar wij regelen op seconden dus *60! */
            WriteSmartEeprom16(SEEP_ADDR_HOT_WATER_MAX_TIME_HEATING_ELEMENT_ON_IN_HEATING_MODE_SEC, (value * 60));
            break;               
        }               
        case ADDRESS_TULIP_CONNECT_PUMP_ON_TIME_AFTER_OFF_TIME_REACHED: {
            WriteSmartEeprom16(SEEP_ADDR_PUMP_LAG_TIME_AFTER_THERMOSTAT_CONTACT_DISCONNECTED_SEC, value);
            break;               
        }        
        
        case ADDRESS_TULIP_CONNECT_DEGREES_PER_MINUTE_DELTA: {
            WriteSmartEeprom16(SEEP_ADDR_HEATING_TIME_CONSTANT_SEC, (value * 60));
            break;
        }
        case ADDRESS_TULIP_CONNECT_EMERGENCY_FORCE_HEATING_ELEMENT: {
            WriteSmartEeprom16(SEEP_ADDR_EMERGENCY_MODE_HEATING_ENABLED, value);
            break;
        }
        case ADDRESS_TULIP_CONNECT_EMERGENCY_FORCE_HOTWATER_ELEMENT: {
            WriteSmartEeprom16(SEEP_ADDR_EMERGENCY_MODE_HOTWATER_ENABLED, value);
            break;
        }
        
        /*
            WORDT NIET MEER GEBRUIKT, WEL LATEN STAAN
        */
        /*
            case ADDRESS_TULIP_CONNECT_CIRCULATION_TEMPERATURE_PUMP_OFF_DELTA_LOW: {
                WriteSmartEeprom16(SEEP_ADDR_PUMP_OFF_TEMP_TOO_LOW, value);
                break;
            }
            case ADDRESS_TULIP_CONNECT_CIRCULATION_TEMPERATURE_PUMP_ON_DELTA_LOW: {
                WriteSmartEeprom16(SEEP_ADDR_PUMP_ON_TEMP_AFTER_TOO_LOW_TEMP, value);
                break;
            }
            case ADDRESS_TULIP_CONNECT_CIRCULATION_TEMPERATURE_PUMP_OFF_DELTA_HIGH: {
                WriteSmartEeprom16(SEEP_ADDR_PUMP_OFF_TEMP_TOO_HIGH, value);
                break;
            }
            case ADDRESS_TULIP_CONNECT_CIRCULATION_TEMPERATURE_PUMP_ON_DELTA_HIGH: {
                WriteSmartEeprom16(SEEP_ADDR_PUMP_ON_TEMP_AFTER_TOO_HIGH_TEMP, value);
                break;
            }
        */    
        
        case ADDRESS_TULIP_CONNECT_CIRCULATION_ON_TEMPERATURE: {
            WriteSmartEeprom16(SEEP_ADDR_CIRCULATION_PUMP_ON_TEMPERATURE, value);
            break;
        }
        case ADDRESS_TULIP_CONNECT_CIRCULATION_CONTROL_AT_AMBIENT_TEMP: {
            WriteSmartEeprom16(SEEP_ADDR_CIRCULATION_PUMP_CONTROL_AT_AMBIENT_TEMPERATURE, value);
            break;
        }
        case ADDRESS_TULIP_CONNECT_HYBRID_SYSTEM_ENABLED_ON_HEATING_ELEMENT_RELAIS: {
            WriteSmartEeprom16(SEEP_ADDR_HYBRID_SYSTEM_ENABLED_ON_HEATING_ELEMENT_RELAIS, value);
            break;
        }
        case ADDRESS_TULIP_CONNECT_HEATING_MODE_MAX_TIME_WITH_ELEMENT_ON: {
            WriteSmartEeprom16(SEEP_ADDR_HEATING_MODE_MAX_TIME_WITH_ELEMENT_ON, value);
            break;
        }
        case ADDRESS_TULIP_CONNECT_HEATING_MODE_MAX_TIME_WITH_CIRCULATION_PUMP_OFF: {
            WriteSmartEeprom16(SEEP_ADDR_HEATING_MODE_MAX_TIME_WITH_CIRCULATION_PUMP_OFF, value);
            break;
        }
        case ADDRESS_TULIP_CONNECT_HEATING_MODE_MAX_TIME_WITH_ELEMENT_ON_AND_CIRCULATION_PUMP_OFF: {
            WriteSmartEeprom16(SEEP_ADDR_HEATING_MODE_MAX_TIME_WITH_ELEMENT_ON_AND_CIRCULATION_PUMP_OFF, value);
            break;
        }
        case ADDRESS_TULIP_CONNECT_ENABLE_FREQUENCY_CONTROLLER_FUNCTION: {
            // if (value == true) {
            //    WriteSmartEeprom16(SEEP_ADDR_MAXIMUM_TARGET_COMPRESSOR_FREQUENCY_CONSTANT_B, getDataFromMemoryCallable(ADDRESS_HEATING_TARGET_FREQUENCY_CONSTANT_B, MASTER_HEATPUMP_IN_CASCADE));
            //    WriteSmartEeprom16(SEEP_ADDR_MAXIMUM_TARGET_COMPRESSOR_FREQUENCY, getDataFromMemoryCallable(ADDRESS_HEATING_TARGET_FREQUENCY_UPPER_LIMIT, MASTER_HEATPUMP_IN_CASCADE));
            //    WriteSmartEeprom16(SEEP_ADDR_MINIMUM_TARGET_COMPRESSOR_FREQUENCY, getDataFromMemoryCallable(ADDRESS_HEATING_TARGET_FREQUENCY_LOWER_LIMIT, MASTER_HEATPUMP_IN_CASCADE));
            //}   
            WriteSmartEeprom16(SEEP_ADDR_ENABLE_FREQUENCY_CONTROLLER_FUNCTION, value);
            break;
        }
        case ADDRESS_TULIP_CONNECT_RESET_EEPROM: {
            restoreEepromValuesToDefault();
            WriteSmartEeprom8(SEEP_ADDR_SOFTWARE_RESET, value);
            break;
        }
        
        case ADDRESS_HEATING_TARGET_FREQUENCY_CONSTANT_B: {
            WriteSmartEeprom16(SEEP_ADDR_MAXIMUM_TARGET_COMPRESSOR_FREQUENCY_CONSTANT_B, value);
            break;
        }
        case ADDRESS_HEATING_TARGET_FREQUENCY_UPPER_LIMIT: {
            WriteSmartEeprom16(SEEP_ADDR_MAXIMUM_TARGET_COMPRESSOR_FREQUENCY, value);
            break;
        }
        case ADDRESS_HEATING_TARGET_FREQUENCY_LOWER_LIMIT: {
            WriteSmartEeprom16(SEEP_ADDR_MINIMUM_TARGET_COMPRESSOR_FREQUENCY, value);
            break;
        }  
        case ADDRESS_DC_FAN_INITIAL_FREQUENCY: {
            WriteSmartEeprom16(SEEP_ADDR_INITIAL_FAN_SPEED, value);
            ChangeHeatpumpSetting(address, value);
            break;            
        }
        //
        case RETURN_DIFFERENTIAL_VALUE_HEATING: {
            WriteSmartEeprom16(SEEP_ADDR_RETURN_DIFFERENTIAL_VALUE_HEATING, value);
            break;
        }
        case RETURN_DIFFERENTIAL_VALUE_COOLING: {
            WriteSmartEeprom16(SEEP_ADDR_RETURN_DIFFERENTIAL_VALUE_COOLING, value);
            break;
        }
        
        default: {
            ChangeHeatpumpSetting(address, value);
            break;
        }
    }
}



int setting_count = 0;
bool parse_modbus_settings(void)
{
    setting_count = 0;
    if(loggingResponse.status==204&&buffer_len==0)return false;

    cJSON *root = loggingParseJson(readBuffer,buffer_len);
    if (!root) {
        buffer_len = 0;
        SYS_CONSOLE_PRINT("Failed to parse JSON\n");
        loggingFault=true;return false;
    }

    cJSON *updates = cJSON_GetObjectItem(root, "settings_updates");
    if (!cJSON_IsArray(updates)) {
        loggingFault=true;
        SYS_CONSOLE_PRINT("settings_updates not found or not an array\n");
        cJSON_Delete(root);
        buffer_len = 0;
        return false;
    }

    if (cJSON_GetArraySize(updates) == 0) {
        SYS_CONSOLE_PRINT("No settings to update\n");
        cJSON_Delete(root);
        buffer_len = 0;
        return false;
    }

    /* Validate the complete batch before applying any setting. */
    cJSON *item=NULL;
    if(cJSON_GetArraySize(updates)>MAX_SETTINGS) {cJSON_Delete(root);loggingFault=true;return false;}
    cJSON_ArrayForEach(item,updates) {
        const cJSON *addr=cJSON_GetObjectItem(item,"modbus_address");
        const cJSON *value=cJSON_GetObjectItem(item,"value");
        char *end;long number;
        if(!cJSON_IsObject(item)||!cJSON_IsString(addr)||!cJSON_IsString(value)||!addr->valuestring[0]||!value->valuestring[0])goto invalidBatch;
        errno=0;number=strtol(addr->valuestring,&end,0);
        if(errno||*end||number<0||number>65535)goto invalidBatch;
        errno=0;number=strtol(value->valuestring,&end,10);
        if(errno||*end||number< -32768||number>65535)goto invalidBatch;
    }
    cJSON_ArrayForEach(item,updates) {
        const cJSON *addr=cJSON_GetObjectItem(item,"modbus_address");
        const cJSON *value=cJSON_GetObjectItem(item,"value");
        processModbusSettingsFromServer((uint16_t)strtol(addr->valuestring,NULL,0),(uint16_t)strtol(value->valuestring,NULL,10));
        setting_count++;
    }
    cJSON_Delete(root);
    buffer_len = 0;

    return true;
invalidBatch:
    cJSON_Delete(root);loggingFault=true;buffer_len=0;return false;
}














static const uint32_t eep_addrs_8_bit_1[]={
        SEEP_ADDR_EVU_CONTACT_ENABLE,       
        SEEP_ADDR_HEATPUMP_WAS_ON_BEFORE_FORCED_OFF,     
        SEEP_ADDR_SWITCH_HEATPUMP_ON_OFF_WITH_THERMOSTAT,
        SEEP_ADDR_SOFTWARE_RESET,                        
        SEEP_ADDR_SILENT_MODE,      
    };
static const uint32_t eep_addrs_16_bit_1[]={
        SEEP_ADDR_START_TIME_SILENT_MODE,                
        SEEP_ADDR_END_TIME_SILENT_MODE,   
    };
static const uint32_t eep_addrs_8_bit_2[]={
        SEEP_ADDR_BOOST_MODE,                            
        SEEP_ADDR_USE_SILENT_MODE_TIMERS,                
        SEEP_ADDR_BLOCK_HOTWATER,                                    
     };
static const uint32_t eep_addrs_16_bit_2[]={
        SEEP_ADDR_START_TIME_BLOCK_HOTWATER,             
        SEEP_ADDR_END_TIME_BLOCK_HOTWATER,    
        SEEP_ADDR_HOT_WATER_MIN_TIME_IN_HOT_WATER_MODE,
        SEEP_ADDR_HOT_WATER_RUNNING_TIME_BEFORE_TURNING_ON_HEATING_ELEMENT,
        SEEP_ADDR_HOT_WATER_MAX_TIME_HEATING_ELEMENT_ON_IN_HEATING_MODE_SEC,
        SEEP_ADDR_PUMP_LAG_TIME_AFTER_THERMOSTAT_CONTACT_DISCONNECTED_SEC,
     };
static const uint32_t eep_addrs_8_bit_3[]={
        SEEP_ADDR_DIGITAL_INPUT_TWO,
        SEEP_ADDR_DIGITAL_INPUT_ONE,
        SEEP_ADDR_COOLING_CONTACT_ENABLE,
        SEEP_ADDR_RELAIS_OUTPUT_TWO,
        SEEP_ADDR_RELAIS_OUTPUT_THREE,
     };
static const uint32_t eep_addrs_16_bit_3[]={
        SEEP_ADDR_HEATING_TIME_CONSTANT_SEC,      
        SEEP_ADDR_HYBRID_SYSTEM_ENABLED_ON_HEATING_ELEMENT_RELAIS,
        SEEP_ADDR_HEATING_MODE_MAX_TIME_WITH_ELEMENT_ON,
        SEEP_ADDR_HEATING_MODE_MAX_TIME_WITH_CIRCULATION_PUMP_OFF,
        SEEP_ADDR_HEATING_MODE_MAX_TIME_WITH_ELEMENT_ON_AND_CIRCULATION_PUMP_OFF,
        SEEP_ADDR_HYBRID_SYSTEM_ENABLED,
        SEEP_ADDR_HYBRID_SYSTEM_ENABLED_ON_HEATING_ELEMENT_RELAIS,
        SEEP_ADDR_HEATING_MODE_MAX_TIME_WITH_ELEMENT_ON,
        SEEP_ADDR_HEATING_MODE_MAX_TIME_WITH_CIRCULATION_PUMP_OFF,
        SEEP_ADDR_HEATING_MODE_MAX_TIME_WITH_ELEMENT_ON_AND_CIRCULATION_PUMP_OFF,
        SEEP_ADDR_MINIMUM_TARGET_COMPRESSOR_FREQUENCY,
        SEEP_ADDR_MAXIMUM_TARGET_COMPRESSOR_FREQUENCY,
        SEEP_ADDR_ENABLE_FREQUENCY_CONTROLLER_FUNCTION,
        SEEP_ADDR_MAXIMUM_TARGET_COMPRESSOR_FREQUENCY_CONSTANT_B,
        SEEP_ADDR_RETURN_DIFFERENTIAL_VALUE_HEATING,
        SEEP_ADDR_RETURN_DIFFERENTIAL_VALUE_COOLING,
     };
typedef struct { unsigned bits;uint16_t first,end;const uint32_t *addresses;size_t count; } LOG_SETTING_SEGMENT;
static const LOG_SETTING_SEGMENT settingSegments[]={
{0,0x0100,0x022C,NULL,0},
{0,0x0300,0x0319,NULL,0},
{0,0x0330,0x0346,NULL,0},
{0,0x0360,0x0363,NULL,0},
{0,0x0800,0x0831,NULL,0},
{0,0x1000,0x1024,NULL,0},
{8,0,0,eep_addrs_8_bit_1,sizeof(eep_addrs_8_bit_1)/sizeof(eep_addrs_8_bit_1[0])},
{16,0,0,eep_addrs_16_bit_1,sizeof(eep_addrs_16_bit_1)/sizeof(eep_addrs_16_bit_1[0])},
{8,0,0,eep_addrs_8_bit_2,sizeof(eep_addrs_8_bit_2)/sizeof(eep_addrs_8_bit_2[0])},
{16,0,0,eep_addrs_16_bit_2,sizeof(eep_addrs_16_bit_2)/sizeof(eep_addrs_16_bit_2[0])},
{8,0,0,eep_addrs_8_bit_3,sizeof(eep_addrs_8_bit_3)/sizeof(eep_addrs_8_bit_3[0])},
{16,0,0,eep_addrs_16_bit_3,sizeof(eep_addrs_16_bit_3)/sizeof(eep_addrs_16_bit_3[0])}
};
static unsigned settingsSegment;
static size_t settingsIndex;
static int nextSettings(void *user,const uint8_t **data,size_t *length) {
    size_t at=0;unsigned budget=0;(void)user;
    while(settingsSegment<sizeof(settingSegments)/sizeof(settingSegments[0])&&budget++<64) {
        const LOG_SETTING_SEGMENT *g=&settingSegments[settingsSegment];unsigned value;int n;
        size_t count=g->bits?g->count:(size_t)(g->end-g->first);
        if(settingsIndex==count) {settingsIndex=0;settingsSegment++;continue;}
        value=g->bits==8?ReadSmartEeprom8(g->addresses[settingsIndex]):g->bits==16?ReadSmartEeprom16(g->addresses[settingsIndex]):getDataFromMemoryCallable(g->first+(int)settingsIndex,MASTER_HEATPUMP_IN_CASCADE);
        n=snprintf(loggingChunk+at,sizeof(loggingChunk)-at,"%u,",value);
        if(n<0)return -1;
        if((size_t)n>=sizeof(loggingChunk)-at)break;
        at+=(size_t)n;settingsIndex++;
    }
    if(!at&&settingsSegment<sizeof(settingSegments)/sizeof(settingSegments[0]))return -1;
    *data=(const uint8_t*)loggingChunk;*length=at;return at?1:0;
}
bool sendUpdatedSettingsList(void) {
    int n;char hardware[50];
    if(loggingTransmitBusy())return false;
    snprintf(hardware,sizeof(hardware),"%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X-W-1",eui64[0],eui64[1],eui64[2],eui64[3],eui64[4],eui64[5],eui64[6],eui64[7]);
    n=snprintf(settingsNetworkBuffer,sizeof(settingsNetworkBuffer),"POST /api/v1/settings?hardware_id=%s&type=heatpump HTTP/1.1\r\nHost: %s\r\nAccept: application/json\r\nAuthorization: Bearer %s\r\nAccept-Encoding: identity\r\nConnection: keep-alive\r\nContent-Type: text/plain\r\nTransfer-Encoding: chunked\r\n\r\n",hardware,HOST,TOKEN);
    {
        unsigned mode=getDataFromMemoryCallable(ADDRESS_FREQUENCY_CONVERSION_MODE,MASTER_HEATPUMP_IN_CASCADE);
        uint8_t silent=(uint8_t)(mode==2),boost=(uint8_t)(mode==1);
        if(ReadSmartEeprom8(SEEP_ADDR_SILENT_MODE)!=silent)WriteSmartEeprom8(SEEP_ADDR_SILENT_MODE,silent);
        if(ReadSmartEeprom8(SEEP_ADDR_BOOST_MODE)!=boost)WriteSmartEeprom8(SEEP_ADDR_BOOST_MODE,boost);
    }
    settingsSegment=0;settingsIndex=0;
    return n>0&&(size_t)n<sizeof(settingsNetworkBuffer)&&beginLoggingRequest((size_t)n,nextSettings);
}



bool readServerResponseUpdatedSettingsDone(void) { return readServerResponseDone(); }
