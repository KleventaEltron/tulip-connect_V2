#include "s2_tls.h"
#include "definitions.h"
#include "wolfssl/ssl.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static S2_TLS tls;
static S2_TRANSPORT_PLATFORM io;
static WOLFSSL_CTX context;
static WOLFSSL session;
static WOLFSSL_METHOD method;
static bool network, connected;
static uint32_t utc;
static unsigned ctxCount, sslCount, socketCount, libraryCount, closes;
static int failure, handshakeResult, lastError, writeResult, readResult;
static long verifyResult;
static int verifyMode;
static bool hostnameSet, sniSet;
static void *readUser, *writeUser;
static CallbackIORecv receiveCallback;
static CallbackIOSend sendCallback;
static const void *pendingPointer;
static int pendingLength;
static uint16_t tcpReadReady, tcpWriteReady;
static unsigned tcpPending;
static int shutdownResult;
static char integrationInput[512];
static size_t integrationLength;
static S2_TRANSPORT transport;
bool TCPIP_TCP_SocketInfoGet(int socket,TCP_SOCKET_INFO *info)
{ assert(socket==3);info->txPending=tcpPending;info->remotePort=443;return true; }
int wolfSSL_shutdown(WOLFSSL *s) { assert(s==&session);return shutdownResult; }
static const uint8_t ca[] = {1,2,3};

bool NETWORK_MANAGER_IsReady(void) { return network; }
uint32_t TCPIP_SNTP_UTCSecondsGet(void) { return utc; }
bool TULIP_CryptoRandom(void *user,uint8_t *out,size_t length)
{ (void)user; memset(out,0x55,length); return true; }
int wolfSSL_Init(void) { if (failure==1) return -1; libraryCount++; return 1; }
int wolfSSL_Cleanup(void) { assert(libraryCount>1); libraryCount--; return 1; }
WOLFSSL_METHOD *wolfTLSv1_2_client_method(void) { return &method; }
WOLFSSL_CTX *wolfSSL_CTX_new(WOLFSSL_METHOD *m)
{ assert(m==&method); if(failure==2)return NULL;ctxCount++;return &context; }
void wolfSSL_CTX_free(WOLFSSL_CTX *c) { assert(c==&context && ctxCount==1);ctxCount--; }
void wolfSSL_CTX_set_verify(WOLFSSL_CTX *c,int mode,void *callback)
{ assert(c==&context && mode==SSL_VERIFY_PEER && callback==NULL);verifyMode=mode; }
int wolfSSL_CTX_load_verify_buffer(WOLFSSL_CTX *c,const unsigned char *data,long length,int format)
{ assert(c==&context && data==ca && length==3 && format==SSL_FILETYPE_ASN1);return failure==3?-1:1; }
void wolfSSL_SetIORecv(WOLFSSL_CTX *c,CallbackIORecv callback) { assert(c==&context);receiveCallback=callback; }
void wolfSSL_SetIOSend(WOLFSSL_CTX *c,CallbackIOSend callback) { assert(c==&context);sendCallback=callback; }
WOLFSSL *wolfSSL_new(WOLFSSL_CTX *c) { assert(c==&context);if(failure==4)return NULL;sslCount++;return &session; }
void wolfSSL_free(WOLFSSL *s) { assert(s==&session && sslCount==1);sslCount--; }
int wolfSSL_check_domain_name(WOLFSSL *s,const char *host)
{ assert(s==&session && strcmp(host,"cem.local")==0);hostnameSet=true;return failure==5?-1:1; }
int wolfSSL_UseSNI(WOLFSSL *s,unsigned char type,const void *host,unsigned short length)
{ assert(s==&session && type==0 && length==9 && memcmp(host,"cem.local",9)==0);sniSet=true;return failure==6?-1:1; }
void wolfSSL_SetIOReadCtx(WOLFSSL *s,void *user) { assert(s==&session);readUser=user; }
void wolfSSL_SetIOWriteCtx(WOLFSSL *s,void *user) { assert(s==&session);writeUser=user; }
int TCPIP_TCP_ClientOpen(int family,int port,IP_MULTI_ADDRESS *address)
{ assert(family==IP_ADDRESS_TYPE_IPV4 && port==443 && address->v4Add.Val==0x01020304);if(failure==7)return -1;socketCount++;return 3; }
bool TCPIP_TCP_OptionsSet(int socket,int option,void *data)
{ TCP_OPTION_LINGER_DATA *p=data;assert(socket==3 && option==TCP_OPTION_LINGER && !p->gracefulEnable && !p->lingerEnable);return failure!=8; }
bool TCPIP_TCP_Close(int socket) { assert(socket==3 && socketCount==1);socketCount--;closes++;return true; }
bool TCPIP_TCP_IsConnected(int socket) { assert(socket==3);return connected; }
uint16_t TCPIP_TCP_GetIsReady(int socket) { assert(socket==3);return tcpReadReady; }
uint16_t TCPIP_TCP_PutIsReady(int socket) { assert(socket==3);return tcpWriteReady; }
uint16_t TCPIP_TCP_ArrayGet(int socket,uint8_t *out,uint16_t length)
{ assert(socket==3 && length<=tcpReadReady);memset(out,'x',length);return length; }
uint16_t TCPIP_TCP_ArrayPut(int socket,const uint8_t *data,uint16_t length)
{ assert(socket==3 && data!=NULL && length<=tcpWriteReady);return length; }
int wolfSSL_connect(WOLFSSL *s)
{ assert(s==&session && verifyMode==SSL_VERIFY_PEER && hostnameSet && sniSet && readUser==&tls && writeUser==&tls);return handshakeResult; }
long wolfSSL_get_verify_result(const WOLFSSL *s) { assert(s==&session);return verifyResult; }
int wolfSSL_get_error(WOLFSSL *s,int result) { assert(s==&session);(void)result;return lastError; }
int wolfSSL_write(WOLFSSL *s,const void *data,int length)
{
    assert(s==&session);
    if(pendingPointer!=NULL) { assert(data==pendingPointer && length==pendingLength); }
    if(writeResult<0) { pendingPointer=data;pendingLength=length;return -1; }
    pendingPointer=NULL;
    return writeResult==0?length:writeResult;
}
int wolfSSL_read(WOLFSSL *s,void *out,int capacity)
{ assert(s==&session && capacity==512);
  if (integrationLength != 0) { size_t n=integrationLength; memcpy(out,integrationInput,n);integrationLength=0;return (int)n; }
  if(readResult>0) { assert(readResult<=6);memcpy(out,"abcdef",(size_t)readResult); }return readResult; }
