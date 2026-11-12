# S2 Connect session-initiation engine

## Scope of this iteration

Added src/files/s2/s2_session.c/.h and registered them in both MPLAB
configurations. The module implements the JSON/request and token-rotation state
machine for an ALREADY PAIRED RM communication client. It is not yet called by
the live application task. There is no new traffic, hardware storage write or
physical control on boot; APP_S2 remains disabled without supplied setup.

The specification and current official OpenAPI were checked on 2026-11-11:

- https://docs.s2standard.org/s2-connect/1.0.0/discovery-pairing-authentication/
- https://github.com/flexiblepower/s2-connect/tree/main/openapi

Pinned official files, Apache license and retrieval/SHA256 metadata are under
tests/s2_session/spec. API major version v1 and S2 message wire string v1.0.0
are separate identifiers. No legacy WebSocket Handshake messages are added.

## Implemented flow

1. Begin loads one or two saved access tokens, the paired client/server UUIDs,
   a selected versioned HTTPS API base and an atomic durable storage callback.
2. Request produces POST .../v1/initiateSession, Bearer access token and JSON
   offering WebSocket plus v1.0.0. An initiation 401 tries the other saved token
   once if available; confirmation 401 does not silently switch tokens.
3. A TLS-verified success response must select the offered protocol/version and
   provide a valid new access token. The engine persists accepted-old + pending
   tokens before allowing confirmAccessToken to be constructed.
4. The confirmation request uses the PENDING token as Bearer, with no JSON body.
5. A valid confirmation response supplies WebSocket URL/token. Only after durable
   pruning to the newly active access token may the connection details be taken.
6. TakeDetails exports host/port/path/commToken plus conservative remaining
   initiation lifetime ONCE. It clears the engine's copied communication details.

The storage callback is mandatory. It must atomically replace the snapshot and
return true only when durable. On failure/uncertain commit, either the entire
previous or entire new snapshot must remain recoverable; a partial set is not
allowed. The engine stops and never confirms or exports a session on failure.
After process interruption the owner reloads ALL persisted tokens, not a guessed
active one. No RAM-only callback pretending to be durable may be used in production.
The module provides the callback boundary, not a flash/SEEP layout or journal.

NoLongerPaired is surfaced distinctly and no automatic retry occurs. TLS failure,
HTTP error, authentication failure, validation failure, storage failure and
expiry are separate results. Tokens are never written to console logs. Parsed
JSON value strings and temporary token sets are erased before release; callers
must also erase their HTTP/request/TakeDetails buffers after use.

## Bounds and current profile

- Context: 2172 bytes on SAME54, 2200 bytes on the tested host. No global instance.
- Two saved access-token slots (accepted-old plus pending); token capacity is
  256 base64 characters plus NUL. Canonical base64 must decode to at least 32 bytes.
- Complete decoded HTTP response body: at most 4096 bytes, JSON nesting 16 and
  structural complexity 128. Prechecks reject invalid UTF-8, decoded NUL, control
  characters and malformed number syntax; duplicate object keys are rejected.
- Uses the project's existing cJSON with bounded input/tree complexity. Temporary
  cJSON allocations use the existing heap. Its global settings are unchanged.
- Supported message fields are type/value checked. Optional node/endpoint metadata
  is ignored rather than applied; this is not a complete schema validator for
  every optional description or a claim of full S2 Connect conformance.
- API base must be HTTPS and end in /v1 (trailing slash normalized). API-index
  negotiation must already have happened; this engine does not discover versions.
- WSS URL must use a DNS hostname and match the HTTPS API's host and effective
  port. Different transport authorities are rejected by this initial profile.
  Cross-endpoint DNS/trust/certificate policy is deferred, not automatically trusted.
- Phase deadline: 15 seconds. Pending-token confirmation conservatively uses
  initiation-request dispatch as its deadline origin, not response receipt.
- Communication-token lifetime is conservatively bounded to 30 seconds from
  confirmation-request dispatch. The owner must pass the exported remaining time
  to the application, not start another full 30-second lifetime.
- All clock inputs are monotonic uint32 milliseconds; wrap is tested.

The target's unoptimized stack report gives 1432 bytes for Begin and 832 for
Response, excluding called cJSON recursion, libc and caller frames. Full runtime
stack/heap budget must be checked before enabling the workflow. No context or
request buffer is automatically allocated in the running firmware by this step.

## Verification

Host-only prerequisites:

    python -m pip install -r tests/s2_session/requirements.txt
    python tests/s2_session/run_tests.py --cc gcc

A Zig executable may be supplied instead. Tests compile the actual production
engine and existing cJSON. They exercise success, alternate-token authentication,
restart between persistence and confirmation, both failed and uncertain commits,
pruning failure, expired phases/communication token including wrap, single-use
export, TLS rejection, unsupported selections, URI restrictions, duplicate fields,
invalid types/UTF-8/base64/JSON/NUL/depth, allocation failure and context erasure.
The generated request and happy-path response fixtures are checked against the
pinned official OpenAPI schemas, with only local reference resolution.

XC32 SAME54 compilation passes with -Wall -Wextra -Werror. Partial target linking
with the existing cJSON resolves all internal engine/JSON symbols. Full firmware
linking and real CEM/storage/power-cut integration are still pending. The callback
mocks simulate atomic commits; they do not establish hardware persistence.

In MPLAB Clean and Build, reopening or adding existing s2_session.c/.h if the
loaded project view is stale. No live S2 behavior changes in this iteration.

## Next wiring step

Implement the bounded HTTPS request/response driver over the dedicated TLS
adapter, selected-version discovery and a genuine durable pairing/token store.
The driver must abort I/O when engine deadlines expire, forbid redirects that
would expose credentials and provide complete decoded HTTP bodies only after
TLS validation. After TakeDetails it must resolve/bind the endpoint trust and
supply APP_S2_ProvideSession, reducing lifetime for any further DNS/setup delay.
Discovery/initial pairing, heartbeat/pong/reconnect policy, S2 JSON resource
messages, FRBC and Tulip control arbitration remain separate work.
