/*******************************************************************************
  WINC Example Application

  File Name:
    example.c

  Summary:
    Wi-Fi TCP Server with soft AP example.

  Description:
    This example demonstrates the use of the WINC1500 with the SAMD21 Xplained Pro
    board to start TCP server on WINC1500 running as soft AP

    The configuration defines for this demo are:
        WLAN_SSID           -- Soft AP SSID to create
        WLAN_CHANNEL        -- Channel on which to beacon
        WLAN_AUTH           -- Security for the BSS
        WLAN_WEB_KEY        -- WEP key
        WLAN_WEB_KEY_INDEX  -- WEP key index
        WLAN_DHCP_SRV_ADDR  -- IP address of DHCP server to create
        TCP_BUFFER_SIZE     -- Size of the socket buffer holding the receive data
*******************************************************************************/

/*******************************************************************************
* Copyright (C) 2020 Microchip Technology Inc. and its subsidiaries.
*
* Subject to your compliance with these terms, you may use Microchip software
* and any derivatives exclusively with Microchip products. It is your
* responsibility to comply with third party license terms applicable to your
* use of third party software (including open source software) that may
* accompany Microchip software.
*
* THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS". NO WARRANTIES, WHETHER
* EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE, INCLUDING ANY IMPLIED
* WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY, AND FITNESS FOR A
* PARTICULAR PURPOSE.
*
* IN NO EVENT WILL MICROCHIP BE LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE,
* INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND
* WHATSOEVER RELATED TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP HAS
* BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE FORESEEABLE. TO THE
* FULLEST EXTENT ALLOWED BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL CLAIMS IN
* ANY WAY RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT OF FEES, IF ANY,
* THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS SOFTWARE.
*******************************************************************************/


/** \mainpage
 * \section intro Introduction
 * This example demonstrates the use of the WINC1500 with the SAMD21 Xplained Pro board
 * to setup a TCP Server in softap mode.<br>
 * It uses the following hardware:
 * - the SAMD21 Xplained Pro.
 * - the WINC1500 on EXT1. / WINC3400 on EXT1
 *
 *
 * \section usage Usage
 * -# Assemble the devices and connect to USB debug cable from PC.
 * -# On the computer, open and configure a terminal application as the follows.
 * \code
 *    Baud Rate : 115200
 *    Data : 8bit
 *    Parity bit : none
 *    Stop bit : 1bit
 *    Flow control : none
 * \endcode
 *
 * -# 1. Power on the board, the board will enter softAP mode
 *  # 2. Connect your personal computer to the network named defined by macro "WLAN_SSID" in example_conf.h file
 * -# 3. For creating a TCP connection, you can use any open source programs (e.g. packet sender or Tera Term)

 * \code
 * ===========================================
 * WINC WiFi TCP Server Soft AP Example
 * ===========================================
 *
 * AP started, you can connect to WINC1500_SOFT_AP
 * On the connected device, start a TCP client connection to 192.168.1.1 on port 6666
 *
 * AP Mode: Station connected
 * AP Mode: Station IP address is 192.168.1.100
 * Bind on socket 0 successful, server_socket = 0
 * Listen on socket 0 successful
 * Connection from  192.168.1.100:55686
 * Receive on socket 1 successful
 * Client sent 13 bytes
 * Client sent Hello Server!
 * Sending a test message to client
 * Socket 1 send completed
 * TCP Server Test Complete!
 * Closing sockets
 *
 * \endcode
 *
 */

#include <xc.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>   
#include "../app_wifi_access_point_controller.h"
#include "wdrv_winc_client_api.h"
#include "wincSoftAp.h"
#include "i2c/mac.h"

#define CLOSE_CMD   "CLOSE"
#define SOFTAP_SSID_MAX_LEN   32
#define RX_ACCUM_MAX   1024
#define TX_MAX         512

extern APP_WIFI_ACCESS_POINT_CONTROLLER_DATA app_wifi_access_point_controllerData;

static char g_softApSsid[SOFTAP_SSID_MAX_LEN + 1];
static char hardwareId[24];
static char g_softApPsk[9];

volatile bool activateWifiAp = false;
static volatile bool g_closeRequested = false;


typedef struct
{
    SOCKET sock;
    char   rx[RX_ACCUM_MAX];
    size_t rxLen;
    bool   closeAfterSend;
} TcpSession;



static TcpSession g_sess = { .sock = -1, .rxLen = 0, .closeAfterSend = false };

static void tcpSendText(SOCKET s, const char *text)
{
    if (s < 0 || text == NULL) return;

    SYS_CONSOLE_Print(app_wifi_access_point_controllerData.consoleHandle,
                      "TX: %s", text);   // text already ends with \r\n typically

    send(s, (void*)text, (uint16_t)strlen(text), 0);
}

bool getActivateWifiAp() {
    return activateWifiAp;
}

void setActivateWifiAp(bool value) {
    activateWifiAp = value;
}

static void toUpperInPlace(char *s)
{
    for (; *s; s++)
    {
        if (*s >= 'a' && *s <= 'z')
            *s = (char)(*s - 'a' + 'A');
    }
}



static bool shouldCloseConnection(const uint8_t *buf, int len)
{
    const char cmd[] = CLOSE_CMD;
    int cmdLen = (int)(sizeof(cmd) - 1);

    if (len < cmdLen) return false;

    // Match at start; you can make this more flexible if needed
    return (0 == memcmp(buf, cmd, cmdLen));
}

typedef bool (*CmdHandler)(TcpSession *sess, const char *arg); 
// return true => handled, false => not this command

static bool cmd_ping(TcpSession *sess, const char *arg)
{
    (void)arg;
    tcpSendText(sess->sock, "OK PONG\r\n");
    return true;
}

static bool cmd_get_id(TcpSession *sess, const char *arg)
{
    (void)arg;
    // You already have hardwareId as "AA:BB:..."
    char tmp[64];
    snprintf(tmp, sizeof(tmp), "OK ID=%s\r\n", hardwareId);
    tcpSendText(sess->sock, tmp);
    return true;
}

static bool cmd_close(TcpSession *sess, const char *arg)
{
    (void)arg;
    tcpSendText(sess->sock, "OK BYE\r\n");
    sess->closeAfterSend = true;   // close only after the reply is sent
    return true;
}

typedef struct
{
    const char *name;     // command keyword
    CmdHandler  fn;
} CmdEntry;

static const CmdEntry g_cmds[] =
{
    { "PING",   cmd_ping   },
    { "GETID",  cmd_get_id },
    { "CLOSE",  cmd_close  },
};

