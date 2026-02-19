#include <xc.h>
#include <string.h>
#include <stdbool.h>                   
#include "definitions.h"
#include "configuration.h"

#include "logging.h"
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


//extern const TCPIP_NETWORK_CONFIG __attribute__((unused))  TCPIP_HOSTS_CONFIGURATION[];
extern const TCPIP_STACK_MODULE_CONFIG TCPIP_STACK_MODULE_CONFIG_TBL [];
//extern const size_t TCPIP_HOSTS_CONFIGURATION_SIZE;
extern const size_t TCPIP_STACK_MODULE_CONFIG_TBL_SIZE;
//extern APP_HEATING_AND_HOT_WATER_DATA app_Data;

NET_PRES_SKT_HANDLE_T   log_socket;
IP_MULTI_ADDRESS        address;

DEVICE_TYPE DEVICE_TYPE_REQUESTED = -1; 
char NTP_TIME_BUFFER[40];

uint8_t TcpIPStackResetCount = 0;
        
static size_t totalBytesReceived = 0;
static char readBuffer[READ_BUFFER_SIZE + 1];
//static uint8_t readBuffer[READ_BUFFER_SIZE + 1];
//static size_t expectedContentLength = 0;

bool settingChangedInDisplay = false;
bool newLogRequired = false;

void setSettingChangedInDisplay(bool value) {
    settingChangedInDisplay = value;
}

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

void increaseTcpIpResetCounter() {
    TcpIPStackResetCount += 1;
}

