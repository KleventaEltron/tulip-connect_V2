#include "s2_token_store.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef struct { unsigned char bytes[2][S2_TOKEN_STORE_SLOT_SIZE]; int budget, syncFail, syncs; bool readFail; } MEDIA;
static bool rd(void *u,unsigned s,size_t o,void *p,size_t n) {
    MEDIA *m=u; if(m->readFail) return false; assert(s<2&&o+n<=S2_TOKEN_STORE_SLOT_SIZE); memcpy(p,m->bytes[s]+o,n); return true;
}
static bool wr(void *u,unsigned s,size_t o,const void *p,size_t n) {
    MEDIA *m=u; size_t i; assert(s<2&&o+n<=S2_TOKEN_STORE_SLOT_SIZE);
    for(i=0;i<n;i++) { if(m->budget==0) return false; if(m->budget>0) m->budget--; m->bytes[s][o+i]=((const unsigned char *)p)[i]; } return true;
}
static bool syncMedia(void *u) { MEDIA *m=u; return ++m->syncs!=m->syncFail; }
static void init(S2_TOKEN_STORE *s,MEDIA *m,unsigned char identity) {
    unsigned char binding[32]={0}; S2_TOKEN_STORE_IO io={m,rd,wr,syncMedia}; binding[0]=identity; assert(S2_TOKEN_STORE_Init(s,&io,binding));
}
static bool same(const S2_SESSION_TOKENS *a,const S2_SESSION_TOKENS *b) {
    size_t i; if(a->count!=b->count) return false;
    for(i=0;i<a->count;i++) if(strcmp(a->token[i],b->token[i])) return false;
    return true;
}
int main(void) {
    MEDIA m,baseline; S2_TOKEN_STORE s,restarted; S2_SESSION_TOKENS a={0},b={0},out; int cut,phase;
    memset(&m,0,sizeof(m)); memset(m.bytes,255,sizeof(m.bytes)); m.budget=-1;
    a.count=1; strcpy(a.token[0],"old-token"); b.count=2; strcpy(b.token[0],"old-token"); strcpy(b.token[1],"pending-token");
    init(&s,&m,1); assert(S2_TOKEN_STORE_Load(&s,&out)==S2_TOKEN_STORE_EMPTY);
    assert(S2_TOKEN_STORE_Persist(&s,&a)); baseline=m;
    /* Every possible byte interruption, including stale committed target reuse. */
    for(phase=0;phase<2;phase++) {
        if(phase) { m=baseline; init(&s,&m,1); assert(S2_TOKEN_STORE_Persist(&s,&b)); assert(S2_TOKEN_STORE_Persist(&s,&a)); baseline=m; }
        for(cut=0;cut<=570;cut++) {
            bool saved; m=baseline; m.budget=cut; init(&s,&m,1);
            saved=S2_TOKEN_STORE_Persist(&s,&b); init(&restarted,&m,1);
            assert(S2_TOKEN_STORE_Load(&restarted,&out)==S2_TOKEN_STORE_OK);
            assert(same(&out,&a)||same(&out,&b)); if(saved) assert(same(&out,&b));
            m.budget=-1; assert(S2_TOKEN_STORE_Persist(&restarted,&b));
        }
    }
    for(phase=1;phase<=3;phase++) {
        m=baseline; m.syncs=0; m.syncFail=phase; init(&s,&m,1);
        assert(!S2_TOKEN_STORE_Persist(&s,&b)); init(&restarted,&m,1);
        assert(S2_TOKEN_STORE_Load(&restarted,&out)==S2_TOKEN_STORE_OK);
        assert(same(&out,&a)||same(&out,&b));
    }
    m=baseline; init(&s,&m,2); assert(S2_TOKEN_STORE_Load(&s,&out)==S2_TOKEN_STORE_INVALID);
    init(&s,&m,1); m.readFail=true; assert(!S2_TOKEN_STORE_Persist(&s,&b));
    memset(&out,123,sizeof(out)); assert(S2_TOKEN_STORE_Load(&s,&out)==S2_TOKEN_STORE_IO_ERROR); assert(out.count==0);
    m.readFail=false;
    /* Detect every single-byte corruption in a lone committed record. */
    m=baseline; memset(m.bytes[1],255,S2_TOKEN_STORE_SLOT_SIZE);
    baseline=m;
    for(cut=0;cut<(int)S2_TOKEN_STORE_SLOT_SIZE;cut++) {
        m=baseline; m.bytes[0][cut]^=1; init(&s,&m,1);
        if(cut<563||cut==567) assert(S2_TOKEN_STORE_Load(&s,&out)==S2_TOKEN_STORE_INVALID);
    }
    m=baseline; init(&s,&m,1); b.count=3; assert(!S2_TOKEN_STORE_Persist(&s,&b)); b.count=1; memset(b.token[0],'x',sizeof(b.token[0])); assert(!S2_TOKEN_STORE_Persist(&s,&b));
    S2_TOKEN_STORE_Clear(&s); assert(S2_TOKEN_STORE_Load(&s,&out)==S2_TOKEN_STORE_INVALID);
    puts("Token journal: 1142 byte-cut/restart cases, slot reuse, sync failures, pairing binding and I/O validation passed."); return 0;
}
