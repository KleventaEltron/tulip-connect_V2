#include "network_manager.h"
#include "definitions.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

SYSTEM_OBJECTS sysObj;
static SYS_STATUS stackStatus;
static TCPIP_NET_HANDLE currentHandle;
static bool interfaceUp, linked, addressReady;
static uint32_t address;
static unsigned stackQueries, interfaceQueries;

SYS_STATUS TCPIP_STACK_Status(SYS_MODULE_OBJ object)
{
    assert(object == sysObj.tcpip && object != SYS_MODULE_OBJ_INVALID);
    stackQueries++;
    return stackStatus;
}
TCPIP_NET_HANDLE TCPIP_STACK_NetHandleGet(const char *name)
{
    assert(stackStatus == SYS_STATUS_READY && strcmp(name, "eth0") == 0);
    interfaceQueries++;
    return currentHandle;
}
bool TCPIP_STACK_NetIsUp(TCPIP_NET_HANDLE handle)
{
    assert(handle != NULL && handle == currentHandle);
    return interfaceUp;
}
bool TCPIP_STACK_NetIsLinked(TCPIP_NET_HANDLE handle)
{
    assert(handle != NULL && handle == currentHandle && interfaceUp);
    return linked;
}
bool TCPIP_STACK_NetIsReady(TCPIP_NET_HANDLE handle)
{
    assert(handle != NULL && handle == currentHandle && linked);
    return addressReady;
}
uint32_t TCPIP_STACK_NetAddress(TCPIP_NET_HANDLE handle)
{
    assert(handle != NULL && handle == currentHandle && addressReady);
    return address;
}
static void expect(NETWORK_MANAGER_STATE state, uint32_t expectedAddress)
{
    NETWORK_MANAGER_Tasks();
    NETWORK_MANAGER_STATUS snapshot = NETWORK_MANAGER_GetStatus();
    assert(snapshot.state == state && snapshot.ipv4Address == expectedAddress);
    assert(NETWORK_MANAGER_IsReady() == (state == NETWORK_MANAGER_READY));
}
static uint64_t now;
static unsigned starts, stops, notifications;
static bool failStart, clientSocket;
const int testMacDriver = 0;
const TCPIP_STACK_MODULE_CONFIG TCPIP_STACK_MODULE_CONFIG_TBL[] = {{0}};
const size_t TCPIP_STACK_MODULE_CONFIG_TBL_SIZE = 1;
uint64_t SYS_TIME_Counter64Get(void) { return now; }
uint32_t SYS_TIME_FrequencyGet(void) { return 120000000u; }
SYS_MODULE_OBJ TCPIP_STACK_Initialize(int index, const SYS_MODULE_INIT *init)
{
    const TCPIP_STACK_INIT *config = (const TCPIP_STACK_INIT *)init;
    assert(index == 0 && sysObj.tcpip == SYS_MODULE_OBJ_INVALID);
    assert(config->nNets == 1 && config->nModules == 1);
    assert(strcmp(config->pNetConf->macAddr, "3E:11:22:33:44:55") == 0);
    starts++;
    stackStatus = 0;
    return failStart ? SYS_MODULE_OBJ_INVALID : 1;
}
void TCPIP_STACK_Deinitialize(SYS_MODULE_OBJ object)
{
    assert(object == sysObj.tcpip && !clientSocket);
    assert(notifications > stops); /* Clients notified before teardown. */
    stops++;
    currentHandle = NULL;
}
static void clientDown(void)
{
    assert(sysObj.tcpip != SYS_MODULE_OBJ_INVALID);
    notifications++;
    clientSocket = false;
}
static void other1(void) {}
static void other2(void) {}
static void other3(void) {}
static void other4(void) {}
static void tick(uint32_t ms) { now += (uint64_t)ms * 120000u; NETWORK_MANAGER_Tasks(); }
int main(void)
{
    int interfaceA, interfaceB;
    const uint8_t eui[8] = {0xFC,0xC2,0x3D,0x11,0x22,0x33,0x44,0x55};
    sysObj.tcpip = SYS_MODULE_OBJ_INVALID;
    NETWORK_MANAGER_Tasks();
    assert(stackQueries == 0 && starts == 0);
    NETWORK_MANAGER_Initialize();
    expect(NETWORK_MANAGER_WAITING_MAC, 0);
    tick(999999);
    assert(starts == 0);
    assert(!NETWORK_MANAGER_SetHardwareEui(NULL));
    assert(NETWORK_MANAGER_SetHardwareEui(eui));
    assert(!NETWORK_MANAGER_SetHardwareEui(eui));
    assert(!NETWORK_MANAGER_RegisterDownCallback(NULL));
    assert(NETWORK_MANAGER_RegisterDownCallback(clientDown));
    assert(NETWORK_MANAGER_RegisterDownCallback(clientDown));
    assert(NETWORK_MANAGER_RegisterDownCallback(other1));
    assert(NETWORK_MANAGER_RegisterDownCallback(other2));
    assert(NETWORK_MANAGER_RegisterDownCallback(other3));
    assert(!NETWORK_MANAGER_RegisterDownCallback(other4));
    failStart = true;
    expect(NETWORK_MANAGER_STACK_UNAVAILABLE, 0);
    assert(starts == 1);
    tick(4999); assert(starts == 1);
    failStart = false;
    tick(1); assert(starts == 2);
    expect(NETWORK_MANAGER_STACK_NOT_READY, 0);
    tick(29999); assert(stops == 0);
    tick(1); assert(stops == 1 && sysObj.tcpip == SYS_MODULE_OBJ_INVALID);
    assert(NETWORK_MANAGER_GetStatus().stackRestarts == 1);
    tick(4999); assert(starts == 2);
    tick(1); assert(starts == 3);
    stackStatus = SYS_STATUS_READY;
    expect(NETWORK_MANAGER_NO_INTERFACE, 0);
    currentHandle = &interfaceA;
    expect(NETWORK_MANAGER_INTERFACE_DOWN, 0);
    interfaceUp = true;
    expect(NETWORK_MANAGER_LINK_DOWN, 0);
    tick(1000000); assert(stops == 1); /* No reset for absent cable. */
    linked = true;
    expect(NETWORK_MANAGER_WAITING_ADDRESS, 0);
    addressReady = true;
    expect(NETWORK_MANAGER_WAITING_ADDRESS, 0); /* Zero address. */
    address = 0x0101A8C0U;
    expect(NETWORK_MANAGER_READY, address);
    uint32_t revision = NETWORK_MANAGER_GetStatus().revision;
    expect(NETWORK_MANAGER_READY, address);
    assert(NETWORK_MANAGER_GetStatus().revision == revision);
    clientSocket = true;
    unsigned addressNotifications = notifications;
    address++;
    expect(NETWORK_MANAGER_READY, address);
    assert(!clientSocket && notifications == addressNotifications + 1);
    assert(NETWORK_MANAGER_GetStatus().revision == revision + 1);
    clientSocket = true;
    unsigned before = notifications;
    linked = false;
    expect(NETWORK_MANAGER_LINK_DOWN, 0);
    assert(!clientSocket && notifications == before + 1);
    expect(NETWORK_MANAGER_LINK_DOWN, 0);
    assert(notifications == before + 1);
    linked = true;
    expect(NETWORK_MANAGER_READY, address);
    clientSocket = true;
    stackStatus = -1;
    expect(NETWORK_MANAGER_STACK_UNAVAILABLE, 0);
    assert(!clientSocket && stops == 2 && notifications == before + 2);
    tick(5000);
    stackStatus = SYS_STATUS_READY;
    currentHandle = &interfaceB;
    expect(NETWORK_MANAGER_READY, address); /* New handle after reset. */
    clientSocket = true;
    addressReady = false;
    expect(NETWORK_MANAGER_WAITING_ADDRESS, 0);
    tick(119999); assert(stops == 2);
    tick(1); assert(stops == 3);
    tick(5000);
    stackStatus = SYS_STATUS_READY;
    currentHandle = NULL;
    expect(NETWORK_MANAGER_NO_INTERFACE, 0);
    tick(30000); assert(stops == 4);
    /* Unsigned deadline arithmetic also works across counter wrap. */
    now = UINT64_MAX - 2000u * 120000u;
    tick(5000);
    stackStatus = SYS_STATUS_READY;
    currentHandle = &interfaceA;
    interfaceUp = false;
    expect(NETWORK_MANAGER_INTERFACE_DOWN, 0);
    tick(30000); assert(stops == 5);
    puts("Network-manager startup, MAC, deadlines, client cleanup and recovery tests passed.");
    return 0;
}
