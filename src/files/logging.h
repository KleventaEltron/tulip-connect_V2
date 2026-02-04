

#ifndef _LOGGING_H   
#define _LOGGING_H

/* Provide C++ Compatibility */
#ifdef __cplusplus
extern "C" {
#endif

#define GET_PATH    "api/v1/heatpump/log"
#define POST_PATH   "api/v1/combined_log"
#define PORT 443         
        
/* TEST PARAMETERS */
//#define HOST        "test.tulip-wise.com"
//#define TOKEN       "6g9Xoo^8Es*V!f&N"        

/* UNCOMMENT VOOR LOGGING NAAR PRODUCTIE */
//#define HOST "admin.tulip-wise.com"
//#define TOKEN "!$nMM7QUsf9NX5kz"            

#define HardwareID                      "TL1"
#define KlantID                         "TL2"
#define SoftwareVersie                  "TL3"
#define BootloaderSoftwareVersie        "TL4"
#define RTCCDatumtijd                   "TL5"
#define NTPDatumtijd                    "TL6"
#define Lognummer                       "TL7"
#define ConnectTemp1                    "TL8"
#define ConnectTemp2                    "TL9"
#define ConnectTemp3                    "TL10"
#define ConnectTemp4                    "TL11"
#define ConnectInput1                   "TL12"
#define ConnectInput2                   "TL13"
#define ConnectInput3                   "TL14"
#define ConnectRelay1                   "TL15"
#define ConnectRelay2                   "TL16"
#define ConnectRelay3                   "TL17"
#define BufferElekElement               "TL18"
#define TapwaterElekElement             "TL19" 
#define Reserved230vRelay               "TL20"    
#define ThreeWayValveRelay              "TL21"
#define CirculatorPump                  "TL22"
#define SwitchInput                     "TL23"
#define VerbondenMetServer              "TL24"
#define DIPswitch1                      "TL25"
#define DIPswitch2                      "TL26"
#define DIPswitch3                      "TL27"
#define DIPswitch4                      "TL28"
#define ConfigModusActief               "TL29"
#define WifiStatus                      "TL30"
#define WifiInfo                        "TL31"
#define SDcardAanwezig                  "TL32"
#define SDstatus                        "TL33"
#define RS485commMetDisplayOK           "TL34"
#define RS485ommMetWarmtepompOK         "TL35"
#define EthernetStatus                  "TL36"
#define USBstatus                       "TL37"
#define PowerFailStatus                 "TL38"
#define SupercapacitorFaultStatus       "TL39"
#define Systemgoodindicator             "TL40"
#define SupercapacitorPowerGoodIndicator "TL41"
#define Alarmbytes                      "TL42"
#define emergencyModeHeating            "TL43"
#define emergencyModeHotwater           "TL44"
#define circPumpOffTempLow              "TL45"
#define circPumpOnTempLow               "TL46"
#define circPumpOffTempHigh             "TL47"
#define circPumpOnTempHigh              "TL48"
    
#define GMAC_NSR_MDIO_READY_Msk (1 << 0)    
#define GMAC_NSR_RX_IDLE_Msk (1 << 1)    
#define GMAC_NSR_LINK_STATUS_Msk (1 << 2)    
//#define GMAC_NSR_LINK_STATUS_Msk (1 << 2)    
    
typedef enum {
    SOCKET_NOT_CONNECTED,
    SSL_CREATE_CONNECTION_FAILED,
    SSL_CONNECTION_SUCCES,       
} SSL_CONNECTION_STATES;



typedef enum {
    SSL_BUSY_NEGOTIATING,
    SSL_SOCKET_NOT_SECURE,
    SSL_NEGOTIATION_SUCCES,       
} SSL_NEGOTIATION_STATES;



typedef enum {
    SSL_SOCKET_NOT_READY,
    SSL_SOCKET_WAS_DISCONNECTED,
    SSL_SOCKET_READY,       
} SSL_SOCKET_STATES;



typedef enum {
    TULIP_PRINT,
    HEATPUMP,
    BATTERY,
    SMART_METER,
    INVERTER,
    HEATPUMP_BOILER
} DEVICE_TYPE;



typedef enum {
    GET,
    POST,
    PUT,
    PATCH,
    DELETE,
} REQUEST_TYPE;

extern char NTP_TIME_BUFFER[40];

void postDevicesLogDataJson( void );
bool readNetworkBufferHeaderSslResponse( void );
bool readNetworkBufferBodySslResponse( void );
bool startSocketConnection( void );
SSL_NEGOTIATION_STATES waitForConnection ( void );
bool waitOnDNS( void );
SSL_NEGOTIATION_STATES waitForSslConnection ( void );
SSL_SOCKET_STATES socketReady( void );
void closeSocket( void );
bool loggingRequestBuilder ( REQUEST_TYPE rqt ); 
bool getCurrentUtcTimestamp ( void );
void getRealTimeDataStatussen ( char requestBuilder[] );
void setHardwareId( char device[], char* hardwareId, int modbusIndex );
bool setLoggingLock();
bool releaseLoggingLock();
void initializeMutex();
bool setLoggingLockNoPrint();
bool releaseLoggingLockNoPrint();
bool isEthernetCableConnected(TCPIP_NET_HANDLE netH);
bool setupNewTcpipStack();
void increaseTcpIpResetCounter();
uint8_t getTcpIpResetCounter();
char * getLoggingStateToString(APP_LOGGING_TASKS_STATES logState);
bool getUpdateSettingsFromServer ( void );
bool areUpdateSettingsAvailable();
bool readServerResponseDone();
bool parse_modbus_settings();
bool sendUpdatedSettingsList ( void );
bool readServerResponseUpdatedSettingsDone();
void setSettingChangedInDisplay(bool value);
bool getSettingChangedInDisplay();
void setNewLogRequired(bool value);
bool getNewLogRequired();
void resetRxBuffer(void);

//extern char NTP_TIME_BUFFER[40];

#endif