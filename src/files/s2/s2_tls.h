#ifndef S2_TLS_H
#define S2_TLS_H
#include "s2_limits.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "s2_transport.h"
#ifdef __cplusplus
extern "C" {
#endif
#define S2_TLS_HOST_MAX 253u
/* Authenticated TLS close_notify, distinct from a truncated/error read (-1). */
#define S2_TLS_READ_EOF (-2)
#define S2_TLS_PHASE_TIMEOUT_MS 30000u

typedef enum { S2_TLS_IDLE = 0, S2_TLS_TCP_CONNECTING, S2_TLS_HANDSHAKING,
    S2_TLS_VERIFIED, S2_TLS_CLOSING, S2_TLS_FAILED } S2_TLS_STATE;
typedef enum { S2_TLS_ERROR_NONE = 0, S2_TLS_ERROR_ARGUMENT, S2_TLS_ERROR_NETWORK,
    S2_TLS_ERROR_CLOCK, S2_TLS_ERROR_MEMORY, S2_TLS_ERROR_TRUST,
    S2_TLS_ERROR_SOCKET, S2_TLS_ERROR_TIMEOUT, S2_TLS_ERROR_HANDSHAKE,
    S2_TLS_ERROR_IO } S2_TLS_ERROR;

typedef struct {
    bool initialized, libraryOwned, shutdownComplete, readNeedsWrite;
    int socket;
    void *context, *session;
    S2_TLS_STATE state;
    S2_TLS_ERROR error;
    int wolfError;
    uint32_t phaseStarted, lastNow;
    char hostname[S2_TLS_HOST_MAX + 1u];
    uint8_t writeBuffer[S2_TLS_IO_SIZE], readBuffer[S2_TLS_IO_SIZE];
    size_t writeLength, readLength, readOffset;
    void *consumer;
    bool (*text)(void *consumer, const char *message, size_t length);
} S2_TLS;

/* Main-loop only, one caller-owned static context per connection. Initialize
 * once before use. Start/Tasks never use logging's socket or TLS context. */
bool S2_TLS_Initialize(S2_TLS *tls, void *consumer,
    bool (*text)(void *, const char *, size_t));
/* Hostname is DNS name only; IPv4 is already resolved, in Harmony byte order.
 * CA is DER, selected by trusted provisioning/pairing; no trust-on-first-use.
 * The CA is loaded during this call and need not remain in memory afterward.
 * Requires NETWORK_MANAGER readiness and nonzero synchronized SNTP UTC.
 * Active connections reject Start without altering their resources. */
bool S2_TLS_Start(S2_TLS *tls, uint32_t ipv4, uint16_t port, const char *hostname,
    const uint8_t *caDer, size_t caLength, uint32_t nowMs);
void S2_TLS_Tasks(S2_TLS *tls, uint32_t nowMs);
bool S2_TLS_ClockReady(void);
/* Bind an HTTP origin to the verified DNS name and actual TCP peer port. */
bool S2_TLS_PeerMatches(S2_TLS *tls,const char *hostname,uint16_t port);
/* Application must register a network-manager callback that stops the transport
 * then calls Close BEFORE shared-stack teardown. Close is idempotent. */
void S2_TLS_Close(S2_TLS *tls);
/* Transport close callback requests bounded TLS/TCP draining; keep calling
 * Tasks until IDLE. Close above is the immediate network-loss/teardown path. */
/* Produces callbacks for the existing transport, using tls as platform.user.
 * Only call transport Start once TLS state is VERIFIED; pairing/session
 * initiation must independently supply the communication token and URL. */
bool S2_TLS_GetPlatform(S2_TLS *tls, S2_TRANSPORT_PLATFORM *platform);
#ifdef __cplusplus
}
#endif
#endif
