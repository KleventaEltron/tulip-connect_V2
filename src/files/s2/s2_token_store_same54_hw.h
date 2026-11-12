#ifndef S2_TOKEN_STORE_SAME54_HW_H
#define S2_TOKEN_STORE_SAME54_HW_H
#include <xc.h>
#include "system/time/sys_time.h"
typedef struct {
    bool configured,locked,busy,loaded,ready,complete;
    bool controllerError;
} S2_SEE_HW_STATE;
static inline S2_SEE_HW_STATE S2_SEE_HW_State(void) {
    uint32_t st=NVMCTRL_REGS->NVMCTRL_SEESTAT;
    uint16_t flags=NVMCTRL_REGS->NVMCTRL_INTFLAG;
    S2_SEE_HW_STATE s;
    s.configured=(st&(NVMCTRL_SEESTAT_SBLK_Msk|NVMCTRL_SEESTAT_PSZ_Msk))==
        (NVMCTRL_SEESTAT_SBLK(1u)|NVMCTRL_SEESTAT_PSZ(3u)) &&
        !(NVMCTRL_REGS->NVMCTRL_SEECFG&NVMCTRL_SEECFG_APRDIS_Msk);
    s.locked=(st&NVMCTRL_SEESTAT_LOCK_Msk)!=0;
    s.busy=(st&NVMCTRL_SEESTAT_BUSY_Msk)!=0;
    s.loaded=(st&NVMCTRL_SEESTAT_LOAD_Msk)!=0;
    s.ready=(NVMCTRL_REGS->NVMCTRL_STATUS&NVMCTRL_STATUS_READY_Msk)!=0;
    s.complete=(flags&NVMCTRL_INTFLAG_SEEWRC_Msk)!=0;
    s.controllerError=(flags&(NVMCTRL_INTFLAG_ADDRE_Msk|NVMCTRL_INTFLAG_ECCDE_Msk|
        NVMCTRL_INTFLAG_PROGE_Msk|NVMCTRL_INTFLAG_LOCKE_Msk|
        NVMCTRL_INTFLAG_NVME_Msk|NVMCTRL_INTFLAG_SEESOVF_Msk))!=0;
    return s;
}
static inline uint64_t S2_SEE_HW_Now(void) { return SYS_TIME_Counter64Get(); }
static inline uint32_t S2_SEE_HW_Frequency(void) { return SYS_TIME_FrequencyGet(); }
static inline uint8_t S2_SEE_HW_Read(size_t address) {
    return ((volatile const uint8_t *)SEEPROM_ADDR)[address];
}
static inline void S2_SEE_HW_Write(size_t address,uint8_t value) {
    ((volatile uint8_t *)SEEPROM_ADDR)[address]=value;
}
static inline void S2_SEE_HW_ClearComplete(void) {
    /* W1C only our completion flag: preserve existing users' error evidence. */
    NVMCTRL_REGS->NVMCTRL_INTFLAG=NVMCTRL_INTFLAG_SEEWRC_Msk;
}
static inline void S2_SEE_HW_Flush(void) {
    NVMCTRL_REGS->NVMCTRL_CTRLB=NVMCTRL_CTRLB_CMD_SEEFLUSH|NVMCTRL_CTRLB_CMDEX_KEY;
}
#endif