static void handleLine(TcpSession *sess, const char *line)
{
    // Trim leading spaces
    while (*line == ' ' || *line == '\t') line++;

    // Split "CMD rest-of-line"
    const char *sp = line;
    while (*sp && *sp != ' ' && *sp != '\t' && *sp != '\r' && *sp != '\n') sp++;

    size_t cmdLen = (size_t)(sp - line);

    // arg points after spaces
    while (*sp == ' ' || *sp == '\t') sp++;
    const char *arg = sp;

    // ---- Copy command into writable buffer ----
    char cmdBuf[16];
    if (cmdLen >= sizeof(cmdBuf))
        cmdLen = sizeof(cmdBuf) - 1;

    memcpy(cmdBuf, line, cmdLen);
    cmdBuf[cmdLen] = '\0';

    // ---- Normalize to upper case ----
    toUpperInPlace(cmdBuf);

    // ---- Match against table ----
    for (size_t i = 0; i < (sizeof(g_cmds)/sizeof(g_cmds[0])); i++)
    {
        if (0 == strcmp(cmdBuf, g_cmds[i].name))
        {
            g_cmds[i].fn(sess, arg);
            return;
        }
    }

    tcpSendText(sess->sock, "ERR UNKNOWN_CMD\r\n");
}



static void processIncoming(TcpSession *sess, const uint8_t *data, size_t len)
{
    // Append with overflow protection
    size_t space = RX_ACCUM_MAX - 1 - sess->rxLen;
    if (len > space) len = space;

    memcpy(&sess->rx[sess->rxLen], data, len);
    sess->rxLen += len;
    sess->rx[sess->rxLen] = '\0';

    // DEBUG: show raw received bytes length
    SYS_CONSOLE_Print(app_wifi_access_point_controllerData.consoleHandle,
                      "RX %u bytes\r\n", (unsigned)len);

    // Treat either '\n' or '\r' as end-of-command
    char *start = sess->rx;

    for (;;)
    {
        // Find first line ending char (\r or \n)
        char *eol = strpbrk(start, "\r\n");
        if (!eol) break;

        *eol = '\0';                 // terminate line

        if (*start != '\0')          // ignore empty lines
        {
            SYS_CONSOLE_Print(app_wifi_access_point_controllerData.consoleHandle,
                              "CMD: '%s'\r\n", start);
            handleLine(sess, start);
        }

        // Skip all consecutive \r\n
        char *next = eol + 1;
        while (*next == '\r' || *next == '\n') next++;

        start = next;
    }

    // Move leftover partial command to front
    size_t remaining = &sess->rx[sess->rxLen] - start;
    memmove(sess->rx, start, remaining);
    sess->rxLen = remaining;
    sess->rx[sess->rxLen] = '\0';

    // OPTIONAL: If you want to also accept commands with NO newline at all,
    // you can add a "timeout" approach. Without a timer, easiest is:
    // if buffer exactly matches a command and has no whitespace, dispatch immediately.
    // (This is safe for simple one-word commands.)
    if (sess->rxLen > 0 && sess->rxLen < 16)   // small command
    {
        char tmp[16];
        size_t n = sess->rxLen;
        if (n >= sizeof(tmp)) n = sizeof(tmp) - 1;

        memcpy(tmp, sess->rx, n);
        tmp[n] = '\0';

        toUpperInPlace(tmp);

        if (0 == strcmp(tmp, "PING") ||
            0 == strcmp(tmp, "GETID") ||
            0 == strcmp(tmp, "CLOSE"))
        {
            // Now this works for "ping", "Ping", etc.
            handleLine(sess, sess->rx);

            sess->rxLen = 0;
            sess->rx[0] = '\0';
        }
    }

}





typedef enum
{
    /* Example's state machine's initial state. */
    EXAMP_STATE_INIT=0,
    EXAMP_STATE_WAIT_FOR_STATION,
    EXAMP_STATE_START_TCP_SERVER,
    EXAMP_STATE_SOCKET_LISTENING,
    EXAMP_STATE_DONE,
    EXAMP_STATE_ERROR,
} EXAMP_STATES;

/** Message format definitions. */
typedef struct s_msg_wifi_product
{
    uint8_t name[9];
} t_msg_wifi_product;

/** Message format declarations. */
static t_msg_wifi_product msg_wifi_product =
{
    .name = "WINC_H3",
};

static EXAMP_STATES state;
static SOCKET serverSocket = -1;
static SOCKET tcp_client_socket = -1;
static uint8_t recvBuffer[TCP_BUFFER_SIZE];
static WDRV_WINC_BSS_CONTEXT  bssCtx;
static WDRV_WINC_AUTH_CONTEXT authCtx;

typedef enum
{
    HTTP_CLIENT_IDLE = 0,
    HTTP_CLIENT_CONNECTING,
    HTTP_CLIENT_RECEIVING
} HTTP_CLIENT_STATE;

static HTTP_CLIENT_STATE httpClientState = HTTP_CLIENT_IDLE;
static SOCKET httpSocket = -1;
static uint32_t connectedStationIp = 0;
static bool connectedStationIpValid = false;
static char connectedStationIpText[20];
static uint32_t nextHttpPollTime = 0;
static uint32_t httpRequestDeadline = 0;
static char httpTxBuffer[320];
static uint8_t httpRxChunk[TCP_BUFFER_SIZE];
static char httpResponse[HTTP_RESPONSE_MAX + 1U];
static size_t httpResponseLength = 0;
static char actualData[HTTP_RESPONSE_MAX + 1U];
static size_t actualDataLength = 0;
static volatile bool actualDataAvailable = false;
static WINC_SM_ACTUAL_DATA parsedActualData;
static volatile bool parsedActualDataAvailable = false;



static bool timeReached(uint32_t now, uint32_t target)
{
    return ((int32_t)(now - target) >= 0);
}

static void httpScheduleNextRequest(uint32_t delayMs)
{
    nextHttpPollTime = SYS_TIME_CounterGet() + SYS_TIME_MSToCount(delayMs);
}

static void httpCloseSocket(void)
{
    SOCKET socketToClose = httpSocket;

    httpSocket = -1;
    httpClientState = HTTP_CLIENT_IDLE;

    if (socketToClose >= 0)
    {
        shutdown(socketToClose);
    }
}

static void httpFinishRequest(void)
{
    httpCloseSocket();
    httpScheduleNextRequest(HTTP_POLL_INTERVAL_MS);
}

static void httpFailRequest(const char *reason)
{
    if (reason != NULL)
    {
        SYS_CONSOLE_Print(app_wifi_access_point_controllerData.consoleHandle,
                          "HTTP request failed: %s\r\n", reason);
    }

    httpFinishRequest();
}

static int asciiToLower(int c)
{
    if ((c >= 'A') && (c <= 'Z'))
    {
        return c - 'A' + 'a';
    }

    return c;
}

static bool textEqualsIgnoreCase(const char *left, const char *right, size_t length)
{
    size_t i;

    for (i = 0; i < length; i++)
    {
        if (asciiToLower((unsigned char)left[i]) !=
            asciiToLower((unsigned char)right[i]))
        {
            return false;
        }
    }

    return true;
}

