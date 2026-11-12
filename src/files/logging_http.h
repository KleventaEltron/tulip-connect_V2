#ifndef LOGGING_HTTP_H
#define LOGGING_HTTP_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define LOG_HTTP_IO_BUDGET 512u
#define LOG_HTTP_STALL_MS 15000u
typedef int (*LOG_HTTP_PRODUCER)(void *,const uint8_t **,size_t *);
typedef int (*LOG_HTTP_WRITE)(void *,const uint8_t *,size_t);
typedef enum { LOG_TX_IDLE=0,LOG_TX_HEADER,LOG_TX_NEXT,LOG_TX_SIZE,
    LOG_TX_BODY,LOG_TX_CRLF,LOG_TX_FINAL,LOG_TX_DONE,LOG_TX_FAILED } LOG_TX_STATE;
typedef struct {
    LOG_TX_STATE state;
    const uint8_t *data;
    size_t length,offset;
    char framing[24];
    LOG_HTTP_PRODUCER produce;
    void *user;
    uint32_t progress;
    bool chunked;
} LOG_HTTP_TX;
bool LOG_HTTP_TxBegin(LOG_HTTP_TX *,const char *,size_t,LOG_HTTP_PRODUCER,void *,uint32_t);
void LOG_HTTP_TxTasks(LOG_HTTP_TX *,LOG_HTTP_WRITE,void *,uint32_t);
void LOG_HTTP_TxReset(LOG_HTTP_TX *);
typedef enum { LOG_HTTP_MORE=0,LOG_HTTP_DONE,LOG_HTTP_BAD } LOG_HTTP_RESULT;
typedef struct { unsigned status;size_t bodyOffset,bodyLength;bool reusable,chunked; } LOG_HTTP_RESPONSE;
/* NUL-terminated raw response with counted length. On DONE compacts decoded
 * body to buffer[0], adds NUL; caller must not feed/parse that buffer again.
 * Bounds, case-insensitive headers, CL/chunks/trailers/EOF and 2xx semantics
 * are separated: DONE also reports non-2xx; owner must reject before applying. */
LOG_HTTP_RESULT LOG_HTTP_Response(char *,size_t,bool,LOG_HTTP_RESPONSE *);
#endif
