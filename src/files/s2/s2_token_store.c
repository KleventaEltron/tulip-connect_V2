#include "s2_token_store.h"
#include <string.h>
#define CRC_OFFSET 559u
#define COMMIT_OFFSET 567u
#define COMMIT 0xa5u
static void wipe(void *p,size_t n) { volatile uint8_t *b=p; while(n--) *b++=0; }
static uint32_t get32(const uint8_t *p) {
    return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}
static void put32(uint8_t *p,uint32_t n) {
    unsigned i; for(i=0;i<4;i++) p[i]=(uint8_t)(n>>(8*i));
}
static uint32_t crc(const uint8_t *p,size_t n) {
    uint32_t c=0xffffffffu; unsigned j;
    while(n--) { c^=*p++; for(j=0;j<8;j++) c=(c>>1)^(0xedb88320u & (0u-(c&1u))); }
    return ~c;
}
static bool valid(const S2_TOKEN_STORE *s,const uint8_t *b) {
    unsigned i; size_t j;
    if(memcmp(b,"S2T1",4)||memcmp(b+8,s->binding,32)||b[40]>2||
       b[COMMIT_OFFSET]!=COMMIT||get32(b+CRC_OFFSET)!=crc(b,CRC_OFFSET)) return false;
    for(i=0;i<b[40];i++) {
        const uint8_t *p=b+41+i*S2_SESSION_TOKEN_SIZE;
        for(j=0;j<S2_SESSION_TOKEN_SIZE && p[j];j++) {}
        if(j==0||j==S2_SESSION_TOKEN_SIZE) return false;
    }
    return true;
}
/* Return newest slot; ambiguous equal/half-range sequences fail closed. */
static int newest(bool a,bool b,const uint8_t *x,const uint8_t *y) {
    uint32_t d;
    if(!a) return b?1:-1;
    if(!b) return 0;
    d=get32(x+4)-get32(y+4);
    if(d==0||d==0x80000000u) return -2;
    return d<0x80000000u?0:1;
}
bool S2_TOKEN_STORE_Init(S2_TOKEN_STORE *s,const S2_TOKEN_STORE_IO *io,const uint8_t binding[32]) {
    if(!s||!io||!binding||!io->read||!io->write||!io->sync) return false;
    s->io=*io; memcpy(s->binding,binding,32); return true;
}
static S2_TOKEN_STORE_RESULT readSlots(S2_TOKEN_STORE *s,uint8_t b[2][S2_TOKEN_STORE_SLOT_SIZE],int *latest) {
    if(!s||!s->io.read||!s->io.write||!s->io.sync) return S2_TOKEN_STORE_INVALID;
    if(!s->io.read(s->io.user,0,0,b[0],S2_TOKEN_STORE_SLOT_SIZE)||
       !s->io.read(s->io.user,1,0,b[1],S2_TOKEN_STORE_SLOT_SIZE)) return S2_TOKEN_STORE_IO_ERROR;
    *latest=newest(valid(s,b[0]),valid(s,b[1]),b[0],b[1]);
    if(*latest==-1) {
        size_t i; unsigned slot;
        /* Unknown pairing/corruption is not an empty provisionable store. */
        for(slot=0;slot<2;slot++) for(i=0;i<S2_TOKEN_STORE_SLOT_SIZE;i++)
            if(b[slot][i]!=0xffu) return S2_TOKEN_STORE_INVALID;
    }
    return *latest==-2?S2_TOKEN_STORE_INVALID:(*latest<0?S2_TOKEN_STORE_EMPTY:S2_TOKEN_STORE_OK);
}
S2_TOKEN_STORE_RESULT S2_TOKEN_STORE_Load(S2_TOKEN_STORE *s,S2_SESSION_TOKENS *out) {
    uint8_t b[2][S2_TOKEN_STORE_SLOT_SIZE]; int latest=-1;
    S2_TOKEN_STORE_RESULT r;
    if(!out) return S2_TOKEN_STORE_INVALID;
    wipe(out,sizeof(*out)); r=readSlots(s,b,&latest);
    if(r==S2_TOKEN_STORE_OK) {
        out->count=b[latest][40]; memcpy(out->token,b[latest]+41,sizeof(out->token));
    }
    wipe(b,sizeof(b)); return r;
}
bool S2_TOKEN_STORE_Persist(void *user,const S2_SESSION_TOKENS *tokens) {
    S2_TOKEN_STORE *s=user; uint8_t b[2][S2_TOKEN_STORE_SLOT_SIZE];
    uint8_t zero=0,marker=COMMIT; int latest=-1; unsigned target,i; size_t n;
    uint32_t generation; bool ok=false; S2_TOKEN_STORE_RESULT r;
    if(!tokens||tokens->count>2) return false;
    for(i=0;i<tokens->count;i++) {
        for(n=0;n<S2_SESSION_TOKEN_SIZE&&tokens->token[i][n];n++) {}
        if(!n||n==S2_SESSION_TOKEN_SIZE) return false;
    }
    r=readSlots(s,b,&latest);
    if(r!=S2_TOKEN_STORE_OK&&r!=S2_TOKEN_STORE_EMPTY) goto done;
    target=latest==0?1u:0u; generation=latest<0?0u:get32(b[latest]+4)+1u;
    memset(b[target],0,S2_TOKEN_STORE_SLOT_SIZE);
    memcpy(b[target],"S2T1",4); put32(b[target]+4,generation);
    memcpy(b[target]+8,s->binding,32); b[target][40]=(uint8_t)tokens->count;
    for(i=0;i<tokens->count;i++) {
        n=strlen(tokens->token[i]); memcpy(b[target]+41+i*S2_SESSION_TOKEN_SIZE,tokens->token[i],n);
    }
    put32(b[target]+CRC_OFFSET,crc(b[target],CRC_OFFSET));
    /* Invalidate old target durably BEFORE rewriting any of its payload. */
    if(!s->io.write(s->io.user,target,COMMIT_OFFSET,&zero,1)||!s->io.sync(s->io.user)||
       !s->io.write(s->io.user,target,0,b[target],COMMIT_OFFSET)||!s->io.sync(s->io.user)||
       !s->io.write(s->io.user,target,COMMIT_OFFSET,&marker,1)||!s->io.sync(s->io.user)) goto done;
    b[target][COMMIT_OFFSET]=marker;
    /* Confirm readable committed contents; an error here leaves commit uncertain. */
    if(!s->io.read(s->io.user,target,0,b[1u-target],S2_TOKEN_STORE_SLOT_SIZE)) goto done;
    ok=memcmp(b[target],b[1u-target],S2_TOKEN_STORE_SLOT_SIZE)==0;
done:
    wipe(b,sizeof(b)); return ok;
}
void S2_TOKEN_STORE_Clear(S2_TOKEN_STORE *s) { if(s) wipe(s,sizeof(*s)); }
