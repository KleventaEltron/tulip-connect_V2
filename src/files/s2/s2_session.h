#ifndef S2_SESSION_H
#define S2_SESSION_H
#include "s2_limits.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define S2_SESSION_TOKEN_SIZE 257u
#define S2_SESSION_MESSAGE_VERSION "v1.0.0"
#define S2_SESSION_PHASE_MS 15000u
#define S2_SESSION_COMM_MS 30000u

typedef enum { S2_SESSION_IDLE=0, S2_SESSION_INITIATE, S2_SESSION_CONFIRM,
    S2_SESSION_READY, S2_SESSION_CONSUMED, S2_SESSION_FAILED } S2_SESSION_STATE;
typedef enum { S2_SESSION_OK=0, S2_SESSION_INVALID, S2_SESSION_TLS_ERROR,
    S2_SESSION_HTTP_ERROR, S2_SESSION_AUTH_ERROR, S2_SESSION_STORAGE_ERROR,
    S2_SESSION_EXPIRED, S2_SESSION_UNPAIRED } S2_SESSION_RESULT;
typedef struct { size_t count; char token[2][S2_SESSION_TOKEN_SIZE]; } S2_SESSION_TOKENS;
typedef struct {
    void *user;
    /* Synchronous ATOMIC DURABLE replacement. Copy bytes before returning.
     * On false, storage must retain an intact previous OR complete new snapshot
     * (commit may be uncertain); never a partial set. No reentry. */
    bool (*persist)(void *user,const S2_SESSION_TOKENS *tokens);
} S2_SESSION_STORAGE;
typedef struct {
    char hostname[254], path[S2_SESSION_URL_SIZE], commToken[S2_SESSION_TOKEN_SIZE];
    uint16_t port;
    uint32_t lifetimeMs;
} S2_SESSION_DETAILS;
typedef struct {
    const char *method;
    char url[640], bearer[S2_SESSION_TOKEN_SIZE], body[512];
} S2_SESSION_REQUEST;
typedef struct {
    S2_SESSION_STATE state;
    S2_SESSION_RESULT error;
    S2_SESSION_STORAGE storage;
    S2_SESSION_TOKENS tokens;
    char base[S2_SESSION_URL_SIZE], client[37], server[37];
    S2_SESSION_DETAILS details;
    size_t tryToken;
    bool requestIssued;
    uint32_t phaseStart, requestStart, commStart;
} S2_SESSION;

/* Zero-initialize/Clear a context before first use. Main-loop, non-reentrant.
 * Requires real successful pairing and selected session API major version v1.
 * apiBase includes /v1, e.g. https://cem.local/connection/v1.
 * This engine builds/consumes JSON; it does NOT perform HTTP, TLS or storage. */
/* Validate selected /v1 HTTPS API and paired UUIDs without reading tokens or
 * starting a deadline. Output hostname (254 bytes)/port cleared on failure. */
bool S2_SESSION_Configuration(const char *apiBase,const char *clientNodeId,
    const char *serverNodeId,char hostname[254],uint16_t *port);
S2_SESSION_RESULT S2_SESSION_Begin(S2_SESSION *session,const S2_SESSION_STORAGE *storage,
    const char *apiBase,const char *clientNodeId,const char *serverNodeId,
    const S2_SESSION_TOKENS *savedTokens,uint32_t nowMs);
S2_SESSION_RESULT S2_SESSION_Request(S2_SESSION *session,S2_SESSION_REQUEST *request,uint32_t nowMs);
/* Complete decoded HTTP body, length EXCLUDES NUL. No redirects/plaintext.
 * tlsVerified must come from the authenticated HTTPS transport, not a setting. */
S2_SESSION_RESULT S2_SESSION_Response(S2_SESSION *session,unsigned httpStatus,
    const char *json,size_t length,bool tlsVerified,uint32_t nowMs);
void S2_SESSION_Tasks(S2_SESSION *session,uint32_t nowMs);
/* Abort pending I/O without discarding the recoverable access-token set. */
void S2_SESSION_Abort(S2_SESSION *session,S2_SESSION_RESULT reason);
/* Single-use export; lifetime is conservatively measured from confirm request.
 * Caller must resolve hostname, bind trust, hand to APP_S2_ProvideSession, then
 * erase this output. Current profile accepts same host+port as the HTTPS API. */
S2_SESSION_RESULT S2_SESSION_TakeDetails(S2_SESSION *session,S2_SESSION_DETAILS *out,uint32_t nowMs);
void S2_SESSION_Clear(S2_SESSION *session);
#ifdef __cplusplus
}
#endif
#endif
