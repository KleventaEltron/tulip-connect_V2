#ifndef LOGGING_SOCKET_STUBS_H
#define LOGGING_SOCKET_STUBS_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef int NET_PRES_SKT_HANDLE_T;
typedef int NET_PRES_SKT_ERROR_T;
typedef int NET_PRES_SKT_OPTION_TYPE;
typedef int TCP_SOCKET;
typedef void *TCPIP_STACK_HEAP_HANDLE;
typedef struct { uint32_t value; } NET_PRES_ADDRESS;
typedef struct { bool lingerEnable, gracefulEnable; uint16_t lingerTmo; } TCP_OPTION_LINGER_DATA;
typedef struct { int state; unsigned txSize, rxSize, txPending; } TCP_SOCKET_INFO;
#define INVALID_SOCKET (-1)
#define NET_PRES_SKT_OK 0
#define NET_PRES_SKT_UNENCRYPTED_STREAM_CLIENT 1
#define IP_ADDRESS_TYPE_IPV4 1
#define PORT 443
#define TCP_OPTION_LINGER 6
#define TCPIP_TCP_MAX_SOCKETS 10
#define TCPIP_STACK_HEAP_TYPE_INTERNAL_HEAP 1
extern NET_PRES_SKT_HANDLE_T log_socket;
extern NET_PRES_ADDRESS address;
void testPrint(const char *format, ...);
#define SYS_CONSOLE_PRINT testPrint
#define SYS_CONSOLE_MESSAGE(message) testPrint("%s", message)
NET_PRES_SKT_HANDLE_T NET_PRES_SocketOpen(int, int, int, int, NET_PRES_ADDRESS *, NET_PRES_SKT_ERROR_T *);
bool NET_PRES_SocketOptionsSet(int, NET_PRES_SKT_OPTION_TYPE, void *);
bool NET_PRES_SocketWasReset(int);
void NET_PRES_SocketClose(int);
TCPIP_STACK_HEAP_HANDLE TCPIP_STACK_HeapHandleGet(int, int);
size_t TCPIP_STACK_HEAP_FreeSize(TCPIP_STACK_HEAP_HANDLE);
size_t TCPIP_STACK_HEAP_Size(TCPIP_STACK_HEAP_HANDLE);
size_t TCPIP_STACK_HEAP_MaxSize(TCPIP_STACK_HEAP_HANDLE);
bool TCPIP_TCP_SocketInfoGet(TCP_SOCKET, TCP_SOCKET_INFO *);
bool startSocketConnection(void);
void closeSocket(void);
#endif
