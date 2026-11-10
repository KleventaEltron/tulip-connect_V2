#ifndef TULIP_CRYPTO_RANDOM_H
#define TULIP_CRYPTO_RANDOM_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Hardware entropy for wolfSSL's DRBG seed/reseed hook: 0 success, -1 failure.
 * Main-loop only, non-reentrant. No heap, interrupts, timer or rand() fallback.
 * NULL is valid only for length 0. Up to 1024 bytes per bounded call.
 * On a hardware timeout, the entire requested output is cleared. */
int TULIP_CryptoSeed(uint8_t *out, size_t length);
/* Callback for S2 WebSocket keys/masks. user is ignored. */
bool TULIP_CryptoRandom(void *user, uint8_t *out, size_t length);
#ifdef __cplusplus
}
#endif
#endif
