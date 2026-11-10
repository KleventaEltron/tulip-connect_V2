#include "crypto_random.h"
#include "rng_adapter_stubs.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned enabled, polls, words;
static unsigned delay, failAfter;
static bool neverReady;
void TULIP_TRNG_Enable(void) { enabled++; }
bool TULIP_TRNG_Ready(void)
{
    polls++;
    if (neverReady || words >= failAfter) return false;
    if (delay != 0) { delay--; return false; }
    return true;
}
uint32_t TULIP_TRNG_Read(void)
{
    assert(!neverReady && delay == 0 && words < failAfter);
    return 0x03020100u + 0x04040404u * words++;
}
static void reset(void)
{
    enabled = polls = words = delay = 0;
    failAfter = 10000; neverReady = false;
}
static CRYPT_RNG_CTX context;
static bool lockFails, generateFails;
static unsigned unlocks;
CRYPT_RNG_CTX *_SYS_RANDOM_CryptoLock(void) { return lockFails ? NULL : &context; }
void _SYS_RANDOM_CryptoUnlock(void) { unlocks++; }
int CRYPT_RNG_BlockGenerate(CRYPT_RNG_CTX *ctx, uint8_t *out, unsigned int length)
{
    assert(ctx == &context);
    memset(out,0x5a,length);
    return generateFails ? -1 : 0;
}
int main(void)
{
    uint8_t buffer[1026];
    reset();
    assert(TULIP_CryptoSeed(NULL,0) == 0 && enabled == 0);
    assert(TULIP_CryptoSeed(NULL,1) == -1 && enabled == 0);
    assert(TULIP_CryptoSeed(buffer,1025) == -1 && enabled == 0);
    for (size_t length = 1; length <= 33; length++) {
        reset(); memset(buffer,0xa5,sizeof(buffer)); delay = 3;
        assert(TULIP_CryptoSeed(buffer + 1,length) == 0);
        assert(enabled == 1 && words == (length + 3) / 4 && polls == words + 3);
        for (size_t i = 0; i < length; i++) assert(buffer[1+i] == (uint8_t)i);
        assert(buffer[0] == 0xa5 && buffer[length+1] == 0xa5);
    }
    reset(); assert(TULIP_CryptoRandom(NULL,buffer,1024) && words == 256);
    reset(); neverReady = true; memset(buffer,0xa5,sizeof(buffer));
    assert(!TULIP_CryptoRandom(NULL,buffer + 1,16));
    assert(polls == 100000 && words == 0);
    for (unsigned i = 1; i <= 16; i++) assert(buffer[i] == 0);
    assert(buffer[0] == 0xa5 && buffer[17] == 0xa5);
    reset(); failAfter = 1; memset(buffer,0xa5,sizeof(buffer));
    assert(TULIP_CryptoSeed(buffer + 1,7) == -1 && words == 1 && polls == 100001);
    for (unsigned i = 1; i <= 7; i++) assert(buffer[i] == 0);
    assert(buffer[0] == 0xa5 && buffer[8] == 0xa5);
    reset(); assert(TULIP_CryptoSeed(buffer,16) == 0); /* Recovery requires fresh hardware bytes. */
    assert(SYS_RANDOM_CryptoBlockGet(buffer,16) == 16 && buffer[0] == 0x5a);
    generateFails = true;
    assert(SYS_RANDOM_CryptoBlockGet(buffer,16) == 0);
    for (unsigned i = 0; i < 16; i++) assert(buffer[i] == 0);
    lockFails = true;
    assert(SYS_RANDOM_CryptoBlockGet(buffer,16) == 0 && unlocks == 3);
    assert(SYS_RANDOM_CryptoBlockGet(NULL,16) == 0);
    assert(SYS_RANDOM_CryptoBlockGet(buffer,0) == 0 && unlocks == 3);
    puts("Hardware entropy readiness, bounds, timeout, partial-output clearing and recovery tests passed.");
    return 0;
}
