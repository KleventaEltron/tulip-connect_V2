#include "stubs.h"
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
TEST_SYSTEM sysObj;
static uint64_t now;
static bool network,owned,settingsDue,fullDue,failOpen,ioFail,txBusy,reusable,respond,changes,serverCloses;
static unsigned opens,closes,locks,releases,overviews,requests,uploads,dnsForce;
static uint32_t lastProgress,displayRevision;
static bool displayChanged;
static void (*down)(void);
uint64_t SYS_TIME_Counter64Get(void) {return now;}
uint32_t SYS_TIME_FrequencyGet(void) {return 1000;}
void testPrint(const char *p,...) {(void)p;}
void initializeMutex(void) {}
bool releaseLoggingLock(void) {assert(owned);owned=false;releases++;return true;}
bool setLoggingLock(void) {assert(!owned);owned=true;locks++;return true;}
bool NETWORK_MANAGER_RegisterDownCallback(void (*cb)(void)) {down=cb;return true;}
bool NETWORK_MANAGER_IsReady(void) {return network;}
void TC2_TimerStop(void) {}
void TC2_TimerStart(void) {}
void TC2_Timer32bitCounterSet(unsigned n) {(void)n;}
void TC2_TimerCallbackRegister(void (*cb)(TC_TIMER_STATUS,uintptr_t),uintptr_t u) {(void)cb;(void)u;}
void closeSocket(void) {closes++;ioFail=txBusy=false;reusable=false;}
void loggingNetworkTasks(void) {}
bool loggingIoFailed(void) {return ioFail;}
bool loggingTransmitBusy(void) {return txBusy;}
uint32_t loggingLastProgress(void) {return lastProgress;}
uint32_t loggingDisplayRevision(void) {return displayRevision;}
bool loggingConnectionReusable(void) {return reusable;}
bool LoggingTimerExpiredSettingsInterval(void) {bool b=settingsDue;settingsDue=false;return b;}
bool LoggingTimerExpired(void) {bool b=fullDue;fullDue=false;return b;}
int TCPIP_STACK_Status(int h) {(void)h;return SYS_STATUS_READY;}
void TCPIP_SNTP_ConnectionInitiate(void) {}
int TCPIP_STACK_NumberOfNetworksGet(void) {return 1;}
int TCPIP_STACK_IndexToNet(int i) {return i;}
bool TCPIP_STACK_NetIsReady(int h) {(void)h;return true;}
uint32_t TCPIP_STACK_NetAddress(int h) {(void)h;return 1;}
bool getCurrentUtcTimestamp(void) {return true;}
int TCPIP_DNS_Resolve(const char *h,int k) {(void)h;(void)k;return 0;}
int TCPIP_DNS_Send_Query(const char *h,int k) {dnsForce++;return TCPIP_DNS_Resolve(h,k);}
bool waitOnDNS(void) {return true;}
bool startSocketConnection(void) {opens++;lastProgress=(uint32_t)now;return !failOpen;}
int waitForConnection(void) {return SSL_CONNECTION_SUCCES;}
int waitForSslConnection(void) {return SSL_NEGOTIATION_SUCCES;}
bool loggingRequestBuilder(int k) {(void)k;uploads++;lastProgress=(uint32_t)now;return true;}
void resetRxBuffer(void) {}
int socketReady(void) {return respond?SSL_SOCKET_READY:SSL_SOCKET_NOT_READY;}
bool readServerResponseDone(void) {if(respond) {lastProgress=(uint32_t)now;reusable=!serverCloses;}return respond;}
bool areUpdateSettingsAvailable(void) {return changes;}
bool getSettingChangedInDisplay(void) {return displayChanged;}
bool getUpdateSettingsFromServer(void) {requests++;lastProgress=(uint32_t)now;return true;}
bool parse_modbus_settings(void) {return false;}
void resetLoggingTimers(void) {}
void setNewLogRequired(bool v) {(void)v;}
void setSecondCounterDelayAfterChangingSettings(uint32_t n) {(void)n;}
uint32_t getSecondCounterDelayAfterChangingSettings(void) {return 100;}
unsigned getSettingsQueuedAmount(void) {return 0;}
bool sendUpdatedSettingsList(void) {overviews++;lastProgress=(uint32_t)now;return true;}
bool readServerResponseUpdatedSettingsDone(void) {return respond;}
void setSettingChangedInDisplay(bool v) {displayChanged=v;}
static void step(void) {APP_LOGGING_TASKS_Tasks();now++;}
static void until(APP_LOGGING_TASKS_STATES state) {
    unsigned i;for(i=0;i<1000&&app_logging_tasksData.state!=state;i++)step();assert(app_logging_tasksData.state==state);
}
static void setup(void) {
    now=0;network=true;owned=false;settingsDue=fullDue=failOpen=ioFail=txBusy=changes=displayChanged=false;
    respond=true;reusable=true;serverCloses=false;opens=closes=locks=releases=overviews=requests=uploads=dnsForce=0;lastProgress=0;
    firstStartAfterBoot=true;APP_LOGGING_TASKS_Initialize();step();assert(app_logging_tasksData.state==APP_LOGGING_TASKS_IDLE);
}
int main(void) {
    unsigned saved;
    setup();settingsDue=true;until(APP_LOGGING_TASKS_WAIT_CONFIRM_CHANGED_SETTINGS);assert(overviews==1&&firstStartAfterBoot);
    until(APP_LOGGING_TASKS_IDLE);assert(!firstStartAfterBoot&&locks==releases);
    setup();settingsDue=true;failOpen=true;until(APP_LOGGING_TASKS_START_CONNECTION);step();step();assert(opens==1&&!owned);
    saved=opens;for(unsigned i=0;i<50;i++)step();assert(opens==saved);now+=5000;failOpen=false;until(APP_LOGGING_TASKS_WAIT_CONFIRM_CHANGED_SETTINGS);assert(dnsForce&&opens==2);
    until(APP_LOGGING_TASKS_IDLE);assert(locks==releases);
    setup();settingsDue=true;until(APP_LOGGING_TASKS_WAIT_FOR_SSL_CONNECT);network=false;down();assert(!owned&&app_logging_tasksData.state==APP_LOGGING_TASKS_IDLE);
    saved=closes;down();assert(closes==saved+1&&!owned);now+=300000;network=true;until(APP_LOGGING_TASKS_WAIT_CONFIRM_CHANGED_SETTINGS);until(APP_LOGGING_TASKS_IDLE);assert(locks==releases);
    setup();settingsDue=true;until(APP_LOGGING_TASKS_WAIT_RECEIVE_SOCKET_READY_UPDATE_SETTINGS);txBusy=true;saved=requests;
    for(unsigned i=0;i<100;i++)step();assert(requests==saved&&app_logging_tasksData.state==APP_LOGGING_TASKS_WAIT_RECEIVE_SOCKET_READY_UPDATE_SETTINGS);
    txBusy=false;respond=false;now+=16000;step();assert(!owned&&app_logging_tasksData.state==APP_LOGGING_TASKS_IDLE);
    setup();fullDue=true;changes=true;reusable=false;serverCloses=true;until(APP_LOGGING_TASKS_WAIT_CONFIRM_CHANGED_SETTINGS);assert(uploads==1&&requests==1&&overviews==1&&opens==3);
    until(APP_LOGGING_TASKS_IDLE);assert(locks==releases);
    setup();settingsDue=true;until(APP_LOGGING_TASKS_WAIT_CONFIRM_CHANGED_SETTINGS);ioFail=true;step();assert(firstStartAfterBoot&&!owned);
    now+=5000;until(APP_LOGGING_TASKS_WAIT_CONFIRM_CHANGED_SETTINGS);until(APP_LOGGING_TASKS_IDLE);assert(overviews==2&&!firstStartAfterBoot);
    setup();settingsDue=true;displayChanged=true;displayRevision=1;until(APP_LOGGING_TASKS_WAIT_CONFIRM_CHANGED_SETTINGS);displayRevision=2;
    until(APP_LOGGING_TASKS_IDLE);assert(displayChanged); /* Later edits aren't acknowledged by earlier upload. */
    puts("Logging recovery state machine: retry delay, forced DNS refresh, cable loss, lock ownership, paced send waits, timeouts, HTTP reconnect, pending overview and later display changes passed.");return 0;
}
