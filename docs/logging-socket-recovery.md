# Logging socket allocation and cleanup fix

S2 implementation is paused. This change affects logging.c only in production;
no network defaults, buffer sizes, linker offsets or S2 runtime are changed.

## Problem and evidence

Logging reported an invalid result from NET_PRES_SocketOpen without retaining
its error. The configured internal TCP/IP heap is 65,536 bytes. A TCP socket
requests 16,385 bytes TX plus 1,025 bytes RX, before control structures/packets.
The ten configured handles do not imply memory for ten such sockets.

The installed Harmony TCPIP_TCP_Close normally starts graceful FIN/ACK closure.
It can retain its control block and buffers until closure/retransmission ends,
although NET_PRES has already released its presentation handle. The changed
logging recovery no longer resets the shared stack on a logging timeout, so
this resource pressure must be handled at the logging socket itself.

The field screenshot establishes socket-open failure, not its exact internal
cause. Delayed release/heap pressure is a supported candidate, not a confirmed
measurement of that device. Hardware checks below are still required.

## Change

- Request TCP_OPTION_LINGER with lingerEnable=false and gracefulEnable=false on
  each newly opened logging socket through NET_PRES_SocketOptionsSet. The actual
  configured transport forwards this option to TCPIP_TCP_OptionsSet.
- Harmony now aborts/frees that socket when NET_PRES_SocketClose is called, rather
  than holding its large buffers during FIN/ACK. Normal writes and TLS processing
  before close are unchanged. This policy applies only to logging connections.
- Abortive TCP close sends RST and discards any outstanding transmit bytes at
  cleanup. Success must therefore still mean a received server response; failure
  and timeout paths intentionally abandon the transaction. This does not provide
  exactly-once server processing for requests interrupted by network failure.
- Refuse to lose an existing owned handle: close it before a replacement open.
- Pass a real NET_PRES error variable. Probe reset status only after successful
  open and option configuration. If option configuration fails, clean up and
  abort the attempt rather than silently assuming immediate-close behavior.
- Route the oversized-response cleanup through closeSocket as well.
- On allocation failure report NET_PRES error, internal heap free space, total capacity, largest allocatable block and
  each occupied TCP slot's numeric state, TX/RX sizes and TX pending bytes.
  NET_PRES UNKNOWN_ERROR can still represent several lower-level failures;
  aggregate free heap does not measure the largest contiguous free block.
  Correction: TCPIP_STACK_HEAP_MaxSize is the largest allocatable block, not
  total capacity. TCPIP_STACK_HEAP_Size supplies total capacity. The original
  free/max slash label was misleading and has been replaced by explicit fields.

The fix does not close other clients' sockets or restart the shared TCP stack.
It prevents future buildup from this logging connection; deeper heap corruption
or unrelated allocation leaks would require further evidence and remediation.

## Verification

    python tests/logging_socket/run_tests.py --cc gcc
    python tests/network_manager/run_tests.py --cc gcc

The logging tests extract the actual report/open/close functions unchanged from
logging.c and compile them with Harmony-contract doubles. They cover 1,000
repeated transactions, duplicate starts, invalid opens, diagnostics without a
heap handle, failed option setup, repeated close and subsequent successful retry.
They do not emulate TCP packets, TLS, real heap fragmentation or peer behavior.
The production logging.c also compiles using XC32 v4.60 for ATSAME54P20A.
Full firmware linking and on-device behavior remain to be verified in MPLAB.

On hardware, Clean and Build, then verify regular logs and settings GET/POST
responses across many one-minute polling intervals. Disconnect/reconnect Ethernet
during logging; temporarily make the server unreachable and verify subsequent
recovery. Check SD-card logging continues. If allocation still fails, retain the
new diagnostic lines, particularly TCP states and heap free/total/largest values. If server
success is confirmed, verify the next transaction also starts successfully.
There are no new production files to add to MPLAB.
