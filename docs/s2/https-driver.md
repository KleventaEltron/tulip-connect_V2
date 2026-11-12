# Bounded S2 session HTTPS exchange driver

## This iteration

Added src/files/s2/s2_https.c/.h and tests/s2_https. The driver connects the
existing session-initiation engine to the real dedicated TLS adapter's byte-I/O
callbacks. One Begin handles one initiateSession or confirmAccessToken exchange.
It does not open a socket, resolve DNS, perform pairing or implement flash storage.
It is not scheduled by the application yet and has no global context instance.
There is no new traffic or physical control on boot; APP_S2 remains unconfigured.

The TLS adapter now distinguishes authenticated close_notify (S2_TLS_READ_EOF)
from truncated/error reads. Its PeerMatches helper binds the HTTP origin to the
verified DNS hostname AND actual TCP socket remote port. The session engine has
an Abort operation that clears communication details while preserving its
recoverable access-token set. Logging and app_s2_tasks state handling are unchanged.

## Request and ownership

Begin requires an already VERIFIED S2_TLS connection and an engine in INITIATE or
CONFIRM. It builds the engine's POST as an HTTP/1.1 origin-form request, with Host,
Bearer authorization, JSON content type, Accept, identity content encoding and
an exact Content-Length. The URL host/port must match the TLS peer. Redirects
are never followed. This is not a general-purpose arbitrary-URL HTTP client.

The driver borrows the TLS and engine contexts until Stop. The caller must keep
both alive and must not concurrently manipulate the engine or run a WebSocket
transport on that TLS instance. While an exchange is active, Tasks polls TLS and
honors engine deadlines before more I/O. It writes at most 512 plaintext bytes
and reads at most 512 per call. Partial writes/would-block retain request position;
accepted bytes are erased from the transmit buffer. Temporary draft credentials
are erased after formatting; response buffers are erased after engine delivery.
No production console logging of credentials or response bodies is added.

Network-manager teardown must call S2_HTTPS_Stop before invalidating the shared
stack. Stop immediately closes only the dedicated TLS/socket connection and
aborts an active exchange. Completed session details survive Stop and retain their
original lifetime; they are not granted a new lifetime by closing HTTP.

## Response framing

The parser handles split headers/body at arbitrary TLS-read boundaries:

- HTTP/1.1 and HTTP/1.0 final responses.
- Content-Length and chunked bodies, including bounded ignored chunk extensions
  and validated/discarded trailers. Trailers cannot change framing/authentication.
- Up to four informational responses, except unsupported 101 upgrades.
- Close-delimited bodies ONLY with a clean authenticated TLS close_notify.
  Plain TCP FIN/reset, TLS errors and premature EOF in framed bodies fail closed.
- Successful 200 responses must have application/json content type; JSON parsing
  and protocol field validation are performed by the session engine.

Initial profile deliberately rejects duplicate Content-Length, TE+CL ambiguity,
unsupported transfer/compression codings, folded/non-ASCII headers, pipelining,
redirects and unframed persistent-connection bodies. It does not decompress data.
Bytes after a completed response in the same read are rejected before persistence.
It is not a complete general HTTP implementation. Future framing/encoding
extensions need their own bounds and tests.

A complete response is delivered to S2_SESSION_Response exactly once. A 401 can
advance the engine to its other saved token; the owner explicitly begins another
exchange. No HTTP request is automatically replayed on I/O failure. Storage or
protocol failure stops the driver without sending a confirmation on its own.

DONE/reusable means framing permits HTTP reuse, not a guarantee the peer will
remain connected. If Connection: close was received, the adapter begins bounded
TLS/TCP shutdown. Continue calling HTTPS Tasks to advance that cleanup, or Stop
for immediate release. Reopen verified TLS before beginning the next exchange.
The owner must continue polling the session engine during any reconnection delay
so pending-token deadlines remain enforced. Begin also rechecks them.

## Bounds and memory

- Fixed encoded request buffer: 2048 bytes.
- Complete response headers, including interim responses: 4096 bytes total.
- Decoded body: 4096 bytes; no token processing before the entire body completes.
- Chunk-size/trailer line: 128 bytes; trailers: 1024 bytes, at most 16 fields.
- Exchange deadlines come from the engine's 15-second phase policy. This driver
  does not extend deadlines on would-block or partial progress.
- S2_HTTPS context: 12,400 bytes on SAME54, 12,504 on the tested host.
- Caller must allocate it statically when configuring S2, not on the main stack.
  No instance is allocated by this iteration. The driver uses no heap; the
  session engine still uses bounded cJSON allocations and TLS/TCP use their heaps.
  Concurrent logging/S2 RAM, stack and heap-fragmentation budgets remain to measure.

## Verification

    python tests/s2_https/run_tests.py --cc gcc
    python tests/s2_tls/run_tests.py --cc gcc
    python tests/s2_session/run_tests.py --cc gcc

Zig cc is also supported. The HTTPS tests compile the actual driver, actual
session engine and existing cJSON with a controlled TLS byte stream. They cover
initiate/confirm on a reusable connection, alternate-token 401 fallback, 1-byte
and other read splits, partial writes, chunked data/extensions/trailers, HTTP/1.0,
interim responses, explicit connection close, authenticated/unclean EOF, exact body
limit, oversized headers/trailers, malformed/ambiguous framing, redirects, unsupported
encoding, truncated bodies, unexpected trailing bytes, trust/port mismatch,
backend count violations, deadlines, storage failure and cleanup. Generated request
syntax and Content-Length are independently checked with Python's stdlib.
TLS tests cover the new clean-EOF and actual-peer binding helpers. Session tests
still validate happy-path JSON against the pinned official OpenAPI schemas.
These tests do not perform real TLS/certificate negotiation or physical power cuts.

XC32 SAME54 compilation passes with strict warnings (existing generated-header
unused parameters suppressed). Partial target linking resolves the driver/TLS/
session/JSON internal symbols. Both MPLAB configurations register the source/header.
Full firmware linking and real CEM/hardware interoperability remain pending.
Reopen/add existing s2_https.c/.h if MPLAB's in-memory view is stale, then Clean
and Build. Factory startup should still report APP_S2_DISABLED with normal logging.

## Next integration

The session owner still needs a real durable pairing/token store, API-index
version discovery, DNS and scheduling across HTTPS initiation/confirmation and
the application WebSocket task. Only real successful pairing establishes trust
and initial access tokens. After session READY, stop HTTP, TakeDetails, bind/resolve
its trusted endpoint and reduce lifetime for additional setup delay before
APP_S2_ProvideSession. Discovery/pairing, heartbeat/reconnect, S2 JSON/FRBC and
physical-control arbitration are still separate work.

HTTP framing reference: https://www.rfc-editor.org/rfc/rfc9112.html
S2 flow reference: https://docs.s2standard.org/s2-connect/1.0.0/discovery-pairing-authentication/

The application session coordinator is now implemented; see session-coordinator.md
for explicit configuration, retry, storage qualification and disabled boot behavior.
