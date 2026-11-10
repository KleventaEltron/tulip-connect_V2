#ifndef _WOLFCRYPT_REQUIRED_CONFIG_H_
#define _WOLFCRYPT_REQUIRED_CONFIG_H_

#include "configuration.h"
#include "files/crypto_random.h"

/* SAME54 TRNG seed hook; replaces wolfSSL's PIC32 timer/rand fallback. */
#define CUSTOM_RAND_GENERATE_SEED TULIP_CryptoSeed

#include <stddef.h>

#endif