static bool textContainsIgnoreCase(const char *text, size_t textLength,
                                   const char *needle)
{
    size_t needleLength = strlen(needle);
    size_t i;

    if ((needleLength == 0U) || (needleLength > textLength))
    {
        return false;
    }

    for (i = 0; i <= (textLength - needleLength); i++)
    {
        if (textEqualsIgnoreCase(&text[i], needle, needleLength))
        {
            return true;
        }
    }

    return false;
}

static bool httpGetHeaderValue(const char *headers, size_t headersLength,
                               const char *headerName,
                               const char **value, size_t *valueLength)
{
    const char *cursor = headers;
    const char *headersEnd = headers + headersLength;
    size_t headerNameLength = strlen(headerName);

    /* Skip the HTTP status line. */
    while ((cursor < headersEnd) && (*cursor != '\n'))
    {
        cursor++;
    }

    if (cursor < headersEnd)
    {
        cursor++;
    }

    while (cursor < headersEnd)
    {
        const char *lineStart = cursor;
        const char *lineEnd;
        const char *colon;
        const char *headerValue;
        const char *headerValueEnd;

        while ((cursor < headersEnd) && (*cursor != '\n'))
        {
            cursor++;
        }

        lineEnd = cursor;
        if ((lineEnd > lineStart) && (*(lineEnd - 1) == '\r'))
        {
            lineEnd--;
        }

        if (cursor < headersEnd)
        {
            cursor++;
        }

        if (lineEnd == lineStart)
        {
            break;
        }

        colon = lineStart;
        while ((colon < lineEnd) && (*colon != ':'))
        {
            colon++;
        }

        if ((colon < lineEnd) &&
            ((size_t)(colon - lineStart) == headerNameLength) &&
            textEqualsIgnoreCase(lineStart, headerName, headerNameLength))
        {
            headerValue = colon + 1;
            while ((headerValue < lineEnd) &&
                   ((*headerValue == ' ') || (*headerValue == '\t')))
            {
                headerValue++;
            }

            headerValueEnd = lineEnd;
            while ((headerValueEnd > headerValue) &&
                   ((*(headerValueEnd - 1) == ' ') ||
                    (*(headerValueEnd - 1) == '\t')))
            {
                headerValueEnd--;
            }

            *value = headerValue;
            *valueLength = (size_t)(headerValueEnd - headerValue);
            return true;
        }
    }

    return false;
}

static bool parseDecimalSize(const char *text, size_t textLength, size_t *value)
{
    size_t result = 0;
    size_t i;

    if (textLength == 0U)
    {
        return false;
    }

    for (i = 0; i < textLength; i++)
    {
        unsigned char c = (unsigned char)text[i];

        if ((c < '0') || (c > '9'))
        {
            return false;
        }

        if (result > ((SIZE_MAX - (size_t)(c - '0')) / 10U))
        {
            return false;
        }

        result = (result * 10U) + (size_t)(c - '0');
    }

    *value = result;
    return true;
}

static bool parseHexSize(const char *text, size_t textLength, size_t *value)
{
    size_t result = 0;
    size_t i;
    bool digitFound = false;

    for (i = 0; i < textLength; i++)
    {
        unsigned char c = (unsigned char)text[i];
        unsigned int digit;

        if ((c == ';') || (c == ' ') || (c == '\t'))
        {
            break;
        }

        if ((c >= '0') && (c <= '9'))
        {
            digit = (unsigned int)(c - '0');
        }
        else if ((c >= 'a') && (c <= 'f'))
        {
            digit = (unsigned int)(c - 'a') + 10U;
        }
        else if ((c >= 'A') && (c <= 'F'))
        {
            digit = (unsigned int)(c - 'A') + 10U;
        }
        else
        {
            return false;
        }

        digitFound = true;

        if (result > ((SIZE_MAX - digit) / 16U))
        {
            return false;
        }

        result = (result * 16U) + digit;
    }

    if (!digitFound)
    {
        return false;
    }

    *value = result;
    return true;
}

static bool httpDecodeChunkedBody(const char *encoded, size_t encodedLength,
                                  char *decoded, size_t decodedCapacity,
                                  size_t *decodedLength)
{
    size_t inputOffset = 0;
    size_t outputOffset = 0;

    while (inputOffset < encodedLength)
    {
        size_t lineStart = inputOffset;
        size_t lineEnd;
        size_t chunkSize;

        while (((inputOffset + 1U) < encodedLength) &&
               !((encoded[inputOffset] == '\r') &&
                 (encoded[inputOffset + 1U] == '\n')))
        {
            inputOffset++;
        }

        if ((inputOffset + 1U) >= encodedLength)
        {
            return false;
        }

        lineEnd = inputOffset;
        if (!parseHexSize(&encoded[lineStart], lineEnd - lineStart, &chunkSize))
        {
            return false;
        }

        inputOffset += 2U;

        if (chunkSize == 0U)
        {
            decoded[outputOffset] = '\0';
            *decodedLength = outputOffset;
            return true;
        }

        if ((chunkSize > (encodedLength - inputOffset)) ||
            (chunkSize > (decodedCapacity - outputOffset)))
        {
            return false;
        }

        memcpy(&decoded[outputOffset], &encoded[inputOffset], chunkSize);
        outputOffset += chunkSize;
        inputOffset += chunkSize;

        if (((inputOffset + 1U) >= encodedLength) ||
            (encoded[inputOffset] != '\r') ||
            (encoded[inputOffset + 1U] != '\n'))
        {
            return false;
        }

        inputOffset += 2U;
    }

    return false;
}

static const char *jsonFindObjectForKey(const char *json, const char *key,
                                            const char **objectEnd)
{
    const char *match = json;
    size_t keyLength;

    if ((json == NULL) || (key == NULL) || (objectEnd == NULL))
    {
        return NULL;
    }

    keyLength = strlen(key);

    while ((match = strstr(match, key)) != NULL)
    {
        const char *cursor;
        const char *end;

        /* Require an exact JSON member name: "key". */
        if ((match > json) && (match[-1] == '"') &&
            (match[keyLength] == '"'))
        {
            cursor = match + keyLength + 1U;

            while ((*cursor == ' ') || (*cursor == '\t') ||
                   (*cursor == '\r') || (*cursor == '\n'))
            {
                cursor++;
            }

            if (*cursor != ':')
            {
                match += keyLength;
                continue;
            }

            cursor++;
            while ((*cursor == ' ') || (*cursor == '\t') ||
                   (*cursor == '\r') || (*cursor == '\n'))
            {
                cursor++;
            }

            if (*cursor != '{')
            {
                match += keyLength;
                continue;
            }

            end = strchr(cursor + 1, '}');
            if (end == NULL)
            {
                return NULL;
            }

            *objectEnd = end;
            return cursor;
        }

        match += keyLength;
    }

    return NULL;
}

