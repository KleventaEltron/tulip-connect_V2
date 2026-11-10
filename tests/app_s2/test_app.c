#include "app_s2_tasks.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static APP_S2_WORKSPACE work;
static void (*downCallback)(void);
static unsigned registrations, starts, upgrades, tlsCloses, transportStops, sends;
static bool network, clockReady, registerFails, tlsFails, handshake, upgrade, peerClose, wsFails;
static uint64_t timeMs;
static unsigned cleanupOrder;
static const uint8_t ca[] = {1,2,3};
uint64_t SYS_TIME_Counter64Get(void) { return timeMs*120000u; }
uint32_t SYS_TIME_FrequencyGet(void) { return 120000000u; }
bool NETWORK_MANAGER_IsReady(void) { return network; }
bool NETWORK_MANAGER_RegisterDownCallback(void (*callback)(void))
{ registrations++;downCallback=callback;return !registerFails; }
bool S2_TLS_ClockReady(void) { return clockReady; }
bool S2_TLS_Initialize(S2_TLS *t,void *consumer,bool (*text)(void *,const char *,size_t))
{ memset(t,0,sizeof(*t));t->initialized=true;t->consumer=consumer;t->text=text;return true; }
bool S2_TLS_GetPlatform(S2_TLS *t,S2_TRANSPORT_PLATFORM *platform)
{ memset(platform,0,sizeof(*platform));platform->user=t;return true; }
bool S2_TRANSPORT_Initialize(S2_TRANSPORT *t,const S2_TRANSPORT_PLATFORM *platform)
{ memset(t,0,sizeof(*t));t->initialized=true;t->platform=*platform;return true; }
bool S2_TLS_Start(S2_TLS *t,uint32_t ip,uint16_t port,const char *host,
    const uint8_t *cert,size_t certLength,uint32_t now)
{
    assert(registrations==1 && network && clockReady);
    assert(ip==0x01020304 && port==443 && strcmp(host,"cem.local")==0);
    assert(certLength==3 && memcmp(cert,ca,3)==0 && now==(uint32_t)timeMs);
    starts++; t->state=S2_TLS_TCP_CONNECTING;
    if(tlsFails) { t->state=S2_TLS_FAILED;t->error=S2_TLS_ERROR_HANDSHAKE;t->wolfError=-155;return false; }
    return true;
}
void S2_TLS_Tasks(S2_TLS *t,uint32_t now)
{
    assert(now==(uint32_t)timeMs);
    if(t->state==S2_TLS_TCP_CONNECTING && handshake)t->state=S2_TLS_VERIFIED;
    else if(t->state==S2_TLS_CLOSING)t->state=S2_TLS_IDLE;
}
void S2_TLS_Close(S2_TLS *t)
{ tlsCloses++;assert(cleanupOrder==1);cleanupOrder=2;t->state=S2_TLS_IDLE; }
void S2_TRANSPORT_Stop(S2_TRANSPORT *t)
{ transportStops++;cleanupOrder=1;t->state=S2_TRANSPORT_IDLE; }
bool S2_TRANSPORT_Start(S2_TRANSPORT *t,const char *host,const char *path,const char *token,uint32_t now)
{
    assert(work.tls.state==S2_TLS_VERIFIED && now==(uint32_t)timeMs);
    assert(strcmp(host,"cem.local:443")==0 && strcmp(path,"/s2")==0 && strcmp(token,"fresh-token")==0);
    upgrades++;t->state=S2_TRANSPORT_UPGRADING;return true;
}
void S2_TRANSPORT_Tasks(S2_TRANSPORT *t,bool ready,uint32_t now)
{
    assert(ready && now==(uint32_t)timeMs);
    if(wsFails)t->state=S2_TRANSPORT_FAILED;
    else if(t->state==S2_TRANSPORT_UPGRADING && upgrade)t->state=S2_TRANSPORT_OPEN;
    else if(t->state==S2_TRANSPORT_OPEN && peerClose)t->state=S2_TRANSPORT_DRAINING;
    else if(t->state==S2_TRANSPORT_DRAINING) {t->state=S2_TRANSPORT_IDLE;work.tls.state=S2_TLS_CLOSING;}
}
S2_TRANSPORT_STATE S2_TRANSPORT_GetState(const S2_TRANSPORT *t) {return t->state;}
S2_WS_RESULT S2_TRANSPORT_SendText(S2_TRANSPORT *t,const char *text,size_t length)
{ assert(t->state==S2_TRANSPORT_OPEN && length==2 && memcmp(text,"{}",2)==0);sends++;return S2_WS_OK; }
static bool text(void *consumer,const char *message,size_t length)
{ (void)consumer;(void)message;(void)length;return true; }
static APP_S2_SESSION session(void)
{ return (APP_S2_SESSION){0x01020304,443,"cem.local","/s2","fresh-token",ca,3,60000}; }
static void attach(void)
{
    network=clockReady=handshake=upgrade=true;tlsFails=peerClose=wsFails=false;
    assert(APP_S2_Attach(&work,NULL,text));
}
static void provide(void)
{ APP_S2_SESSION s=session();assert(APP_S2_ProvideSession(&s)); }
static void tick(uint32_t delta) { timeMs+=delta;APP_S2_Tasks(); }
int main(int argc,char **argv)
{
    (void)argv;
    assert(APP_S2_GetStatus().state==APP_S2_NOT_INITIALIZED);
    if(argc>1) {
        registerFails=true;assert(!APP_S2_Initialize());
        assert(APP_S2_GetStatus().error==APP_S2_ERROR_REGISTRATION);
        assert(!APP_S2_Attach(&work,NULL,text));APP_S2_Tasks();assert(starts==0);return 0;
    }
    assert(APP_S2_Initialize());assert(APP_S2_Initialize() && registrations==1);
    for(unsigned i=0;i<100;i++)tick(1000);
    assert(APP_S2_GetStatus().state==APP_S2_DISABLED && starts==0 && tlsCloses==0);
    assert(APP_S2_SendText("{}",2)==S2_WS_CLOSED);
    attach();assert(!APP_S2_Attach(&work,NULL,text));
    APP_S2_SESSION bad=session();bad.path="/bad\r\n";
    assert(!APP_S2_ProvideSession(&bad));
    network=false;provide();tick(1);assert(APP_S2_GetStatus().state==APP_S2_WAIT_NETWORK && starts==0);
    network=true;clockReady=false;tick(1);assert(APP_S2_GetStatus().state==APP_S2_WAIT_TIME && starts==0);
    clockReady=true;tick(1);assert(APP_S2_GetStatus().state==APP_S2_WS_OPEN && starts==1 && upgrades==1);
    assert(work.token[0]==0 && APP_S2_SendText("{}",2)==S2_WS_OK && sends==1);
    assert(!APP_S2_ProvideSession(&bad));
    for(unsigned i=0;i<100;i++)tick(1000);
    assert(APP_S2_GetStatus().state==APP_S2_WS_OPEN && starts==1); /* Lease only limits initiation. */
    network=false;downCallback();assert(APP_S2_GetStatus().state==APP_S2_WAIT_SESSION);
    assert(cleanupOrder==2 && work.token[0]==0 && transportStops==1 && tlsCloses==1);
    network=true;tick(1);assert(starts==1); /* No reuse/reconnect of consumed session. */
    tlsFails=true;provide();tick(1);
    assert(APP_S2_GetStatus().error==APP_S2_ERROR_TLS && APP_S2_GetStatus().wolfError==-155);
    assert(work.token[0]==0);tlsFails=false;tick(1);assert(starts==2);
    /* Pending sessions expire even while offline. */
    provide();network=false;tick(60000);
    assert(APP_S2_GetStatus().error==APP_S2_ERROR_EXPIRED && work.token[0]==0 && starts==2);
    /* Expiry during TCP/TLS setup cannot lead to a late upgrade. */
    network=true;handshake=false;provide();tick(1);
    assert(APP_S2_GetStatus().state==APP_S2_TLS_CONNECTING);
    tick(60000);assert(APP_S2_GetStatus().error==APP_S2_ERROR_EXPIRED && upgrades==1);
    /* Initiation expiry uses wrap-safe monotonic milliseconds. */
    APP_S2_Disable();attach();timeMs=UINT32_MAX-10u;
    APP_S2_SESSION shortSession=session();shortSession.lifetimeMs=20;
    network=false;assert(APP_S2_ProvideSession(&shortSession));
    tick(19);assert(APP_S2_GetStatus().state==APP_S2_WAIT_NETWORK);
    tick(1);assert(APP_S2_GetStatus().error==APP_S2_ERROR_EXPIRED);
    network=true;provide();tick(1);assert(APP_S2_GetStatus().state==APP_S2_WS_OPEN);
    peerClose=true;tick(1);assert(APP_S2_GetStatus().state==APP_S2_CLOSING);
    tick(1);tick(1);assert(APP_S2_GetStatus().state==APP_S2_WAIT_SESSION);
    peerClose=false;provide();tick(1);wsFails=true;tick(1);
    assert(APP_S2_GetStatus().error==APP_S2_ERROR_TRANSPORT);wsFails=false;
    APP_S2_Disable();
    const uint8_t *bytes=(const uint8_t*)&work;
    for(size_t i=0;i<sizeof(work);i++)assert(bytes[i]==0);
    assert(APP_S2_GetStatus().state==APP_S2_DISABLED);
    unsigned before=starts;tick(1000);assert(starts==before);
    printf("S2 application disabled boot, session lifecycle, expiry, cleanup and one-use tests passed. Workspace: %zu bytes.\n",sizeof(work));
    return 0;
}