uint8_t getTcpIpResetCounter() {
    return TcpIPStackResetCount;
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


bool setupNewTcpipStack() {
    TCPIP_STACK_INIT    tcpipInit;
    
    char macToString[40];
    
    uint8_t firstMacByte = eui64[2];
    firstMacByte |= 0x02;  // Set "locally administered" bit
    firstMacByte &= ~0x01; 
    
    sprintf(macToString, "%02X:%02X:%02X:%02X:%02X:%02X", 
            firstMacByte,eui64[3],eui64[4],eui64[5],eui64[6],eui64[7]);
    
    //SYS_CONSOLE_PRINT("!!! MAC ADDRESS SET TO %s !!!\r\n", macToString);        
    
    TCPIP_NETWORK_CONFIG __attribute__((unused))  TCPIP_HOSTS_CONFIGURATION_CUSTOM[] =
    {
        /*** Network Configuration Index 0 ***/
        {
            .interface = TCPIP_NETWORK_DEFAULT_INTERFACE_NAME_IDX0,
            .hostName = TCPIP_NETWORK_DEFAULT_HOST_NAME_IDX0,
            .macAddr = macToString, // Change default MAC to MAC obtained from IC
            .ipAddr = TCPIP_NETWORK_DEFAULT_IP_ADDRESS_IDX0,
            .ipMask = TCPIP_NETWORK_DEFAULT_IP_MASK_IDX0,
            .gateway = TCPIP_NETWORK_DEFAULT_GATEWAY_IDX0,
            .priDNS = TCPIP_NETWORK_DEFAULT_DNS_IDX0,
            .secondDNS = TCPIP_NETWORK_DEFAULT_SECOND_DNS_IDX0,
            .powerMode = TCPIP_NETWORK_DEFAULT_POWER_MODE_IDX0,
            .startFlags = TCPIP_NETWORK_DEFAULT_INTERFACE_FLAGS_IDX0,
            .pMacObject = &TCPIP_NETWORK_DEFAULT_MAC_DRIVER_IDX0,
        },
    };
    size_t TCPIP_HOSTS_CONFIGURATION_SIZE_CUSTOM = sizeof (TCPIP_HOSTS_CONFIGURATION_CUSTOM) / sizeof (*TCPIP_HOSTS_CONFIGURATION_CUSTOM);
    
    tcpipInit.pNetConf = TCPIP_HOSTS_CONFIGURATION_CUSTOM;
    tcpipInit.nNets = TCPIP_HOSTS_CONFIGURATION_SIZE_CUSTOM;
    tcpipInit.pModConfig = TCPIP_STACK_MODULE_CONFIG_TBL;
    tcpipInit.nModules = TCPIP_STACK_MODULE_CONFIG_TBL_SIZE;
    tcpipInit.initCback = 0;

    sysObj.tcpip = TCPIP_STACK_Initialize(0, &tcpipInit.moduleInit);
    if (sysObj.tcpip == SYS_MODULE_OBJ_INVALID) {
        SYS_CONSOLE_PRINT("TCPIP_STACK_Init Failed!\n");
        return false;
    }    
    return true;
}



void setHardwareId( char device[], char* hardwareId, int modbusIndex ) {
    switch (DEVICE_TYPE_REQUESTED){
        case TULIP_PRINT:{
            sprintf(hardwareId, "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
                    eui64[0], eui64[1], eui64[2], eui64[3], eui64[4], eui64[5], eui64[6], eui64[7]);       
            break;
        }
        
        case HEATPUMP:{
            sprintf(hardwareId, "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X-W-%i",
                    eui64[0], eui64[1], eui64[2], eui64[3], eui64[4], eui64[5], eui64[6], eui64[7], modbusIndex);
            break;
        }
        
        case BATTERY:{
            sprintf(hardwareId, "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X-B-%i",
                    eui64[0], eui64[1], eui64[2], eui64[3], eui64[4], eui64[5], eui64[6], eui64[7], modbusIndex);            
            break;
        }
        
        case SMART_METER:{
            sprintf(hardwareId, "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X-SM-%i",
                    eui64[0], eui64[1], eui64[2], eui64[3], eui64[4], eui64[5], eui64[6], eui64[7], modbusIndex);            
            break;
        }
        
        case INVERTER:{
            sprintf(hardwareId, "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X-I-%i",
                    eui64[0], eui64[1], eui64[2], eui64[3], eui64[4], eui64[5], eui64[6], eui64[7], modbusIndex);            
            break;
        }
        
        case HEATPUMP_BOILER:{
            sprintf(hardwareId, "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X-HB-%i",
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
void setLogValue_STRING( char* requestBuilder, char settingName[], char value[]) {
    sprintf(parameter, "\"%s\": \"%s\",", settingName, value);
    strcat(requestBuilder, parameter);
    memset(parameter, 0, sizeof (parameter));
    return;
}



void setLogValue_BOOLEAN( char* requestBuilder, char settingName[], bool value) {
    sprintf(parameter, "\"%s\": %d,", settingName, value);
    strcat(requestBuilder, parameter);
    memset(parameter, 0, sizeof (parameter));
    return;
}



void setLogValue_NUMBER( char* requestBuilder, char settingName[], int value ) {
    sprintf(parameter, "\"%s\": %i,", settingName, value);
    strcat(requestBuilder, parameter);
    memset(parameter, 0, sizeof (parameter));
    return;
}

static bool setLogValue_NUMBER_S(char* rb, size_t rbSize, const char* name, int value)
{
    return appendf(rb, rbSize, "\"%s\": %i,", name, value);
}

static bool setLogValue_STRING_S(char* rb, size_t rbSize, const char* name, const char* value)
{
    return appendf(rb, rbSize, "\"%s\": \"%s\",", name, value);
}






bool setLoggingDataPerDeviceType(char* requestBuilder, size_t requestBuilderSize, char device[], int index) {
    char hardwareId[50];
   
    /* SET THE DEVICE TYPE IN THE HTTP HEADER */
    sprintf(parameter, "{\"type\": \"%s\",", device);
    strcat(requestBuilder, parameter);
    memset(parameter, 0, sizeof (parameter));
    
    /* SET THE HARDWARE ID IN THE HTTP HEADER */
    setHardwareId( device, hardwareId, index );
    sprintf(parameter, "\"hardware_id\": \"%s\",", hardwareId);
    strcat(requestBuilder, parameter);
    memset(parameter, 0, sizeof (parameter));
        
    /* ADD THE REQUEST BODY TO THE HTTP */
    strcat(requestBuilder, "\"values\": {");
    switch (DEVICE_TYPE_REQUESTED){
        
        case TULIP_PRINT:{
            setLogValue_STRING(requestBuilder, HardwareID, hardwareId);          
            setLogValue_STRING(requestBuilder, KlantID, UNDEFINED_STRING);
            
            /* Software versie */
            sprintf(parameter, "\"%s\": \"%d-%d-%d\",", SoftwareVersie, (int)((THIS_FIRMWARE_VERSION / 1000000)), (int)((THIS_FIRMWARE_VERSION / 1000) % 1000), (int)(THIS_FIRMWARE_VERSION % 1000));
            strcat(requestBuilder, parameter);
            memset(parameter, 0, sizeof (parameter));
            
            /* Bootloader software versie */
            uint32_t BOOTLOADER_FIRMWARE_VERSION = ReadSmartEeprom32(BOOTLOADER_SOFTWARE_VERSION);
            sprintf(parameter, "\"%s\": \"%d-%d-%d\",", BootloaderSoftwareVersie, (int)((BOOTLOADER_FIRMWARE_VERSION / 1000000)), (int)((BOOTLOADER_FIRMWARE_VERSION / 1000) % 1000), (int)(BOOTLOADER_FIRMWARE_VERSION % 1000));
            strcat(requestBuilder, parameter);
            memset(parameter, 0, sizeof (parameter));
                
            //setLogValue_STRING(requestBuilder, BootloaderSoftwareVersie, UNDEFINED_STRING);
            setLogValue_STRING(requestBuilder, RTCCDatumtijd, NTP_TIME_BUFFER);    
            setLogValue_STRING(requestBuilder, NTPDatumtijd, NTP_TIME_BUFFER);   
            setLogValue_NUMBER(requestBuilder, Lognummer, UNDEFINED_INT16_T);
            
            setLogValue_NUMBER(requestBuilder, ConnectTemp1, GetNtcTemperature(NTC_HEATING_BUFFER));
            setLogValue_NUMBER(requestBuilder, ConnectTemp2, GetNtcTemperature(NTC_HOT_WATER_BUFFER));
            setLogValue_NUMBER(requestBuilder, ConnectTemp3, GetNtcTemperature(NTC_RESERVED_1));
            setLogValue_NUMBER(requestBuilder, ConnectTemp4, GetNtcTemperature(NTC_RESERVED_2));
            
            setLogValue_BOOLEAN(requestBuilder, ConnectInput1, GetThermostatContact());
            setLogValue_BOOLEAN(requestBuilder, ConnectInput2, GetDigitalInput2());
            setLogValue_BOOLEAN(requestBuilder, ConnectInput3, GetDigitalInput3());
            
            setLogValue_BOOLEAN(requestBuilder, ConnectRelay1, RelayPotfree1_Get());
            setLogValue_BOOLEAN(requestBuilder, ConnectRelay2, RelayPotfree2_Get());
            setLogValue_BOOLEAN(requestBuilder, ConnectRelay3, RelayPotfree3_Get());
            setLogValue_BOOLEAN(requestBuilder, BufferElekElement, getStatusHeatingElementHeatingBuffer());
            setLogValue_BOOLEAN(requestBuilder, TapwaterElekElement, getStatusHeatingElementHotWaterBuffer());
            
            setLogValue_BOOLEAN(requestBuilder, Reserved230vRelay, getReserved230VRelay());
            setLogValue_BOOLEAN(requestBuilder, ThreeWayValveRelay, getStatus3WayValve());
            
            setLogValue_BOOLEAN(requestBuilder, CirculatorPump, getStatusCirculationPump());
            setLogValue_BOOLEAN(requestBuilder, SwitchInput, getInputSlideSwitch());
            
            setLogValue_BOOLEAN(requestBuilder, VerbondenMetServer, TRUE);
            setLogValue_BOOLEAN(requestBuilder, DIPswitch1, GetDip1());
            setLogValue_BOOLEAN(requestBuilder, DIPswitch2, GetDip2());
            setLogValue_BOOLEAN(requestBuilder, DIPswitch3, GetDip3());
            setLogValue_BOOLEAN(requestBuilder, DIPswitch4, GetDip4());
            
            //setLogValue_BOOLEAN(requestBuilder, ConfigModusActief, FALSE);
            //setLogValue_STRING(requestBuilder, WifiStatus, UNDEFINED_CHAR);   
            //setLogValue_NUMBER(requestBuilder, WifiInfo, UNDEFINED_INT16_T);
            
            setLogValue_BOOLEAN(requestBuilder, SDcardAanwezig, SD_CARD_MOUNT_FLAG);
            setLogValue_NUMBER(requestBuilder, SDstatus, getSdCardState());          
            
            //setLogValue_STRING(requestBuilder, RS485commMetDisplayOK, UNDEFINED_CHAR);
            //setLogValue_STRING(requestBuilder, RS485ommMetWarmtepompOK, UNDEFINED_CHAR);
            //setLogValue_NUMBER(requestBuilder, EthernetStatus, UNDEFINED_INT16_T);  
            //setLogValue_NUMBER(requestBuilder, USBstatus, UNDEFINED_INT16_T);  
            
            setLogValue_BOOLEAN(requestBuilder, PowerFailStatus, getPowerFailStatus());
            setLogValue_BOOLEAN(requestBuilder, SupercapacitorFaultStatus, getSupercapFaultStatus());
            setLogValue_BOOLEAN(requestBuilder, Systemgoodindicator, getSystemGoodIndicator());
            setLogValue_BOOLEAN(requestBuilder, SupercapacitorPowerGoodIndicator, getSupercapacitorPowerGoodIndicator());
            
            setLogValue_NUMBER(requestBuilder, Alarmbytes, GetAlarm(0)); 
            
            setLogValue_BOOLEAN(requestBuilder, emergencyModeHeating, ReadSmartEeprom16(SEEP_ADDR_EMERGENCY_MODE_HEATING_ENABLED));
            setLogValue_BOOLEAN(requestBuilder, emergencyModeHotwater, ReadSmartEeprom16(SEEP_ADDR_EMERGENCY_MODE_HOTWATER_ENABLED));            
            setLogValue_NUMBER(requestBuilder, circPumpOffTempLow, ReadSmartEeprom16(SEEP_ADDR_PUMP_OFF_TEMP_TOO_LOW)); 
            setLogValue_NUMBER(requestBuilder, circPumpOnTempLow, ReadSmartEeprom16(SEEP_ADDR_PUMP_ON_TEMP_AFTER_TOO_LOW_TEMP)); 
            setLogValue_NUMBER(requestBuilder, circPumpOffTempHigh, ReadSmartEeprom16(SEEP_ADDR_PUMP_OFF_TEMP_TOO_HIGH)); 
            setLogValue_NUMBER(requestBuilder, circPumpOnTempHigh, ReadSmartEeprom16(SEEP_ADDR_PUMP_ON_TEMP_AFTER_TOO_HIGH_TEMP)); 
            
            setLogValue_NUMBER(requestBuilder, circPumpOnTemp, ReadSmartEeprom16(SEEP_ADDR_CIRCULATION_PUMP_ON_TEMPERATURE)); 
            setLogValue_NUMBER(requestBuilder, circPumpControlOnAmbTemp, ReadSmartEeprom16(SEEP_ADDR_CIRCULATION_PUMP_CONTROL_AT_AMBIENT_TEMPERATURE)); 
            
            requestBuilder[strlen(requestBuilder)-1] = '\0';
            break;
        }
        
        
        
        case HEATPUMP:{
            index -= 1;
            if (!setLogValue_NUMBER_S(requestBuilder, requestBuilderSize, "WL1",  getDataFromMemoryCallable(ADDRESS_RUNNING_STATUS_1, index))) return false;
            if (!setLogValue_NUMBER_S(requestBuilder, requestBuilderSize, "WL2",  getDataFromMemoryCallable(ADDRESS_RUNNING_STATUS_2, index))) return false;
            if (!setLogValue_NUMBER_S(requestBuilder, requestBuilderSize, "WL3",  getDataFromMemoryCallable(ADDRESS_FAULT_STATE_1, index))) return false;
            if (!setLogValue_NUMBER_S(requestBuilder, requestBuilderSize, "WL4",  getDataFromMemoryCallable(ADDRESS_FAULT_STATE_2, index))) return false;
            if (!setLogValue_NUMBER_S(requestBuilder, requestBuilderSize, "WL5",  getDataFromMemoryCallable(ADDRESS_FAULT_STATE_3, index))) return false;
            if (!setLogValue_NUMBER_S(requestBuilder, requestBuilderSize, "WL6",  getDataFromMemoryCallable(ADDRESS_SYSTEM_1_FAULT_STATE_1, index))) return false;
            if (!setLogValue_NUMBER_S(requestBuilder, requestBuilderSize, "WL7",  getDataFromMemoryCallable(ADDRESS_SYSTEM_1_FAULT_STATE_2, index))) return false;
            if (!setLogValue_NUMBER_S(requestBuilder, requestBuilderSize, "WL8",  getDataFromMemoryCallable(ADDRESS_SYSTEM_1_DRIVE_FAULT_STATE_1, index))) return false;
            if (!setLogValue_NUMBER_S(requestBuilder, requestBuilderSize, "WL9",  getDataFromMemoryCallable(ADDRESS_SYSTEM_1_DRIVE_FAULT_STATE_2, index))) return false;
            if (!setLogValue_NUMBER_S(requestBuilder, requestBuilderSize, "WL10", getDataFromMemoryCallable(ADDRESS_SYSTEM_1_DRIVE_FAULT_STATE_3, index))) return false;

            if (!setLogValue_NUMBER_S(requestBuilder, requestBuilderSize, "WL11", getDataFromMemoryCallable(ADDRESS_RELAY_OUTPUT_STATUS_1, index))) return false;
            if (!setLogValue_NUMBER_S(requestBuilder, requestBuilderSize, "WL12", getDataFromMemoryCallable(ADDRESS_RELAY_OUTPUT_STATUS_2, index))) return false;
            if (!setLogValue_NUMBER_S(requestBuilder, requestBuilderSize, "WL13", getDataFromMemoryCallable(ADDRESS_RELAY_OUTPUT_STATUS_3, index))) return false;
            if (!setLogValue_NUMBER_S(requestBuilder, requestBuilderSize, "WL14", getDataFromMemoryCallable(ADDRESS_RELAY_OUTPUT_STATUS_4, index))) return false;
            if (!setLogValue_NUMBER_S(requestBuilder, requestBuilderSize, "WL15", getDataFromMemoryCallable(ADDRESS_SWITCH_PORT_STATE_1, index))) return false;
            if (!setLogValue_NUMBER_S(requestBuilder, requestBuilderSize, "WL16", getDataFromMemoryCallable(ADDRESS_SWITCH_PORT_STATE_2, index))) return false;
            if (!setLogValue_NUMBER_S(requestBuilder, requestBuilderSize, "WL17", getDataFromMemoryCallable(ADDRESS_SWITCH_PORT_STATE_3, index))) return false;
            if (!setLogValue_NUMBER_S(requestBuilder, requestBuilderSize, "WL18", getDataFromMemoryCallable(ADDRESS_SWITCH_PORT_STATE_4, index))) return false;
            if (!setLogValue_NUMBER_S(requestBuilder, requestBuilderSize, "WL19", getDataFromMemoryCallable(ADDRESS_CURRENT_UNIT_TOOLING_NO, index))) return false;
            if (!setLogValue_NUMBER_S(requestBuilder, requestBuilderSize, "WL20", (int16_t)getDataFromMemoryCallable(ADDRESS_COMPRESSOR_1_TARGET_FREQUENCY, index))) return false;

            if (!setLogValue_NUMBER_S(requestBuilder, requestBuilderSize, "WL21", (int16_t)getDataFromMemoryCallable(ADDRESS_COMPRESSOR_OPERATING_FREQUENCY, index))) return false;
            if (!setLogValue_NUMBER_S(requestBuilder, requestBuilderSize, "WL22", (int16_t)getDataFromMemoryCallable(ADDRESS_FAN_OPERATING_FREQUENCY_ROTATIONAL_SPEED, index))) return false;
            if (!setLogValue_NUMBER_S(requestBuilder, requestBuilderSize, "WL23", (int16_t)getDataFromMemoryCallable(ADDRESS_ELECTRONIC_EXPANSION_VALVE_STEPS_COUNT, index))) return false;
            if (!setLogValue_NUMBER_S(requestBuilder, requestBuilderSize, "WL24", (int16_t)getDataFromMemoryCallable(ADDRESS_NUMBER_OF_EVI_VALVE_STEPS, index))) return false;
            if (!setLogValue_NUMBER_S(requestBuilder, requestBuilderSize, "WL25", (int16_t)getDataFromMemoryCallable(ADDRESS_AC_INPUT_VOLTAGE, index))) return false;
            if (!setLogValue_NUMBER_S(requestBuilder, requestBuilderSize, "WL26", (int16_t)getDataFromMemoryCallable(ADDRESS_AC_INPUT_CURRENT, index))) return false;
            if (!setLogValue_NUMBER_S(requestBuilder, requestBuilderSize, "WL27", (int16_t)getDataFromMemoryCallable(ADDRESS_COMPRESSOR_PHASE_CURRENT, index))) return false;
            if (!setLogValue_NUMBER_S(requestBuilder, requestBuilderSize, "WL28", (int16_t)getDataFromMemoryCallable(ADDRESS_COMPRESSOR_IPM_TEMPERATURE, index))) return false;
            if (!setLogValue_NUMBER_S(requestBuilder, requestBuilderSize, "WL29", (int16_t)getDataFromMemoryCallable(ADDRESS_HIGH_PRESSURE_SATURATION_TEMPERATURE, index))) return false;
            if (!setLogValue_NUMBER_S(requestBuilder, requestBuilderSize, "WL30", (int16_t)getDataFromMemoryCallable(ADDRESS_LOW_PRESSURE_SATURATION_TEMPERATURE, index))) return false;

            if (!setLogValue_NUMBER_S(requestBuilder, requestBuilderSize, "WL31", (int16_t)getDataFromMemoryCallable(ADDRESS_EXTERNAL_AMBIENT_TEMPERATURE_T1, index))) return false;
            if (!setLogValue_NUMBER_S(requestBuilder, requestBuilderSize, "WL32", (int16_t)getDataFromMemoryCallable(ADDRESS_EXTERNAL_COIL_TUBE_FIN_T2, index))) return false;
            if (!setLogValue_NUMBER_S(requestBuilder, requestBuilderSize, "WL33", (int16_t)getDataFromMemoryCallable(ADDRESS_INTERNAL_COIL_TUBE_PLATE_REPLACEMENT_T3, index))) return false;
            if (!setLogValue_NUMBER_S(requestBuilder, requestBuilderSize, "WL34", (int16_t)getDataFromMemoryCallable(ADDRESS_RETURN_AIR_TEMPERATURE_T4, index))) return false;
            if (!setLogValue_NUMBER_S(requestBuilder, requestBuilderSize, "WL35", (int16_t)getDataFromMemoryCallable(ADDRESS_EXHAUST_TEMPERATURE_T5, index))) return false;
            if (!setLogValue_NUMBER_S(requestBuilder, requestBuilderSize, "WL36", (int16_t)getDataFromMemoryCallable(ADDRESS_RETURN_WATER_TEMPERATURE_T6, index))) return false;
            if (!setLogValue_NUMBER_S(requestBuilder, requestBuilderSize, "WL37", (int16_t)getDataFromMemoryCallable(ADDRESS_WATER_OUTLET_TEMPERATURE_T7, index))) return false;
            if (!setLogValue_NUMBER_S(requestBuilder, requestBuilderSize, "WL38", (int16_t)getDataFromMemoryCallable(ADDRESS_ECONOMIZER_INLET_TUBE_T8, index))) return false;
            if (!setLogValue_NUMBER_S(requestBuilder, requestBuilderSize, "WL39", (int16_t)getDataFromMemoryCallable(ADDRESS_ECONOMIZER_OUTLET_TUBE_T9, index))) return false;
            if (!setLogValue_NUMBER_S(requestBuilder, requestBuilderSize, "WL40", (int16_t)getDataFromMemoryCallable(ADDRESS_WATER_TANK_TEMPERATURE, index))) return false;

            if (!setLogValue_NUMBER_S(requestBuilder, requestBuilderSize, "WL41", (int16_t)getDataFromMemoryCallable(ADDRESS_FLUORINE_OUTLET_TEMPERATURE_OF_PLATE_HEAT_EXCHANGER, index))) return false;
            if (!setLogValue_NUMBER_S(requestBuilder, requestBuilderSize, "WL42", (int16_t)getDataFromMemoryCallable(ADDRESS_DRIVE_MANUFACTURER, index))) return false;
            if (!setLogValue_NUMBER_S(requestBuilder, requestBuilderSize, "WL43", (int16_t)getDataFromMemoryCallable(ADDRESS_PUMP_SPEED_PWM, index))) return false;
            if (!setLogValue_NUMBER_S(requestBuilder, requestBuilderSize, "WL44", (int16_t)getDataFromMemoryCallable(ADDRESS_WATER_FLOW, index))) return false;
            if (!setLogValue_NUMBER_S(requestBuilder, requestBuilderSize, "WL45", (int16_t)getDataFromMemoryCallable(ADDRESS_USER_RETURN_WATER_TEMPERATURE, index))) return false;

            if (!setLogValue_NUMBER_S(requestBuilder, requestBuilderSize, "WL46", (int16_t)getDataFromMemoryCallable(ADDRESS_DEVICE_INPUT_VOLTAGE, index))) return false;
            if (!setLogValue_NUMBER_S(requestBuilder, requestBuilderSize, "WL47", (int16_t)getDataFromMemoryCallable(ADDRESS_DEVICE_INPUT_CURRENT, index))) return false;
            if (!setLogValue_NUMBER_S(requestBuilder, requestBuilderSize, "WL48", (int16_t)getDataFromMemoryCallable(ADDRESS_DEVICE_INPUT_POWER_KW, index))) return false;
            if (!setLogValue_NUMBER_S(requestBuilder, requestBuilderSize, "WL49", (int16_t)getDataFromMemoryCallable(ADDRESS_TOTAL_UNIT_ELECTRICITY_CONSUMPTION_KWH, index))) return false;
            if (!setLogValue_NUMBER_S(requestBuilder, requestBuilderSize, "WL50", (int16_t)getDataFromMemoryCallable(ADDRESS_AUXILIARY_HEATING_SOURCE_HOT_WATER_TEMPERATURE_VALUE, index))) return false;

            if (!setLogValue_NUMBER_S(requestBuilder, requestBuilderSize, "WL51", (int16_t)getDataFromMemoryCallable(ADDRESS_AUXILIARY_HEATING_SOURCE_HEATING_TEMPERATURE_VALUE, index))) return false;
            if (!setLogValue_NUMBER_S(requestBuilder, requestBuilderSize, "WL52", (int16_t)getDataFromMemoryCallable(ADDRESS_BUFFER_TANK_FOR_HEATING_TEMPERATURE_VALUE, index))) return false;
            if (!setLogValue_NUMBER_S(requestBuilder, requestBuilderSize, "WL53", (int16_t)getDataFromMemoryCallable(ADDRESS_MAIN_OUTLET_WATER_TEMPERATURE_VALUE, index))) return false;
            if (!setLogValue_NUMBER_S(requestBuilder, requestBuilderSize, "WL54", (int16_t)getDataFromMemoryCallable(ADDRESS_WATER_INLET_TEMPERATURE, index))) return false;
            if (!setLogValue_NUMBER_S(requestBuilder, requestBuilderSize, "WL55", (int16_t)getDataFromMemoryCallable(ADDRESS_WATER_OUTLET_TEMPERATURE, index))) return false;
            if (!setLogValue_NUMBER_S(requestBuilder, requestBuilderSize, "WL56", (int16_t)getDataFromMemoryCallable(ADDRESS_EXTERNAL_ENVIRONMENT_TEMPERATURE, index))) return false;
            if (!setLogValue_NUMBER_S(requestBuilder, requestBuilderSize, "WL57", (int16_t)getActiveStateFromActiveMode(getActiveStateValue()))) return false;

            if (!setLogValue_NUMBER_S(requestBuilder, requestBuilderSize, "WL58", (int16_t)getActiveStateValue())) return false;
            if (!setLogValue_NUMBER_S(requestBuilder, requestBuilderSize, "WL59", UserParameters[ADDRESS_HEATING_SET_TEMPERATURE - START_ADDRESS_USER_PARAMETERS][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP])) return false;
            if (!setLogValue_NUMBER_S(requestBuilder, requestBuilderSize, "WL60", ReadSmartEeprom16(SEEP_ADDR_HEATING_SETPOINT))) return false;
            if (!setLogValue_NUMBER_S(requestBuilder, requestBuilderSize, "WL61", ReadSmartEeprom16(SEEP_ADDR_HOT_WATER_SETPOINT))) return false;
            if (!setLogValue_NUMBER_S(requestBuilder, requestBuilderSize, "WL62", UnitSystemParameterL[ADDRESS_STERILIZATION_TEMPERATURE_SETTING - START_ADDRESS_UNIT_SYSTEM_PARAMETER_L][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP])) return false;
            if (!setLogValue_NUMBER_S(requestBuilder, requestBuilderSize, "WL63", UserParameters[ADDRESS_COOLING_SET_TEMPERATURE - START_ADDRESS_USER_PARAMETERS][PARAMETER_ARRAY_DATA_READ_FROM_HEATPUMP])) return false;
            
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
    strcat(requestBuilder, "}}");
    return true;
}



void parseDeviceType (DEVICE_TYPE dvt, char* device) {
    DEVICE_TYPE_REQUESTED = dvt;
    switch (dvt) {
        case TULIP_PRINT:{
            strcpy(device, "tulip_print");
            break;
        }
        
        case HEATPUMP:{
            strcpy(device, "heatpump");
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
            strcpy(request, "GET");
            break;
        }    
        case POST:{
            strcpy(request, "POST");
            break;
        }
        case PUT:{
            strcpy(request, "PUT");
            break;
        } 
        case PATCH:{
            strcpy(request, "PATCH");
            break;
        }  
        case DELETE:{
            strcpy(request, "DELETE");
            break;
        }
        default:{
            break;
        }        
    }
    return;
}



void parsePath (char* path, REQUEST_TYPE rqt) {
    if (rqt == GET) { sprintf(path, "%s", GET_PATH); }
    else if (rqt == POST) { sprintf(path, "%s", POST_PATH); }
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



static bool socketWriteAll(const uint8_t* data, size_t len)
{
    uint32_t spins = 0;

    while (len > 0)
    {
        // how much can we write right now?
        uint16_t canWrite = NET_PRES_SocketWriteIsReady(log_socket, (uint16_t)len, 0);

        if (canWrite == 0)
        {
            // Not an error: TX buffer full, let stack progress
            TCPIP_STACK_Task(sysObj.tcpip);
            NET_PRES_Tasks(sysObj.netPres);

            if (++spins > 50000u)   // timeout guard
            {
                SYS_CONSOLE_PRINT("socketWriteAll timeout\r\n");
                return false;
            }
            continue;
        }

        int w = NET_PRES_SocketWrite(log_socket, data, canWrite);
        if (w < 0) return false;

        data += (size_t)w;
        len  -= (size_t)w;
        spins = 0;
    }

    return true;
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



static size_t measureRealtimeBodyLen(uint8_t deviceCount)
{
    (void)deviceCount; // no longer used

    size_t total = 0;
    uint16_t cascadeMask = getCascadeSlaveStatus();

    char tmp[256];
    int n = snprintf(tmp, sizeof(tmp),
        "{\"date_time_utc\":\"%s\",\"checksum\":\"xxxyyy\",\"devices\":[",
        NTP_TIME_BUFFER);
    if (n < 0 || (size_t)n >= sizeof(tmp))
        return 0;

    total += (size_t)n;

    char devObj[DEVICE_JSON_MAX];
    char device[20];

    /* ---------- Device 0: controller (always) ---------- */
    memset(devObj, 0, sizeof(devObj));
    memset(device, 0, sizeof(device));

    parseDeviceType(TULIP_PRINT, device);
    if (!setLoggingDataPerDeviceType(devObj, sizeof(devObj), device, 0))
        return 0;

    total += strlen(devObj);

    /* ---------- WL-1: heatpump index 1 (ALWAYS) ---------- */
    total += 1; // comma

    memset(devObj, 0, sizeof(devObj));
    memset(device, 0, sizeof(device));

    parseDeviceType(HEATPUMP, device);
    if (!setLoggingDataPerDeviceType(devObj, sizeof(devObj), device, 1))
        return 0;

    total += strlen(devObj);
  

    /* ---------- WL-2 .. WL-16 (mask controlled) ---------- */
    for (uint8_t bit = 1; bit < 16; bit++)
    {
        if (!CASCADE_BIT_IS_SET(cascadeMask, bit))
            continue;

        total += 1; // comma

        memset(devObj, 0, sizeof(devObj));
        memset(device, 0, sizeof(device));

        parseDeviceType(HEATPUMP, device);

        int hpIndex = (int)bit + 1; // bit 1 ? index 2

        if (!setLoggingDataPerDeviceType(devObj, sizeof(devObj), device, hpIndex))
            return 0;

        total += strlen(devObj);
    }

    total += 2; // "]}"
    return total;
}





static bool sendRealtimeBody(uint8_t deviceCount)
{
    (void)deviceCount; // no longer used

    char tmp[256];
    uint16_t cascadeMask = getCascadeSlaveStatus();

    int n = snprintf(tmp, sizeof(tmp),
        "{\"date_time_utc\":\"%s\",\"checksum\":\"xxxyyy\",\"devices\":[",
        NTP_TIME_BUFFER);
    if (n < 0 || (size_t)n >= sizeof(tmp))
        return false;

    if (!socketWriteAll((const uint8_t*)tmp, (size_t)n))
        return false;

    // Optional: print once per request
    // SYS_CONSOLE_PRINT("Cascade mask = 0x%04X (WL-1 forced)\r\n", cascadeMask);

    char devObj[DEVICE_JSON_MAX];
    char device[20];

    /* ---------- Device 0: controller (always) ---------- */
    memset(devObj, 0, sizeof(devObj));
    memset(device, 0, sizeof(device));

    parseDeviceType(TULIP_PRINT, device);
    if (!setLoggingDataPerDeviceType(devObj, sizeof(devObj), device, 0))
        return false;

    if (!socketWriteAll((const uint8_t*)devObj, strlen(devObj)))
        return false;

    /* ---------- WL-1: heatpump index 1 (ALWAYS) ---------- */
    if (!socketWriteAll((const uint8_t*)",", 1))
        return false;

    memset(devObj, 0, sizeof(devObj));
    memset(device, 0, sizeof(device));

    parseDeviceType(HEATPUMP, device);
    if (!setLoggingDataPerDeviceType(devObj, sizeof(devObj), device, 1))
        return false;

    if (!socketWriteAll((const uint8_t*)devObj, strlen(devObj)))
        return false;

    
    char maskStr[20];
    formatCascadeMaskBinary(cascadeMask, maskStr, sizeof(maskStr));
    SYS_CONSOLE_PRINT("Cascade mask = %s (0x%04X)\r\n", maskStr, cascadeMask);  
    
    
    /* ---------- WL-2 .. WL-16 (mask controlled) ---------- */
    for (uint8_t bit = 1; bit < 16; bit++)
    {
        if (!CASCADE_BIT_IS_SET(cascadeMask, bit))
            continue;

        if (!socketWriteAll((const uint8_t*)",", 1))
            return false;

        memset(devObj, 0, sizeof(devObj));
        memset(device, 0, sizeof(device));

        parseDeviceType(HEATPUMP, device);

        int hpIndex = (int)bit + 1; // bit 1 -> index 2

        if (!setLoggingDataPerDeviceType(devObj, sizeof(devObj), device, hpIndex))
            return false;

        if (!socketWriteAll((const uint8_t*)devObj, strlen(devObj)))
            return false;
    }

    return socketWriteAll((const uint8_t*)"]}", 2);
}




static bool httpWriteChunk(const char* s, size_t len)
{
    char hdr[16];
    int n = sprintf(hdr, "%X\r\n", (unsigned)len);
    if (n <= 0) return false;

    if (!socketWriteAll((const uint8_t*)hdr, (size_t)n)) return false;
    if (len && !socketWriteAll((const uint8_t*)s, len)) return false;
    return socketWriteAll((const uint8_t*)"\r\n", 2);
}



bool TULIP_REQUEST_TESTER(char request[], char path[])
{
    // decide how many devices you want to send
    // total devices = 1 controller + (up to) 15 heatpumps
    uint8_t deviceCount = MAX_LOG_DEVICES; // e.g. 16
    if (GetAlarm(ALARM_HEATPUMP_COMMUNICATION))
    {
        deviceCount = 1; // only controller if heatpump comm alarm
    }

    size_t bodyLen = measureRealtimeBodyLen(deviceCount);
    if (bodyLen == 0) return false;

    char hdr[768];
    int hn = snprintf(hdr, sizeof(hdr),
        "%s /%s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Accept: application/json\r\n"
        "Content-Type: application/json\r\n"
        "Authorization: Bearer %s\r\n"
        "Connection: keep-alive\r\n"
        "Content-Length: %lu\r\n"
        "\r\n",
        request, path, HOST, TOKEN, (unsigned long)bodyLen);

    if (hn < 0 || (size_t)hn >= sizeof(hdr)) return false;
    if (!socketWriteAll((const uint8_t*)hdr, (size_t)hn)) return false;

    if (!sendRealtimeBody(deviceCount)) return false;

    SYS_CONSOLE_PRINT("*** REQUEST SEND (content-length): %s, %s, %s, len=%lu\r\n",
                      request, path, NTP_TIME_BUFFER, (unsigned long)bodyLen);

    DEVICE_TYPE_REQUESTED = -1;
    return true;
}





void getRealTimeDataStatussen ( char requestBuilder[] ) {   
    char dateTimeSet[150];
    char device[20];
              
    sprintf(dateTimeSet, "{\"date_time_utc\": \"%s\",", NTP_TIME_BUFFER);
    strcat(requestBuilder, dateTimeSet);  
    strcat(requestBuilder, "\"checksum\": \"xxxyyy\",");
    strcat(requestBuilder, "\"devices\": [");
    
    /* LOG TULIP PRINT DATA */
    parseDeviceType(TULIP_PRINT, device);  
    //setLoggingDataPerDeviceType(requestBuilder, device, 0);
    memset(device, 0, sizeof (device));   
    
    /* ALS GEEN COMMUNICATIE ERROR MET DE WARMTE POMP LOG OOK DEZE WAARDES */
    if (!GetAlarm(ALARM_HEATPUMP_COMMUNICATION)) {
    //if (GetAlarm(ALARM_HEATPUMP_COMMUNICATION)) {
        strcat(requestBuilder, ",");
        parseDeviceType(HEATPUMP, device);
        //setLoggingDataPerDeviceType(requestBuilder, device, 1);
    }
    
    strcat(requestBuilder, "]}");
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
bool startSocketConnection ( void ) {
    //SYS_CONSOLE_PRINT("Starting TCP/IPv4 Connection to : %d.%d.%d.%d port  '%d'\r\n",
    //    address.v4Add.v[0], address.v4Add.v[1], address.v4Add.v[2], address.v4Add.v[3], PORT);
                
    NET_PRES_SKT_ERROR_T *error = NULL;
    log_socket = NET_PRES_SocketOpen(0, NET_PRES_SKT_UNENCRYPTED_STREAM_CLIENT, IP_ADDRESS_TYPE_IPV4, PORT, (NET_PRES_ADDRESS *)&address, error);                    
    NET_PRES_SocketWasReset(log_socket);
            
    if (log_socket == INVALID_SOCKET) {
        SYS_CONSOLE_MESSAGE("Could not create socket - aborting\r\n");
        return false;
    }
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
SSL_SOCKET_STATES socketReady( void ) {
    if (NET_PRES_SocketReadIsReady(log_socket) == 0) {
        if (NET_PRES_SocketWasReset(log_socket) || NET_PRES_SocketWasDisconnected(log_socket)) {
            return SSL_SOCKET_WAS_DISCONNECTED;
        }
        return SSL_SOCKET_NOT_READY;
    }    
    return SSL_SOCKET_READY;
}



/*
 * Close an open socket
 */
void closeSocket ( void ) {
    NET_PRES_SocketClose(log_socket);
}



bool getUpdateSettingsFromServer ( void ) {
    char networkBuffer[512];

    char HARDWARE_ID[50];
    sprintf(HARDWARE_ID, "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X-W-1",
                    eui64[0], eui64[1], eui64[2], eui64[3], eui64[4], eui64[5], eui64[6], eui64[7]);
    
    
    
    sprintf(networkBuffer, "GET /api/v1/settings?hardware_id=%s&type=heatpump HTTP/1.1\r\n"
                    "Host: %s\r\n"
                    "Accept: application/json\r\n"
                    "Authorization: Bearer %s\r\n"
                    "Accept-Encoding: Identity\r\n"
                    "Connection: keep-alive\r\n"
                    "Content-Type: application/json\r\n"            
                    "Content-Length: 0\r\n\r\n",
                    HARDWARE_ID, HOST, TOKEN);     
 
    uint16_t bytesSend = NET_PRES_SocketWrite(log_socket, (uint8_t*) networkBuffer, strlen(networkBuffer));
    
    if (bytesSend <= 0) {
        SYS_CONSOLE_PRINT("***** COULD NOT COMPLETE REQUEST, CHECK IF NETWORK TX BUFFER SIZE IS BIG ENOUGH TO SEND THE REQUEST! SIZE NOW >> %d *****\r\n", strlen(networkBuffer));  
        return false;
    } else {
        SYS_CONSOLE_PRINT("*** REQUEST SEND WITH FOLLOWING SETTINGS: GET, /api/v1/settings?hardware_id=%s&type=heatpump, %s, SIZE %i\r\n", HARDWARE_ID, NTP_TIME_BUFFER, strlen(networkBuffer));
    }
    return true;    
}



const char* find_http_body(const char *response, size_t response_len, size_t *out_body_len) {
    const char *marker = "\r\n\r\n";
    const char *pos = strstr(response, marker);
    if (!pos) return NULL;

    const char *body = pos + 4;
    *out_body_len = response + response_len - body;
    return body;
}

char* dechunk_http_body(const char *chunked_body, size_t chunked_len, size_t *out_len) {
    const char *ptr = chunked_body;
    const char *end = chunked_body + chunked_len;
    char *output = malloc(chunked_len);
    if (!output) return NULL;
    size_t output_offset = 0;

    while (ptr < end) {
        // Read chunk size in hex
        char size_buf[16] = {0};
        int i = 0;
        while (ptr < end && *ptr != '\r' && i < 15) {
            size_buf[i++] = *ptr++;
        }
        size_buf[i] = '\0';

        if (*ptr == '\r') ptr++;
        if (*ptr == '\n') ptr++;

        size_t chunk_size = strtoul(size_buf, NULL, 16);
        if (chunk_size == 0) break;

        if (ptr + chunk_size > end) break;  // Malformed

        memcpy(output + output_offset, ptr, chunk_size);
        output_offset += chunk_size;
        ptr += chunk_size;

        if (*ptr == '\r') ptr++;
        if (*ptr == '\n') ptr++;
    }

    *out_len = output_offset;
    return output;
}

bool parse_update_settings_from_json(const char *json_str, size_t len) {
    cJSON *root = cJSON_ParseWithLength(json_str, len);
    if (!root) {
        SYS_CONSOLE_PRINT("Failed to parse JSON\n");
        return false;
    }

    cJSON *upd = cJSON_GetObjectItemCaseSensitive(root, "update_settings");
    if (cJSON_IsBool(upd)) {
        SYS_CONSOLE_PRINT("update_settings: %s\n", cJSON_IsTrue(upd) ? "true" : "false");
        bool updateSettings = cJSON_IsTrue(upd);
        cJSON_Delete(root);
        return updateSettings;
    } else {
        SYS_CONSOLE_PRINT("update_settings field missing\n");
        cJSON_Delete(root);
        return false;
    }

    cJSON_Delete(root);
    return false;
}


int bytesRead = 0;
size_t buffer_len;

void resetRxBuffer(void) { buffer_len = 0; readBuffer[0] = '\0'; }

bool areUpdateSettingsAvailable() {
    size_t body_len;
    
    SYS_CONSOLE_PRINT(
        "RAW HTTP BUFFER (%u bytes):\n%.*s\n",
        (unsigned)buffer_len,
        (int)buffer_len,
        readBuffer
    );
    
    const char *chunked = find_http_body(readBuffer, buffer_len, &body_len);
    if (!chunked) {
        SYS_CONSOLE_PRINT("HTTP body not found\n");
        return false;
    }

    size_t json_len;
    char *json_data = dechunk_http_body(chunked, body_len, &json_len);
    if (!json_data) {
        buffer_len = 0;
        free(json_data);
        SYS_CONSOLE_PRINT("Dechunking failed\n");
        return false;
    }
    
    buffer_len = 0;
    bool updateSettings = parse_update_settings_from_json(json_data, json_len);
    free(json_data);
    return updateSettings;
}




static bool http_response_complete(const char* buf, size_t len)
{
    if (len < 4) return false;

    // Ensure strstr can work safely (caller should keep buf[len] = '\0')
    const char* hdrEnd = strstr(buf, "\r\n\r\n");
    if (!hdrEnd) return false;

    size_t headerLen = (size_t)(hdrEnd + 4 - buf);

    // Chunked?
    if (strstr(buf, "Transfer-Encoding: chunked") || strstr(buf, "transfer-encoding: chunked"))
    {
        // Look for the terminating chunk: "\r\n0\r\n\r\n"
        // Search only after headers:
        const char* body = buf + headerLen;
        const char* end  = buf + len;

        for (const char* p = body; p + 5 < end; p++)
        {
            if (p[0] == '\r' && p[1] == '\n' && p[2] == '0' &&
                p[3] == '\r' && p[4] == '\n' && p[5] == '\r' && p[6] == '\n')
            {
                return true;
            }
        }
        return false;
    }

    // Content-Length?
    const char* cl = strstr(buf, "Content-Length:");
    if (!cl) return false;

    unsigned long contentLen = strtoul(cl + strlen("Content-Length:"), NULL, 10);
    return len >= (headerLen + (size_t)contentLen);
}



bool readServerResponseDone(void)
{
    // if disconnected/reset, stop
    if (NET_PRES_SocketWasReset(log_socket) || NET_PRES_SocketWasDisconnected(log_socket))
    {
        return true;
    }

    int ready = NET_PRES_SocketReadIsReady(log_socket);
    if (ready <= 0)
    {
        // not done, just no data yet
        return false;
    }

    // room left
    size_t room = READ_BUFFER_SIZE - buffer_len;
    if (room == 0)
    {
        SYS_CONSOLE_PRINT("readBuffer overflow risk (READ_BUFFER_SIZE=%u)\r\n", READ_BUFFER_SIZE);
        return true; // or false + force close; but do NOT keep appending
    }

    size_t toRead = READ_CHUNK_SIZE;
    if (toRead > (size_t)ready) toRead = (size_t)ready;
    if (toRead > room) toRead = room;

    bytesRead = NET_PRES_SocketRead(log_socket, (uint8_t*)readBuffer + buffer_len, toRead);
    if (bytesRead > 0)
    {
        buffer_len += (size_t)bytesRead;
        readBuffer[buffer_len] = '\0';   // keep it string-safe for strstr

        // Only return true when HTTP response is complete
        return http_response_complete(readBuffer, buffer_len);
    }

    return false;
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
         
        
        
        default: {
            ChangeHeatpumpSetting(address, value);
            break;
        }
    }
}



int setting_count = 0;
bool parse_modbus_settings() {
    
    //for(int i = 0; i < buffer_len; i++) {
    //    SYS_CONSOLE_PRINT("%c", readBuffer[i]);
    //}
    
    size_t body_len;
    const char *chunked = find_http_body(readBuffer, buffer_len, &body_len);
    if (!chunked) {
        SYS_CONSOLE_PRINT("HTTP body not found\n");
        return false;
    }

    size_t json_len;
    char *json_data = dechunk_http_body(chunked, body_len, &json_len);
    if (!json_data) {
        buffer_len = 0;
        free(json_data);
        SYS_CONSOLE_PRINT("Dechunking failed\n");
        return false;
    }    
    
    cJSON *root = cJSON_ParseWithLength(json_data, json_len);
    if (!root) {
        buffer_len = 0;
        free(json_data);
        SYS_CONSOLE_PRINT("Failed to parse JSON\n");
        return false;
    }

    cJSON *updates = cJSON_GetObjectItem(root, "settings_updates");
    if (!cJSON_IsArray(updates)) {
        SYS_CONSOLE_PRINT("settings_updates not found or not an array\n");
        free(json_data);
        cJSON_Delete(root);
        buffer_len = 0;
        return false;
    }

    if (cJSON_GetArraySize(updates) == 0) {
        SYS_CONSOLE_PRINT("No settings to update\n");
        buffer_len = 0;
        free(json_data);
        cJSON_Delete(root);
        return false;   // or false, depending on how you want to handle "no updates"
    }    
    
    cJSON *item = NULL;
    cJSON_ArrayForEach(item, updates) {
        if (setting_count >= MAX_SETTINGS) break;

        cJSON *addr = cJSON_GetObjectItem(item, "modbus_address");
        cJSON *val  = cJSON_GetObjectItem(item, "value");

        if (cJSON_IsString(addr) && cJSON_IsString(val)) {
            
            SYS_CONSOLE_PRINT("\nModbus Addy >> %i\n", (uint16_t)strtol(addr->valuestring, NULL, 0));
            SYS_CONSOLE_PRINT("value >> %i\n", (uint16_t)strtol(val->valuestring, NULL, 10));
            
            processModbusSettingsFromServer((uint16_t)strtol(addr->valuestring, NULL, 0), (uint16_t)strtol(val->valuestring, NULL, 10));

            setting_count++;
        }
    }
    
    buffer_len = 0;
    free(json_data);
    cJSON_Delete(root);
    return true;
}



void send_http_chunk(const char *data, size_t len) {
    char header[16];
    int n = sprintf(header, "%X\r\n", (unsigned int)len);

    NET_PRES_SocketWrite(log_socket, header, n);
    NET_PRES_SocketWrite(log_socket, data, len);
    NET_PRES_SocketWrite(log_socket, "\r\n", 2);
}



void getSettingValuesByModusIndex (int MODBUS_INDEX_START, int MODBUS_INDEX_END) {   
    
    char chunk[512];
    memset(chunk, 0, sizeof (chunk));
    size_t offset = 0;
   
    for(int modbus_index = MODBUS_INDEX_START; modbus_index < MODBUS_INDEX_END; modbus_index++) {
        int n = 0;
        n = snprintf(chunk + offset, sizeof(chunk) - offset, "%i,", getDataFromMemoryCallable(modbus_index, MASTER_HEATPUMP_IN_CASCADE));
        
        if (n < 0 || n >= (int)(sizeof(chunk) - offset)) {
            // Buffer vol stuur chunk
            send_http_chunk(chunk, offset);
            offset = 0;
            modbus_index--;
            continue;
        }
        offset += n;        
    }
    
    // Send remaining data if any
    if (offset > 0) {
        send_http_chunk(chunk, offset);
    }      
    return;
}



void getSettingValuesByEepromList8Bit(const uint32_t *addresses, size_t count)
{
    char chunk[512];
    memset(chunk, 0, sizeof(chunk));
    size_t offset = 0;

    for (size_t i = 0; i < count; i++) {
        int n = snprintf(chunk + offset, sizeof(chunk) - offset, "%u,", (unsigned)ReadSmartEeprom8(addresses[i]));

        if (n < 0 || n >= (int)(sizeof(chunk) - offset)) {
            // Buffer full: flush and retry this index
            send_http_chunk(chunk, offset);
            offset = 0;
            i--;
            continue;
        }
        offset += (size_t)n;
    }

    if (offset > 0) {
        send_http_chunk(chunk, offset);
    }
}

void getSettingValuesByEepromList16Bit(const uint32_t *addresses, size_t count)
{
    char chunk[512];
    memset(chunk, 0, sizeof(chunk));
    size_t offset = 0;

    for (size_t i = 0; i < count; i++) {
        int n = snprintf(chunk + offset, sizeof(chunk) - offset, "%u,", (unsigned)ReadSmartEeprom16(addresses[i]));

        if (n < 0 || n >= (int)(sizeof(chunk) - offset)) {
            // Buffer full: flush and retry this index
            send_http_chunk(chunk, offset);
            offset = 0;
            i--;
            continue;
        }
        offset += (size_t)n;
    }

    if (offset > 0) {
        send_http_chunk(chunk, offset);
    }
}




bool sendUpdatedSettingsList ( void ) {
    char networkBuffer[4096];
    char requestBody[512];
    char HARDWARE_ID[50];
    
    memset(networkBuffer, 0, sizeof (networkBuffer));
    memset(requestBody, 0, sizeof (requestBody));
    memset(HARDWARE_ID, 0, sizeof (HARDWARE_ID));
    
    sprintf(HARDWARE_ID, "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X-W-1",
                    eui64[0], eui64[1], eui64[2], eui64[3], eui64[4], eui64[5], eui64[6], eui64[7]);
    
    sprintf(networkBuffer, "POST /api/v1/settings?hardware_id=%s&type=heatpump HTTP/1.1\r\n"
                    "Host: %s\r\n"
                    "Accept: application/json\r\n"
                    "Authorization: Bearer %s\r\n"
                    "Accept-Encoding: Identity\r\n"
                    "Connection: close\r\n"
                    "Content-Type: text/plain\r\n"
                    "Cache-Control: no-cache\r\n"     
                    "Transfer-Encoding: chunked\r\n\r\n",
                    HARDWARE_ID, HOST, TOKEN);
    
    uint16_t bytesSend = NET_PRES_SocketWrite(log_socket, (uint8_t*) networkBuffer, strlen(networkBuffer));
    
    getSettingValuesByModusIndex(0x0100, 0x022C);  
    getSettingValuesByModusIndex(0x0300, 0x0319);
    getSettingValuesByModusIndex(0x0330, 0x0346);
    getSettingValuesByModusIndex(0x0360, 0x0363);
    getSettingValuesByModusIndex(0x0800, 0x0831);    
    getSettingValuesByModusIndex(0x1000, 0x1024);      
    
    if (getDataFromMemoryCallable(ADDRESS_FREQUENCY_CONVERSION_MODE, MASTER_HEATPUMP_IN_CASCADE) == 2) {
        WriteSmartEeprom8(SEEP_ADDR_SILENT_MODE, true);
        WriteSmartEeprom8(SEEP_ADDR_BOOST_MODE, false);
    } else if (getDataFromMemoryCallable(ADDRESS_FREQUENCY_CONVERSION_MODE, MASTER_HEATPUMP_IN_CASCADE) == 1) {
        WriteSmartEeprom8(SEEP_ADDR_SILENT_MODE, false);
        WriteSmartEeprom8(SEEP_ADDR_BOOST_MODE, true);
    } else {
        WriteSmartEeprom8(SEEP_ADDR_SILENT_MODE, false);
        WriteSmartEeprom8(SEEP_ADDR_BOOST_MODE, false);
    }    
    
    const uint32_t eep_addrs_8_bit_1[] = {
        SEEP_ADDR_EVU_CONTACT_ENABLE,       
        SEEP_ADDR_HEATPUMP_WAS_ON_BEFORE_FORCED_OFF,     
        SEEP_ADDR_SWITCH_HEATPUMP_ON_OFF_WITH_THERMOSTAT,
        SEEP_ADDR_SOFTWARE_RESET,                        
        SEEP_ADDR_SILENT_MODE,      
    };
    getSettingValuesByEepromList8Bit(eep_addrs_8_bit_1, sizeof(eep_addrs_8_bit_1)/sizeof(eep_addrs_8_bit_1[0]));  
   
    
    const uint32_t eep_addrs_16_bit_1[] = {    
        SEEP_ADDR_START_TIME_SILENT_MODE,                
        SEEP_ADDR_END_TIME_SILENT_MODE,   
    };
    getSettingValuesByEepromList16Bit(eep_addrs_16_bit_1, sizeof(eep_addrs_16_bit_1)/sizeof(eep_addrs_16_bit_1[0]));    
    
    
    const uint32_t eep_addrs_8_bit_2[] = {    
        SEEP_ADDR_BOOST_MODE,                            
        SEEP_ADDR_USE_SILENT_MODE_TIMERS,                
        SEEP_ADDR_BLOCK_HOTWATER,                                    
     };
    getSettingValuesByEepromList8Bit(eep_addrs_8_bit_2, sizeof(eep_addrs_8_bit_2)/sizeof(eep_addrs_8_bit_2[0]));  
    
    const uint32_t eep_addrs_16_bit_2[] = {
        SEEP_ADDR_START_TIME_BLOCK_HOTWATER,             
        SEEP_ADDR_END_TIME_BLOCK_HOTWATER,    
        SEEP_ADDR_HOT_WATER_MIN_TIME_IN_HOT_WATER_MODE,
        SEEP_ADDR_HOT_WATER_RUNNING_TIME_BEFORE_TURNING_ON_HEATING_ELEMENT,
        SEEP_ADDR_HOT_WATER_MAX_TIME_HEATING_ELEMENT_ON_IN_HEATING_MODE_SEC,
        SEEP_ADDR_PUMP_LAG_TIME_AFTER_THERMOSTAT_CONTACT_DISCONNECTED_SEC,
     };
    getSettingValuesByEepromList16Bit(eep_addrs_16_bit_2, sizeof(eep_addrs_16_bit_2)/sizeof(eep_addrs_16_bit_2[0]));    
    
    
    const uint32_t eep_addrs_8_bit_3[] = {
        SEEP_ADDR_DIGITAL_INPUT_TWO,
        SEEP_ADDR_DIGITAL_INPUT_ONE,
        SEEP_ADDR_COOLING_CONTACT_ENABLE,
        SEEP_ADDR_RELAIS_OUTPUT_TWO,
        SEEP_ADDR_RELAIS_OUTPUT_THREE,
     };
    getSettingValuesByEepromList8Bit(eep_addrs_8_bit_3, sizeof(eep_addrs_8_bit_3)/sizeof(eep_addrs_8_bit_3[0]));      
    
    
    const uint32_t eep_addrs_16_bit_3[] = {
        SEEP_ADDR_HEATING_TIME_CONSTANT_SEC,      
        SEEP_ADDR_HYBRID_SYSTEM_ENABLED_ON_HEATING_ELEMENT_RELAIS,
        SEEP_ADDR_HEATING_MODE_MAX_TIME_WITH_ELEMENT_ON,
        SEEP_ADDR_HEATING_MODE_MAX_TIME_WITH_CIRCULATION_PUMP_OFF,
        SEEP_ADDR_HEATING_MODE_MAX_TIME_WITH_ELEMENT_ON_AND_CIRCULATION_PUMP_OFF,
        SEEP_ADDR_HYBRID_SYSTEM_ENABLED,
     };
    
    getSettingValuesByEepromList16Bit(eep_addrs_16_bit_3, sizeof(eep_addrs_16_bit_3)/sizeof(eep_addrs_16_bit_3[0]));    
        
    
    
    // Laatste write zodat de server weet dat ontvangen klaar is
    NET_PRES_SocketWrite(log_socket, "0\r\n\r\n", 5);    
    
    if (bytesSend <= 0) {
        SYS_CONSOLE_PRINT("***** COULD NOT COMPLETE REQUEST, CHECK IF NETWORK TX BUFFER SIZE IS BIG ENOUGH TO SEND THE REQUEST! SIZE NOW >> %d *****\r\n", strlen(networkBuffer));  
        return false;
    } else {
        SYS_CONSOLE_PRINT("*** REQUEST SEND WITH FOLLOWING SETTINGS: POST, /api/v1/settings?hardware_id=%s&type=heatpump, SIZE %i\r\n", HARDWARE_ID, strlen(requestBody));
    }
    return true;    
}


bool readServerResponseUpdatedSettingsDone() {
    bytesRead = NET_PRES_SocketRead(log_socket, (uint8_t*)readBuffer + buffer_len, READ_CHUNK_SIZE);
    SYS_CONSOLE_PRINT("bytes read %i\n", bytesRead);
    
    for(int i = 0; i < buffer_len; i++) {
        SYS_CONSOLE_PRINT("%c", readBuffer[i]);
    }
    
    if (bytesRead > 0) {
        buffer_len += bytesRead;
        return false;
    }    
    SYS_CONSOLE_PRINT("Total bytes read %i\n", buffer_len);
    
    buffer_len = 0;
    return true;
}