static const char *jsonFindValueInObject(const char *objectStart,
                                         const char *objectEnd)
{
    const char *valueKey;
    const char *cursor;

    if ((objectStart == NULL) || (objectEnd == NULL) ||
        (objectStart >= objectEnd))
    {
        return NULL;
    }

    valueKey = strstr(objectStart, "\"value\"");
    if ((valueKey == NULL) || (valueKey >= objectEnd))
    {
        return NULL;
    }

    cursor = valueKey + sizeof("\"value\"") - 1U;

    while ((cursor < objectEnd) &&
           ((*cursor == ' ') || (*cursor == '\t') ||
            (*cursor == '\r') || (*cursor == '\n')))
    {
        cursor++;
    }

    if ((cursor >= objectEnd) || (*cursor != ':'))
    {
        return NULL;
    }

    cursor++;
    while ((cursor < objectEnd) &&
           ((*cursor == ' ') || (*cursor == '\t') ||
            (*cursor == '\r') || (*cursor == '\n')))
    {
        cursor++;
    }

    return (cursor < objectEnd) ? cursor : NULL;
}

static bool jsonGetFloatValue(const char *json, const char *key, float *value)
{
    const char *objectEnd;
    const char *objectStart;
    const char *valueStart;
    char *numberEnd;
    float parsedValue;

    if (value == NULL)
    {
        return false;
    }

    objectStart = jsonFindObjectForKey(json, key, &objectEnd);
    valueStart = jsonFindValueInObject(objectStart, objectEnd);

    if ((valueStart == NULL) || (*valueStart == '"'))
    {
        return false;
    }

    parsedValue = strtof(valueStart, &numberEnd);
    if ((numberEnd == valueStart) || (numberEnd > objectEnd))
    {
        return false;
    }

    *value = parsedValue;
    return true;
}

static bool jsonGetStringValue(const char *json, const char *key,
                               char *output, size_t outputSize)
{
    const char *objectEnd;
    const char *objectStart;
    const char *valueStart;
    const char *valueEnd;
    size_t valueLength;

    if ((output == NULL) || (outputSize == 0U))
    {
        return false;
    }

    objectStart = jsonFindObjectForKey(json, key, &objectEnd);
    valueStart = jsonFindValueInObject(objectStart, objectEnd);

    if ((valueStart == NULL) || (*valueStart != '"'))
    {
        return false;
    }

    valueStart++;
    valueEnd = valueStart;

    while ((valueEnd < objectEnd) && (*valueEnd != '"'))
    {
        /* These endpoint strings do not contain JSON escape sequences. */
        if (*valueEnd == '\\')
        {
            return false;
        }
        valueEnd++;
    }

    if ((valueEnd >= objectEnd) || (*valueEnd != '"'))
    {
        return false;
    }

    valueLength = (size_t)(valueEnd - valueStart);
    if (valueLength >= outputSize)
    {
        return false;
    }

    memcpy(output, valueStart, valueLength);
    output[valueLength] = '\0';
    return true;
}

static void printParsedActualData(const WINC_SM_ACTUAL_DATA *data)
{
    if (data == NULL)
    {
        return;
    }

    SYS_CONSOLE_PRINT("\r\n--- Parsed smart-meter actual data ---\r\n");
    SYS_CONSOLE_PRINT("timestamp: %s\r\n", data->timestamp);
    SYS_CONSOLE_PRINT("energy_delivered_tariff1: %.3f kWh\r\n",
                      (double)data->energyDeliveredTariff1KWh);
    SYS_CONSOLE_PRINT("energy_delivered_tariff2: %.3f kWh\r\n",
                      (double)data->energyDeliveredTariff2KWh);
    SYS_CONSOLE_PRINT("energy_returned_tariff1: %.3f kWh\r\n",
                      (double)data->energyReturnedTariff1KWh);
    SYS_CONSOLE_PRINT("energy_returned_tariff2: %.3f kWh\r\n",
                      (double)data->energyReturnedTariff2KWh);
    SYS_CONSOLE_PRINT("electricity_tariff: %s\r\n", data->electricityTariff);
    SYS_CONSOLE_PRINT("power_delivered: %.3f kW\r\n",
                      (double)data->powerDeliveredKW);
    SYS_CONSOLE_PRINT("power_returned: %.3f kW\r\n",
                      (double)data->powerReturnedKW);
    SYS_CONSOLE_PRINT("voltage_l1: %.1f V\r\n", (double)data->voltageL1V);
    SYS_CONSOLE_PRINT("voltage_l2: %.1f V\r\n", (double)data->voltageL2V);
    SYS_CONSOLE_PRINT("voltage_l3: %.1f V\r\n", (double)data->voltageL3V);
    SYS_CONSOLE_PRINT("current_l1: %.3f A\r\n", (double)data->currentL1A);
    SYS_CONSOLE_PRINT("current_l2: %.3f A\r\n", (double)data->currentL2A);
    SYS_CONSOLE_PRINT("current_l3: %.3f A\r\n", (double)data->currentL3A);
    SYS_CONSOLE_PRINT("power_delivered_l1: %.3f kW\r\n",
                      (double)data->powerDeliveredL1KW);
    SYS_CONSOLE_PRINT("power_delivered_l2: %.3f kW\r\n",
                      (double)data->powerDeliveredL2KW);
    SYS_CONSOLE_PRINT("power_delivered_l3: %.3f kW\r\n",
                      (double)data->powerDeliveredL3KW);
    SYS_CONSOLE_PRINT("power_returned_l1: %.3f kW\r\n",
                      (double)data->powerReturnedL1KW);
    SYS_CONSOLE_PRINT("power_returned_l2: %.3f kW\r\n",
                      (double)data->powerReturnedL2KW);
    SYS_CONSOLE_PRINT("power_returned_l3: %.3f kW\r\n",
                      (double)data->powerReturnedL3KW);
    if (data->gasDataAvailable)
    {
        SYS_CONSOLE_PRINT("gas_delivered: %.3f m3\r\n",
                          (double)data->gasDeliveredM3);
        SYS_CONSOLE_PRINT("gas_delivered_timestamp: %s\r\n",
                          data->gasDeliveredTimestamp);
    }
    else
    {
        SYS_CONSOLE_PRINT("gas data: not included in this response\r\n");
    }

    SYS_CONSOLE_PRINT("--------------------------------------\r\n\r\n");
}

