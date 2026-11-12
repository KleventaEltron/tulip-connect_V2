#ifndef TEST_TCPIP_H
#define TEST_TCPIP_H
#include <stdint.h>
typedef enum { TCPIP_DNS_RES_OK=0,TCPIP_DNS_RES_PENDING=1,TCPIP_DNS_RES_NO_SERVICE=-6 } TCPIP_DNS_RESULT;
typedef union { struct { uint32_t Val; } v4Add; } IP_MULTI_ADDRESS;
#define TCPIP_DNS_CLIENT_MAX_HOSTNAME_LEN 64
#define TCPIP_DNS_TYPE_A 1
#define IP_ADDRESS_TYPE_IPV4 1
TCPIP_DNS_RESULT TCPIP_DNS_Resolve(const char *,int);
TCPIP_DNS_RESULT TCPIP_DNS_IsResolved(const char *,IP_MULTI_ADDRESS *,int);
#endif
