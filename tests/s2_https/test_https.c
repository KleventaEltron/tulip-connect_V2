#include "s2_https.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static S2_TLS tls;
static S2_SESSION session;
static S2_HTTPS http;
static S2_SESSION_TOKENS disk;
static uint8_t input[12000],output[4096];
static size_t inputLength,inputOffset,outputLength,readChunk,writeChunk;
static bool trust,blockRead,blockWrite,badRead,badWrite,storageFails;
static int eof;
static unsigned closes,graceful,writes;
static uint32_t now;
static char json[4097];
static const char *oldToken="AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=",*pendingToken="AQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQE=",*alternateToken="AgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgI=",*commToken="AwMDAwMDAwMDAwMDAwMDAwMDAwMDAwMDAwMDAwMDAwM=";
static const char *client="11111111-1111-4111-8111-111111111111",*server="22222222-2222-4222-8222-222222222222";
static bool verified(void *user) {assert(user==&tls);return trust && tls.state==S2_TLS_VERIFIED;}
static int readBytes(void *user,uint8_t *out,size_t capacity)
{
    assert(user==&tls && capacity<=S2_TLS_IO_SIZE);
    if(badRead)return (int)capacity+1;
    if(blockRead)return 0;
    size_t count=inputLength-inputOffset;
    if(count==0) {if(eof==S2_TLS_READ_EOF)tls.state=S2_TLS_IDLE;return eof;}
    if(count>capacity)count=capacity;if(count>readChunk)count=readChunk;
    memcpy(out,input+inputOffset,count);inputOffset+=count;return (int)count;
}
static int writeBytes(void *user,const uint8_t *data,size_t length)
{
    assert(user==&tls && length<=S2_TLS_IO_SIZE);
    if(badWrite)return (int)length+1;
    if(blockWrite)return 0;
    if(length>writeChunk)length=writeChunk;
    assert(outputLength+length<sizeof(output));
    memcpy(output+outputLength,data,length);outputLength+=length;output[outputLength]=0;return (int)length;
}
static void closeLater(void *user) {assert(user==&tls);graceful++;tls.state=S2_TLS_CLOSING;}
bool S2_TLS_GetPlatform(S2_TLS *t,S2_TRANSPORT_PLATFORM *platform)
{
    assert(t==&tls);memset(platform,0,sizeof(*platform));platform->user=t;
    platform->verified=verified;platform->read=readBytes;platform->write=writeBytes;platform->close=closeLater;return true;
}
bool S2_TLS_PeerMatches(S2_TLS *t,const char *host,uint16_t port)
{assert(t==&tls);return verified(t) && strcmp(host,tls.hostname)==0 && port==443;}
void S2_TLS_Tasks(S2_TLS *t,uint32_t time)
{assert(t==&tls && time==now);if(t->state==S2_TLS_CLOSING)t->state=S2_TLS_IDLE;}
void S2_TLS_Close(S2_TLS *t) {assert(t==&tls);closes++;t->state=S2_TLS_IDLE;}
static bool persist(void *user,const S2_SESSION_TOKENS *tokens)
{assert(user==&disk);writes++;if(storageFails)return false;disk=*tokens;return true;}
static void initiation(void)
{
    snprintf(json,sizeof(json),"{\"selectedCommunicationProtocol\":\"WebSocket\",\"selectedS2MessageVersion\":\"v1.0.0\",\"accessToken\":\"%s\"}",pendingToken);
}
static void confirmation(void)
{
    snprintf(json,sizeof(json),"{\"communicationProtocol\":\"WebSocket\",\"websocketToken\":\"%s\",\"websocketUrl\":\"wss://cem.local/s2\"}",commToken);
}
static void wireLength(unsigned status,const char *extra)
{
    inputLength=(size_t)snprintf((char*)input,sizeof(input),"HTTP/1.1 %u Response\r\nContent-Type: application/json; charset=utf-8\r\nContent-Length: %zu\r\n%s\r\n%s",status,strlen(json),extra,json);
    inputOffset=0;
}
static void wireChunked(void)
{
    size_t at=0,length=strlen(json),used;
    inputLength=(size_t)snprintf((char*)input,sizeof(input),"HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nTransfer-Encoding: chunked\r\n\r\n");
    while(at<length) {
        used=length-at;if(used>17)used=17;
        int n=snprintf((char*)input+inputLength,sizeof(input)-inputLength,"%zx;trace=ok\r\n",used);assert(n>0);inputLength+=(size_t)n;
        memcpy(input+inputLength,json+at,used);inputLength+=used;memcpy(input+inputLength,"\r\n",2);inputLength+=2;at+=used;
    }
    memcpy(input+inputLength,"0\r\nX-Trace: ok\r\n\r\n",18);inputLength+=18;inputOffset=0;
}
static void setup(void)
{
    memset(&http,0,sizeof(http));memset(&tls,0,sizeof(tls));S2_SESSION_Clear(&session);
    tls.initialized=true;tls.state=S2_TLS_VERIFIED;strcpy(tls.hostname,"cem.local");
    trust=true;blockRead=blockWrite=badRead=badWrite=storageFails=false;
    eof=0;closes=graceful=writes=0;now=0;inputLength=inputOffset=outputLength=0;
    readChunk=writeChunk=512;memset(output,0,sizeof(output));
    memset(&disk,0,sizeof(disk));disk.count=1;strcpy(disk.token[0],oldToken);
    S2_SESSION_STORAGE storage={&disk,persist};
    assert(S2_SESSION_Begin(&session,&storage,"https://cem.local/connection/v1",client,server,&disk,now)==S2_SESSION_OK);
}
static void start(void) {assert(S2_HTTPS_Begin(&http,&tls,443,&session,now));}
static void drive(void)
{
    unsigned iterations=0;
    while(http.state==S2_HTTPS_SENDING || http.state==S2_HTTPS_READING) {
        S2_HTTPS_Tasks(&http,++now);assert(++iterations<20000);
    }
}
static void cleared(void)
{
    for(size_t i=0;i<sizeof(http.tx);i++)assert(http.tx[i]==0);
    for(size_t i=0;i<sizeof(http.body);i++)assert(http.body[i]==0);
    assert(http.request.bearer[0]==0);
}
static void badResponse(const char *wire)
{
    setup();inputLength=strlen(wire);memcpy(input,wire,inputLength);eof=-1;
    start();drive();assert(http.state==S2_HTTPS_FAILED && writes==0 && disk.count==1 && closes==1);cleared();
}
int main(void)
{
    const size_t splits[]={1,2,3,7,511,512};
    for(unsigned split=0;split<sizeof(splits)/sizeof(splits[0]);split++) {
        setup();readChunk=splits[split];writeChunk=3;initiation();wireLength(200,"");start();drive();
        assert(http.state==S2_HTTPS_DONE && http.reusable && session.state==S2_SESSION_CONFIRM && disk.count==2 && writes==1);
        assert(strstr((char*)output,"POST /connection/v1/initiateSession HTTP/1.1\r\n")!=NULL);
        assert(strstr((char*)output,"Host: cem.local:443\r\n")!=NULL);
        assert(strstr((char*)output,oldToken)!=NULL && closes==0 && graceful==0);cleared();
        if(split==0) {printf("REQUEST_HEX ");for(size_t i=0;i<outputLength;i++)printf("%02x",output[i]);putchar('\n');}
        outputLength=0;output[0]=0;confirmation();wireChunked();start();drive();
        assert(http.state==S2_HTTPS_DONE && session.state==S2_SESSION_READY && disk.count==1 && writes==2);
        assert(strstr((char*)output,"POST /connection/v1/confirmAccessToken HTTP/1.1\r\n")!=NULL);
        assert(strstr((char*)output,"Content-Length: 0\r\n")!=NULL && strstr((char*)output,pendingToken)!=NULL);
        cleared();S2_HTTPS_Stop(&http);assert(session.state==S2_SESSION_READY);
    }
    /* Authenticated EOF versus a truncated/error stream for close-delimited data. */
    setup();initiation();inputLength=(size_t)snprintf((char*)input,sizeof(input),"HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n%s",json);
    eof=S2_TLS_READ_EOF;start();drive();assert(http.state==S2_HTTPS_DONE && !http.reusable && writes==1);
    setup();initiation();inputLength=(size_t)snprintf((char*)input,sizeof(input),"HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n%s",json);
    eof=-1;start();drive();assert(http.state==S2_HTTPS_FAILED && writes==0);
    setup();initiation();inputLength=(size_t)snprintf((char*)input,sizeof(input),"HTTP/1.0 200 OK\r\nContent-Type: application/json\r\n\r\n%s",json);
    eof=S2_TLS_READ_EOF;start();drive();assert(http.state==S2_HTTPS_DONE && !http.reusable);
    /* Explicit close after framed response requests bounded TLS cleanup. */
    setup();initiation();wireLength(200,"Connection: close\r\n");start();drive();assert(graceful==1 && !http.reusable);
    S2_HTTPS_Tasks(&http,++now);assert(tls.state==S2_TLS_IDLE);
    /* Interim and final response can arrive together or split arbitrarily. */
    setup();initiation();wireLength(200,"");
    const char interim[]="HTTP/1.1 100 Continue\r\n\r\n";
    memmove(input+sizeof(interim)-1,input,inputLength);memcpy(input,interim,sizeof(interim)-1);inputLength+=sizeof(interim)-1;
    readChunk=1;start();drive();assert(http.state==S2_HTTPS_DONE && writes==1);
    /* Two saved tokens: 401 is framed, then retry is a new request. */
    setup();disk.count=2;strcpy(disk.token[1],alternateToken);S2_SESSION_Clear(&session);
    S2_SESSION_STORAGE store={&disk,persist};assert(S2_SESSION_Begin(&session,&store,"https://cem.local/connection/v1",client,server,&disk,0)==S2_SESSION_OK);
    inputLength=(size_t)snprintf((char*)input,sizeof(input),"HTTP/1.1 401 Unauthorized\r\nContent-Length: 0\r\n\r\n");
    start();drive();assert(http.state==S2_HTTPS_DONE && session.state==S2_SESSION_INITIATE && writes==0);
    outputLength=0;output[0]=0;initiation();wireLength(200,"");start();drive();
    assert(http.state==S2_HTTPS_DONE && strstr((char*)output,alternateToken)!=NULL);
    /* Framing ambiguity, redirect, invalid type/encoding and chunk errors. */
    badResponse("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: 2\r\nContent-Length: 2\r\n\r\n{}");
    badResponse("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: 2\r\nTransfer-Encoding: chunked\r\n\r\n{}");
    badResponse("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: 4097\r\n\r\n");
    badResponse("HTTP/1.1 302 Found\r\nLocation: https://other.local/\r\nContent-Length: 0\r\n\r\n");
    badResponse("HTTP/1.1 200 OK\nContent-Length: 2\n\n{}");
    badResponse("HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 2\r\n\r\n{}");
    badResponse("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Encoding: gzip\r\nContent-Length: 2\r\n\r\n{}");
    badResponse("HTTP/1.1 200 OK\r\n Content-Type: application/json\r\nContent-Length: 2\r\n\r\n{}");
    badResponse("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nTransfer-Encoding: chunked\r\n\r\nx\r\n");
    badResponse("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nTransfer-Encoding: chunked\r\n\r\n2\r\n{}X\n0\r\n\r\n");
    badResponse("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nTransfer-Encoding: chunked\r\n\r\n0\r\nContent-Length: 0\r\n\r\n");
    badResponse("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n{}");
    badResponse("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: 99\r\n\r\n{}");
    badResponse("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nTransfer-Encoding: chunked\r\n\r\n1001\r\n");
    badResponse("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nTransfer-Encoding: gzip, chunked\r\n\r\n");
    setup();memcpy(input,"HTTP/1.1 200 OK\r\nX: ",20);memset(input+20,'A',5000);inputLength=5020;
    start();drive();assert(http.state==S2_HTTPS_FAILED && writes==0);cleared();
    setup();initiation();wireChunked();inputLength-=2;
    memcpy(input+inputLength,"Y:",2);inputLength+=2;memset(input+inputLength,'A',130);inputLength+=130;
    memcpy(input+inputLength,"\r\n\r\n",4);inputLength+=4;
    start();drive();assert(http.state==S2_HTTPS_FAILED && writes==0);cleared();
    /* The exact maximum decoded response body fits, including trailing JSON space. */
    setup();initiation();size_t n=strlen(json);memset(json+n,' ',4096-n);json[4096]=0;wireLength(200,"");start();drive();
    assert(http.state==S2_HTTPS_DONE && writes==1);cleared();
    setup();initiation();wireLength(200,"");input[inputLength++]='X';start();drive();assert(http.state==S2_HTTPS_FAILED && writes==0);
    /* TLS/session deadline/error cleanup occurs before further I/O. */
    setup();trust=false;assert(!S2_HTTPS_Begin(&http,&tls,443,&session,0) && outputLength==0 && writes==0);
    setup();strcpy(tls.hostname,"other.local");assert(!S2_HTTPS_Begin(&http,&tls,443,&session,0) && outputLength==0);
    setup();assert(!S2_HTTPS_Begin(&http,&tls,444,&session,0) && outputLength==0);
    setup();blockWrite=true;start();now=15000;S2_HTTPS_Tasks(&http,now);assert(http.state==S2_HTTPS_FAILED && session.error==S2_SESSION_EXPIRED && outputLength==0);
    setup();initiation();wireLength(200,"");start();trust=false;drive();assert(http.state==S2_HTTPS_FAILED && writes==0);
    setup();initiation();wireLength(200,"");blockRead=true;start();drive();assert(http.state==S2_HTTPS_FAILED && session.error==S2_SESSION_EXPIRED);
    setup();initiation();wireLength(200,"");start();badWrite=true;drive();assert(http.state==S2_HTTPS_FAILED && writes==0);
    setup();initiation();wireLength(200,"");start();badRead=true;drive();assert(http.state==S2_HTTPS_FAILED && writes==0);
    setup();initiation();wireLength(200,"");storageFails=true;start();drive();assert(http.state==S2_HTTPS_FAILED && session.error==S2_SESSION_STORAGE_ERROR && disk.count==1);
    setup();start();S2_HTTPS_Stop(&http);assert(session.state==S2_SESSION_FAILED && http.state==S2_HTTPS_IDLE);
    printf("HTTPS partial I/O, framing, token-flow integration, EOF, bounds and failure tests passed. Context: %zu bytes.\n",sizeof(http));
    return 0;
}
