#include "s2_token_store_same54.h"
#include <files/s2/s2_token_store_same54_hw.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static S2_SEE_HW_STATE hw;
static uint8_t cache[4096],disk[4096];
static uint64_t ticks;
static uint32_t hz;
static unsigned writes,flushes,delay;
static bool buffered,stuck,freezeClock,ignoreWrite,flushError;
static int cut;
S2_SEE_HW_STATE S2_SEE_HW_State(void) {
    if(delay&&!stuck&&!--delay) {
        hw.busy=false; hw.ready=true; hw.complete=true;
    }
    return hw;
}
uint64_t S2_SEE_HW_Now(void) { if(!freezeClock) ticks++; return ticks; }
uint32_t S2_SEE_HW_Frequency(void) { return hz; }
uint8_t S2_SEE_HW_Read(size_t at) { assert(at<sizeof(cache)); return cache[at]; }
void S2_SEE_HW_ClearComplete(void) { hw.complete=false; }
void S2_SEE_HW_Write(size_t at,uint8_t value) {
    assert((at>=1024&&at<1592)||(at>=2048&&at<2616)); writes++;
    if(cut==0) { hw.controllerError=true; return; }
    if(cut>0) cut--;
    if(!ignoreWrite) { cache[at]=value; if(!buffered) disk[at]=value; }
    hw.loaded=buffered; hw.busy=true; hw.ready=false; delay=2;
}
void S2_SEE_HW_Flush(void) {
    flushes++;
    if(flushError) { hw.controllerError=true; return; }
    if(!stuck) { memcpy(disk,cache,sizeof(disk)); hw.loaded=false; }
    hw.busy=true; hw.ready=false; delay=2;
    /* Deliberately don't rely on SEEWRC as flush completion. */
}
static void reset(void) {
    memset(&hw,0,sizeof(hw)); hw.configured=true; hw.ready=true;
    memset(cache,255,sizeof(cache)); memcpy(disk,cache,sizeof(disk));
    hz=1000000; ticks=0; writes=flushes=delay=0;
    buffered=stuck=freezeClock=ignoreWrite=flushError=false; cut=-1;
}
static void powerCycle(void) {
    memcpy(cache,disk,sizeof(cache)); hw.controllerError=false; hw.busy=false;
    hw.loaded=false; hw.complete=false; hw.ready=true; delay=0; cut=-1;
}
static void bind(S2_TOKEN_STORE *store,S2_TOKEN_STORE_SAME54 *adapter,S2_TOKEN_STORE_IO *io) {
    uint8_t identity[32]={1};
    assert(S2_TOKEN_STORE_SAME54_Init(adapter,io));
    assert(S2_TOKEN_STORE_Init(store,io,identity));
}
int main(void) {
    S2_TOKEN_STORE_SAME54 a; S2_TOKEN_STORE_IO io; S2_TOKEN_STORE store;
    S2_SESSION_TOKENS old={0},next={0},out; uint8_t byte=7; unsigned mode; int n;
    uint8_t baseline[4096];
    reset(); hw.configured=false; assert(!S2_TOKEN_STORE_SAME54_Init(&a,&io)); assert(a.error==S2_SEE_CONFIGURATION&&!io.write&&writes==0);
    reset(); hw.locked=true; assert(!S2_TOKEN_STORE_SAME54_Init(&a,&io)); assert(a.error==S2_SEE_LOCKED);
    reset(); hz=0; assert(!S2_TOKEN_STORE_SAME54_Init(&a,&io));
    reset(); hw.controllerError=true; assert(!S2_TOKEN_STORE_SAME54_Init(&a,&io)); assert(hw.controllerError);
    reset(); hw.busy=true; hw.ready=false; assert(!S2_TOKEN_STORE_SAME54_Init(&a,&io)); assert(a.error==S2_SEE_TIMEOUT);
    reset(); hw.busy=true; freezeClock=true; assert(!S2_TOKEN_STORE_SAME54_Init(&a,&io)); assert(a.error==S2_SEE_TIMEOUT);
    reset(); assert(S2_TOKEN_STORE_SAME54_Init(&a,&io)); assert(!writes&&!flushes);
    assert(!io.write(io.user,2,0,&byte,1)); assert(!io.write(io.user,0,SIZE_MAX,&byte,1));
    assert(!io.read(io.user,0,568,&byte,1)); assert(!io.write(io.user,0,0,NULL,1)); assert(!writes);
    ignoreWrite=true; assert(!io.write(io.user,0,0,&byte,1)); assert(a.error==S2_SEE_VERIFY_ERROR);
    reset(); assert(S2_TOKEN_STORE_SAME54_Init(&a,&io)); stuck=true;
    assert(!io.write(io.user,0,0,&byte,1)); assert(a.error==S2_SEE_TIMEOUT);
    reset(); assert(S2_TOKEN_STORE_SAME54_Init(&a,&io)); buffered=true;
    assert(io.write(io.user,0,0,&byte,1)); assert(disk[1024]==255&&hw.loaded);
    flushError=true; assert(!io.sync(io.user)); assert(a.error==S2_SEE_CONTROLLER_ERROR);
    old.count=1; strcpy(old.token[0],"old"); next.count=2; strcpy(next.token[0],"old"); strcpy(next.token[1],"pending");
    for(mode=0;mode<2;mode++) {
        reset(); buffered=mode!=0; bind(&store,&a,&io);
        assert(S2_TOKEN_STORE_Persist(&store,&old)); memcpy(baseline,disk,sizeof(disk));
        for(n=0;n<=570;n++) {
            reset(); buffered=mode!=0; memcpy(disk,baseline,sizeof(disk)); powerCycle();
            bind(&store,&a,&io); cut=n;
            (void)S2_TOKEN_STORE_Persist(&store,&next); powerCycle(); bind(&store,&a,&io);
            assert(S2_TOKEN_STORE_Load(&store,&out)==S2_TOKEN_STORE_OK);
            assert((out.count==1&&!strcmp(out.token[0],"old"))||
                   (out.count==2&&!strcmp(out.token[0],"old")&&!strcmp(out.token[1],"pending")));
            assert(cache[0]==255&&cache[1023]==255&&cache[1592]==255&&cache[2047]==255&&cache[2616]==255&&cache[4095]==255);
        }
    }
    reset(); assert(S2_TOKEN_STORE_SAME54_Init(&a,&io)); assert(io.write(io.user,0,0,&byte,1)); n=(int)writes;
    assert(io.write(io.user,0,0,&byte,1)); assert(writes==(unsigned)n);
    S2_TOKEN_STORE_SAME54_Disable(&a); assert(!io.write(io.user,0,0,&byte,1)); assert(a.error==S2_SEE_DISABLED);
    puts("SAME54 adapter: buffered/unbuffered restart injection, configuration, errors, timeouts, readback, range isolation and wear checks passed.");
    return 0;
}
