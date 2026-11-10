#ifndef TULIP_CRYPTO_RANDOM_HW_H
#define TULIP_CRYPTO_RANDOM_HW_H
#include <xc.h>
#include <stdbool.h>
#include <stdint.h>
/* SAME54 DFP register names; DATA read acknowledges DATARDY automatically.
 * Keep the peripheral enabled: repeated seed requests must not restart it. */
static inline void TULIP_TRNG_Enable(void)
{
    MCLK_REGS->MCLK_APBCMASK |= MCLK_APBCMASK_TRNG_Msk;
    TRNG_REGS->TRNG_CTRLA |= TRNG_CTRLA_ENABLE_Msk;
}
static inline bool TULIP_TRNG_Ready(void)
{
    return (TRNG_REGS->TRNG_INTFLAG & TRNG_INTFLAG_DATARDY_Msk) != 0;
}
static inline uint32_t TULIP_TRNG_Read(void)
{
    return TRNG_REGS->TRNG_DATA;
}
#endif
