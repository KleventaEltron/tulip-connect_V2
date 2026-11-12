#ifndef S2_TEST_MESSAGES_H
#define S2_TEST_MESSAGES_H
#include "s2_limits.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define S2_TEST_MESSAGE_MAX 1024u
#define S2_TEST_VERSION "v1.0.0"
/* Diagnostic simulated resource only. No control or settings dependencies. */
typedef struct {
    bool active,failed,cemHandshake,negotiated,handshakeAck,detailsAck,selected,measurementAck,rejectedControl;
    uint32_t received,sent,rejected;
    char resource[65],handshakeId[37],detailsId[37],measurementId[37],cemId[65],responseId[65];
    char queue[S2_TEST_QUEUE_DEPTH][S2_TEST_OUTPUT_MAX];
    size_t head,count;
    bool (*random)(void *,uint8_t *,size_t);
} S2_TEST_MESSAGES;
bool S2_TEST_Start(S2_TEST_MESSAGES *,const char *resourceId,bool (*random)(void *,uint8_t *,size_t));
/* Complete UTF-8 text from the WebSocket callback; queues replies only. */
bool S2_TEST_Receive(void *,const char *,size_t);
const char *S2_TEST_Peek(const S2_TEST_MESSAGES *);
void S2_TEST_Sent(S2_TEST_MESSAGES *);
bool S2_TEST_Measurement(S2_TEST_MESSAGES *,const char *utcTimestamp);
void S2_TEST_Reset(S2_TEST_MESSAGES *);
#endif
