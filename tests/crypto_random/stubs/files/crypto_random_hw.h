#ifndef TULIP_CRYPTO_RANDOM_HW_TEST_H
#define TULIP_CRYPTO_RANDOM_HW_TEST_H
#include <stdbool.h>
#include <stdint.h>
void TULIP_TRNG_Enable(void);
bool TULIP_TRNG_Ready(void);
uint32_t TULIP_TRNG_Read(void);
#endif
