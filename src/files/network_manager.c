#include "network_manager.h"
#include "definitions.h"
#include <stdio.h>
#include <string.h>

extern const TCPIP_STACK_MODULE_CONFIG TCPIP_STACK_MODULE_CONFIG_TBL[];
extern const size_t TCPIP_STACK_MODULE_CONFIG_TBL_SIZE;

#define RETRY_MS 5000u
#define STARTUP_MS 30000u
#define ADDRESS_MS 120000u
#define MAX_CLIENTS 4u
static NETWORK_MANAGER_DOWN_CALLBACK clients[MAX_CLIENTS];
static uint8_t hardwareEui[8];
static bool macAvailable, retryWaiting;
static uint64_t retrySince, phaseSince;
static NETWORK_MANAGER_STATE timedPhase;

static void notifyClients(void)
{
    unsigned i;
    for (i = 0; i < MAX_CLIENTS; i++)
        if (clients[i] != NULL) clients[i]();
}

static bool elapsed(uint64_t since, uint32_t ms)
{
    /* SYSTICK runs fast enough that a 32-bit count cannot hold 120 seconds. */
    return (uint64_t)(SYS_TIME_Counter64Get() - since) >=
           ((uint64_t)SYS_TIME_FrequencyGet() * ms) / 1000u;
}


static bool initialized = false;
static NETWORK_MANAGER_STATUS status;

static bool startStack(void) {
    TCPIP_STACK_INIT tcpipInit = {0};
    
    static char macToString[40];
    
    /* Preserve the deployed EUI-to-MAC mapping during this ownership change. */
    uint8_t firstMacByte = hardwareEui[2];
    firstMacByte |= 0x02;  // Set "locally administered" bit
    firstMacByte &= ~0x01; 
    
    snprintf(macToString, sizeof(macToString), "%02X:%02X:%02X:%02X:%02X:%02X", 
            firstMacByte,hardwareEui[3],hardwareEui[4],hardwareEui[5],hardwareEui[6],hardwareEui[7]);

    
    static TCPIP_NETWORK_CONFIG TCPIP_HOSTS_CONFIGURATION_CUSTOM[] =
    {
        /*** Network Configuration Index 0 ***/
        {
            .interface = TCPIP_NETWORK_DEFAULT_INTERFACE_NAME_IDX0,
            .hostName = TCPIP_NETWORK_DEFAULT_HOST_NAME_IDX0,
            .macAddr = macToString, // Change default MAC to MAC obtained from IC
            .ipAddr = TCPIP_NETWORK_DEFAULT_IP_ADDRESS_IDX0,
            .ipMask = TCPIP_NETWORK_DEFAULT_IP_MASK_IDX0,
            .gateway = TCPIP_NETWORK_DEFAULT_GATEWAY_IDX0,
            .priDNS = TCPIP_NETWORK_DEFAULT_DNS_IDX0,
            .secondDNS = TCPIP_NETWORK_DEFAULT_SECOND_DNS_IDX0,
            .powerMode = TCPIP_NETWORK_DEFAULT_POWER_MODE_IDX0,
            .startFlags = TCPIP_NETWORK_DEFAULT_INTERFACE_FLAGS_IDX0,
            .pMacObject = &TCPIP_NETWORK_DEFAULT_MAC_DRIVER_IDX0,
        },
    };
    size_t TCPIP_HOSTS_CONFIGURATION_SIZE_CUSTOM = sizeof (TCPIP_HOSTS_CONFIGURATION_CUSTOM) / sizeof (*TCPIP_HOSTS_CONFIGURATION_CUSTOM);
    
    tcpipInit.pNetConf = TCPIP_HOSTS_CONFIGURATION_CUSTOM;
    tcpipInit.nNets = TCPIP_HOSTS_CONFIGURATION_SIZE_CUSTOM;
    tcpipInit.pModConfig = TCPIP_STACK_MODULE_CONFIG_TBL;
    tcpipInit.nModules = TCPIP_STACK_MODULE_CONFIG_TBL_SIZE;
    tcpipInit.initCback = 0;

    sysObj.tcpip = TCPIP_STACK_Initialize(0, &tcpipInit.moduleInit);
    if (sysObj.tcpip == SYS_MODULE_OBJ_INVALID) {
        SYS_CONSOLE_PRINT("TCPIP_STACK_Init Failed!\n");
        return false;
    }    
    return true;
}


static void publishStatus(NETWORK_MANAGER_STATE state, uint32_t address)
{
    if (status.state != state || status.ipv4Address != address)
    {
        if (status.state == NETWORK_MANAGER_READY &&
            (state != NETWORK_MANAGER_READY || status.ipv4Address != address))
            notifyClients();
        status.state = state;
        status.ipv4Address = address;
        status.revision++;
    }
}

void NETWORK_MANAGER_Initialize(void)
{
    status.state = NETWORK_MANAGER_NOT_INITIALIZED;
    status.ipv4Address = 0;
    status.revision = 0;
    status.stackRestarts = 0;
    memset(clients, 0, sizeof(clients));
    macAvailable = false;
    retryWaiting = false;
    timedPhase = NETWORK_MANAGER_NOT_INITIALIZED;
    initialized = true;
}

