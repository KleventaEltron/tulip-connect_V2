#ifndef S2_TOKEN_STORE_SAME54_HW_H
#define S2_TOKEN_STORE_SAME54_HW_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef struct {
    bool configured,locked,busy,loaded,ready,complete;
    bool controllerError;
} S2_SEE_HW_STATE;
S2_SEE_HW_STATE S2_SEE_HW_State(void);
uint64_t S2_SEE_HW_Now(void);
uint32_t S2_SEE_HW_Frequency(void);
uint8_t S2_SEE_HW_Read(size_t);
void S2_SEE_HW_Write(size_t,uint8_t);
void S2_SEE_HW_ClearComplete(void);
void S2_SEE_HW_Flush(void);
#endif
