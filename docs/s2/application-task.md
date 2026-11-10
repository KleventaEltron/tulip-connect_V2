# S2 application task and session configuration boundary

## Added and modified

- Added src/app_s2_tasks.c/.h, with lifecycle/configuration tests under tests/app_s2.
- main.c initializes the task after NETWORK_MANAGER_Initialize and polls it after
  NETWORK_MANAGER_Tasks. Generated Harmony init/tasks files are not changed.
- The existing TLS adapter exposes its existing SNTP readiness check through
  S2_TLS_ClockReady. Both MPLAB configurations register the new application files.

This is application orchestration, not S2 JSON/FRBC control or completed pairing.
It adds no heat-pump setpoint, mode or physical-output writes. There is no
hardcoded CEM endpoint, trust anchor, communication token or enabled default.

## Boot and memory

On boot APP_S2_Initialize registers a network-down cleanup callback and reports
DISABLED. It opens no socket and reserves no connection workspace. Logging later
registers its own independent callback; neither application owns shared recovery.

The future provisioning/application owner explicitly attaches a caller-owned
static APP_S2_WORKSPACE and a complete-text consumer. The workspace must outlive
the attachment and must not be placed on the main stack or allocated repeatedly
from a fragmented heap. Size: 32,232 bytes on SAME54 (32,352 bytes on host),
including TLS/transport contexts, copied endpoint/session fields and a 4096-byte
DER trust-anchor buffer. TLS/TCP heap allocations are additional. No instance of
this workspace is allocated by current boot code; only small coordinator state
and a pointer are static. Final concurrent logging/S2 memory and image size still
need full target link/runtime measurements before production activation.

## Supplying a session

APP_S2_ProvideSession takes a descriptor whose hostname/path/token/CA are copied
into the attached workspace. The authority is derived from hostname and port.
IPv4 is already resolved in Harmony representation. Local limits are DNS host
253 characters, path/token 511 characters each, DER anchor 4096 bytes.

The descriptor must represent ONE fresh S2 Connect communication initiation with
trusted pairing material. lifetimeMs is the remaining lifetime for initiating
this connection, measured using the coordinator's monotonic clock from the call.
It is not a negotiated S2 JSON field and is not a lifetime for an established
session. Accepted range is 1..INT32_MAX milliseconds; unsigned wrap is supported.
Discovery, pairing, access-token rotation/persistence and the HTTPS initiation
exchange are not implemented by this descriptor API. Supplying an arbitrary
manual token does not replace those protocol steps or provide conformance.

Pending sessions wait for network and valid SNTP time, and expire while waiting.
One TLS attempt consumes the supplied session, even if setup fails. The token
remains only until upgrade construction, then its copied field is cleared; the
transport erases request bytes as its TLS backend consumes them. On failure or
network loss the task requires fresh initiation data. It does not automatically
reuse the token, reconnect using it, or persist it. The caller is responsible
for never resubmitting a previously consumed session descriptor.

## State machine

    DISABLED --Attach--> WAIT_SESSION --ProvideSession--> WAIT_NETWORK / WAIT_TIME
       -> TLS_CONNECTING -> WS_UPGRADING -> WS_OPEN -> CLOSING -> WAIT_SESSION

Any failed attempt returns to WAIT_SESSION with an error. GetStatus reports state,
application error, captured TLS/wolfSSL error and attempt count, without secrets.
WS_OPEN means a verified TLS + WebSocket channel only. It does not assert S2
message negotiation, resource registration, FRBC readiness or physical authority.

Each Tasks call polls TLS first, then the WebSocket transport at most once.
Closure continues to poll both layers until the WebSocket reply and TLS/TCP
shutdown complete. The registered network callback first stops the transport,
then immediately closes TLS before the network manager deinitializes the stack.
A disconnect also invalidates a pending session, requiring fresh initiation data.
Disable forces cleanup, erases the workspace and releases the attachment.
No connection operation is performed from an interrupt.

Text consumers must enqueue/process messages without reentering application/
transport APIs. SendText is provided for future JSON output from the main loop;
it does not validate messages. No production consumer is attached at present.

## Verification

    python tests/app_s2/run_tests.py --cc gcc
    python tests/s2_tls/run_tests.py --cc gcc

Zig cc is also supported. Application tests compile the actual coordinator with
network/time/TLS/transport doubles and cover disabled boot, callback registration
failure, no duplicate registration, copied config validation, network/time gates,
monotonic session expiry including counter wrap, token clearing, single attempt
without automatic reuse, failures, ordered pre-teardown cleanup, closure drain,
disabling and workspace erasure. Layer tests exercise actual TLS-adapter/transport/
codec composition with mocked TLS primitives. They do not prove real CEM pairing,
certificate interoperability, hardware timing or actual certificate rejection.
The application, changed main and TLS adapter compile with XC32 for SAME54.

In MPLAB Clean and Build; reopen/add existing app_s2_tasks.c/.h if needed. On
hardware APP_S2_GetStatus should remain DISABLED, regular logging/settings must
continue, and cable reconnection must work. A live S2 test requires a provisioned
trusted endpoint and a fresh session provider, not just enabling a setting.

## Remaining steps

Next implement the S2 Connect initiation/provisioning path that supplies these
sessions and integration tests against a real CEM or simulator. Pairing roles
and discovery remain to be finalized within the current S2 Connect rules. Add
heartbeat/pong deadlines and reconnect/backoff around FRESH session initiation
before deploying long-lived communication. Then attach S2 JSON handling and the
single-resource FRBC model with Tulip safety/control arbitration. The current
configuration boundary is not yet a complete interoperable S2 implementation.
