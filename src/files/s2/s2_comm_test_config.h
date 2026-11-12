#ifndef S2_COMM_TEST_CONFIG_H
#define S2_COMM_TEST_CONFIG_H
/* Explicit bench build only. Generate s2_comm_test_local.h using the PC tool,
 * then set to 1 for the test build. Default adds no workspace/storage writes. */
#ifndef TULIP_S2_COMM_TEST_ENABLE
#define TULIP_S2_COMM_TEST_ENABLE 1
#endif
#endif