bool NETWORK_MANAGER_SetHardwareEui(const uint8_t eui[8])
{
    if (eui == NULL || macAvailable) return false;
    memcpy(hardwareEui, eui, sizeof(hardwareEui));
    macAvailable = true;
    return true;
}

bool NETWORK_MANAGER_RegisterDownCallback(NETWORK_MANAGER_DOWN_CALLBACK callback)
{
    unsigned i;
    if (callback == NULL) return false;
    for (i = 0; i < MAX_CLIENTS; i++)
        if (clients[i] == callback) return true;
    for (i = 0; i < MAX_CLIENTS; i++)
        if (clients[i] == NULL) { clients[i] = callback; return true; }
    return false;
}

static void recoverStack(void)
{
    /* Clients close presentation sockets while the stack still exists. */
    if (status.state != NETWORK_MANAGER_READY) notifyClients();
    publishStatus(NETWORK_MANAGER_STACK_UNAVAILABLE, 0);
    status.stackRestarts++;
    if (sysObj.tcpip != SYS_MODULE_OBJ_INVALID)
        TCPIP_STACK_Deinitialize(sysObj.tcpip);
    sysObj.tcpip = SYS_MODULE_OBJ_INVALID;
    retrySince = SYS_TIME_Counter64Get();
    retryWaiting = true;
    timedPhase = NETWORK_MANAGER_NOT_INITIALIZED;
    publishStatus(NETWORK_MANAGER_STACK_UNAVAILABLE, 0);
}

static void watchPhase(NETWORK_MANAGER_STATE phase, uint32_t timeout)
{
    if (timedPhase != phase)
    {
        timedPhase = phase;
        phaseSince = SYS_TIME_Counter64Get();
    }
    else if (elapsed(phaseSince, timeout)) recoverStack();
}

void NETWORK_MANAGER_Tasks(void)
{
    SYS_STATUS stackStatus;
    TCPIP_NET_HANDLE interfaceHandle;
    uint32_t address;

    if (!initialized)
    {
        return;
    }

    if (!macAvailable)
    {
        publishStatus(NETWORK_MANAGER_WAITING_MAC, 0);
        return;
    }
    if (sysObj.tcpip == SYS_MODULE_OBJ_INVALID)
    {
        publishStatus(NETWORK_MANAGER_STACK_UNAVAILABLE, 0);
        if (retryWaiting && !elapsed(retrySince, RETRY_MS)) return;
        retryWaiting = false;
        if (!startStack())
        {
            retryWaiting = true;
            retrySince = SYS_TIME_Counter64Get();
        }
        timedPhase = NETWORK_MANAGER_NOT_INITIALIZED;
        return;
    }

    stackStatus = TCPIP_STACK_Status(sysObj.tcpip);
    if (stackStatus < 0)
    {
        recoverStack();
        return;
    }
    if (stackStatus != SYS_STATUS_READY)
    {
        publishStatus(NETWORK_MANAGER_STACK_NOT_READY, 0);
        watchPhase(NETWORK_MANAGER_STACK_NOT_READY, STARTUP_MS);
        return;
    }

    /* Never retain interface handles across stack recovery. */
    interfaceHandle = TCPIP_STACK_NetHandleGet("eth0");
    if (interfaceHandle == NULL)
    {
        publishStatus(NETWORK_MANAGER_NO_INTERFACE, 0);
        watchPhase(NETWORK_MANAGER_NO_INTERFACE, STARTUP_MS);
        return;
    }
    if (!TCPIP_STACK_NetIsUp(interfaceHandle))
    {
        publishStatus(NETWORK_MANAGER_INTERFACE_DOWN, 0);
        watchPhase(NETWORK_MANAGER_INTERFACE_DOWN, STARTUP_MS);
        return;
    }
    if (!TCPIP_STACK_NetIsLinked(interfaceHandle))
    {
        publishStatus(NETWORK_MANAGER_LINK_DOWN, 0);
        timedPhase = NETWORK_MANAGER_NOT_INITIALIZED;
        return;
    }
    if (!TCPIP_STACK_NetIsReady(interfaceHandle))
    {
        publishStatus(NETWORK_MANAGER_WAITING_ADDRESS, 0);
        watchPhase(NETWORK_MANAGER_WAITING_ADDRESS, ADDRESS_MS);
        return;
    }

    address = TCPIP_STACK_NetAddress(interfaceHandle);
    if (address == 0)
    {
        publishStatus(NETWORK_MANAGER_WAITING_ADDRESS, 0);
        watchPhase(NETWORK_MANAGER_WAITING_ADDRESS, ADDRESS_MS);
        return;
    }

    timedPhase = NETWORK_MANAGER_NOT_INITIALIZED;
    publishStatus(NETWORK_MANAGER_READY, address);
}

NETWORK_MANAGER_STATUS NETWORK_MANAGER_GetStatus(void)
{
    return status;
}

bool NETWORK_MANAGER_IsReady(void)
{
    return status.state == NETWORK_MANAGER_READY;
}
