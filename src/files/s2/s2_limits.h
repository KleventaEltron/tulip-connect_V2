#ifndef S2_LIMITS_H
#define S2_LIMITS_H
#include "s2_comm_test_config.h"
/* All translation units must use the same profile; Clean and Build on changes.
 * Compact limits cover only the bench fixture, not future FRBC payloads. */
#if TULIP_S2_COMM_TEST_ENABLE
#define S2_WS_MESSAGE_MAX 640u
#define S2_WS_HEADERS_MAX 512u
#define S2_TLS_IO_SIZE 128u
#define APP_S2_CA_MAX 1024u
#define APP_S2_PATH_MAX 255u
#define APP_S2_TOKEN_MAX 256u
#define S2_HTTPS_TX_MAX 768u
#define S2_HTTPS_HEADERS_MAX 512u
#define S2_SESSION_JSON_MAX 1024u
#define S2_SESSION_URL_SIZE 256u
#define S2_TEST_QUEUE_DEPTH 3u
#define S2_TEST_OUTPUT_MAX 640u
#else
#define S2_WS_MESSAGE_MAX 4096u
#define S2_WS_HEADERS_MAX 4096u
#define S2_TLS_IO_SIZE 512u
#define APP_S2_CA_MAX 4096u
#define APP_S2_PATH_MAX 511u
#define APP_S2_TOKEN_MAX 511u
#define S2_HTTPS_TX_MAX 2048u
#define S2_HTTPS_HEADERS_MAX 4096u
#define S2_SESSION_JSON_MAX 4096u
#define S2_SESSION_URL_SIZE 512u
#define S2_TEST_QUEUE_DEPTH 6u
#define S2_TEST_OUTPUT_MAX 768u
#endif
#endif
