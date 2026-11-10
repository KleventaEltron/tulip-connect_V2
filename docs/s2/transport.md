# S2 integration - step 4: bounded transport orchestration

S2 work resumes after logging socket recovery. This step adds s2_transport.c/.h
alongside s2_ws.c/.h and registers them in both MPLAB configurations. It does not
modify logging, its recovery, heat-pump regulation or image offsets.

## What is functional

The portable transport owns the WebSocket codec, a bounded encoded-output queue,
HTTP response accumulation and per-task I/O budgets. Its backend interface accepts
an already established, verified TLS connection owned exclusively by S2. It
handles partial writes, would-block, split HTTP headers, the first frame arriving
with the HTTP upgrade response, peer close reply draining, network loss and
failed I/O. Tests use real codec/transport functions with deterministic mock I/O.

No production backend or global S2_TRANSPORT instance exists yet. These functions
are compiled but not called from main. This step does not open a live S2 socket
or create a complete S2 Connect session. Pairing, persistent tokens/trust,
session initiation, production cryptography and JSON/FRBC remain separate steps.

## Interface and ownership

1. Initialize once with backend callbacks (read/write/close, verified trust,
   cryptographic randomness/SHA-1, text consumer).
2. The future S2 application registers a network-manager down callback that calls
   S2_TRANSPORT_Stop BEFORE it opens a dedicated TLS/socket connection.
3. It performs pairing/session initiation as required and validates the TLS peer
   against the correct trust anchor/pin, hostname and certificate policy.
4. Start takes ownership of this new backend connection and receives the selected
   authority, path and commToken. It rejects an active connection without taking
   a second one. Invalid trust or invalid request setup closes the backend.
5. Tasks runs after NETWORK_MANAGER_Tasks, using its readiness and an actual
   monotonic millisecond clock. There is no automatic reconnect or saved token.
6. Complete text is delivered through a callback for later JSON parsing/queueing.
   SendText queues complete encoded frames. No API may be called from callbacks.

The verified callback is a backend contract, not a certificate implementation.
Tests prove that a false result prevents credential transmission; they do not
prove real TLS authentication. The existing NET_PRES provider explicitly uses
SSL_VERIFY_NONE and a fixed SNI hostname. It must not be used as-is for S2.
A dedicated adapter must perform certificate/hostname validation and must keep
logging's context and S2's credentials/sockets separate. Its nonblocking wolfSSL
write handling must also preserve any buffers required across WANT_WRITE.

The close callback releases ONLY this backend's socket/TLS resources. It must
return promptly. Stop immediately discards unsent frames, erases queued request
bytes and closes once. With no pending application output, peer close attempts drain the reply first, then
release the backend. If application output is still pending, peer close aborts
instead of interleaving a close frame into partially written data. Queue overflow or an I/O error discards outstanding work
and closes rather than treating a partial frame as accepted. Recovery never
resets the shared TCP/IP stack.

## Bounds

- Encoded queue: 8,208 bytes (two maximum-size frames).
- HTTP response headers: 4,096 bytes maximum.
- Per Tasks call: at most 512 bytes written and 512 bytes read.
- Upgrade deadline: 10 seconds from Start.
- Write no-progress deadline: 10 seconds from the first queued write attempt.
- Peer-close drain deadline: 2 seconds from close processing.
- Timeouts use uint32 monotonic milliseconds with unsigned wrap arithmetic.
- Context size with embedded codec: 25,244 bytes on XC32/SAME54; 25,320 on host.
- No heap use and no production context allocated yet. Future code must allocate
  the context statically, not on the main stack, and budget TLS/TCP buffers in
  addition. This does not fix or conceal TCP heap fragmentation.

Heartbeat/pong timeout scheduling, local graceful-close initiation, reconnect
backoff, DNS, socket opening, TLS negotiation and S2 Connect REST exchange are
not part of this engine. Open means WebSocket OPEN only, not paired or FRBC ready.

## Verification

    python tests/s2_transport/run_tests.py --cc gcc
    python tests/s2_ws/run_tests.py --cc gcc

Zig cc is also supported via --cc pointing to zig.exe. Tests cover partial I/O,
trust rejection/loss, split/coalesced HTTP/frames, complete frame queueing, peer
close draining/deadline, network cleanup, oversized headers, EOF/backend contract
violations, write/upgrade timeouts including wrap, queue exhaustion and restart
with a new verified connection. Deterministic test crypto stays in host tests.
The new C module compiles for SAME54 using XC32 v4.60 with -Wall -Wextra -Werror.
Full firmware linking and real TLS/CEM interoperability are still pending.

If MPLAB's project view is stale, reopen or add existing files
src/files/s2/s2_transport.c and src/files/s2/s2_transport.h. No firmware network
behavior changes on hardware yet. The next step is the dedicated platform/TLS
adapter and S2 application task, followed by pairing/session-initiation handling.
