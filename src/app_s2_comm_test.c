#include "app_s2_comm_test.h"
#include "files/s2/s2_comm_test_config.h"
#if TULIP_S2_COMM_TEST_ENABLE
#include "app_s2_session_tasks.h"
#include "files/s2/s2_test_messages.h"
#include "files/s2/s2_token_store_same54.h"
#include "files/s2/s2_comm_test_local.h"
#include "files/crypto_random.h"
#include "files/network_manager.h"
#include "definitions.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
static APP_S2_SESSION_WORKSPACE work;
static S2_TEST_MESSAGES messages;
static S2_TOKEN_STORE_SAME54 backend;
static S2_TOKEN_STORE store;
static bool armed,running,passed;
static uint32_t started;
static uint32_t milliseconds(void) {
    uint64_t n=SYS_TIME_Counter64Get();uint32_t hz=SYS_TIME_FrequencyGet();
    return hz?(uint32_t)((n/hz)*1000u+(n%hz)*1000u/hz):0;
}
static bool beginMessages(void) {
    started=milliseconds();return S2_TEST_Start(&messages,S2_COMM_TEST_CLIENT,TULIP_CryptoRandom);
}
static bool receive(void *u,const char *text,size_t n) {
    (void)u;
    if(!messages.active&&!beginMessages())return false;
    return S2_TEST_Receive(&messages,text,n);
}
static void failed(const char *why) {
    SYS_CONSOLE_PRINT("S2 COMM TEST FAILED: %s\r\n",why);
    APP_S2_SESSION_Stop();S2_TEST_Reset(&messages);running=armed=false;
}
void APP_S2_COMM_TEST_Initialize(void) { armed=true; }
void APP_S2_COMM_TEST_Tasks(void) {
    if(armed&&!running) {
        S2_TOKEN_STORE_IO io;S2_SESSION_TOKENS saved={0};S2_TOKEN_STORE_RESULT result;
        APP_S2_SESSION_CONFIG cfg;
        if(!NETWORK_MANAGER_IsReady()||!S2_TLS_ClockReady())return;
        if(!S2_TOKEN_STORE_SAME54_Init(&backend,&io)||!S2_TOKEN_STORE_Init(&store,&io,s2_comm_test_binding)) {failed("EEPROM configuration/status");return;}
        result=S2_TOKEN_STORE_Load(&store,&saved);
        if(result==S2_TOKEN_STORE_EMPTY) {
            saved.count=1;strcpy(saved.token[0],S2_COMM_TEST_INITIAL_TOKEN);
            if(!S2_TOKEN_STORE_Persist(&store,&saved)) {memset(&saved,0,sizeof(saved));failed("initial token save");return;}
        } else if(result!=S2_TOKEN_STORE_OK||saved.count==0) {
            memset(&saved,0,sizeof(saved));failed("token store invalid or different pairing; not overwritten");return;
        }
        memset(&saved,0,sizeof(saved));memset(&cfg,0,sizeof(cfg));
        cfg.apiBase=S2_COMM_TEST_API;cfg.clientNodeId=S2_COMM_TEST_CLIENT;cfg.serverNodeId=S2_COMM_TEST_SERVER;
        cfg.caDer=s2_comm_test_ca;cfg.caLength=sizeof(s2_comm_test_ca);cfg.tokens=&store;
        cfg.text=receive;cfg.ipv4Override=S2_COMM_TEST_IPV4;
        if(!APP_S2_SESSION_Configure(&work,&cfg)) {failed("coordinator configuration");return;}
        running=true;SYS_CONSOLE_PRINT("S2 COMM TEST: starting simulated NOT_CONTROLABLE resource\r\n");
    }
    if(!running)return;
    if(APP_S2_SESSION_GetStatus().state==APP_S2_SESSION_BLOCKED) {
        SYS_CONSOLE_PRINT("S2 COMM TEST: coordinator error=%u TLS=%u HTTP=%u\r\n",(unsigned)APP_S2_SESSION_GetStatus().error,(unsigned)APP_S2_SESSION_GetStatus().tlsError,APP_S2_SESSION_GetStatus().httpStatus);
        failed("session blocked");return;
    }
    if(messages.failed) {failed("message validation or queue");return;}
    switch(APP_S2_GetStatus().state) {
        case APP_S2_WS_OPEN: {
            const char *out;
            if(!messages.active&&!beginMessages()) {failed("random source");return;}
            if(messages.selected&&messages.detailsAck&&!messages.measurementId[0]) {
                char timestamp[24];time_t utc=(time_t)TCPIP_SNTP_UTCSecondsGet();struct tm *tm=gmtime(&utc);
                if(!tm||!strftime(timestamp,sizeof(timestamp),"%Y-%m-%dT%H:%M:%SZ",tm)||!S2_TEST_Measurement(&messages,timestamp)) {failed("test measurement");return;}
            }
            out=S2_TEST_Peek(&messages);
            if(out) {
                if(APP_S2_SendText(out,strlen(out))!=S2_WS_OK) {failed("send queue");return;}
                S2_TEST_Sent(&messages);
            }
            if(!passed&&messages.handshakeAck&&messages.detailsAck&&messages.measurementAck&&messages.selected&&messages.rejectedControl&&messages.count==0) {
                passed=true;SYS_CONSOLE_PRINT("S2 COMM TEST PASS: handshake, resource details, simulated measurement and rejected control acknowledged; TX=%lu RX=%lu\r\n",(unsigned long)messages.sent,(unsigned long)messages.received);
            }
            if(!passed&&(uint32_t)(milliseconds()-started)>30000u)failed("message exchange timeout");
            break;
        }
        default:
            if(messages.active) {S2_TEST_Reset(&messages);passed=false;}
            break;
    }
}
#else
void APP_S2_COMM_TEST_Initialize(void) {}
void APP_S2_COMM_TEST_Tasks(void) {}
#endif
