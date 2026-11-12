#include "app_s2_session_tasks.h"
#include "files/network_manager.h"
#include "library/tcpip/tcpip.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static APP_S2_SESSION_WORKSPACE work;
static S2_TOKEN_STORE diskStore;
static uint8_t disk[2][S2_TOKEN_STORE_SLOT_SIZE];
static void (*callbacks[4])(void);
static unsigned registrations,dnsQueries,starts,requests,provides,saveWrites;
static uint64_t now;
static bool network,clockReady,dnsPending,handshake,wsFail,storageFail,closeResponses;
static bool authFail,oldFails,dropConfirm,dropRead,malformed,tlsFail,clockStopped;
static TCPIP_DNS_RESULT dnsStart;
static bool registrationFails;
static uint32_t saveDelay;
static char tx[4096],rx[4096];static size_t txLength,rxLength,rxOffset;
static const char *old="AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=";
static const char *pending="AQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQE=";
static const char *alternate="AgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgI=";
static const char *comm="AwMDAwMDAwMDAwMDAwMDAwMDAwMDAwMDAwMDAwMDAwM=";
static const uint8_t ca[]={1,2,3};
uint64_t SYS_TIME_Counter64Get(void) { return now; }
uint32_t SYS_TIME_FrequencyGet(void) { return clockStopped?0:1000; }
bool NETWORK_MANAGER_IsReady(void) { return network; }
bool NETWORK_MANAGER_RegisterDownCallback(void (*cb)(void)) { if(registrationFails)return false;assert(registrations<4);callbacks[registrations++]=cb;return true; }
static bool text(void *u,const char *s,size_t n) { (void)u;(void)s;(void)n;return true; }
TCPIP_DNS_RESULT TCPIP_DNS_Resolve(const char *host,int kind) {
    assert(network&&!strcmp(host,"cem.example")&&kind==TCPIP_DNS_TYPE_A);dnsQueries++;return dnsStart;
}
TCPIP_DNS_RESULT TCPIP_DNS_IsResolved(const char *host,IP_MULTI_ADDRESS *ip,int kind) {
    assert(network&&!strcmp(host,"cem.example")&&kind==IP_ADDRESS_TYPE_IPV4);
    ip->v4Add.Val=0x01020304;return dnsPending?TCPIP_DNS_RES_PENDING:TCPIP_DNS_RES_OK;
}
static bool rd(void *u,unsigned slot,size_t at,void *out,size_t n) {
    (void)u;assert(slot<2&&at+n<=S2_TOKEN_STORE_SLOT_SIZE);memcpy(out,disk[slot]+at,n);return true;
}
static bool wr(void *u,unsigned slot,size_t at,const void *in,size_t n) {
    (void)u;saveWrites++;if(storageFail)return false;
    assert(slot<2&&at+n<=S2_TOKEN_STORE_SLOT_SIZE);memcpy(disk[slot]+at,in,n);return true;
}
static bool syncDisk(void *u) { (void)u;now+=saveDelay;return !storageFail; }
bool S2_TLS_ClockReady(void) { return clockReady; }
bool S2_TLS_Initialize(S2_TLS *t,void *u,bool (*cb)(void *,const char *,size_t)) {
    memset(t,0,sizeof(*t));t->initialized=true;t->consumer=u;t->text=cb;return true;
}
bool S2_TLS_Start(S2_TLS *t,uint32_t ip,uint16_t port,const char *host,const uint8_t *cert,size_t n,uint32_t time) {
    assert(network&&clockReady&&ip==0x01020304&&port==443&&!strcmp(host,"cem.example"));
    assert(n==sizeof(ca)&&!memcmp(cert,ca,n)&&time==(uint32_t)now);starts++;
    if(tlsFail) {t->error=S2_TLS_ERROR_HANDSHAKE;t->state=S2_TLS_FAILED;return false;}
    strcpy(t->hostname,host);t->state=S2_TLS_TCP_CONNECTING;
    if(t==&work.httpsTls) {txLength=rxLength=rxOffset=0;dropRead=false;}
    return true;
}
void S2_TLS_Tasks(S2_TLS *t,uint32_t time) {
    assert(time==(uint32_t)now);
    if(t->state==S2_TLS_TCP_CONNECTING&&handshake)t->state=S2_TLS_VERIFIED;
    if(t->state==S2_TLS_CLOSING)t->state=S2_TLS_IDLE;
}
void S2_TLS_Close(S2_TLS *t) { t->state=S2_TLS_IDLE; }
static bool verified(void *u) { return ((S2_TLS*)u)->state==S2_TLS_VERIFIED; }
bool S2_TLS_PeerMatches(S2_TLS *t,const char *host,uint16_t port) { return verified(t)&&!strcmp(host,t->hostname)&&port==443; }
static int writeTls(void *u,const uint8_t *p,size_t n) {
    assert(u==&work.httpsTls&&verified(u));
    if(n>7)n=7;assert(txLength+n<sizeof(tx));memcpy(tx+txLength,p,n);txLength+=n;tx[txLength]=0;return (int)n;
}
static int readTls(void *u,uint8_t *out,size_t n) {
    char body[1500];unsigned code=200;size_t take;
    assert(u==&work.httpsTls&&verified(u));
    if(dropRead)return -1;
    if(rxLength==0) {
        assert(strstr(tx,"\r\n\r\n"));requests++;
        if(strstr(tx,"/initiateSession ")) {
            char bearer[300];snprintf(bearer,sizeof(bearer),"Authorization: Bearer %s\r\n",old);
            bool usingOld=strstr(tx,bearer)!=NULL;
            if(authFail||(oldFails&&usingOld)) {code=401;strcpy(body,"{}");}
            else if(malformed)strcpy(body,"{}");
            else snprintf(body,sizeof(body),"{\"selectedCommunicationProtocol\":\"WebSocket\",\"selectedS2MessageVersion\":\"v1.0.0\",\"accessToken\":\"%s\"}",usingOld?pending:alternate);
        } else {
            assert(strstr(tx,"/confirmAccessToken "));
            if(dropConfirm) {dropConfirm=false;dropRead=true;oldFails=true;return -1;}
            snprintf(body,sizeof(body),"{\"communicationProtocol\":\"WebSocket\",\"websocketToken\":\"%s\",\"websocketUrl\":\"wss://cem.example/s2\"}",comm);
        }
        rxLength=(size_t)snprintf(rx,sizeof(rx),"HTTP/1.1 %u Response\r\nContent-Type: application/json\r\nContent-Length: %zu\r\n%s\r\n%s",code,strlen(body),closeResponses?"Connection: close\r\n":"",body);
        rxOffset=0;
    }
    take=rxLength-rxOffset;if(take>n)take=n;if(take>11)take=11;
    memcpy(out,rx+rxOffset,take);rxOffset+=take;
    if(rxOffset==rxLength) {rxLength=rxOffset=txLength=0;tx[0]=0;}
    return (int)take;
}
static void closeTls(void *u) { ((S2_TLS*)u)->state=S2_TLS_CLOSING; }
bool S2_TLS_GetPlatform(S2_TLS *t,S2_TRANSPORT_PLATFORM *p) {
    memset(p,0,sizeof(*p));p->user=t;p->verified=verified;p->write=writeTls;p->read=readTls;p->close=closeTls;return true;
}
bool S2_TRANSPORT_Initialize(S2_TRANSPORT *t,const S2_TRANSPORT_PLATFORM *p) { memset(t,0,sizeof(*t));t->initialized=true;t->platform=*p;return true; }
bool S2_TRANSPORT_Start(S2_TRANSPORT *t,const char *host,const char *path,const char *token,uint32_t time) {
    assert(t==&work.channel.transport&&!strcmp(host,"cem.example:443")&&!strcmp(path,"/s2")&&!strcmp(token,comm)&&time==(uint32_t)now);
    assert(work.httpsTls.state==S2_TLS_IDLE&&work.channel.lifetimeMs<S2_SESSION_COMM_MS);
    provides++;t->state=S2_TRANSPORT_UPGRADING;return true;
}
void S2_TRANSPORT_Tasks(S2_TRANSPORT *t,bool ready,uint32_t time) {
    assert(ready&&time==(uint32_t)now);t->state=wsFail?S2_TRANSPORT_FAILED:S2_TRANSPORT_OPEN;
}
S2_TRANSPORT_STATE S2_TRANSPORT_GetState(const S2_TRANSPORT *t) { return t->state; }
void S2_TRANSPORT_Stop(S2_TRANSPORT *t) { t->state=S2_TRANSPORT_IDLE; }
S2_WS_RESULT S2_TRANSPORT_SendText(S2_TRANSPORT *t,const char *p,size_t n) { (void)t;(void)p;(void)n;return S2_WS_OK; }
static void step(void) { APP_S2_SESSION_Tasks();APP_S2_Tasks();now++; }
static APP_S2_SESSION_CONFIG config(void) {
    APP_S2_SESSION_CONFIG c={"https://cem.example/connection/v1","11111111-1111-4111-8111-111111111111","22222222-2222-4222-8222-222222222222",ca,sizeof(ca),&diskStore,NULL,text,0};return c;
}
static void setup(bool two) {
    APP_S2_SESSION_CONFIG c;S2_SESSION_TOKENS tokens={0};uint8_t binding[32]={1};
    S2_TOKEN_STORE_IO io={NULL,rd,wr,syncDisk};
    APP_S2_SESSION_Stop();saveDelay=0;network=clockReady=handshake=true;
    dnsPending=wsFail=storageFail=closeResponses=authFail=oldFails=dropConfirm=dropRead=malformed=tlsFail=clockStopped=false;
    dnsStart=TCPIP_DNS_RES_PENDING;dnsQueries=starts=requests=provides=saveWrites=0;now=0;
    memset(disk,255,sizeof(disk));assert(S2_TOKEN_STORE_Init(&diskStore,&io,binding));
    tokens.count=two?2:1;strcpy(tokens.token[0],old);if(two)strcpy(tokens.token[1],pending);
    assert(S2_TOKEN_STORE_Persist(&diskStore,&tokens));saveWrites=0;
    c=config();assert(APP_S2_SESSION_Configure(&work,&c));assert(!saveWrites&&!dnsQueries&&!starts);
}
static void until(APP_S2_SESSION_STATE state) {
    unsigned i;for(i=0;i<5000&&APP_S2_SESSION_GetStatus().state!=state;i++)step();
    assert(APP_S2_SESSION_GetStatus().state==state);
}
static void connected(void) { unsigned i;for(i=0;i<5000&&APP_S2_GetStatus().state!=APP_S2_WS_OPEN;i++)step();assert(APP_S2_GetStatus().state==APP_S2_WS_OPEN);step(); }
static void saved(const char *wanted,size_t count) {
    S2_SESSION_TOKENS out;assert(S2_TOKEN_STORE_Load(&diskStore,&out)==S2_TOKEN_STORE_OK&&out.count==count);assert(!strcmp(out.token[count-1],wanted));
}
int main(int argc,char **argv) {
    APP_S2_SESSION_CONFIG c;unsigned i;
    (void)argv;
    assert(APP_S2_Initialize());
    if(argc>1) {
        registrationFails=true;assert(!APP_S2_SESSION_Initialize());
        assert(!APP_S2_SESSION_Initialize());c=config();
        assert(!APP_S2_SESSION_Configure(&work,&c));
        assert(APP_S2_SESSION_GetStatus().error==APP_S2_SESSION_ERROR_REGISTRATION);
        puts("Coordinator registration failure leaves startup disabled.");return 0;
    }
    assert(APP_S2_SESSION_Initialize());assert(APP_S2_SESSION_Initialize());assert(registrations==2);
    for(i=0;i<5;i++)step();assert(!dnsQueries&&!starts&&APP_S2_SESSION_GetStatus().state==APP_S2_SESSION_DISABLED);
    setup(false);network=false;for(i=0;i<5;i++)step();assert(!dnsQueries&&!starts);
    network=true;clockReady=false;for(i=0;i<5;i++)step();assert(!dnsQueries&&!starts);
    clockReady=true;connected();saved(pending,1);assert(requests==2&&starts==2&&provides==1);
    assert(work.session.tokens.count==0&&!work.details.commToken[0]&&!work.saved.count);
    now+=60000;step();assert(APP_S2_GetStatus().state==APP_S2_WS_OPEN);
    setup(false);APP_S2_SESSION_Stop();c=config();c.ipv4Override=0x01020304;
    assert(APP_S2_SESSION_Configure(&work,&c));connected();assert(dnsQueries==0);saved(pending,1);
    setup(false);closeResponses=true;connected();assert(starts==3&&requests==2);saved(pending,1);
    setup(true);oldFails=true;connected();assert(requests==3);saved(alternate,1);
    setup(false);memset(disk,255,sizeof(disk));until(APP_S2_SESSION_BLOCKED);
    assert(APP_S2_SESSION_GetStatus().error==APP_S2_SESSION_ERROR_UNPAIRED&&!requests);
    setup(false);tlsFail=true;until(APP_S2_SESSION_RETRY_WAIT);
    assert(APP_S2_SESSION_GetStatus().error==APP_S2_SESSION_ERROR_TLS&&!requests);
    setup(false);saveDelay=8000;until(APP_S2_SESSION_RETRY_WAIT);
    assert(APP_S2_SESSION_GetStatus().error==APP_S2_SESSION_ERROR_EXPIRED&&requests==1);saveDelay=0;saved(pending,2);
    setup(false);clockStopped=true;step();assert(APP_S2_SESSION_GetStatus().state==APP_S2_SESSION_BLOCKED&&APP_S2_GetStatus().state==APP_S2_DISABLED);clockStopped=false;
    setup(false);authFail=true;until(APP_S2_SESSION_BLOCKED);assert(APP_S2_SESSION_GetStatus().error==APP_S2_SESSION_ERROR_AUTH);saved(old,1);
    setup(false);malformed=true;until(APP_S2_SESSION_BLOCKED);assert(APP_S2_SESSION_GetStatus().error==APP_S2_SESSION_ERROR_PROTOCOL);
    setup(false);storageFail=true;until(APP_S2_SESSION_BLOCKED);assert(APP_S2_SESSION_GetStatus().error==APP_S2_SESSION_ERROR_STORAGE);storageFail=false;saved(old,1);
    setup(false);dropConfirm=true;until(APP_S2_SESSION_RETRY_WAIT);saved(pending,2);
    now+=5000;connected();saved(alternate,1);assert(APP_S2_SESSION_GetStatus().attempts==2);
    setup(false);dnsPending=true;until(APP_S2_SESSION_DNS);now+=APP_S2_SESSION_DNS_MS;step();assert(APP_S2_SESSION_GetStatus().state==APP_S2_SESSION_RETRY_WAIT);
    setup(false);dnsStart=TCPIP_DNS_RES_NO_SERVICE;
    for(i=1;i<=APP_S2_SESSION_MAX_FAILURES;i++) {
        until(i==APP_S2_SESSION_MAX_FAILURES?APP_S2_SESSION_BLOCKED:APP_S2_SESSION_RETRY_WAIT);
        assert(APP_S2_SESSION_GetStatus().consecutiveFailures==i);
        if(i<APP_S2_SESSION_MAX_FAILURES) {assert(APP_S2_SESSION_GetStatus().retryDelayMs==(5000u<<(i-1)));now+=APP_S2_SESSION_GetStatus().retryDelayMs;step();}
    }
    assert(dnsQueries==APP_S2_SESSION_MAX_FAILURES);
    setup(false);until(APP_S2_SESSION_HTTP);network=false;
    for(i=0;i<registrations;i++)callbacks[i]();
    assert(work.httpsTls.state==S2_TLS_IDLE&&APP_S2_GetStatus().state==APP_S2_WAIT_SESSION);
    for(i=0;i<registrations;i++)callbacks[i]();assert(APP_S2_SESSION_GetStatus().consecutiveFailures==1);
    network=true;now+=5000;connected();
    /* Losing an established channel obtains a fresh session, never reuses comm token. */
    wsFail=true;step();step();assert(APP_S2_SESSION_GetStatus().state==APP_S2_SESSION_RETRY_WAIT);wsFail=false;now+=5000;connected();assert(provides==2);
    setup(false);until(APP_S2_SESSION_HANDOFF);now+=S2_SESSION_COMM_MS;step();assert(APP_S2_SESSION_GetStatus().error==APP_S2_SESSION_ERROR_EXPIRED&&provides==0);
    setup(false);handshake=false;until(APP_S2_SESSION_HTTPS_WAIT);network=false;for(i=0;i<registrations;i++)callbacks[i]();assert(work.httpsTls.state==S2_TLS_IDLE);
    APP_S2_SESSION_Stop();for(i=0;i<sizeof(work);i++)assert(((uint8_t*)&work)[i]==0);
    assert(APP_S2_GetStatus().state==APP_S2_DISABLED);
    c=config();c.apiBase="http://cem.example/v1";assert(!APP_S2_SESSION_Configure(&work,&c));
    c=config();c.clientNodeId="bad";assert(!APP_S2_SESSION_Configure(&work,&c));
    c=config();c.caLength=0;assert(!APP_S2_SESSION_Configure(&work,&c));
    c=config();c.tokens=NULL;assert(!APP_S2_SESSION_Configure(&work,&c));
    { char host[254];uint16_t port=1;
      assert(S2_SESSION_Configuration("https://cem.example:8443/api/v1/",c.clientNodeId,c.serverNodeId,host,&port)&&port==8443);
      assert(!S2_SESSION_Configuration("https://cem.example/api/v2",c.clientNodeId,c.serverNodeId,host,&port)&&!host[0]&&!port);
    }
    puts("Session coordinator: actual HTTPS/session/journal flow, close/reconnect, auth fallback, uncertain rotation recovery, retry limits, expiry, network teardown and disabled startup passed.");
    printf("Coordinator workspace: %zu bytes (includes existing WebSocket workspace).\n",sizeof(work));return 0;
}
