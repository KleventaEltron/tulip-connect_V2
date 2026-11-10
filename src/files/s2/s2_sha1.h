#ifndef S2_SHA1_H
#define S2_SHA1_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* RFC6455 WebSocket accept calculation ONLY. Not for signatures, password
 * storage, token authentication or TLS certificate verification.
 * Matches the S2_WS_PLATFORM/S2_TRANSPORT_PLATFORM sha1 callback signature.
 * user is ignored. NULL data is valid only for zero length. No heap/globals. */
bool S2_SHA1_WebSocketAccept(void *user, const uint8_t *data, size_t length, uint8_t out[20]);
#ifdef __cplusplus
}
#endif
#endif
