# S2 integration - step 3: portable WebSocket client codec

## Scope and baseline

The existing shared network manager has been verified on hardware. This step adds
src/files/s2/s2_ws.c and .h, a portable, callback-driven WebSocket CLIENT codec.
It is built by both MPLAB configurations but is not yet called by main or logging.
No sockets open, no global connection context is allocated, and no heat-pump
control behavior changes. This is not a completed S2 transport or S2 session.

The current official S2 Connect documentation identifies version 1.0.0 and S2 JSON
1.0.0. This codec implements the underlying RFC6455 mechanics, not S2 JSON version
negotiation. It introduces no legacy Handshake/HandshakeResponse messages.

Sources checked on 2026-11-10:

- https://docs.s2standard.org/docs/specs/
- https://docs.s2standard.org/s2-connect/1.0.0/discovery-pairing-authentication/
- https://www.rfc-editor.org/rfc/rfc6455.html

S2 Connect requires HTTPS/WSS. The codec never supplies a plaintext connection
mode or bypasses certificate validation. A future client transport must complete
pairing/session initiation and verified TLS before using the upgrade API. There
is no assumed CEM URL, token, certificate or default port hardcoded in production.
Outbound client operation is the first implementation scope; any server-side
role required by the selected deployment is a separate layer.

## Data flow

    verified TLS connection (future Harmony adapter)
       -> BuildUpgrade: generate authenticated HTTP upgrade request
       -> AcceptUpgrade: validate complete HTTP response headers
       -> Feed: decode arbitrary TLS read boundaries into complete UTF-8 text
       -> text callback: future S2 JSON parser/message queue

    future S2 JSON output -> SendText -> masked WebSocket frame
       -> send callback -> future bounded socket-write queue -> TLS connection

The future adapter owns header accumulation and must split HTTP headers from
frames that arrive in the same TLS read. Feed handles frame coalescing, split
headers/payloads, fragmented text messages and interleaved control frames.
Callbacks must not reenter any codec API; queue application replies for the next
task iteration. Message pointers expire after text returns. No buffer may be
retained by reference. Feed is synchronous; the future task must bound bytes
processed per iteration so the control loop keeps running.

## Implemented

- HTTP/1.1 upgrade request with fresh random key and Bearer commToken.
- RFC6455 SHA-1/base64 accept validation; counted response parsing without
  requiring NUL termination; case-insensitive/list-valued headers.
- Unsolicited extensions/subprotocols, duplicate accept fields, malformed HTTP
  headers and non-101 responses rejected. No compression is negotiated.
- Fresh random mask per client frame, 7/16-bit lengths, bounded message assembly.
- Strict UTF-8 validation across fragments; binary messages unsupported.
- Ping response with matching pong payload; outgoing ping function; incoming
  pong accepted. Heartbeat scheduling and pong timeout tracking are future work.
- Peer close validation/reply; protocol failures attempt an appropriate close.
  Connection-ended notification occurs once, including send/random failures.
- Reset for abrupt network loss, with partial messages discarded and sends
  disabled until Initialize is called for a new connection.

send must synchronously copy/queue the entire frame or return false. The codec
buffer is reused immediately. The future adapter must support partial socket
writes while retaining queued bytes. On a valid peer close it must drain the
queued close reply before closing TLS; on network loss or I/O failure it must
discard queued writes. It owns close/drain deadlines. Local graceful close
initiation, TLS shutdown, reconnect/backoff and heartbeat deadlines are not yet
implemented. Reset is an abrupt local stop, not a graceful close handshake.

## Resource limits

- Maximum inbound/outbound text message: 4096 bytes, including all fragments.
- Maximum complete HTTP upgrade response headers: 4096 bytes.
- Messages exceeding the local limit fail with close code 1009.
- No heap allocation. Three fixed buffers retain receive frame, assembled text
  and encoded transmit frame; large buffers are not allocated on the C stack.
- sizeof(S2_WS) measured by XC32/SAME54 ABI: 12,368 bytes (host: 12,400 bytes).
- No production S2_WS instance exists yet, so this step reserves no connection
  buffer RAM at runtime. A future adapter must allocate one static context,
  not put it on the main-loop stack. Its HTTP/write/TLS buffers add memory.

These bounds are implementation choices. FRBC descriptions and supported CEM
message sizes must be measured before choosing final limits. This is not a
claim that all possible S2 messages fit within 4096 bytes. RFC7692 compression
is deferred; the client does not advertise it and rejects unsolicited use.

## Tests and build

    python tests/s2_ws/run_tests.py --cc gcc

Pass a Zig executable to use Zig cc. Tests compile the real production codec and
use deterministic callbacks exclusively in host tests. They check RFC masking
and upgrade vectors, all small-frame split positions, byte-wise extended reads,
multiple frames/read, split UTF-8 with interleaved ping, boundary-sized messages,
oversized frame/message rejection, invalid lengths/masks/opcodes/UTF-8/close
codes, malformed upgrades, randomness/SHA/send/consumer failures and repeated
reset. They do not implement or test production TLS/cryptographic adapters.

The codec compiles for ATSAME54P20A with XC32 v4.60, C99 and -Wall -Wextra -Werror.
Full firmware linking, stack usage and on-device CEM interoperability still need
verification when the transport is connected.

MPLAB project XML and both generated makefiles list the files. Reopen the project
if its in-memory view is stale; if needed add existing src/files/s2/s2_ws.c and .h
under Source/Header Files > files > s2. Clean and Build. There is no new network
behavior to test on the heat pump in this step.

## Next step

Add the S2 application/transport task with its own socket and bounded write/read
queues, register its cleanup with NETWORK_MANAGER_RegisterDownCallback, and
supply cryptographic randomness/SHA-1 and verified TLS via the platform adapter.
Pairing, persistent tokens/trust and session initiation are required before an
actual S2 session. Only after transport and JSON handling are verified should
FRBC modeling and Tulip control arbitration be attached. Cascade remains one
resource, with the master controlling its followers.