static bool parseActualDataJson(const char *json)
{
    WINC_SM_ACTUAL_DATA parsed;
    bool valid = true;

    memset(&parsed, 0, sizeof(parsed));

#define PARSE_STRING(jsonKey, member)                                      \
    do                                                                     \
    {                                                                      \
        if (!jsonGetStringValue(json, jsonKey,                             \
                                parsed.member, sizeof(parsed.member)))     \
        {                                                                  \
            SYS_CONSOLE_PRINT("JSON field missing/invalid: %s\r\n",      \
                              jsonKey);                                    \
            valid = false;                                                 \
        }                                                                  \
    } while (0)

#define PARSE_FLOAT(jsonKey, member)                                       \
    do                                                                     \
    {                                                                      \
        if (!jsonGetFloatValue(json, jsonKey, &parsed.member))             \
        {                                                                  \
            SYS_CONSOLE_PRINT("JSON field missing/invalid: %s\r\n",      \
                              jsonKey);                                    \
            valid = false;                                                 \
        }                                                                  \
    } while (0)

    PARSE_STRING("timestamp", timestamp);
    PARSE_FLOAT("energy_delivered_tariff1", energyDeliveredTariff1KWh);
    PARSE_FLOAT("energy_delivered_tariff2", energyDeliveredTariff2KWh);
    PARSE_FLOAT("energy_returned_tariff1", energyReturnedTariff1KWh);
    PARSE_FLOAT("energy_returned_tariff2", energyReturnedTariff2KWh);
    PARSE_STRING("electricity_tariff", electricityTariff);
    PARSE_FLOAT("power_delivered", powerDeliveredKW);
    PARSE_FLOAT("power_returned", powerReturnedKW);
    PARSE_FLOAT("voltage_l1", voltageL1V);
    PARSE_FLOAT("voltage_l2", voltageL2V);
    PARSE_FLOAT("voltage_l3", voltageL3V);
    PARSE_FLOAT("current_l1", currentL1A);
    PARSE_FLOAT("current_l2", currentL2A);
    PARSE_FLOAT("current_l3", currentL3A);
    PARSE_FLOAT("power_delivered_l1", powerDeliveredL1KW);
    PARSE_FLOAT("power_delivered_l2", powerDeliveredL2KW);
    PARSE_FLOAT("power_delivered_l3", powerDeliveredL3KW);
    PARSE_FLOAT("power_returned_l1", powerReturnedL1KW);
    PARSE_FLOAT("power_returned_l2", powerReturnedL2KW);
    PARSE_FLOAT("power_returned_l3", powerReturnedL3KW);

    /*
     * Gas data is optional. Only mark it available when both the value and
     * its timestamp are present and valid. Missing gas fields must not make
     * an otherwise valid electrical-data response fail.
     */
    parsed.gasDataAvailable =
        jsonGetFloatValue(json, "gas_delivered",
                          &parsed.gasDeliveredM3) &&
        jsonGetStringValue(json, "gas_delivered_timestamp",
                           parsed.gasDeliveredTimestamp,
                           sizeof(parsed.gasDeliveredTimestamp));

    if (!parsed.gasDataAvailable)
    {
        parsed.gasDeliveredM3 = 0.0f;
        parsed.gasDeliveredTimestamp[0] = '\0';
    }

#undef PARSE_FLOAT
#undef PARSE_STRING

    if (!valid)
    {
        parsedActualDataAvailable = false;
        return false;
    }

    parsedActualData = parsed;
    parsedActualDataAvailable = true;
    printParsedActualData(&parsedActualData);
    return true;
}

static void processHttpResponse(void)
{
    char *headerEnd;
    const char *body;
    size_t headersLength;
    size_t bodyLength;
    size_t payloadLength;
    int statusCode = 0;
    const char *headerValue;
    size_t headerValueLength;
    bool isChunked = false;

    httpResponse[httpResponseLength] = '\0';

    if (httpResponseLength == 0U)
    {
        SYS_CONSOLE_PRINT("HTTP response was empty\r\n");
        return;
    }

    if (sscanf(httpResponse, "HTTP/%*u.%*u %d", &statusCode) != 1)
    {
        SYS_CONSOLE_PRINT("Invalid HTTP status line\r\n");
        return;
    }

    headerEnd = strstr(httpResponse, "\r\n\r\n");
    if (headerEnd == NULL)
    {
        SYS_CONSOLE_PRINT("HTTP header terminator not found\r\n");
        return;
    }

    headersLength = (size_t)(headerEnd - httpResponse);
    body = headerEnd + 4;
    bodyLength = httpResponseLength - (size_t)(body - httpResponse);

    SYS_CONSOLE_Print(app_wifi_access_point_controllerData.consoleHandle,
                      "HTTP status %d, received %u bytes\r\n",
                      statusCode, (unsigned int)httpResponseLength);

    if (statusCode != 200)
    {
        SYS_CONSOLE_Print(app_wifi_access_point_controllerData.consoleHandle,
                          "Endpoint returned HTTP status %d\r\n", statusCode);
        return;
    }

    if (httpGetHeaderValue(httpResponse, headersLength,
                           "Transfer-Encoding", &headerValue,
                           &headerValueLength))
    {
        isChunked = textContainsIgnoreCase(headerValue, headerValueLength,
                                           "chunked");
    }

    if (isChunked)
    {
        if (!httpDecodeChunkedBody(body, bodyLength, actualData,
                                   HTTP_RESPONSE_MAX, &payloadLength))
        {
            SYS_CONSOLE_PRINT("Could not decode chunked HTTP body\r\n");
            return;
        }
    }
    else
    {
        payloadLength = bodyLength;

        if (httpGetHeaderValue(httpResponse, headersLength,
                               "Content-Length", &headerValue,
                               &headerValueLength))
        {
            size_t contentLength;

            if (!parseDecimalSize(headerValue, headerValueLength,
                                  &contentLength))
            {
                SYS_CONSOLE_PRINT("Invalid Content-Length header\r\n");
                return;
            }

            if (contentLength > bodyLength)
            {
                SYS_CONSOLE_PRINT("HTTP body ended before Content-Length\r\n");
                return;
            }

            payloadLength = contentLength;
        }

        if (payloadLength > HTTP_RESPONSE_MAX)
        {
            SYS_CONSOLE_PRINT("HTTP body is larger than HTTP_RESPONSE_MAX\r\n");
            return;
        }

        memcpy(actualData, body, payloadLength);
        actualData[payloadLength] = '\0';
    }

    actualDataLength = payloadLength;
    actualDataAvailable = true;

    SYS_CONSOLE_Print(app_wifi_access_point_controllerData.consoleHandle,
                      "Actual data updated: %u bytes\r\n",
                      (unsigned int)actualDataLength);

#if HTTP_PRINT_RAW_PREVIEW
    if (actualDataLength > 0U)
    {
        size_t previewLength = actualDataLength;

        if (previewLength > HTTP_RESPONSE_PREVIEW_MAX)
        {
            previewLength = HTTP_RESPONSE_PREVIEW_MAX;
        }

        SYS_CONSOLE_Print(app_wifi_access_point_controllerData.consoleHandle,
                          "Actual data preview:\r\n%.*s\r\n",
                          (int)previewLength, actualData);
    }
#endif

    if (!parseActualDataJson(actualData))
    {
        SYS_CONSOLE_PRINT("Actual-data JSON parsing failed\r\n");
    }
}

