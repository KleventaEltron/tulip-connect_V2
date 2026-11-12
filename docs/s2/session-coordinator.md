# S2 session coordinator

Added `src/app_s2_session_tasks.c/.h` and registered both in MPLAB default/Debug.
`main.c` initializes the owner after APP_S2_Initialize and polls it between the
network manager and the existing WebSocket application task. Initialization
only registers a cleanup callback and leaves the owner DISABLED. There is no
workspace instance, Configure call, storage access, DNS query or connection at
boot. Existing manual APP_S2 session injection remains available when the owner
has not been configured. Existing app_s2_tasks.c switch/case handling is unchanged.

## Ownership and configuration

The future provisioning owner supplies APP_S2_SESSION_Configure with:
- Caller-owned static APP_S2_SESSION_WORKSPACE (kept alive until Stop).
- Already paired client/RM and server/CEM UUIDs and selected HTTPS /v1 API base.
- DER trust anchor from verified pairing/provisioning, not a downloaded unchecked CA.
- Initialized S2_TOKEN_STORE bound to that pairing, with a qualified durable backend.
- Application text callback and consumer (no S2 JSON or physical actuation added).

Config strings, CA and journal callbacks/binding are copied. The backend's
IO.user and text consumer must remain alive. Config inputs must not alias the
workspace. Configure requires an initialized, DISABLED APP_S2 task and attaches
its channel context exclusively; don't separately Attach, Disable or ProvideSession
while this owner is configured. APP_S2_SendText/GetStatus remain the channel API.
An invalid Configure returns false without attaching or accessing storage.

Configure does not pair, save initial credentials, select API versions from an
index or assert that the backend is power-cut safe. The existing SAME54 adapter
still needs qualification documented in token-store-same54.md. No EEPROM layout
or allocation was changed in this step.

## State flow

WAIT_NETWORK -> WAIT_TIME -> LOAD -> DNS -> HTTPS_CONNECT -> HTTPS_WAIT -> HTTP
-> HANDOFF -> CHANNEL.

LOAD reads the journal on every attempt; EMPTY/count-zero is unpaired, invalid
records or I/O errors block. DNS uses the existing Harmony IPv4 resolver (A
record) and a 10-second owner deadline. No cache deletion, private DNS socket,
stack reset or global DNS setting change is introduced. The current stack
accepts names shorter than 64 characters; longer names are rejected by Configure.
There is no mDNS/DNS-SD implementation here, and `.local` resolution must not be
assumed to work through ordinary unicast DNS. Endpoint discovery remains separate.

After verified TLS, the actual S2 session engine is started and HTTPS builds the
request. Its initial phase deadline starts after initial DNS/TLS setup; subsequent
pending-token deadlines continue through any required TLS reconnection. The
read-only S2_SESSION_Configuration helper shares the engine's endpoint/UUID
validation without reading credentials or starting a phase timer.

Successful initiation saves old+pending tokens through the journal before
confirmation. A reusable HTTPS connection handles confirmation or old/new token
fallback; Connection: close causes a fresh TLS connection without restarting the
session engine or its deadlines. No failed POST is blindly replayed in-place.
After a transport failure a later attempt reloads the durable snapshot, allowing
old/pending recovery after an uncertain server confirmation.

After confirmation, HTTPS is stopped before exporting the single-use communication
token. The engine validates the WSS origin against the HTTPS origin. The owner
reuses the resolved IPv4 for this same origin and passes copied endpoint/trust
and the remaining communication-token TTL to APP_S2_ProvideSession. Time spent
handing off is deducted, with an extra 1 ms margin. Temporary access tokens,
communication details and request buffers are cleared. An established channel
continues beyond the setup token TTL; its normal close/failure starts a new
initiation with freshly loaded access tokens. Communication tokens are never
reused for reconnect.

## Failures, retry and teardown

Transient DNS/TLS/HTTP/channel/expiry failures wait 5, 10, 20 and 40 seconds.
Five consecutive failures enter BLOCKED (initial attempt plus up to four retries).
A channel observed OPEN resets that failure budget. Offline/time-not-ready states
wait without launching attempts. Repeated network-down notifications during
backoff do not multiply failures. Retry timing uses unsigned monotonic differences,
including millisecond wrap. A missing SYS_TIME frequency disables the channel
and blocks the owner rather than disabling expiry checks.

