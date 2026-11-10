# S2 dedicated TLS/socket adapter

This step adds src/files/s2/s2_tls.c/.h. It provides real Harmony TCP and wolfSSL
calls behind the existing S2 transport interface. It has no global connection
instance and is not called by main yet. No live CEM connection or physical
control is enabled. Logging's socket/context/configuration are unchanged.

## Ownership and verification

The adapter owns one raw Harmony TCP client socket, an independent wolfSSL
context/session, and a balanced wolfSSL_Init/Cleanup library reference. It uses
an explicit TLS 1.2 client method supported by the installed build. The context
uses SSL_VERIFY_PEER with no verification override, loads an explicitly supplied
DER CA/trust anchor, sets per-session SNI and configures hostname checking before
handshake. There is no trust-on-first-use, plaintext fallback or ignored date/
hostname validation. Certificate validation is part of successful wolfSSL_connect.
The build omits OPENSSL_EXTRA, so code does not rely on the unavailable
wolfSSL_get_verify_result implementation merely declared by ssl.h.

Start requires NETWORK_MANAGER_IsReady, valid nonzero SNTP UTC, a DNS hostname,
an already resolved Harmony-format IPv4 address, port and trusted CA. It does
not resolve DNS, discover CEMs, perform pairing, pin certificates or persist
credentials. Trusted provisioning/pairing must supply the correct trust anchor.
The clock uses wolfSSL's existing pic32_time -> TCPIP_SNTP_UTCSecondsGet hook.
Its availability does not authenticate SNTP or provide rollback protection.
No certificate policy is relaxed if time is unavailable.

## Cooperative operation

Initialize once, then call Start for a configured endpoint. Start creates the
context/session/socket; Tasks advances TCP connect and TLS handshake separately,
with a 30-second deadline for each. WANT_READ/WANT_WRITE are retried from later
task calls. Phase deadline arithmetic supports uint32 millisecond wrap.
A synchronous wolfSSL call may perform certificate/ECC work; target CPU timing
still needs measurement. No claim of strict per-call crypto execution time is
made. There is no reconnect loop or hardcoded CEM/trust/token in this module.

GetPlatform returns the existing S2_TRANSPORT_PLATFORM callbacks. Once TLS state
is VERIFIED, the future application may initiate the HTTPS S2 Connect exchange
or the WebSocket upgrade with previously obtained communication URL/token.
VERIFIED describes TLS only; it does not imply pairing, S2 session negotiation,
FRBC availability or permission to control the heat pump.

Ciphertext callbacks respect TCP ready counts and partial writes. Plaintext
writes retain a private 512-byte buffer and exact SSL_write pointer/length across
WANT retries, even when the outer transport compacts or appends its queue. A
pending write prevents competing SSL_read; SSL_read WANT_WRITE is retried before
starting new plaintext writes. Reads use a fixed 512-byte private buffer and
serve smaller caller capacities without losing bytes.

The transport close callback requests bounded TLS shutdown and TCP draining.
Keep calling TLS Tasks even after the WebSocket transport becomes idle: the TLS
state may remain CLOSING for up to two seconds. It waits for close_notify and
TCP pending bytes before freeing resources, or force-closes on timeout/failure.
Start refuses a still-owned connection during this drain.

S2_TLS_Close is the immediate abort/idempotent cleanup operation. The future
application MUST register a network-manager down callback before opening sockets:

    static void s2NetworkDown(void)
    {
        S2_TRANSPORT_Stop(&s2Transport);
        S2_TLS_Close(&s2Tls);
    }

The wrapper is essential before shared-stack teardown; this module does not
register a global singleton automatically. It ensures no raw handle survives a
stack restart, including during close draining. Cleanup never resets the shared
stack or closes logging's socket. The S2 socket uses the same abortive TCP close
option as logging so forced cleanup cannot indefinitely retain its buffers.

## Resource and integration status

S2_TLS context: 1,332 bytes on SAME54 / 1,368 on host. Its fixed buffers are part
of the caller-owned context; no context is instantiated in running firmware yet.
wolfSSL and raw TCP allocate additional heap only when Start is invoked. Current
TCP defaults still require the existing 16 KiB TX allocation on open. Concurrent
logging/S2 heap use and fragmentation must be measured before enabling both.

The next step is the application task and configuration/provisioning boundary:
register cleanup, schedule manager -> TLS -> transport polling, provide endpoint
and trust, handle state/errors/reconnect, and keep transport disabled until
configured. Pairing/session initiation, durable tokens/trust, JSON handling and
FRBC/control arbitration remain separate steps.

## Verification

    python tests/s2_tls/run_tests.py --cc gcc

Zig cc is supported. Tests compile the actual TLS adapter, WebSocket codec,
transport and hash helper, with mocked wolfSSL/TCP primitives. They cover
certificate/hostname policy setup, missing time/network, initialization failures,
CA/hostname/SNI setup failures, TCP/options failures, handshake error propagation,
phase deadlines/wrap, stable WANT retry buffers, read remainders, EOF, network/
clock loss, idempotent cleanup, TLS/TCP close draining/deadline and the combined
TLS-backend-to-WebSocket upgrade flow. They retain a simulated logging library
reference to check cleanup doesn't release the library underneath another owner.

These are API/ownership tests, NOT real TLS/certificate interoperability tests.
Actual wrong-CA/expired-certificate/hostname rejection, peer shutdown behavior,
TLS RAM/CPU usage and real CEM operation still require a provisioned test endpoint
and hardware. No CEM credentials or trust material are available in the project.

XC32 SAME54 compilation passes with -Wall -Wextra -Werror and unused-parameter
warnings suppressed for existing generated Harmony headers. All referenced
wolfSSL APIs are checked against target-preprocessed library implementations.
Both MPLAB configurations list the files. Full firmware linking remains to be
checked with Clean and Build. Reopen the project or add existing s2_tls.c/.h if
the IDE's in-memory view is stale. This step adds no new network behavior on boot.