static void httpStartActualRequest(void)
{
    struct sockaddr_in serverAddress;
    int requestLength;

    if (!connectedStationIpValid ||
        (httpClientState != HTTP_CLIENT_IDLE))
    {
        return;
    }

    httpResponseLength = 0U;
    httpResponse[0] = '\0';

    httpSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (httpSocket < 0)
    {
        httpFailRequest("socket creation failed");
        return;
    }

    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = _htons((uint16_t)HTTP_SERVER_PORT);
    serverAddress.sin_addr.s_addr = connectedStationIp;

    requestLength = snprintf(httpTxBuffer, sizeof(httpTxBuffer),
                             "GET " HTTP_ENDPOINT " HTTP/1.1\r\n"
                             "Host: %s\r\n"
                             "Accept: application/json\r\n"
                             "Connection: close\r\n"
                             HTTP_EXTRA_HEADERS
                             "\r\n",
                             connectedStationIpText);

    if ((requestLength <= 0) ||
        ((size_t)requestLength >= sizeof(httpTxBuffer)))
    {
        httpFailRequest("request buffer too small");
        return;
    }

    SYS_CONSOLE_Print(app_wifi_access_point_controllerData.consoleHandle,
                      "HTTP GET http://%s:%u%s\r\n",
                      connectedStationIpText,
                      (unsigned int)HTTP_SERVER_PORT,
                      HTTP_ENDPOINT);

    httpClientState = HTTP_CLIENT_CONNECTING;
    httpRequestDeadline = SYS_TIME_CounterGet() +
                          SYS_TIME_MSToCount(HTTP_REQUEST_TIMEOUT_MS);

    if (connect(httpSocket, (struct sockaddr *)&serverAddress,
                sizeof(serverAddress)) < 0)
    {
        httpFailRequest("connect call failed");
    }
}

static void handleHttpSocketEvent(SOCKET socket, uint8_t messageType,
                                  void *pMessage)
{
    switch (messageType)
    {
        case SOCKET_MSG_CONNECT:
        {
            tstrSocketConnectMsg *connectMessage =
                (tstrSocketConnectMsg *)pMessage;

            if ((connectMessage == NULL) ||
                (connectMessage->s8Error != 0))
            {
                httpFailRequest("TCP connection failed");
                return;
            }

            SYS_CONSOLE_PRINT("HTTP TCP connection established\r\n");

            if (send(socket, (void *)httpTxBuffer,
                     (uint16_t)strlen(httpTxBuffer), 0) < 0)
            {
                httpFailRequest("send call failed");
                return;
            }

            memset(httpRxChunk, 0, sizeof(httpRxChunk));
            if (recv(socket, httpRxChunk, sizeof(httpRxChunk), 0) < 0)
            {
                httpFailRequest("receive could not be started");
                return;
            }

            httpClientState = HTTP_CLIENT_RECEIVING;
            break;
        }

        case SOCKET_MSG_SEND:
        {
            SYS_CONSOLE_PRINT("HTTP request sent\r\n");
            break;
        }

        case SOCKET_MSG_RECV:
        {
            tstrSocketRecvMsg *recvMessage =
                (tstrSocketRecvMsg *)pMessage;

            if ((recvMessage != NULL) &&
                (recvMessage->s16BufferSize > 0))
            {
                size_t receivedLength =
                    (size_t)recvMessage->s16BufferSize;
                size_t availableSpace =
                    HTTP_RESPONSE_MAX - httpResponseLength;

                if (receivedLength > availableSpace)
                {
                    httpFailRequest("response exceeded HTTP_RESPONSE_MAX");
                    return;
                }

                memcpy(&httpResponse[httpResponseLength],
                       recvMessage->pu8Buffer, receivedLength);
                httpResponseLength += receivedLength;
                httpResponse[httpResponseLength] = '\0';

                httpRequestDeadline = SYS_TIME_CounterGet() +
                                      SYS_TIME_MSToCount(
                                          HTTP_REQUEST_TIMEOUT_MS);

                memset(httpRxChunk, 0, sizeof(httpRxChunk));
                if (recv(socket, httpRxChunk, sizeof(httpRxChunk), 0) < 0)
                {
                    httpFailRequest("could not continue receive");
                }
            }
            else
            {
                processHttpResponse();
                httpFinishRequest();
            }

            break;
        }

        default:
        {
            break;
        }
    }
}

static void httpClientTasks(void)
{
    uint32_t now;

    if (!connectedStationIpValid)
    {
        return;
    }

    now = SYS_TIME_CounterGet();

    if (httpClientState == HTTP_CLIENT_IDLE)
    {
        if (timeReached(now, nextHttpPollTime))
        {
            httpStartActualRequest();
        }
    }
    else if (timeReached(now, httpRequestDeadline))
    {
        httpFailRequest("request timeout");
    }
}

bool WINC_SoftAPActualDataAvailable(void)
{
    return actualDataAvailable;
}

const char *WINC_SoftAPGetActualData(void)
{
    return actualData;
}

size_t WINC_SoftAPGetActualDataLength(void)
{
    return actualDataLength;
}

void WINC_SoftAPClearActualDataFlag(void)
{
    actualDataAvailable = false;
}

bool WINC_SoftAPParsedActualDataAvailable(void)
{
    return parsedActualDataAvailable;
}

const WINC_SM_ACTUAL_DATA *WINC_SoftAPGetParsedActualData(void)
{
    return &parsedActualData;
}

void WINC_SoftAPClearParsedActualDataFlag(void)
{
    parsedActualDataAvailable = false;
}

static void buildSoftApPassword(char *out, size_t outSize, const uint8_t eui64[8])
{
    static const char hex[] = "0123456789ABCDEF";

    if (!out || outSize < 9) return; // need at least 8 + null

    // last 4 bytes: eui64[4..7], reversed
    uint8_t b3 = eui64[7];
    uint8_t b2 = eui64[6];
    uint8_t b1 = eui64[5];
    uint8_t b0 = eui64[4];

    out[0] = hex[(b3 >> 4) & 0x0F];
    out[1] = hex[b3 & 0x0F];

    out[2] = hex[(b2 >> 4) & 0x0F];
    out[3] = hex[b2 & 0x0F];

    out[4] = hex[(b1 >> 4) & 0x0F];
    out[5] = hex[b1 & 0x0F];

    out[6] = hex[(b0 >> 4) & 0x0F];
    out[7] = hex[b0 & 0x0F];

    out[8] = '\0';
}




