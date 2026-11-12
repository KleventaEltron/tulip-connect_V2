#include "s2_token_store_same54.h"
#include <files/s2/s2_token_store_same54_hw.h>
#include <string.h>
/* Slots must remain separate at the largest possible logical page size (512).
 * Runtime profile below is deliberately restricted to the existing 4 KB setup. */
typedef char layout_check[(S2_SEE_SLOT0>=512u && S2_SEE_SLOT0%512u==0u &&
    S2_SEE_SLOT1%512u==0u && S2_SEE_SLOT_SPAN>=S2_TOKEN_STORE_SLOT_SIZE &&
    S2_SEE_SLOT0+S2_SEE_SLOT_SPAN<=S2_SEE_SLOT1 &&
    S2_SEE_SLOT1+S2_SEE_SLOT_SPAN<=SEEP_FINAL_BYTE_INDEX+1u)?1:-1];
typedef struct { uint64_t start,limit; unsigned polls; } DEADLINE;
static bool fail(S2_TOKEN_STORE_SAME54 *s,S2_SEE_ERROR error) {
    s->error=error; return false;
}
static bool begin(S2_TOKEN_STORE_SAME54 *s,DEADLINE *d) {
    uint32_t hz;
    if(!s||!s->initialized) { if(s) s->error=S2_SEE_DISABLED; return false; }
    hz=S2_SEE_HW_Frequency();
    if(!hz) return fail(s,S2_SEE_CONFIGURATION);
    s->error=S2_SEE_OK; d->start=S2_SEE_HW_Now();
    d->limit=((uint64_t)hz*S2_SEE_TIMEOUT_MS+999u)/1000u; d->polls=0;
    return true;
}
static bool waitReady(S2_TOKEN_STORE_SAME54 *s,DEADLINE *d,bool complete,bool durable) {
    for(;;) {
        S2_SEE_HW_STATE st=S2_SEE_HW_State();
        if(!st.configured) return fail(s,S2_SEE_CONFIGURATION);
        if(st.locked) return fail(s,S2_SEE_LOCKED);
        if(st.controllerError) return fail(s,S2_SEE_CONTROLLER_ERROR);
        if(++d->polls>S2_SEE_POLL_LIMIT||S2_SEE_HW_Now()-d->start>=d->limit)
            return fail(s,S2_SEE_TIMEOUT);
        if(!st.busy&&st.ready&&(!complete||st.complete)&&(!durable||!st.loaded)) return true;
    }
}
static bool range(S2_TOKEN_STORE_SAME54 *s,unsigned slot,size_t offset,size_t length) {
    if(slot>1||offset>S2_TOKEN_STORE_SLOT_SIZE||length>S2_TOKEN_STORE_SLOT_SIZE-offset)
        return fail(s,S2_SEE_ARGUMENT);
    return true;
}
static size_t address(unsigned slot,size_t offset) { return (slot?S2_SEE_SLOT1:S2_SEE_SLOT0)+offset; }
static bool readBytes(void *user,unsigned slot,size_t offset,void *data,size_t length) {
    S2_TOKEN_STORE_SAME54 *s=user; DEADLINE d; size_t i;
    if(!begin(s,&d)) return false;
    if((!data&&length)||!range(s,slot,offset,length)) return fail(s,S2_SEE_ARGUMENT);
    for(i=0;i<length;i++) {
        if(!waitReady(s,&d,false,false)) return false;
        ((uint8_t *)data)[i]=S2_SEE_HW_Read(address(slot,offset+i));
    }
    return true;
}
static bool writeBytes(void *user,unsigned slot,size_t offset,const void *data,size_t length) {
    S2_TOKEN_STORE_SAME54 *s=user; DEADLINE d; size_t i;
    if(!begin(s,&d)) return false;
    if((!data&&length)||!range(s,slot,offset,length)) return fail(s,S2_SEE_ARGUMENT);
    for(i=0;i<length;i++) {
        size_t at=address(slot,offset+i); uint8_t value=((const uint8_t *)data)[i];
        if(!waitReady(s,&d,false,false)) return false;
        if(S2_SEE_HW_Read(at)==value) continue; /* Avoid unnecessary wear. */
        S2_SEE_HW_ClearComplete(); S2_SEE_HW_Write(at,value);
        if(!waitReady(s,&d,true,false)) return false;
        if(S2_SEE_HW_Read(at)!=value) return fail(s,S2_SEE_VERIFY_ERROR);
    }
    return true;
}
static bool syncBytes(void *user) {
    S2_TOKEN_STORE_SAME54 *s=user; DEADLINE d; S2_SEE_HW_STATE st;
    if(!begin(s,&d)||!waitReady(s,&d,false,false)) return false;
    st=S2_SEE_HW_State();
    if(st.loaded) {
        /* SEEWRC acknowledges AHB writes, not a new completion requirement
         * for SEEFLUSH. Require drained LOAD, no BUSY and command-ready. */
        S2_SEE_HW_Flush();
        return waitReady(s,&d,false,true);
    }
    return waitReady(s,&d,false,true);
}
bool S2_TOKEN_STORE_SAME54_Init(S2_TOKEN_STORE_SAME54 *s,S2_TOKEN_STORE_IO *io) {
    DEADLINE d;
    if(io) memset(io,0,sizeof(*io));
    if(!s) return false;
    memset(s,0,sizeof(*s));
    if(!io) return fail(s,S2_SEE_ARGUMENT);
    s->initialized=true;
    if(!begin(s,&d)||!waitReady(s,&d,false,false)) { s->initialized=false; return false; }
    io->user=s; io->read=readBytes; io->write=writeBytes; io->sync=syncBytes;
    return true;
}
void S2_TOKEN_STORE_SAME54_Disable(S2_TOKEN_STORE_SAME54 *s) {
    if(s) { s->initialized=false; s->error=S2_SEE_DISABLED; }
}
