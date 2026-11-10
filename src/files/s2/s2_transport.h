#ifndef S2_TRANSPORT_H
#define S2_TRANSPORT_H
#include "s2_ws.h"
#ifdef __cplusplus
extern "C" {
#endif

#define S2_TRANSPORT_QUEUE_SIZE (2u * (S2_WS_MESSAGE_MAX + 8u))
#define S2_TRANSPORT_IO_BUDGET 512u
#define S2_TRANSPORT_UPGRADE_TIMEOUT_MS 10000u
#define S2_TRANSPORT_DRAIN_TIMEOUT_MS 2000u
#define S2_TRANSPORT_WRITE_TIMEOUT_MS 10000u

typedef enum {
    S2_TRANSPORT_IDLE = 0,
    S2_TRANSPORT_UPGRADING,
    S2_TRANSPORT_OPEN,
    S2_TRANSPORT_DRAINING,
    S2_TRANSPORT_FAILED
} S2_TRANSPORT_STATE;

typedef struct {
    void *user;
    /* Must confirm trusted certificate AND hostname for this TLS connection.
     * The backend owns its own socket; logging's TLS context is unsuitable. */
    bool (*verified)(void *user);
    /* Nonblocking: 0 = would block; negative = closed/error; positive = bytes.
     * Never return more than capacity/length. Write must copy consumed bytes. */
    int (*read)(void *user, uint8_t *out, size_t capacity);
    int (*write)(void *user, const uint8_t *data, size_t length);
    /* Close/release this backend's socket/TLS state only. */
    void (*close)(void *user);
    bool (*random)(void *user, uint8_t *out, size_t length);
    bool (*sha1)(void *user, const uint8_t *data, size_t length, uint8_t out[20]);
    /* Consume/enqueue complete text. Do not call transport APIs from callbacks. */
    bool (*text)(void *user, const char *text, size_t length);
} S2_TRANSPORT_PLATFORM;

typedef struct {
    S2_WS websocket;
    S2_TRANSPORT_PLATFORM platform;
    S2_TRANSPORT_STATE state;
    bool initialized, backendOwned, drainTimerActive, writeTimerActive;
    uint32_t phaseStarted, writeProgress;
    size_t queueOffset, queueLength, headerLength;
    uint8_t queue[S2_TRANSPORT_QUEUE_SIZE];
    char headers[S2_WS_HEADERS_MAX];
    uint8_t readBuffer[S2_TRANSPORT_IO_BUDGET];
} S2_TRANSPORT;

/* One caller-owned static context per connection. No global instance/heap.
 * Main-loop only, non-reentrant. Initialize once before first use. */
bool S2_TRANSPORT_Initialize(S2_TRANSPORT *transport, const S2_TRANSPORT_PLATFORM *platform);
/* Takes ownership of a NEW TLS backend connection even if verification fails.
 * Pairing/session initiation must already have supplied the commToken and URL.
 * No socket is opened here. Active transports reject Start without side effects. */
bool S2_TRANSPORT_Start(S2_TRANSPORT *transport, const char *authority,
    const char *path, const char *commToken, uint32_t nowMs);
/* Call after network-manager polling. False readiness aborts without flushing.
 * nowMs is a monotonic millisecond counter; unsigned wrap is supported. */
void S2_TRANSPORT_Tasks(S2_TRANSPORT *transport, bool networkReady, uint32_t nowMs);
/* Register a wrapper calling Stop with the network manager before opening TLS. */
void S2_TRANSPORT_Stop(S2_TRANSPORT *transport);
S2_WS_RESULT S2_TRANSPORT_SendText(S2_TRANSPORT *transport, const char *text, size_t length);
S2_TRANSPORT_STATE S2_TRANSPORT_GetState(const S2_TRANSPORT *transport);
#ifdef __cplusplus
}
#endif
#endif
