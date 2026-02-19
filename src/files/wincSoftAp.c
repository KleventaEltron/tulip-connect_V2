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
                accept(serverSocket, NULL, NULL);
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

                accept(serverSocket, NULL, NULL);
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
    if (WDRV_WINC_CONN_STATE_CONNECTED == currentState)
    {
        SYS_CONSOLE_Print(app_wifi_access_point_controllerData.consoleHandle, "AP Mode: Station connected\r\n");
    }
    else if (WDRV_WINC_CONN_STATE_DISCONNECTED == currentState)
    {
        SYS_CONSOLE_Print(app_wifi_access_point_controllerData.consoleHandle, "AP Mode: Station disconnected\r\n");

        if (-1 != serverSocket)
        {
            shutdown(serverSocket);
            serverSocket = -1;
        }
    }
}

#if defined(WLAN_DHCP_SRV_ADDR) && defined(WLAN_DHCP_SRV_NETMASK)
static void APP_ExampleDHCPAddressEventCallback(DRV_HANDLE handle, uint32_t ipAddress)
{
    char s[20];

    SYS_CONSOLE_Print(app_wifi_access_point_controllerData.consoleHandle, "AP Mode: Station IP address is %s\r\n", inet_ntop(AF_INET, &ipAddress, s, sizeof(s)));
    state = EXAMP_STATE_START_TCP_SERVER;
}
#endif

void APP_ExampleInitialize(DRV_HANDLE handle)
{
    SYS_CONSOLE_Print(app_wifi_access_point_controllerData.consoleHandle, "\r\n");
    SYS_CONSOLE_Print(app_wifi_access_point_controllerData.consoleHandle, "===========================================\r\n");
    SYS_CONSOLE_Print(app_wifi_access_point_controllerData.consoleHandle, "WINC WiFi TCP Server Soft AP Example\r\n");
    SYS_CONSOLE_Print(app_wifi_access_point_controllerData.consoleHandle, "===========================================\r\n");
    SYS_CONSOLE_Print(app_wifi_access_point_controllerData.consoleHandle, "\r\n");

    state = EXAMP_STATE_INIT;

    serverSocket = -1;
}

void APP_ExampleTasks(DRV_HANDLE handle)
{
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
