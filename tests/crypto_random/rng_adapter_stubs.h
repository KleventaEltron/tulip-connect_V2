#include <stddef.h>
#include <stdint.h>
#include <string.h>
typedef int CRYPT_RNG_CTX;
CRYPT_RNG_CTX *_SYS_RANDOM_CryptoLock(void);
void _SYS_RANDOM_CryptoUnlock(void);
int CRYPT_RNG_BlockGenerate(CRYPT_RNG_CTX *, uint8_t *, unsigned int);
size_t SYS_RANDOM_CryptoBlockGet(void *, size_t);
