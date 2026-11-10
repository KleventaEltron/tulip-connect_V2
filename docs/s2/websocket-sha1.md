# S2 WebSocket hash prerequisite

The planned dedicated TLS/socket adapter exposed an existing build prerequisite:
configuration.h defines NO_SHA, disabling wolfSSL SHA-1. The WebSocket codec
requires SHA-1 for the RFC6455 accept calculation. This step implements that
small calculation separately, leaving the shared logging/wolfSSL configuration
unchanged. It does not implement or enable the live TLS/socket adapter or task.

## Added files

- src/files/s2/s2_sha1.c/.h: S2_SHA1_WebSocketAccept, directly usable as the
  WebSocket/transport sha1 callback. user is ignored. NULL data with zero length
  hashes the empty string. Invalid arguments return false without writing output.
- tests/s2_sha1/: real production implementation tested against standard vectors
  and independent Python hashlib calculations.

The implementation uses a sixteen-word circular schedule, bounded automatic
workspace and no heap/global state. It supports padding and multi-block inputs
and rejects lengths that cannot be represented as a 64-bit bit length where
applicable. The normal upgrade input is 60 bytes (base64 nonce plus RFC GUID).
SHA-1 here is solely a required WebSocket wire calculation. It must not be used
for TLS certificate signatures, password storage, token authentication or pinning.
No algorithms or verification options in shared wolfSSL have been changed.

Algorithm reference: https://www.rfc-editor.org/rfc/rfc3174.html
WebSocket usage: https://www.rfc-editor.org/rfc/rfc6455.html#section-4.1
This is an independent implementation, not a copy of the RFC sample code.

## Verification

    python tests/s2_sha1/run_tests.py --cc gcc
    python tests/s2_ws/run_tests.py --cc gcc
    python tests/s2_transport/run_tests.py --cc gcc

Zig is also supported. Forty-four vector comparisons cover empty input, abc,
the multi-block standard vector, one million a characters, the WebSocket RFC
accept vector, padding/block boundaries, binary bytes and reproducible host test
inputs. Invalid pointer arguments are checked. Windows stdin is set to binary
mode in the host harness so control bytes and CR/LF do not alter test data.
The existing WebSocket/transport callback wrappers now call the actual helper
and verify the known digest; fault-injection coverage remains in place.
The new module compiles for SAME54 using XC32 v4.60 and -Wall -Wextra -Werror.
Logging-socket and network-manager regressions pass. Full firmware linking and
hardware behavior remain to be checked in MPLAB.

## Integration status

Both MPLAB configurations list the new source/header. Reopen or add existing
src/files/s2/s2_sha1.c/.h if the loaded project view does not refresh. There are
still no S2 socket/task calls from main and no runtime connection buffer instance.
The future TLS adapter should select S2_SHA1_WebSocketAccept as its sha1 callback.

Remaining before live S2 transport: audit the actual wolfSSL entropy/seed hook
and failure propagation (NO_DEV_RANDOM is configured), establish a reliable UTC
source for certificate validity checks, implement independent TLS trust/hostname
validation, and implement nonblocking socket/TLS I/O without sharing logging's
SSL_VERIFY_NONE context or fixed SNI hostname. Entropy quality has not yet been
verified; this step supplies no random-number implementation or fallback.
Pairing/session initiation and persistent trust/tokens follow as separate work.