static void APP_ExampleSocketEventCallback(SOCKET socket, uint8_t messageType, void *pMessage)
{
    /* Keep outgoing HTTP client events separate from the TCP command server. */
    if (socket == httpSocket)
    {
        handleHttpSocketEvent(socket, messageType, pMessage);
        return;
    }

    switch(messageType)
    {
        case SOCKET_MSG_BIND:
        {
            tstrSocketBindMsg *pBindMessage = (tstrSocketBindMsg*)pMessage;

            if ((NULL != pBindMessage) && (0 == pBindMessage->status))
            {
                SYS_CONSOLE_Print(app_wifi_access_point_controllerData.consoleHandle, "Bind on socket %d successful, server_socket = %d\r\n", socket, serverSocket);
                listen(serverSocket, 0);
            }
            else
            {
                SYS_CONSOLE_Print(app_wifi_access_point_controllerData.consoleHandle, "Bind on socket %d failed\r\n", socket);

                shutdown(serverSocket);
                serverSocket =  -1;
                state = EXAMP_STATE_ERROR;
            }
            break;
        }

        case SOCKET_MSG_LISTEN:
        {
            tstrSocketListenMsg *pListenMessage = (tstrSocketListenMsg*)pMessage;

            if ((NULL != pListenMessage) && (0 == pListenMessage->status))
            {
                SYS_CONSOLE_Print(app_wifi_access_point_controllerData.consoleHandle, "Listen on socket %d successful\r\n", socket);
                accept(serverSocket, NULL, NULL);
            }
            else
            {
                SYS_CONSOLE_Print(app_wifi_access_point_controllerData.consoleHandle, "Listen on socket %d failed\r\n", socket);

                shutdown(serverSocket);
                serverSocket =  -1;
                state = EXAMP_STATE_ERROR;
            }
            break;
        }

        case SOCKET_MSG_ACCEPT:
        {
            tstrSocketAcceptMsg *pAcceptMessage = (tstrSocketAcceptMsg*)pMessage;

            if (NULL != pAcceptMessage)
            {
                char s[20];

                accept(serverSocket, NULL, 0);

                if (tcp_client_socket > 0) // close any open client (only one client supported at one time)
                {
                    shutdown(tcp_client_socket);
                }

                tcp_client_socket = pAcceptMessage->sock;

                g_sess.sock = tcp_client_socket;
                g_sess.rxLen = 0;
                g_sess.closeAfterSend = false;

                SYS_CONSOLE_Print(app_wifi_access_point_controllerData.consoleHandle, "Connection from %s:%d\r\n", inet_ntop(AF_INET, &pAcceptMessage->strAddr.sin_addr.s_addr, s, sizeof(s)), _ntohs(pAcceptMessage->strAddr.sin_port));

                memset(recvBuffer, 0, TCP_BUFFER_SIZE);
                recv(tcp_client_socket, recvBuffer, TCP_BUFFER_SIZE, 0);
            }
            else
            {
                SYS_CONSOLE_Print(app_wifi_access_point_controllerData.consoleHandle, "Accept on socket %d failed\r\n", socket);

                shutdown(serverSocket);
                serverSocket =  -1;
                state = EXAMP_STATE_ERROR;
            }
            break;
        }

        case SOCKET_MSG_RECV:
        {
            tstrSocketRecvMsg *pRecvMessage = (tstrSocketRecvMsg*)pMessage;

            if ((NULL != pRecvMessage) && (pRecvMessage->s16BufferSize > 0))
            {
                processIncoming(&g_sess, pRecvMessage->pu8Buffer, (size_t)pRecvMessage->s16BufferSize);

                // Re-arm recv to keep connection alive
                memset(recvBuffer, 0, TCP_BUFFER_SIZE);
                recv(tcp_client_socket, recvBuffer, TCP_BUFFER_SIZE, 0);
            }
            else
            {
                // client disconnected or error
                if (tcp_client_socket > 0)
                {
                    shutdown(tcp_client_socket);
                    tcp_client_socket = -1;
                }
                g_sess.sock = -1;
                g_sess.rxLen = 0;
                g_sess.closeAfterSend = false;

                // keep server open and accept next client
                if (serverSocket >= 0)
                {
                    accept(serverSocket, NULL, NULL);
                }
            }
            break;
        }


        case SOCKET_MSG_SEND:
        {
            SYS_CONSOLE_Print(app_wifi_access_point_controllerData.consoleHandle,
                              "Socket %d send completed\r\n", socket);

            if (g_sess.closeAfterSend)
            {
                g_sess.closeAfterSend = false;

                shutdown(tcp_client_socket);
                tcp_client_socket = -1;

                g_sess.sock = -1;
                g_sess.rxLen = 0;

                if (serverSocket >= 0)
                {
                    accept(serverSocket, NULL, NULL);
                }
            }
            break;
        }

        default:
        {
            break;
        }
    }
}

static void APP_ExampleAPConnectNotifyCallback(DRV_HANDLE handle, WDRV_WINC_ASSOC_HANDLE assocHandle, WDRV_WINC_CONN_STATE currentState, WDRV_WINC_CONN_ERROR errorCode)
{
    (void)handle;
    (void)assocHandle;
    (void)errorCode;

    if (WDRV_WINC_CONN_STATE_CONNECTED == currentState)
    {
        SYS_CONSOLE_Print(app_wifi_access_point_controllerData.consoleHandle, "AP Mode: Station connected\r\n");
    }
    else if (WDRV_WINC_CONN_STATE_DISCONNECTED == currentState)
    {
        SYS_CONSOLE_Print(app_wifi_access_point_controllerData.consoleHandle, "AP Mode: Station disconnected\r\n");

        connectedStationIpValid = false;
        connectedStationIp = 0;
        connectedStationIpText[0] = '\0';
        httpCloseSocket();

        if (tcp_client_socket >= 0)
        {
            shutdown(tcp_client_socket);
            tcp_client_socket = -1;
        }

        g_sess.sock = -1;
        g_sess.rxLen = 0;
        g_sess.closeAfterSend = false;

        if (serverSocket >= 0)
        {
            shutdown(serverSocket);
            serverSocket = -1;
        }

        state = EXAMP_STATE_WAIT_FOR_STATION;
    }
}

#if defined(WLAN_DHCP_SRV_ADDR) && defined(WLAN_DHCP_SRV_NETMASK)
static void APP_ExampleDHCPAddressEventCallback(DRV_HANDLE handle, uint32_t ipAddress)
{
    (void)handle;

    if (connectedStationIpValid && (connectedStationIp != ipAddress))
    {
        httpCloseSocket();
    }

    connectedStationIp = ipAddress;
    connectedStationIpValid = true;

    inet_ntop(AF_INET, &connectedStationIp, connectedStationIpText,
              sizeof(connectedStationIpText));

    SYS_CONSOLE_Print(app_wifi_access_point_controllerData.consoleHandle,
                      "AP Mode: Station IP address is %s\r\n",
                      connectedStationIpText);

    httpScheduleNextRequest(HTTP_FIRST_REQUEST_DELAY_MS);

    if (serverSocket < 0)
    {
        state = EXAMP_STATE_START_TCP_SERVER;
    }
}
#endif

