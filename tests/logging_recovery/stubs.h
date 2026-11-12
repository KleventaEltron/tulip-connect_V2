#ifndef LOGGING_RECOVERY_STUBS_H
#define LOGGING_RECOVERY_STUBS_H
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "app_logging_tasks.h"
typedef int TCPIP_NET_HANDLE;
typedef int SYS_STATUS;
typedef int TCPIP_DNS_RESULT;
typedef int TC_TIMER_STATUS;
typedef struct {uint32_t Val;} IPV4_ADDR;
#define SYS_STATUS_READY 1
#define TCPIP_DNS_TYPE_A 1
#define TCPIP_DNS_RES_NAME_IS_IPADDRESS 2
#define POST 1
#define HOST "fixture.invalid"
#define SYS_ASSERT(a,b) ((void)(a))
void testPrint(const char *,...);
#define SYS_CONSOLE_PRINT testPrint
#define SYS_CONSOLE_MESSAGE(s) testPrint("%s",s)
typedef struct {int tcpip;} TEST_SYSTEM;
extern TEST_SYSTEM sysObj;
extern APP_LOGGING_TASKS_DATA app_logging_tasksData;
extern bool firstStartAfterBoot;
uint64_t SYS_TIME_Counter64Get(void);
uint32_t SYS_TIME_FrequencyGet(void);
void initializeMutex(void);
bool releaseLoggingLock(void);
bool setLoggingLock(void);
bool NETWORK_MANAGER_RegisterDownCallback(void (*)(void));
bool NETWORK_MANAGER_IsReady(void);
void TC2_TimerStop(void);
void TC2_TimerStart(void);
void TC2_Timer32bitCounterSet(unsigned);
void TC2_TimerCallbackRegister(void (*)(TC_TIMER_STATUS,uintptr_t),uintptr_t);
void closeSocket(void);
void loggingNetworkTasks(void);
bool loggingIoFailed(void);
bool loggingTransmitBusy(void);
uint32_t loggingLastProgress(void);
uint32_t loggingDisplayRevision(void);
bool loggingConnectionReusable(void);
bool LoggingTimerExpiredSettingsInterval(void);
bool LoggingTimerExpired(void);
int TCPIP_STACK_Status(int);
void TCPIP_SNTP_ConnectionInitiate(void);
int TCPIP_STACK_NumberOfNetworksGet(void);
int TCPIP_STACK_IndexToNet(int);
bool TCPIP_STACK_NetIsReady(int);
uint32_t TCPIP_STACK_NetAddress(int);
bool getCurrentUtcTimestamp(void);
int TCPIP_DNS_Resolve(const char *,int);
int TCPIP_DNS_Send_Query(const char *,int);
bool waitOnDNS(void);
bool startSocketConnection(void);
typedef enum {SOCKET_NOT_CONNECTED,SSL_CREATE_CONNECTION_FAILED,SSL_CONNECTION_SUCCES} SSL_CONNECTION_STATES;
typedef enum {SSL_BUSY_NEGOTIATING,SSL_SOCKET_NOT_SECURE,SSL_NEGOTIATION_SUCCES} SSL_NEGOTIATION_STATES;
typedef enum {SSL_SOCKET_NOT_READY,SSL_SOCKET_WAS_DISCONNECTED,SSL_SOCKET_READY} SSL_SOCKET_STATES;
int waitForConnection(void);
int waitForSslConnection(void);
bool loggingRequestBuilder(int);
void resetRxBuffer(void);
int socketReady(void);
bool readServerResponseDone(void);
bool areUpdateSettingsAvailable(void);
bool getSettingChangedInDisplay(void);
bool getUpdateSettingsFromServer(void);
bool parse_modbus_settings(void);
void resetLoggingTimers(void);
void setNewLogRequired(bool);
void setSecondCounterDelayAfterChangingSettings(uint32_t);
uint32_t getSecondCounterDelayAfterChangingSettings(void);
unsigned getSettingsQueuedAmount(void);
bool sendUpdatedSettingsList(void);
bool readServerResponseUpdatedSettingsDone(void);
void setSettingChangedInDisplay(bool);
#endif