Exhausted access-token authentication, NoLongerPaired, invalid protocol data,
invalid configuration or storage errors block immediately. TLS trust setup and
argument errors also block immediately; the TLS adapter reports other certificate/
handshake errors as HANDSHAKE, which receive only the bounded retry budget.
HTTP error responses can receive bounded retries. There is no automatic unpair,
credential erase, insecure fallback or indefinite retry storm. Stop then Configure
after resolving a BLOCKED condition; the owner never silently provisions tokens.

APP_S2_SESSION_GetStatus exposes owner state/error, last observed journal/session/
HTTP/TLS/DNS results, counters and retry delay. APP_S2_GetStatus exposes actual
WebSocket state. No token contents are printed. Some diagnostic fields retain
the last observed result across retries; they are not all current-phase errors.

The existing channel and new HTTPS owner register independent synchronous network
callbacks, so both close owned sockets before stack teardown. Stop immediately
closes both paths, erases the caller workspace and detaches it; persistent
pairing/tokens remain intact. Logging owns its existing independent connection.
The new owner consumes one additional network-manager callback slot (three
clients total with logging and the existing S2 task, of four available).

## Memory and scheduling

SAME54 optional workspace: 54,692 bytes, including the 32,232-byte existing
WebSocket workspace (22,460 additional bytes). No such instance is allocated in
the current firmware. The disabled hook adds only small static bookkeeping.
The HTTPS and WebSocket contexts are separate; HTTPS is closed before WebSocket
TLS starts. They do not hold two live S2 TLS sockets concurrently.

XC32 reports coordinator Configure/Tasks frames of 304/128 bytes and engine Begin
1432 bytes, excluding callees and compiler/library overhead. Journal persistence
and cJSON add call-chain stack requirements. The current journal backend is
synchronous; writes may delay the main loop. This owner does not solve storage
latency or hardware durability qualification. Measure whole-call timing and stack
margin before enabling this on the heat-pump controller.

## Verification

`python tests/app_s2_session/run_tests.py --cc <compiler>`

Uses the actual coordinator, existing APP_S2 task, HTTPS driver, session engine,
journal and cJSON. Doubles replace DNS/network/time, TLS byte stream, WebSocket
transport and storage medium. Partial HTTP I/O exercises actual request parsing
at the fixture boundary. Cases cover disabled startup/registration failure,
network/time gates, success, HTTP close/reconnect, 401 old/pending fallback,
auth/protocol/storage blocks, failed confirmation recovery from durable pending
tokens, slow storage expiry, DNS timeout, retry exhaustion, network teardown,
channel reconnect, communication-token expiry, input validation and secret clearing.
This is not real TLS, filesystem/EEPROM power-cut testing, or CEM interoperability.

Existing APP_S2, HTTPS, official-schema session and TLS regression suites pass.
Strict XC32 SAME54 compilation includes coordinator, existing S2 components and
main. Partial linking resolves internal app/S2/JSON symbols. Project XML and both
MPLAB source/object/dependency registrations are checked. Full firmware linking
and physical testing remain pending; use Clean and Build in MPLAB for the device.

## Following work

Pairing/trust provisioning, API-index version selection and discovery remain
separate. Hardware store qualification is still required before live rotation.
WebSocket heartbeat/liveness policy, S2 resource/model message handling and FRBC
control arbitration follow; no command here changes heat-pump behavior.

## Communication bench milestone

An opt-in PC/device communication fixture is now available; see
communication-bench-test.md. ipv4Override may explicitly route a trusted hostname
to the PC IPv4 without altering DNS. It remains zero for the normal DNS flow.
The diagnostic resource is NOT_CONTROLABLE and contains no pump-control path.

Memory follow-up: the coordinator now keeps one CA copy in channel.ca, used by
both sequential TLS stages. The earlier 54,692-byte figure is historical; bench
profile measurements and the full-link check are in communication-bench-test.md.
Normal protocol limits remain unchanged, while the enabled bench has compact
buffers selected centrally by s2_limits.h.
