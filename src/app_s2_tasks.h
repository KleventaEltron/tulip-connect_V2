#ifndef APP_S2_TASKS_H
#define APP_S2_TASKS_H
#include "files/s2/s2_limits.h"
#include "files/s2/s2_tls.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef enum { APP_S2_NOT_INITIALIZED=0, APP_S2_DISABLED, APP_S2_WAIT_SESSION,
    APP_S2_WAIT_NETWORK, APP_S2_WAIT_TIME, APP_S2_TLS_CONNECTING,
    APP_S2_WS_UPGRADING, APP_S2_WS_OPEN, APP_S2_CLOSING } APP_S2_STATE;
typedef enum { APP_S2_ERROR_NONE=0, APP_S2_ERROR_REGISTRATION, APP_S2_ERROR_SETUP,
    APP_S2_ERROR_EXPIRED, APP_S2_ERROR_NETWORK, APP_S2_ERROR_TLS,
    APP_S2_ERROR_TRANSPORT } APP_S2_ERROR;
typedef struct {
    APP_S2_STATE state;
    APP_S2_ERROR error;
    S2_TLS_ERROR tlsError;
    int wolfError;
    uint32_t attempts;
} APP_S2_STATUS;

/* Future provisioning supplies ONE freshly initiated S2 Connect session.
 * All strings/CA bytes are copied. IPv4 uses Harmony's raw representation.
 * Lifetime is monotonic milliseconds, 1..INT32_MAX, from ProvideSession time.
 * CA/trust and commToken must come from verified pairing/session initiation. */
typedef struct {
    uint32_t ipv4;
    uint16_t port;
    const char *hostname, *path, *commToken;
    const uint8_t *caDer;
    size_t caLength;
    uint32_t lifetimeMs;
} APP_S2_SESSION;

/* Caller allocates statically only when S2 is configured. Keep alive until
 * Disable. No context is allocated by the unconfigured boot/main-loop hooks. */
typedef struct {
    S2_TLS tls;
    S2_TRANSPORT transport;
    char hostname[S2_TLS_HOST_MAX+1u], authority[S2_TLS_HOST_MAX+7u];
    char path[APP_S2_PATH_MAX+1u], token[APP_S2_TOKEN_MAX+1u];
    uint8_t ca[APP_S2_CA_MAX];
    size_t caLength;
    uint32_t ipv4, issuedAt, lifetimeMs;
    uint16_t port;
} APP_S2_WORKSPACE;

/* Main-loop only, non-reentrant. Initialize after NETWORK_MANAGER_Initialize. */
bool APP_S2_Initialize(void);
void APP_S2_Tasks(void);
bool APP_S2_Attach(APP_S2_WORKSPACE *workspace, void *consumer,
    bool (*text)(void *, const char *, size_t));
bool APP_S2_ProvideSession(const APP_S2_SESSION *session);
/* Immediate stop, secret erasure and release of caller-owned workspace. */
void APP_S2_Disable(void);
APP_S2_STATUS APP_S2_GetStatus(void);
/* This sends text over WebSocket only; it does not validate S2 JSON or permit
 * device actuation. Never invoke from the text/other transport callbacks. */
S2_WS_RESULT APP_S2_SendText(const char *text, size_t length);
#ifdef __cplusplus
}
#endif
#endif
