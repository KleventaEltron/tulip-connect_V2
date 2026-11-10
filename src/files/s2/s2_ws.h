#ifndef S2_WS_H
#define S2_WS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Local implementation limits, not limits imposed by S2. */
#define S2_WS_MESSAGE_MAX 4096u
#define S2_WS_HEADERS_MAX 4096u

typedef enum {
    S2_WS_OK = 0,
    S2_WS_INVALID_ARGUMENT,
    S2_WS_PROTOCOL_ERROR,
    S2_WS_IO_ERROR,
    S2_WS_CLOSED
} S2_WS_RESULT;

typedef struct {
    void *user;
    /* Cryptographic randomness; no deterministic production fallback. */
    bool (*random)(void *user, uint8_t *out, size_t length);
    /* SHA-1 is used only for the RFC6455 upgrade accept calculation. */
    bool (*sha1)(void *user, const uint8_t *data, size_t length, uint8_t out[20]);
    /* Copy/queue the entire frame before returning true. The buffer is reused.
     * A partial/non-atomic enqueue must return false and ends this connection. */
    bool (*send)(void *user, const uint8_t *frame, size_t length);
    /* Complete, validated UTF-8 text; pointer is valid only during callback.
     * Return false to close when the consumer cannot accept the message. */
    bool (*text)(void *user, const char *text, size_t length);
    /* Connection ended; called once. Transport owns final-frame drain/close.
     * Callback must not reenter the codec. Network-loss Reset must discard writes. */
    void (*closed)(void *user);
} S2_WS_PLATFORM;

typedef struct {
    S2_WS_PLATFORM platform;
    bool initialized, open, upgradePending, ended, fragmented;
    char expectedAccept[29];
    uint8_t rx[S2_WS_MESSAGE_MAX + 4u];
    uint8_t message[S2_WS_MESSAGE_MAX];
    uint8_t tx[S2_WS_MESSAGE_MAX + 8u];
    size_t rxLength, messageLength;
} S2_WS;

/* One context per connection. Main-loop only, non-reentrant callbacks.
 * Initialize once per NEW verified TLS connection, never from a callback.
 * This codec does not implement sockets, TLS, authentication or pairing. */
S2_WS_RESULT S2_WS_Initialize(S2_WS *ws, const S2_WS_PLATFORM *platform);
/* Invoke only after certificate validation and S2 Connect session initiation.
 * authority = host[:port], path = /path[?query], bearer = commToken.
 * No extension or subprotocol is offered. Output is NUL-terminated on success. */
S2_WS_RESULT S2_WS_BuildUpgrade(S2_WS *ws, const char *authority,
    const char *path, const char *bearer, char *out, size_t capacity);
/* Supply the COMPLETE HTTP headers including CRLF CRLF, WITHOUT trailing frame
 * bytes. The future transport must split headers from any coalesced frames. */
S2_WS_RESULT S2_WS_AcceptUpgrade(S2_WS *ws, const char *headers, size_t length);
/* Arbitrary TCP/TLS read boundaries accepted, including multiple frames/read. */
S2_WS_RESULT S2_WS_Feed(S2_WS *ws, const uint8_t *bytes, size_t length);
S2_WS_RESULT S2_WS_SendText(S2_WS *ws, const char *text, size_t length);
S2_WS_RESULT S2_WS_Ping(S2_WS *ws);
/* Abrupt network loss: clear state and notify closed once; never sends a frame. */
void S2_WS_Reset(S2_WS *ws);
bool S2_WS_IsOpen(const S2_WS *ws);

#ifdef __cplusplus
}
#endif
#endif
