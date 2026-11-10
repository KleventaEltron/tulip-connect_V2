#ifndef NETWORK_MANAGER_TEST_DEFINITIONS_H
#define NETWORK_MANAGER_TEST_DEFINITIONS_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef uintptr_t SYS_MODULE_OBJ;
typedef int SYS_STATUS;
typedef void *TCPIP_NET_HANDLE;
#define SYS_MODULE_OBJ_INVALID ((SYS_MODULE_OBJ)-1)
#define SYS_STATUS_READY 1
typedef struct { SYS_MODULE_OBJ tcpip; } SYSTEM_OBJECTS;
extern SYSTEM_OBJECTS sysObj;
SYS_STATUS TCPIP_STACK_Status(SYS_MODULE_OBJ object);
TCPIP_NET_HANDLE TCPIP_STACK_NetHandleGet(const char *name);
bool TCPIP_STACK_NetIsUp(TCPIP_NET_HANDLE handle);
bool TCPIP_STACK_NetIsLinked(TCPIP_NET_HANDLE handle);
bool TCPIP_STACK_NetIsReady(TCPIP_NET_HANDLE handle);
uint32_t TCPIP_STACK_NetAddress(TCPIP_NET_HANDLE handle);
typedef struct { int unused; } SYS_MODULE_INIT;
typedef struct { int unused; } TCPIP_STACK_MODULE_CONFIG;
typedef struct {
 const char *interface, *hostName, *macAddr, *ipAddr, *ipMask, *gateway;
 const char *priDNS, *secondDNS, *powerMode;
 unsigned startFlags;
 const void *pMacObject;
} TCPIP_NETWORK_CONFIG;
typedef struct {
 SYS_MODULE_INIT moduleInit;
 const TCPIP_NETWORK_CONFIG *pNetConf;
 size_t nNets;
 const TCPIP_STACK_MODULE_CONFIG *pModConfig;
 size_t nModules;
 void *initCback;
} TCPIP_STACK_INIT;
#define TCPIP_NETWORK_DEFAULT_INTERFACE_NAME_IDX0 "eth0"
#define TCPIP_NETWORK_DEFAULT_HOST_NAME_IDX0 "Tulip"
#define TCPIP_NETWORK_DEFAULT_IP_ADDRESS_IDX0 "0.0.0.0"
#define TCPIP_NETWORK_DEFAULT_IP_MASK_IDX0 "255.255.255.0"
#define TCPIP_NETWORK_DEFAULT_GATEWAY_IDX0 "0.0.0.0"
#define TCPIP_NETWORK_DEFAULT_DNS_IDX0 "0.0.0.0"
#define TCPIP_NETWORK_DEFAULT_SECOND_DNS_IDX0 "0.0.0.0"
#define TCPIP_NETWORK_DEFAULT_POWER_MODE_IDX0 "full"
#define TCPIP_NETWORK_DEFAULT_INTERFACE_FLAGS_IDX0 1u
extern const int testMacDriver;
#define TCPIP_NETWORK_DEFAULT_MAC_DRIVER_IDX0 testMacDriver
#define SYS_CONSOLE_PRINT(...) ((void)0)
uint64_t SYS_TIME_Counter64Get(void);
uint32_t SYS_TIME_FrequencyGet(void);
SYS_MODULE_OBJ TCPIP_STACK_Initialize(int index, const SYS_MODULE_INIT *init);
void TCPIP_STACK_Deinitialize(SYS_MODULE_OBJ object);
#endif
