#include "crypto_random.h"
#include <files/crypto_random_hw.h>
#include <string.h>

#define CRYPTO_SEED_MAX_BYTES 1024u
#define CRYPTO_SEED_POLL_LIMIT 100000u

int TULIP_CryptoSeed(uint8_t *out, size_t length)
{
    size_t written = 0;
    unsigned polls = CRYPTO_SEED_POLL_LIMIT;
    if ((out == NULL && length != 0) || length > CRYPTO_SEED_MAX_BYTES) return -1;
    if (length == 0) return 0;
    TULIP_TRNG_Enable();
    while (written < length) {
        uint32_t word;
        unsigned byte;
        while (!TULIP_TRNG_Ready()) {
            if (--polls == 0) {
                memset(out, 0, length);
                return -1;
            }
        }
        word = TULIP_TRNG_Read();
        for (byte = 0; byte < 4 && written < length; byte++) {
            out[written++] = (uint8_t)word;
            word >>= 8;
        }
    }
    return 0;
}

bool TULIP_CryptoRandom(void *user, uint8_t *out, size_t length)
{
    (void)user;
    return TULIP_CryptoSeed(out, length) == 0;
}