void APP_ExampleInitialize(DRV_HANDLE handle)
{
    SYS_CONSOLE_Print(app_wifi_access_point_controllerData.consoleHandle, "\r\n");
    SYS_CONSOLE_Print(app_wifi_access_point_controllerData.consoleHandle, "===========================================\r\n");
    SYS_CONSOLE_Print(app_wifi_access_point_controllerData.consoleHandle, "WINC WiFi TCP Server Soft AP Example\r\n");
    SYS_CONSOLE_Print(app_wifi_access_point_controllerData.consoleHandle, "===========================================\r\n");
    SYS_CONSOLE_Print(app_wifi_access_point_controllerData.consoleHandle, "\r\n");

    (void)handle;

    state = EXAMP_STATE_INIT;
    serverSocket = -1;
    tcp_client_socket = -1;
    g_sess.sock = -1;
    g_sess.rxLen = 0;
    g_sess.closeAfterSend = false;

    httpSocket = -1;
    httpClientState = HTTP_CLIENT_IDLE;
    connectedStationIp = 0;
    connectedStationIpValid = false;
    connectedStationIpText[0] = '\0';
    nextHttpPollTime = 0;
    httpRequestDeadline = 0;
    httpResponseLength = 0;
    httpResponse[0] = '\0';
    actualDataLength = 0;
    actualData[0] = '\0';
    actualDataAvailable = false;
    memset(&parsedActualData, 0, sizeof(parsedActualData));
    parsedActualDataAvailable = false;
}

void APP_ExampleTasks(DRV_HANDLE handle)
{
    httpClientTasks();

    switch (state)
    {
        case EXAMP_STATE_INIT:
        {
            /* Preset the error state incase any following operations fail. */

            state = EXAMP_STATE_ERROR;

            /* Create the BSS context using default values and then set SSID
             and channel. */
            
            // Format EUI-64 as requested
            sprintf(hardwareId, "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
                    eui64[0], eui64[1], eui64[2], eui64[3],
                    eui64[4], eui64[5], eui64[6], eui64[7]);

            sprintf(g_softApSsid, "Tulip-%s", hardwareId);
            buildSoftApPassword(g_softApPsk, sizeof(g_softApPsk), eui64);

            SYS_CONSOLE_PRINT("SoftAP SSID: %s\r\nSoftAP PSK: %s\r\n",
                g_softApSsid, g_softApPsk);

            if (WDRV_WINC_STATUS_OK != WDRV_WINC_BSSCtxSetDefaults(&bssCtx))
            {
                break;
            }

            if (WDRV_WINC_STATUS_OK != WDRV_WINC_BSSCtxSetSSID(&bssCtx, (uint8_t*)g_softApSsid, strlen(g_softApSsid)))
            {
                break;
            }

            if (WDRV_WINC_STATUS_OK != WDRV_WINC_BSSCtxSetChannel(&bssCtx, WLAN_CHANNEL))
            {
                break;
            }

            #if defined(WLAN_AUTH_OPEN)
                /* Create authentication context for Open. */

            if (WDRV_WINC_STATUS_OK !=
                WDRV_WINC_AuthCtxSetWPA(&authCtx,
                                       (uint8_t*)g_softApPsk,
                                       (uint8_t)strlen(g_softApPsk)))
            {
                break;
            }

            #endif

            #if defined(WLAN_DHCP_SRV_ADDR) && defined(WLAN_DHCP_SRV_NETMASK)
                /* Enable use of DHCP for network configuration, DHCP is the default
                 but this also registers the callback for notifications. */

                if (WDRV_WINC_STATUS_OK != WDRV_WINC_IPDHCPServerConfigure(handle, inet_addr(WLAN_DHCP_SRV_ADDR), inet_addr(WLAN_DHCP_SRV_NETMASK), &APP_ExampleDHCPAddressEventCallback))
                {
                    break;
                }
            #endif
            /* Register callback for socket events. */

            WDRV_WINC_SocketRegisterEventCallback(handle, &APP_ExampleSocketEventCallback);

            /* Create the AP using the BSS and authentication context. */

            if (WDRV_WINC_STATUS_OK == WDRV_WINC_APStart(handle, &bssCtx, &authCtx, NULL, &APP_ExampleAPConnectNotifyCallback))
            {
                SYS_CONSOLE_Print(app_wifi_access_point_controllerData.consoleHandle, "AP started, you can connect to %s\r\n", g_softApSsid);
                SYS_CONSOLE_Print(app_wifi_access_point_controllerData.consoleHandle, "On the connected device, start a TCP client connection to %s on port %d\r\n\r\n", WLAN_DHCP_SRV_ADDR, TCP_LISTEN_PORT);

                state = EXAMP_STATE_WAIT_FOR_STATION;
            }
            break;
        }

        case EXAMP_STATE_WAIT_FOR_STATION:
        {
            break;
        }

        case EXAMP_STATE_START_TCP_SERVER:
        {
            /* Do not create/bind a second listening socket. */
            if (serverSocket >= 0)
            {
                state = EXAMP_STATE_SOCKET_LISTENING;
                break;
            }

            /* Create the server socket. */
            serverSocket = socket(AF_INET, SOCK_STREAM, 0);

            if (serverSocket >= 0)
            {
                struct sockaddr_in addr;

                /* Listen on the socket. */

                addr.sin_family = AF_INET;
                addr.sin_port = _htons(TCP_LISTEN_PORT);
                addr.sin_addr.s_addr = 0;

                if (bind(serverSocket, (struct sockaddr*)&addr, sizeof(struct sockaddr_in)) < 0)
                {
                    SYS_CONSOLE_Print(app_wifi_access_point_controllerData.consoleHandle, "Socket bind error\r\n");
                    state = EXAMP_STATE_ERROR;
                    break;
                }

                state = EXAMP_STATE_SOCKET_LISTENING;
            }
            else
            {
                SYS_CONSOLE_Print(app_wifi_access_point_controllerData.consoleHandle, "Socket creation error\r\n");
                state = EXAMP_STATE_ERROR;
                break;
            }
            break;
        }

        case EXAMP_STATE_SOCKET_LISTENING:
        {
            break;
        }

        case EXAMP_STATE_DONE:
        {
            break;
        }

        case EXAMP_STATE_ERROR:
        {
            break;
        }

        default:
        {
            break;
        }
    }
}
