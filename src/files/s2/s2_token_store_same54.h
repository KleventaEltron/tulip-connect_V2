#ifndef S2_TOKEN_STORE_SAME54_H
#define S2_TOKEN_STORE_SAME54_H
#include "s2_token_store.h"
#include "../eeprom.h"
/* Reserved logical layout; initialization does not read/write token bytes.
 * Caller must establish exclusive ownership and bootloader preservation of
 * [1024,2048) and [2048,3072) before using this adapter. No startup calls exist.
 * Different logical pages do not prove isolation during sector reallocation;
 * qualify silicon revision, brownout and power-cut recovery before deployment. */
#define S2_SEE_SLOT0 SEEP_ADDR_S2_TOKEN_SLOT0
#define S2_SEE_SLOT1 SEEP_ADDR_S2_TOKEN_SLOT1
#define S2_SEE_SLOT_SPAN SEEP_S2_TOKEN_SLOT_SPAN
#define S2_SEE_TIMEOUT_MS 1000u
#define S2_SEE_POLL_LIMIT 1000000u
typedef enum { S2_SEE_OK=0, S2_SEE_ARGUMENT, S2_SEE_CONFIGURATION,
    S2_SEE_LOCKED, S2_SEE_CONTROLLER_ERROR, S2_SEE_TIMEOUT,
    S2_SEE_VERIFY_ERROR, S2_SEE_DISABLED } S2_SEE_ERROR;
typedef struct { bool initialized; S2_SEE_ERROR error; } S2_TOKEN_STORE_SAME54;
/* Read-only initialization. Supports only SBLK=1, PSZ=3 (4 KB virtual),
 * automatic reallocation enabled, unlocked data and active SYS_TIME.
 * On failure clears output IO. Main-loop only, no interrupt/other NVM writer
 * may use the controller concurrently. No mode/fuse/flash-layout changes. */
bool S2_TOKEN_STORE_SAME54_Init(S2_TOKEN_STORE_SAME54 *, S2_TOKEN_STORE_IO *);
void S2_TOKEN_STORE_SAME54_Disable(S2_TOKEN_STORE_SAME54 *);
#endif
