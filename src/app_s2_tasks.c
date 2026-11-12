#include "app_s2_tasks.h"
#include "files/network_manager.h"
#include "system/time/sys_time.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>

static APP_S2_WORKSPACE *workspace;
static APP_S2_STATUS status;
static bool initialized, registered, pending, active;

static uint32_t nowMilliseconds(void)
{
    uint64_t count = SYS_TIME_Counter64Get();
    uint32_t frequency = SYS_TIME_FrequencyGet();
    if (frequency == 0) return 0;
    return (uint32_t)((count / frequency) * 1000u +
        ((count % frequency) * 1000u) / frequency);
}

static void clearSession(void)
{
    pending = active = false;
    if (workspace != NULL) {
        memset(workspace->token, 0, sizeof(workspace->token));
        workspace->issuedAt = workspace->lifetimeMs = 0;
    }
}

static void abortConnection(void)
{
    if (workspace != NULL) {
        S2_TRANSPORT_Stop(&workspace->transport);
        /* Stop may request a graceful TLS drain; teardown must be immediate. */
        S2_TLS_Close(&workspace->tls);
    }
    clearSession();
}

static void networkDown(void)
{
    bool hadSession = active || pending;
    abortConnection();
    if (hadSession) status.error = APP_S2_ERROR_NETWORK;
    status.state = workspace != NULL ? APP_S2_WAIT_SESSION : APP_S2_DISABLED;
}

bool APP_S2_Initialize(void)
{
    if (initialized) return registered;
    memset(&status, 0, sizeof(status));
    initialized = true;
    registered = NETWORK_MANAGER_RegisterDownCallback(networkDown);
    status.state = APP_S2_DISABLED;
    if (!registered) status.error = APP_S2_ERROR_REGISTRATION;
    return registered;
}

bool APP_S2_Attach(APP_S2_WORKSPACE *w, void *consumer,
    bool (*text)(void *, const char *, size_t))
{
    S2_TRANSPORT_PLATFORM platform;
    if (!initialized || !registered || workspace != NULL || w == NULL || text == NULL) return false;
    memset(w, 0, sizeof(*w));
    if (!S2_TLS_Initialize(&w->tls, consumer, text) || !S2_TLS_GetPlatform(&w->tls, &platform) ||
        !S2_TRANSPORT_Initialize(&w->transport, &platform)) {
        status.error = APP_S2_ERROR_SETUP;
        return false;
    }
    workspace = w;
    clearSession();
    status.error = APP_S2_ERROR_NONE;
    status.state = APP_S2_WAIT_SESSION;
    return true;
}

static bool visible(const char *value, size_t maximum)
{
    size_t i;
    if (value == NULL) return false;
    for (i = 0; i <= maximum; i++) {
        unsigned c = (unsigned char)value[i];
        if (c == 0) return i != 0;
        if (c < 33 || c > 126) return false;
    }
    return false;
}

bool APP_S2_ProvideSession(const APP_S2_SESSION *s)
{
    APP_S2_WORKSPACE *w = workspace;
    uint32_t nowMs = nowMilliseconds();
    if (w == NULL || s == NULL || pending || active || status.state != APP_S2_WAIT_SESSION) return false;
    if (s->ipv4 == 0 || s->port == 0 || !visible(s->hostname,S2_TLS_HOST_MAX) ||
        !visible(s->path,APP_S2_PATH_MAX) || s->path[0] != '/' || strchr(s->path,'#') != NULL ||
        !visible(s->commToken,APP_S2_TOKEN_MAX) || s->caDer == NULL ||
        s->caLength == 0 || s->caLength > sizeof(w->ca) ||
        s->lifetimeMs == 0 || s->lifetimeMs > INT32_MAX) return false;
    memcpy(w->hostname,s->hostname,strlen(s->hostname)+1);
    memcpy(w->path,s->path,strlen(s->path)+1);
    memcpy(w->token,s->commToken,strlen(s->commToken)+1);
    memmove(w->ca,s->caDer,s->caLength);w->caLength=s->caLength;
    w->ipv4=s->ipv4;w->port=s->port;w->issuedAt=nowMs;w->lifetimeMs=s->lifetimeMs;
    (void)snprintf(w->authority,sizeof(w->authority),"%s:%u",w->hostname,(unsigned)w->port);
    pending = true;
    status.error = APP_S2_ERROR_NONE;
    status.tlsError = S2_TLS_ERROR_NONE;status.wolfError=0;
    return true;
}

void APP_S2_Disable(void)
{
    if (!initialized) return;
    abortConnection();
    if (workspace != NULL) memset(workspace,0,sizeof(*workspace));
    workspace = NULL;
    status.state = APP_S2_DISABLED;
}

