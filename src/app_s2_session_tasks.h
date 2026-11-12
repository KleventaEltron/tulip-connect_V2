#ifndef APP_S2_SESSION_TASKS_H
#define APP_S2_SESSION_TASKS_H
#include "app_s2_tasks.h"
#include "files/s2/s2_https.h"
#include "files/s2/s2_token_store.h"
#define APP_S2_SESSION_DNS_MS 10000u
#define APP_S2_SESSION_MAX_FAILURES 5u
typedef enum { APP_S2_SESSION_DISABLED=0, APP_S2_SESSION_WAIT_NETWORK,
    APP_S2_SESSION_WAIT_TIME, APP_S2_SESSION_LOAD, APP_S2_SESSION_DNS,
    APP_S2_SESSION_HTTPS_CONNECT, APP_S2_SESSION_HTTPS_WAIT,
    APP_S2_SESSION_HTTP, APP_S2_SESSION_HANDOFF, APP_S2_SESSION_CHANNEL,
    APP_S2_SESSION_RETRY_WAIT, APP_S2_SESSION_BLOCKED } APP_S2_SESSION_STATE;
typedef enum { APP_S2_SESSION_ERROR_NONE=0, APP_S2_SESSION_ERROR_REGISTRATION,
    APP_S2_SESSION_ERROR_CONFIGURATION, APP_S2_SESSION_ERROR_NETWORK,
    APP_S2_SESSION_ERROR_STORAGE, APP_S2_SESSION_ERROR_UNPAIRED,
    APP_S2_SESSION_ERROR_DNS, APP_S2_SESSION_ERROR_TLS,
    APP_S2_SESSION_ERROR_HTTP, APP_S2_SESSION_ERROR_AUTH,
    APP_S2_SESSION_ERROR_PROTOCOL, APP_S2_SESSION_ERROR_EXPIRED,
    APP_S2_SESSION_ERROR_CHANNEL } APP_S2_SESSION_ERROR;
typedef struct {
    APP_S2_SESSION_STATE state;
    APP_S2_SESSION_ERROR error;
    S2_TOKEN_STORE_RESULT storageResult;
    S2_SESSION_RESULT sessionResult;
    S2_HTTPS_ERROR httpError;
    S2_TLS_ERROR tlsError;
    unsigned httpStatus;
    int wolfError,dnsResult;
    uint32_t attempts,consecutiveFailures,retryDelayMs;
} APP_S2_SESSION_STATUS;
typedef struct {
    /* Already paired, selected /v1 API. This API does not establish trust or
     * provision credentials. Never pass an unchecked endpoint/certificate. */
    const char *apiBase,*clientNodeId,*serverNodeId;
    const uint8_t *caDer;
    size_t caLength;
    const S2_TOKEN_STORE *tokens;
    void *consumer;
    bool (*text)(void *,const char *,size_t);
    /* Optional explicitly provisioned route; TLS still verifies hostname. */
    uint32_t ipv4Override;
} APP_S2_SESSION_CONFIG;
typedef struct {
    APP_S2_WORKSPACE channel;
    S2_TLS httpsTls;
    S2_HTTPS http;
    S2_SESSION session;
    S2_TOKEN_STORE store;
    S2_SESSION_TOKENS saved;
    S2_SESSION_DETAILS details;
    char apiBase[S2_SESSION_URL_SIZE],client[37],server[37],hostname[254];
    size_t caLength;
    uint16_t port;
    uint32_t ipv4,phaseAt,retryAt,configuredIpv4;
} APP_S2_SESSION_WORKSPACE;
/* Main-loop only/non-reentrant. Initialize after APP_S2_Initialize. No workspace
 * allocation or sockets at boot. Configure attaches the existing WebSocket
 * task exclusively; do not separately call its Attach/Disable/ProvideSession.
 * Caller supplies a zero-initialized static workspace, kept alive until Stop.
 * Config bytes/journal callbacks are copied; journal IO.user must remain alive.
 * Config inputs must not alias workspace. No additional EEPROM allocation. */
bool APP_S2_SESSION_Initialize(void);
bool APP_S2_SESSION_Configure(APP_S2_SESSION_WORKSPACE *,const APP_S2_SESSION_CONFIG *);
void APP_S2_SESSION_Tasks(void);
/* Immediate close/erase/detach. Does not erase durable pairing or tokens. */
void APP_S2_SESSION_Stop(void);
APP_S2_SESSION_STATUS APP_S2_SESSION_GetStatus(void);
#endif
