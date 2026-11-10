#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    NETWORK_MANAGER_NOT_INITIALIZED = 0,
    NETWORK_MANAGER_STACK_UNAVAILABLE,
    NETWORK_MANAGER_STACK_NOT_READY,
    NETWORK_MANAGER_STACK_ERROR,
    NETWORK_MANAGER_NO_INTERFACE,
    NETWORK_MANAGER_INTERFACE_DOWN,
    NETWORK_MANAGER_LINK_DOWN,
    NETWORK_MANAGER_WAITING_ADDRESS,
    NETWORK_MANAGER_READY,
    NETWORK_MANAGER_WAITING_MAC
} NETWORK_MANAGER_STATE;

typedef struct
{
    NETWORK_MANAGER_STATE state;
    /* Raw Harmony IPv4 representation, zero unless READY. */
    uint32_t ipv4Address;
    /* Changes when the observed state or address changes; may wrap. */
    uint32_t revision;
    uint32_t stackRestarts; /* Actual recovery teardowns since initialization. */
} NETWORK_MANAGER_STATUS;

/* Main-loop only. Call Initialize once after SYS_Initialize. */
void NETWORK_MANAGER_Initialize(void);
void NETWORK_MANAGER_Tasks(void);
/* Supply the verified I2C EUI once; the manager preserves the existing MAC mapping. */
bool NETWORK_MANAGER_SetHardwareEui(const uint8_t eui[8]);
/* Synchronous notification on readiness loss, IPv4 changes and before stack teardown.
 * Must close owned sockets, return promptly, and tolerate repeated calls.
 * Register before opening sockets. Maximum four distinct clients. */
typedef void (*NETWORK_MANAGER_DOWN_CALLBACK)(void);
bool NETWORK_MANAGER_RegisterDownCallback(NETWORK_MANAGER_DOWN_CALLBACK callback);
/* Last sampled status; READY does not guarantee server/Internet reachability. */
NETWORK_MANAGER_STATUS NETWORK_MANAGER_GetStatus(void);
bool NETWORK_MANAGER_IsReady(void);

#ifdef __cplusplus
}
#endif

#endif /* NETWORK_MANAGER_H */
