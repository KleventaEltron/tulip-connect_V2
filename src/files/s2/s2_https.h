#ifndef S2_HTTPS_H
#define S2_HTTPS_H
#include "s2_limits.h"
#include "s2_tls.h"
#include "s2_session.h"
#ifdef __cplusplus
extern "C" {
#endif
#define S2_HTTPS_LINE_MAX 128u
#define S2_HTTPS_TRAILERS_MAX 1024u

typedef enum { S2_HTTPS_IDLE=0, S2_HTTPS_SENDING, S2_HTTPS_READING,
    S2_HTTPS_DONE, S2_HTTPS_FAILED } S2_HTTPS_STATE;
typedef enum { S2_HTTPS_ERROR_NONE=0, S2_HTTPS_ERROR_ARGUMENT, S2_HTTPS_ERROR_TLS,
    S2_HTTPS_ERROR_REQUEST, S2_HTTPS_ERROR_PROTOCOL, S2_HTTPS_ERROR_LIMIT,
    S2_HTTPS_ERROR_IO, S2_HTTPS_ERROR_SESSION } S2_HTTPS_ERROR;
typedef enum { S2_HTTPS_HEADERS=0, S2_HTTPS_LENGTH_BODY, S2_HTTPS_EOF_BODY,
    S2_HTTPS_CHUNK_LINE, S2_HTTPS_CHUNK_BODY, S2_HTTPS_CHUNK_CR,
    S2_HTTPS_CHUNK_LF, S2_HTTPS_TRAILERS, S2_HTTPS_COMPLETE } S2_HTTPS_PARSE_STATE;
typedef struct {
    S2_HTTPS_STATE state;
    S2_HTTPS_ERROR error;
    S2_SESSION_RESULT sessionResult;
    unsigned httpStatus;
    S2_TLS *tls;
    S2_SESSION *session;
    S2_TRANSPORT_PLATFORM backend;
    S2_HTTPS_PARSE_STATE parseState;
    bool reusable, connectionClose, http10, hasLength, chunked, jsonType, lineCR;
    unsigned interimCount, trailerCount;
    size_t txLength, txOffset, headerLength, headerTotal, bodyLength,
        contentLength, chunkRemaining, lineLength, trailerTotal;
    S2_SESSION_REQUEST request;
    uint8_t tx[S2_HTTPS_TX_MAX], rx[S2_TLS_IO_SIZE];
    char headers[S2_HTTPS_HEADERS_MAX+1u], body[S2_SESSION_JSON_MAX+1u],
        line[S2_HTTPS_LINE_MAX+1u];
} S2_HTTPS;

/* Zero-initialize before first use. Main-loop/non-reentrant, no heap.
 * tls must already be VERIFIED for the engine's API host. boundPort MUST be
 * the port used for S2_TLS_Start. This driver does not open TLS or resolve DNS.
 * It borrows both contexts until Stop, and owns HTTP use of this TLS connection.
 * Do not run WebSocket transport on the same TLS instance concurrently.
 * One Begin handles one initiate/confirm request, not the full two-step flow. */
bool S2_HTTPS_Begin(S2_HTTPS *http,S2_TLS *tls,uint16_t boundPort,
    S2_SESSION *session,uint32_t nowMs);
void S2_HTTPS_Tasks(S2_HTTPS *http,uint32_t nowMs);
/* Abort active exchange before network-manager teardown. Completed engine
 * details survive Stop so the owner can export them using their original TTL. */
void S2_HTTPS_Stop(S2_HTTPS *http);
#ifdef __cplusplus
}
#endif
#endif