static bool text(void *user,const char *message,size_t length)
{ assert(user==&tls && length==2 && memcmp(message,"{}",2)==0);return true; }
static void initialize(void)
{
    assert(ctxCount==0 && sslCount==0 && socketCount==0);
    network=true;connected=false;utc=1800000000u;
    integrationLength=0;
    failure=0;handshakeResult=-1;lastError=WOLFSSL_ERROR_WANT_READ;
    verifyResult=0;verifyMode=0;hostnameSet=sniSet=false;
    writeResult=0;readResult=-1;pendingPointer=NULL;pendingLength=0;
    tcpReadReady=tcpWriteReady=0;tcpPending=0;shutdownResult=1;libraryCount=1;closes=0;
    assert(S2_TLS_Initialize(&tls,&tls,text));assert(S2_TLS_GetPlatform(&tls,&io));
}
static void start(uint32_t now)
{ assert(S2_TLS_Start(&tls,0x01020304,443,"cem.local",ca,sizeof(ca),now)); }
static void openConnection(void)
{
    initialize();start(0);connected=true;handshakeResult=1;
    S2_TLS_Tasks(&tls,1);assert(tls.state==S2_TLS_VERIFIED && io.verified(io.user));
}
static void checkReleased(void)
{ assert(ctxCount==0 && sslCount==0 && socketCount==0 && libraryCount==1);S2_TLS_Close(&tls);assert(libraryCount==1); }
int main(void)
{
    initialize();network=false;
    assert(!S2_TLS_Start(&tls,0x01020304,443,"cem.local",ca,3,0) && tls.error==S2_TLS_ERROR_NETWORK);checkReleased();
    initialize();utc=0;
    assert(!S2_TLS_Start(&tls,0x01020304,443,"cem.local",ca,3,0) && tls.error==S2_TLS_ERROR_CLOCK);checkReleased();
    const char *invalidHosts[] = {"", "-cem.local", "cem..local", "cem-.local", "127.0.0.1", "cem.local:443", "cem\r\n.local"};
    for (unsigned i=0; i<sizeof(invalidHosts)/sizeof(invalidHosts[0]); i++) {
        initialize();
        assert(!S2_TLS_Start(&tls,0x01020304,443,invalidHosts[i],ca,3,0));
        assert(tls.error==S2_TLS_ERROR_ARGUMENT);checkReleased();
    }
    for(int f=1;f<=8;f++) {
        initialize();failure=f;
        assert(!S2_TLS_Start(&tls,0x01020304,443,"cem.local",ca,3,0));checkReleased();
    }
    initialize();start(0);assert(!io.verified(io.user));
    assert(!S2_TLS_Start(&tls,0x01020304,443,"cem.local",ca,3,0) && closes==0);
    S2_TLS_Tasks(&tls,29999);assert(tls.state==S2_TLS_TCP_CONNECTING);
    S2_TLS_Tasks(&tls,30000);assert(tls.error==S2_TLS_ERROR_TIMEOUT);checkReleased();
    initialize();start(UINT32_MAX-10u);connected=true;
    S2_TLS_Tasks(&tls,5);assert(tls.state==S2_TLS_HANDSHAKING);
    lastError=WOLFSSL_ERROR_WANT_WRITE;S2_TLS_Tasks(&tls,30004);assert(tls.state==S2_TLS_HANDSHAKING);
    S2_TLS_Tasks(&tls,30005);assert(tls.error==S2_TLS_ERROR_TIMEOUT);checkReleased();
    initialize();start(0);connected=true;handshakeResult=-1;lastError=-155;
    S2_TLS_Tasks(&tls,1);assert(tls.state==S2_TLS_FAILED && tls.error==S2_TLS_ERROR_HANDSHAKE);checkReleased();
    initialize();start(0);connected=true;lastError=-188;
    S2_TLS_Tasks(&tls,1);assert(tls.state==S2_TLS_FAILED && tls.wolfError==-188);checkReleased();
    openConnection();
    assert(S2_TLS_PeerMatches(&tls,"CEM.local",443));
    assert(!S2_TLS_PeerMatches(&tls,"other.local",443));
    assert(!S2_TLS_PeerMatches(&tls,"cem.local",444));
    char data[8];tcpReadReady=3;tcpWriteReady=2;
    assert(receiveCallback(&session,data,2,&tls)==2);
    assert(sendCallback(&session,data,8,&tls)==2);
    tcpReadReady=tcpWriteReady=0;
    assert(receiveCallback(&session,data,8,&tls)==WOLFSSL_CBIO_ERR_WANT_READ);
    assert(sendCallback(&session,data,8,&tls)==WOLFSSL_CBIO_ERR_WANT_WRITE);
    uint8_t first[]={1,2,3},larger[]={1,2,3,4,5};
    writeResult=-1;lastError=WOLFSSL_ERROR_WANT_WRITE;
    assert(io.write(io.user,first,3)==0);
    assert(io.read(io.user,(uint8_t*)data,sizeof(data))==0); /* Pending write owns SSL retry. */
    writeResult=0;assert(io.write(io.user,larger,5)==3); /* Stable original buffer/length. */
    readResult=6;
    assert(io.read(io.user,(uint8_t*)data,2)==2 && memcmp(data,"ab",2)==0);
    assert(io.read(io.user,(uint8_t*)data,2)==2 && memcmp(data,"cd",2)==0);
    assert(io.read(io.user,(uint8_t*)data,2)==2 && memcmp(data,"ef",2)==0);
    assert(io.text(io.user,"{}",2));
    readResult=-1;lastError=WOLFSSL_ERROR_WANT_READ;
    assert(io.read(io.user,(uint8_t*)data,8)==0);
    lastError=WOLFSSL_ERROR_WANT_WRITE;
    assert(io.read(io.user,(uint8_t*)data,8)==0);
    assert(io.write(io.user,first,3)==0 && tls.writeLength==0);
    lastError=WOLFSSL_ERROR_WANT_READ;
    assert(io.read(io.user,(uint8_t*)data,8)==0 && !tls.readNeedsWrite);
    lastError=0;readResult=0;assert(io.read(io.user,(uint8_t*)data,8)==-1);checkReleased();
    openConnection();lastError=WOLFSSL_ERROR_ZERO_RETURN;readResult=0;
    assert(io.read(io.user,(uint8_t*)data,8)==S2_TLS_READ_EOF);checkReleased();
    openConnection();writeResult=-1;lastError=WOLFSSL_ERROR_WANT_WRITE;
    assert(io.write(io.user,first,3)==0);larger[0]=9;
    assert(io.write(io.user,larger,5)==-1 && tls.error==S2_TLS_ERROR_IO);checkReleased();
    openConnection();network=false;S2_TLS_Tasks(&tls,2);assert(!io.verified(io.user));checkReleased();
    openConnection();utc=0;S2_TLS_Tasks(&tls,2);assert(tls.error==S2_TLS_ERROR_CLOCK);checkReleased();
    openConnection();tcpPending=4;io.close(io.user);io.close(io.user);
    assert(tls.state==S2_TLS_CLOSING && closes==0 && !io.verified(io.user));
    S2_TLS_Tasks(&tls,2);assert(closes==0);tcpPending=0;
    S2_TLS_Tasks(&tls,3);assert(closes==1);checkReleased();
    openConnection();shutdownResult=WOLFSSL_SHUTDOWN_NOT_DONE;io.close(io.user);
    S2_TLS_Tasks(&tls,2000);assert(closes==0);
    S2_TLS_Tasks(&tls,2001);assert(closes==1);checkReleased();
    /* Real transport + real codec + this backend interface, mocked TLS records. */
    openConnection();assert(S2_TRANSPORT_Initialize(&transport,&io));
    assert(S2_TRANSPORT_Start(&transport,"cem.local","/s2","comm-token",1));
    int n=snprintf(integrationInput,sizeof(integrationInput),
        "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: %s\r\n\r\n",
        transport.websocket.expectedAccept);
    assert(n>0 && (size_t)n+4<sizeof(integrationInput));
    integrationInput[n++]=(char)0x81;integrationInput[n++]=2;
    integrationInput[n++]='{';integrationInput[n++]='}';integrationLength=(size_t)n;
    S2_TRANSPORT_Tasks(&transport,true,2);
    assert(S2_TRANSPORT_GetState(&transport)==S2_TRANSPORT_OPEN);
    network=false;S2_TRANSPORT_Tasks(&transport,false,3);assert(closes==1);checkReleased();
    printf("Dedicated TLS verification gates, independent ownership, partial I/O and cleanup tests passed. Context: %zu bytes.\n",sizeof(tls));
    return 0;
}
