#ifndef TLS_TEST_DEFINITIONS_H
#define TLS_TEST_DEFINITIONS_H
#include <stdint.h>
#include <stdbool.h>
typedef int TCP_SOCKET;
typedef struct { unsigned txPending; } TCP_SOCKET_INFO;
bool TCPIP_TCP_SocketInfoGet(int,TCP_SOCKET_INFO *);
typedef struct { struct { uint32_t Val; } v4Add; } IP_MULTI_ADDRESS;
typedef struct { bool lingerEnable, gracefulEnable; uint16_t lingerTmo; } TCP_OPTION_LINGER_DATA;
#define INVALID_SOCKET (-1)
#define IP_ADDRESS_TYPE_IPV4 1
#define TCP_OPTION_LINGER 6
uint32_t TCPIP_SNTP_UTCSecondsGet(void);
int TCPIP_TCP_ClientOpen(int,int,IP_MULTI_ADDRESS *);
bool TCPIP_TCP_Close(int);
bool TCPIP_TCP_OptionsSet(int,int,void *);
bool TCPIP_TCP_IsConnected(int);
uint16_t TCPIP_TCP_GetIsReady(int);
uint16_t TCPIP_TCP_PutIsReady(int);
uint16_t TCPIP_TCP_ArrayGet(int,uint8_t *,uint16_t);
uint16_t TCPIP_TCP_ArrayPut(int,const uint8_t *,uint16_t);
#endif
