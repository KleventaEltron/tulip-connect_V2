#include "s2_session.h"
#include "files/cJSON.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static S2_SESSION session;
static S2_SESSION_TOKENS disk;
static S2_SESSION_REQUEST request;
static S2_SESSION_DETAILS details;
static unsigned writes, failWrite;
static bool commitOnFailure;
static const char *client="11111111-1111-4111-8111-111111111111";
static const char *server="22222222-2222-4222-8222-222222222222";
static const char *oldToken="AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=", *newToken="AQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQE=", *nextToken="AgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgI=", *commToken="AwMDAwMDAwMDAwMDAwMDAwMDAwMDAwMDAwMDAwMDAwM=";
static char body[4097];
static bool persist(void *user,const S2_SESSION_TOKENS *tokens)
{
    assert(user==&disk && tokens->count>=1 && tokens->count<=2);
    writes++;
    if(writes==failWrite) {if(commitOnFailure)disk=*tokens;return false;}
    disk=*tokens;return true;
}
static void reset(void)
{
    S2_SESSION_Clear(&session);memset(&disk,0,sizeof(disk));
    disk.count=1;strcpy(disk.token[0],oldToken);writes=failWrite=0;commitOnFailure=false;
}
static void begin(uint32_t now)
{
    S2_SESSION_STORAGE storage={&disk,persist};
    assert(S2_SESSION_Begin(&session,&storage,"https://cem.local/connection/v1/",client,server,&disk,now)==S2_SESSION_OK);
    assert(S2_SESSION_Request(&session,&request,now)==S2_SESSION_OK);
}
static void initiation(const char *pending,const char *version)
{
    snprintf(body,sizeof(body),"{\"selectedCommunicationProtocol\":\"WebSocket\",\"selectedS2MessageVersion\":\"%s\",\"accessToken\":\"%s\"}",version,pending);
}
static void confirmation(const char *uri)
{
    snprintf(body,sizeof(body),"{\"communicationProtocol\":\"WebSocket\",\"websocketToken\":\"%s\",\"websocketUrl\":\"%s\"}",commToken,uri);
}
static S2_SESSION_RESULT response(unsigned http,uint32_t now)
{ return S2_SESSION_Response(&session,http,body,strlen(body),true,now); }
static void toConfirm(void)
{
    reset();begin(0);initiation(newToken,S2_SESSION_MESSAGE_VERSION);
    assert(response(200,10)==S2_SESSION_OK && session.state==S2_SESSION_CONFIRM);
    assert(writes==1 && disk.count==2 && strcmp(disk.token[0],oldToken)==0 && strcmp(disk.token[1],newToken)==0);
    assert(S2_SESSION_Request(&session,&request,20)==S2_SESSION_OK);
    assert(strstr(request.url,"/v1/confirmAccessToken") && strcmp(request.bearer,newToken)==0 && request.body[0]==0);
}
static void invalidInitiation(const char *json)
{
    reset();begin(0);strcpy(body,json);
    assert(response(200,1)==S2_SESSION_INVALID && writes==0 && disk.count==1);
}
static void *noMemory(size_t length) { (void)length;return NULL; }
int main(void)
{
    reset();begin(0);
    assert(strcmp(request.method,"POST")==0 && strcmp(request.bearer,oldToken)==0);
    assert(strcmp(request.url,"https://cem.local/connection/v1/initiateSession")==0);
    printf("REQUEST %s\n",request.body);
    initiation(newToken,S2_SESSION_MESSAGE_VERSION);printf("INIT_RESPONSE %s\n",body);
    assert(response(200,10)==S2_SESSION_OK && writes==1);
    assert(S2_SESSION_Response(&session,200,body,strlen(body),true,11)==S2_SESSION_INVALID); /* Out of order. */
    assert(S2_SESSION_Request(&session,&request,20)==S2_SESSION_OK && strcmp(request.bearer,newToken)==0);
    confirmation("wss://CEM.local/s2?resource=1");printf("CONFIRM_RESPONSE %s\n",body);
    assert(response(200,30)==S2_SESSION_OK && disk.count==1 && strcmp(disk.token[0],newToken)==0);
    assert(S2_SESSION_TakeDetails(&session,&details,40)==S2_SESSION_OK);
    assert(strcmp(details.hostname,"CEM.local")==0 && details.port==443 && strcmp(details.path,"/s2?resource=1")==0);
    assert(strcmp(details.commToken,commToken)==0 && details.lifetimeMs==29980 && session.details.commToken[0]==0);
    assert(S2_SESSION_TakeDetails(&session,&details,41)==S2_SESSION_INVALID);
    assert(S2_SESSION_Request(&session,&request,42)==S2_SESSION_INVALID && request.bearer[0]==0);
    /* Power loss before confirmation: both tokens survive and can be tried. */
    toConfirm();S2_SESSION_Clear(&session);writes=0;begin(100);
    assert(strcmp(request.bearer,oldToken)==0);
    strcpy(body,"{}");assert(response(401,101)==S2_SESSION_OK);
    assert(S2_SESSION_Request(&session,&request,102)==S2_SESSION_OK && strcmp(request.bearer,newToken)==0);
    initiation(nextToken,S2_SESSION_MESSAGE_VERSION);assert(response(200,103)==S2_SESSION_OK);
    assert(disk.count==2 && strcmp(disk.token[0],newToken)==0 && strcmp(disk.token[1],nextToken)==0);
    /* Failed durable write prevents confirmation from becoming available. */
    reset();begin(0);failWrite=1;initiation(newToken,S2_SESSION_MESSAGE_VERSION);
    assert(response(200,1)==S2_SESSION_STORAGE_ERROR && disk.count==1);
    assert(S2_SESSION_Request(&session,&request,2)==S2_SESSION_STORAGE_ERROR && request.bearer[0]==0);
    /* Confirmation succeeded at server, but local pruning failed: keep both. */
    toConfirm();failWrite=2;confirmation("wss://cem.local/s2");
    assert(response(200,30)==S2_SESSION_STORAGE_ERROR && disk.count==2);
    assert(S2_SESSION_TakeDetails(&session,&details,31)==S2_SESSION_STORAGE_ERROR);
    /* An atomic commit may complete before a backend reports an error. */
    reset();begin(0);failWrite=1;commitOnFailure=true;initiation(newToken,S2_SESSION_MESSAGE_VERSION);
    assert(response(200,1)==S2_SESSION_STORAGE_ERROR && disk.count==2);
    S2_SESSION_Clear(&session);writes=failWrite=0;begin(2);assert(strcmp(request.bearer,oldToken)==0);
    toConfirm();failWrite=2;commitOnFailure=true;confirmation("wss://cem.local/s2");
    assert(response(200,30)==S2_SESSION_STORAGE_ERROR && disk.count==1);
    S2_SESSION_Clear(&session);writes=failWrite=0;begin(31);assert(strcmp(request.bearer,newToken)==0);
    /* TLS authentication is required before parsing/persistence/fallback. */
    reset();begin(0);initiation(newToken,S2_SESSION_MESSAGE_VERSION);
    assert(S2_SESSION_Response(&session,200,body,strlen(body),false,1)==S2_SESSION_TLS_ERROR && writes==0);
    reset();begin(0);initiation(newToken,"1.0.0");assert(response(200,1)==S2_SESSION_INVALID && writes==0);
    reset();begin(0);initiation(oldToken,S2_SESSION_MESSAGE_VERSION);assert(response(200,1)==S2_SESSION_INVALID);
    invalidInitiation("{}");
    invalidInitiation("{\"selectedCommunicationProtocol\":42}");
    invalidInitiation("{\"selectedCommunicationProtocol\":\"WebSocket\",\"selectedCommunicationProtocol\":\"WebSocket\"}");
    invalidInitiation("{\"x\":\"\\u0000\"}");
    invalidInitiation("{} trailing");
    invalidInitiation("{\"x\":01}");
    invalidInitiation("{\"x\":1.}");
    invalidInitiation("{\"x\":1e+}");
    invalidInitiation("{\"x\":[[[[[[[[[[[[[[[[[0]]]]]]]]]]]]]]]]]}");
    reset();begin(0);initiation(newToken,S2_SESSION_MESSAGE_VERSION);
    char *pad=strstr(body,newToken);assert(pad);pad[strlen(newToken)-2]='B';
    assert(response(200,1)==S2_SESSION_INVALID && writes==0); /* Noncanonical base64 padding. */
    const char *badUris[]={"ws://cem.local/s2","wss://other.local/s2","wss://cem.local:444/s2", "wss://user@cem.local/s2", "wss://cem.local/s2#fragment", "wss://127.0.0.1/s2"};
    for(unsigned i=0;i<sizeof(badUris)/sizeof(badUris[0]);i++) {
        toConfirm();confirmation(badUris[i]);assert(response(200,30)==S2_SESSION_INVALID && disk.count==2 && writes==1);
    }
    reset();begin(0);strcpy(body,"{\"errorMessage\":\"NoLongerPaired\"}");assert(response(400,1)==S2_SESSION_UNPAIRED);
    reset();begin(0);strcpy(body,"{}");assert(response(401,1)==S2_SESSION_AUTH_ERROR);
    toConfirm();strcpy(body,"{}");assert(response(401,21)==S2_SESSION_AUTH_ERROR && disk.count==2);
    reset();begin(0);strcpy(body,"{}");assert(response(500,1)==S2_SESSION_HTTP_ERROR);
    reset();begin(UINT32_MAX-100u);S2_SESSION_Tasks(&session,(UINT32_MAX-100u)+14999u);
    assert(session.state==S2_SESSION_INITIATE);S2_SESSION_Tasks(&session,(UINT32_MAX-100u)+15000u);
    assert(session.error==S2_SESSION_EXPIRED);
    toConfirm();S2_SESSION_Tasks(&session,15000);assert(session.error==S2_SESSION_EXPIRED && disk.count==2);
    toConfirm();confirmation("wss://cem.local");assert(response(200,30)==S2_SESSION_OK);
    S2_SESSION_Tasks(&session,30020);assert(session.error==S2_SESSION_EXPIRED && session.details.commToken[0]==0);
    /* Allocation failure in shared JSON parser cannot advance token state. */
    reset();begin(0);initiation(newToken,S2_SESSION_MESSAGE_VERSION);
    cJSON_Hooks hooks={noMemory,free};cJSON_InitHooks(&hooks);
    assert(response(200,1)==S2_SESSION_INVALID && writes==0);cJSON_InitHooks(NULL);
    /* Embedded raw NUL is rejected using the counted HTTP body length. */
    reset();begin(0);initiation(newToken,S2_SESSION_MESSAGE_VERSION);size_t len=strlen(body);body[10]=0;
    assert(S2_SESSION_Response(&session,200,body,len,true,1)==S2_SESSION_INVALID && writes==0);
    reset();begin(0);strcpy(body,"{\"x\":\"x\"}");body[6]=(char)0xff;
    assert(response(200,1)==S2_SESSION_INVALID && writes==0);
    reset();begin(0);S2_SESSION_Tasks(&session,15000);
    assert(S2_SESSION_Begin(&session,&session.storage,session.base,session.client,
        session.server,&session.tokens,15001)==S2_SESSION_OK); /* Aliased reload inputs. */
    S2_SESSION_Clear(&session);
    const uint8_t *bytes=(const uint8_t*)&session;
    for(size_t i=0;i<sizeof(session);i++)assert(bytes[i]==0);
    printf("Session rotation, durable-save gates, restart recovery, validation and deadline tests passed. Context: %zu bytes.\n",sizeof(session));
    return 0;
}
