#include "stubs.h"
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

NET_PRES_SKT_HANDLE_T log_socket = INVALID_SOCKET;
NET_PRES_ADDRESS address;
static bool allocated[2], immediate[2], failOpen, failOptions, noHeap;
static unsigned closes, probes, opens;
static char output[4096];
void testPrint(const char *format, ...)
{
    va_list args;
    size_t used = strlen(output);
    va_start(args, format);
    vsnprintf(output + used, sizeof(output) - used, format, args);
    va_end(args);
}
NET_PRES_SKT_HANDLE_T NET_PRES_SocketOpen(int index, int type, int family, int port,
    NET_PRES_ADDRESS *remote, NET_PRES_SKT_ERROR_T *error)
{
    assert(index == 0 && type == NET_PRES_SKT_UNENCRYPTED_STREAM_CLIENT);
    assert(family == IP_ADDRESS_TYPE_IPV4 && port == PORT && remote == &address);
    assert(error != NULL);
    opens++;
    if (!failOpen) for (int i = 0; i < 2; i++) if (!allocated[i]) {
        allocated[i] = true; immediate[i] = false; *error = NET_PRES_SKT_OK;
        return i + 1;
    }
    *error = -7;
    return INVALID_SOCKET;
}
bool NET_PRES_SocketOptionsSet(int handle, NET_PRES_SKT_OPTION_TYPE option, void *data)
{
    TCP_OPTION_LINGER_DATA *close = data;
    assert(handle > 0 && handle <= 2 && allocated[handle - 1]);
    assert(option == TCP_OPTION_LINGER);
    assert(!close->lingerEnable && !close->gracefulEnable && close->lingerTmo == 0);
    if (failOptions) return false;
    immediate[handle - 1] = true;
    return true;
}
bool NET_PRES_SocketWasReset(int handle)
{
    assert(handle > 0 && allocated[handle - 1] && immediate[handle - 1]);
    probes++;
    return false;
}
void NET_PRES_SocketClose(int handle)
{
    assert(handle > 0 && handle <= 2 && allocated[handle - 1]);
    closes++;
    /* Mirrors Harmony's contract: abortive close frees; graceful may linger.
     * For option-set failure the unopened/connecting socket can also be closed. */
    if (immediate[handle - 1] || failOptions) allocated[handle - 1] = false;
}
TCPIP_STACK_HEAP_HANDLE TCPIP_STACK_HeapHandleGet(int type, int index)
{
    assert(type == TCPIP_STACK_HEAP_TYPE_INTERNAL_HEAP && index == 0);
    return noHeap ? NULL : allocated;
}
size_t TCPIP_STACK_HEAP_FreeSize(TCPIP_STACK_HEAP_HANDLE heap)
{
    assert(heap != NULL);
    return 1234;
}
size_t TCPIP_STACK_HEAP_Size(TCPIP_STACK_HEAP_HANDLE heap)
{
    assert(heap != NULL);
    return 65536;
}
size_t TCPIP_STACK_HEAP_MaxSize(TCPIP_STACK_HEAP_HANDLE heap)
{
    assert(heap != NULL);
    return 14976;
}
bool TCPIP_TCP_SocketInfoGet(TCP_SOCKET socket, TCP_SOCKET_INFO *info)
{
    assert(socket >= 0 && socket < TCPIP_TCP_MAX_SOCKETS);
    if (socket >= 2 || !allocated[socket]) return false;
    *info = (TCP_SOCKET_INFO){8, 16384, 1024, 0};
    return true;
}
int main(void)
{
    /* Repeated transactions cannot accumulate TCP buffers if peers never FIN. */
    for (unsigned i = 0; i < 1000; i++) {
        assert(startSocketConnection());
        assert(log_socket != INVALID_SOCKET);
        closeSocket(); closeSocket();
        assert(log_socket == INVALID_SOCKET && !allocated[0] && !allocated[1]);
    }
    assert(closes == 1000 && probes == 1000 && opens == 1000);
    /* Starting twice must release the previous owned socket first. */
    assert(startSocketConnection());
    assert(startSocketConnection());
    assert(closes == 1001 && allocated[0] && !allocated[1]);
    closeSocket();
    assert(closes == 1002);
    /* Invalid handles are never queried/configured; report actual error. */
    failOpen = true; output[0] = 0;
    assert(!startSocketConnection() && log_socket == INVALID_SOCKET);
    assert(strstr(output, "NET_PRES error=-7") && strstr(output, "heap free=1234 total=65536 largest=14976"));
    assert(probes == 1002); closeSocket(); assert(closes == 1002);
    noHeap = true; output[0] = 0;
    assert(!startSocketConnection()); assert(strstr(output, "heap free=0 total=0 largest=0"));
    noHeap = failOpen = false;
    /* Failed option setup never advances into TLS or retains the handle. */
    failOptions = true;
    assert(!startSocketConnection());
    assert(log_socket == INVALID_SOCKET && !allocated[0] && closes == 1003 && probes == 1002);
    failOptions = false;
    assert(startSocketConnection()); closeSocket();
    puts("Logging socket ownership, immediate cleanup, failure diagnostics and retry tests passed.");
    return 0;
}