void APP_S2_Tasks(void)
{
    APP_S2_WORKSPACE *w = workspace;
    uint32_t now;
    bool transportPolled = false;
    bool tlsPolled = false;
    bool checkExpiry;

    if (!initialized || !registered || w == NULL) return;
    now = nowMilliseconds();

    /* Session credentials expire during setup, not after the channel is open. */
    switch (status.state)
    {
        case APP_S2_WS_OPEN:
        case APP_S2_CLOSING:
            checkExpiry = pending;
            break;

        default:
            checkExpiry = pending || active;
            break;
    }

    if (checkExpiry && (uint32_t)(now - w->issuedAt) >= w->lifetimeMs)
    {
        abortConnection();
        status.error = APP_S2_ERROR_EXPIRED;
        status.state = APP_S2_WAIT_SESSION;
        return;
    }

    if (!NETWORK_MANAGER_IsReady())
    {
        if (active) networkDown();
        if (pending) status.state = APP_S2_WAIT_NETWORK;
        return;
    }

    /* Existing connections keep their TLS layer progressing in every state. */
    if (active)
    {
        S2_TLS_Tasks(&w->tls, now);
        tlsPolled = true;
        switch (w->tls.state)
        {
            case S2_TLS_FAILED:
                goto tlsFailure;

            default:
                break;
        }
    }

    switch (status.state)
    {
        case APP_S2_WAIT_SESSION:
        case APP_S2_WAIT_NETWORK:
        case APP_S2_WAIT_TIME:
        {
            if (!pending) break;
            if (!S2_TLS_ClockReady())
            {
                status.state = APP_S2_WAIT_TIME;
                break;
            }

            /* One attempt consumes this session, even if TCP/TLS never completes. */
            pending = false;
            active = true;
            status.attempts++;
            if (!S2_TLS_Start(&w->tls, w->ipv4, w->port, w->hostname,
                    w->ca, w->caLength, now)) goto tlsFailure;
            status.state = APP_S2_TLS_CONNECTING;
        }
        /* Preserve immediate progress within the same task call. */
        /* fall through */

        case APP_S2_TLS_CONNECTING:
        {
            if (!tlsPolled) S2_TLS_Tasks(&w->tls, now);
            switch (w->tls.state)
            {
                case S2_TLS_FAILED:
                    goto tlsFailure;

                case S2_TLS_VERIFIED:
                {
                    bool started = S2_TRANSPORT_Start(&w->transport, w->authority,
                        w->path, w->token, now);
                    memset(w->token, 0, sizeof(w->token));
                    if (!started) goto transportFailure;
                    status.state = APP_S2_WS_UPGRADING;
                    break;
                }

                default:
                    return;
            }
        }
        /* The upgrade may also progress during this call. */
        /* fall through */

        case APP_S2_WS_UPGRADING:
        case APP_S2_WS_OPEN:
        {
            S2_TRANSPORT_Tasks(&w->transport, true, now);
            transportPolled = true;
            switch (S2_TRANSPORT_GetState(&w->transport))
            {
                case S2_TRANSPORT_FAILED:
                    goto transportFailure;

                case S2_TRANSPORT_OPEN:
                    status.state = APP_S2_WS_OPEN;
                    return;

                case S2_TRANSPORT_IDLE:
                case S2_TRANSPORT_DRAINING:
                    status.state = APP_S2_CLOSING;
                    break;

                default:
                    return;
            }
        }
        /* Check closure without polling the transport twice. */
        /* fall through */

        case APP_S2_CLOSING:
        {
            if (!transportPolled) S2_TRANSPORT_Tasks(&w->transport, true, now);
            switch (S2_TRANSPORT_GetState(&w->transport))
            {
                case S2_TRANSPORT_FAILED:
                    goto transportFailure;

                case S2_TRANSPORT_IDLE:
                    switch (w->tls.state)
                    {
                        case S2_TLS_IDLE:
                            clearSession();
                            status.state = APP_S2_WAIT_SESSION;
                            break;

                        default:
                            break;
                    }
                    break;

                default:
                    break;
            }
            break;
        }

        case APP_S2_NOT_INITIALIZED:
        case APP_S2_DISABLED:
        default:
            break;
    }
    return;

tlsFailure:
    status.tlsError = w->tls.error;
    status.wolfError = w->tls.wolfError;
    abortConnection();
    status.error = APP_S2_ERROR_TLS;
    status.state = APP_S2_WAIT_SESSION;
    return;

transportFailure:
    status.tlsError = w->tls.error;
    status.wolfError = w->tls.wolfError;
    abortConnection();
    status.error = APP_S2_ERROR_TRANSPORT;
    status.state = APP_S2_WAIT_SESSION;
}

APP_S2_STATUS APP_S2_GetStatus(void) { return status; }
S2_WS_RESULT APP_S2_SendText(const char *message,size_t length)
{
    if (workspace==NULL || status.state!=APP_S2_WS_OPEN) return S2_WS_CLOSED;
    return S2_TRANSPORT_SendText(&workspace->transport,message,length);
}
