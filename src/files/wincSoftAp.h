#ifndef _WINC_SOFT_AP_H
#define _WINC_SOFT_AP_H

#include <stdbool.h>
#include <stddef.h>
#include "wdrv_winc_client_api.h"

#if (defined __SAMD21J18A__)
#define WLAN_SSID               "MicrochipDemoAp"
#define WLAN_CHANNEL            1

#define WLAN_AUTH_OPEN
#define WLAN_WEB_KEY_INDEX      1
#define WLAN_WEB_KEY            "1234567890"

#define TCP_LISTEN_PORT         6666
#define TCP_BUFFER_SIZE         1460

#define WLAN_DHCP_SRV_ADDR      "192.168.1.1"
#define WLAN_DHCP_SRV_NETMASK   "255.255.255.0"

/* Keep this smaller on SAMD21 because RAM is limited. */
#define HTTP_RESPONSE_MAX       2048U
#endif

#if (defined __SAME54P20A__)
#define WLAN_SSID               "MicrochipDemoAp"
#define WLAN_CHANNEL            1

#define WLAN_AUTH_OPEN

#define TCP_LISTEN_PORT         6666
#define TCP_BUFFER_SIZE         1460

#define WLAN_DHCP_SRV_ADDR      "192.168.1.1"
#define WLAN_DHCP_SRV_NETMASK   "255.255.255.0"

/*
 * The expected /api/v2/sm/actual JSON body is about 1 KB. Two buffers of this
 * size are used: one for the complete HTTP response and one for the retained
 * response body. 2 KB leaves room for HTTP headers and modest response growth.
 */
#define HTTP_RESPONSE_MAX       2048U
#endif


/* Parsed values returned by /api/v2/sm/actual. */
typedef struct
{
    char timestamp[20];

    float energyDeliveredTariff1KWh;
    float energyDeliveredTariff2KWh;
    float energyReturnedTariff1KWh;
    float energyReturnedTariff2KWh;
    char electricityTariff[8];

    float powerDeliveredKW;
    float powerReturnedKW;

    float voltageL1V;
    float voltageL2V;
    float voltageL3V;

    float currentL1A;
    float currentL2A;
    float currentL3A;

    float powerDeliveredL1KW;
    float powerDeliveredL2KW;
    float powerDeliveredL3KW;

    float powerReturnedL1KW;
    float powerReturnedL2KW;
    float powerReturnedL3KW;

    /*
     * Gas values are optional. Some smart meters/endpoints do not include
     * these fields in every response. Check gasDataAvailable before use.
     */
    bool gasDataAvailable;
    float gasDeliveredM3;
    char gasDeliveredTimestamp[20];
} WINC_SM_ACTUAL_DATA;

/* Periodic HTTP client configuration. */
#define HTTP_SERVER_PORT             80U
#define HTTP_ENDPOINT                "/api/v2/sm/actual"
#define HTTP_POLL_INTERVAL_MS        10000U
#define HTTP_FIRST_REQUEST_DELAY_MS  1000U
#define HTTP_REQUEST_TIMEOUT_MS      5000U
#define HTTP_RESPONSE_PREVIEW_MAX    512U
#define HTTP_PRINT_RAW_PREVIEW        0

/*
 * Add complete HTTP header lines here when authentication is required.
 * The value must end in "\r\n". Example:
 * #define HTTP_EXTRA_HEADERS "Authorization: Bearer token\r\n"
 */
#define HTTP_EXTRA_HEADERS           ""

void APP_ExampleInitialize(DRV_HANDLE handle);
void APP_ExampleTasks(DRV_HANDLE handle);

bool getActivateWifiAp(void);
void setActivateWifiAp(bool value);

/* Access to the most recently received successful endpoint body. */
bool WINC_SoftAPActualDataAvailable(void);
const char *WINC_SoftAPGetActualData(void);
size_t WINC_SoftAPGetActualDataLength(void);
void WINC_SoftAPClearActualDataFlag(void);

/* Access to the most recently parsed smart-meter values. */
bool WINC_SoftAPParsedActualDataAvailable(void);
const WINC_SM_ACTUAL_DATA *WINC_SoftAPGetParsedActualData(void);
void WINC_SoftAPClearParsedActualDataFlag(void);

#endif /* _WINC_SOFT_AP_H */
