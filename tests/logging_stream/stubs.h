#ifndef LOGGING_STREAM_STUBS_H
#define LOGGING_STREAM_STUBS_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "logging_http.h"
#include "cJSON.h"
#include "eeprom.h"
#define READ_BUFFER_SIZE 4096
#define READ_CHUNK_SIZE 512
#define DEVICE_JSON_MAX 8192u
#define CASCADE_BIT_IS_SET(mask,bit) (((mask)&(1u<<(bit)))!=0)
#define MASTER_HEATPUMP_IN_CASCADE 1
#define ADDRESS_FREQUENCY_CONVERSION_MODE 0x100
#define MAX_SETTINGS 50
#define INVALID_SOCKET -1
#define HOST "fixture.invalid"
#define TOKEN "synthetic-test-token"
#define SYS_CONSOLE_PRINT(...) ((void)0)
#define SYS_CONSOLE_MESSAGE(...) ((void)0)
#define TULIP_PRINT 0
#define HEATPUMP 1
int log_socket=1;
static char settingsNetworkBuffer[4096],devObjSendBuffer[8192],NTP_TIME_BUFFER[40]="2026-11-12 12:00:00";
static struct {char bytes[4097];uint32_t guard;} guardedRx;
#define readBuffer guardedRx.bytes
static size_t buffer_len;
static int DEVICE_TYPE_REQUESTED,setting_count;
static uint8_t eui64[8]={0};
static uint64_t now;
static uint16_t mask;
static bool connected=true,resetPeer,blockWrite;
static size_t pending,wireLength,inputLength,inputOffset,writeSize=37,readSize=23;
static char wire[300000],input[8000];
static uint16_t ee[4096];
static unsigned builds[17],applied;
static uint16_t lastAddress,lastValue;
uint64_t SYS_TIME_Counter64Get(void) {return now;}
uint32_t SYS_TIME_FrequencyGet(void) {return 1000;}
bool NET_PRES_SocketIsConnected(int h) {(void)h;return connected;}
bool NET_PRES_SocketWasDisconnected(int h) {(void)h;return !connected;}
bool NET_PRES_SocketWasReset(int h) {(void)h;return resetPeer;}
uint16_t NET_PRES_SocketWriteIsReady(int h,uint16_t n,uint16_t minimum) {(void)h;(void)minimum;return !blockWrite&&pending+n+64<=4096?n:0;}
uint16_t NET_PRES_SocketWrite(int h,const uint8_t *p,uint16_t n) {(void)h;if(n>writeSize)n=(uint16_t)writeSize;memcpy(wire+wireLength,p,n);wireLength+=n;pending+=n;return n;}
uint16_t NET_PRES_SocketReadIsReady(int h) {(void)h;return (uint16_t)(inputLength-inputOffset);}
uint16_t NET_PRES_SocketRead(int h,uint8_t *p,uint16_t n) {(void)h;if(n>readSize)n=(uint16_t)readSize;memcpy(p,input+inputOffset,n);inputOffset+=n;return n;}
uint16_t getCascadeSlaveStatus(void) {return mask;}
void parseDeviceType(int type,char *out) {strcpy(out,type==0?"TL":"WL");}
bool setLoggingDataPerDeviceType(char *out,size_t n,char *device,int index) {
    char padding[2001];memset(padding,'a',2000);padding[2000]=0;builds[index]++;
    return snprintf(out,n,"{\"device\":\"%s\",\"index\":%d,\"sample\":%u,\"payload\":\"%s\"}",device,index,builds[index],padding)>0;
}
uint16_t getDataFromMemoryCallable(uint16_t addr,uint16_t pump) {(void)pump;return addr==ADDRESS_FREQUENCY_CONVERSION_MODE?2:(uint16_t)(addr*3u);}
uint8_t ReadSmartEeprom8(uint32_t addr) {return (uint8_t)ee[addr];}
uint16_t ReadSmartEeprom16(uint32_t addr) {return ee[addr];}
void WriteSmartEeprom8(uint32_t addr,uint8_t value) {ee[addr]=value;}
void processModbusSettingsFromServer(uint16_t addr,uint16_t value) {applied++;lastAddress=addr;lastValue=value;}
#endif
