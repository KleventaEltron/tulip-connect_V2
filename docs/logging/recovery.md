# Logging recovery and streaming changes

This change targets Ethernet/cloud logging reliability while S2 runs alongside it.
No CEM/pump-control logic, endpoint credentials, EEPROM layout, bootloader offsets,
application heap setting or logging intervals were changed. No shared-stack reset
was added to logging. The existing shared network manager retains ownership.

## Allocation

Harmony TCP default TX is now 4096 bytes, RX stays 1024 bytes, TCP/IP heap stays
65536 bytes. Both src/config/default/configuration.h and the saved tcpipTcp user
value in tulip_connect.X/sam_e54_rs485.mc3 were updated so regeneration retains it.
The installed TCP driver allocates these FIFOs when opening a socket. Applying a
smaller TCP_OPTION_TX_BUFF after opening would still need the original 16 KB
allocation to succeed and would temporarily allocate another buffer.

This default also applies to S2 and other Harmony TCP clients. It is intentional:
S2 already handles partial I/O, and logging now does too. It saves 12288 bytes of
TCP/IP heap per open TCP socket compared with 16384-byte TX, before unchanged RX/
control/packet overhead. With logging and one S2 socket it saves about 24 KB.
Application plaintext chunks and HTTP payload sizes are independent of TCP FIFO
size; large TLS records/certificates and bodies are streamed. Actual concurrent
TLS heap/latency behavior still needs the board test below.

## Nonblocking upload

New files: src/files/logging_http.c/.h. Both MPLAB configurations register them.
The portable TX engine sends at most 512 bytes per task call, retains pointers/
offsets over short/zero writes, checks invalid backend counts and uses a 15-second
monotonic no-progress deadline. It calls the producer once at most per tick and
never pumps the network stack recursively. The old spin-based socketWriteAll and
unchecked active NET_PRES_SocketWrite paths were replaced by this one path.

logging.c now produces the realtime JSON one device at a time using the existing
8192-byte device buffer. It preserves controller + mandatory WL1 + cascade-selected
WL2..WL16, data-field construction, endpoint, auth and timestamp conventions.
The old measuring pass and duplicate 8192-byte buffer were removed. Realtime POST
uses standard HTTP/1.1 chunked transfer instead of a two-pass Content-Length:
values changing between measuring and sending can no longer corrupt that length.
No compression is introduced. The live backend's acceptance must be checked on
hardware; host tests independently decode and parse the emitted HTTP/JSON.

Settings overview keeps its existing CSV order: six exact half-open Modbus ranges
then the six ordered 8/16-bit EEPROM lists, including existing mode mirror writes.
It gathers at most 64 values into a stable 512-byte piece per producer call and
frames every piece as a complete HTTP chunk. Headers, CRLF and final zero chunk
are all checked/paced. A legacy-layout fixture independently validates CSV order.
Unchanged silent/boost mirrors skip redundant EEPROM writes during retries.
The original mirror values and actual setting-address handlers are
preserved. No synchronous network wait remains in the active send path; hardware
EEPROM reads/writes and existing data formatting are still synchronous operations.

## Receiving and applying settings

The receiver reads at most 512 bytes per call into the existing 4096-byte raw HTTP
buffer (plus terminator). It drains available TLS plaintext before treating a
transport disconnect as EOF. A disconnect, empty read, reset or full buffer is not
success by itself. Completion uses counted framing with case-insensitive headers,
Content-Length, chunked sizes/extensions/trailers or explicitly close-delimited
responses. Chunk payload is not scanned for a fake terminating substring.

Conflicting/duplicate lengths, unsupported encodings, invalid chunks, truncated
bodies, extra bytes, oversized responses and non-2xx status fail the attempt.
Decoded complete bodies are compacted in place; a separate 4096-byte dechunk
buffer is no longer needed. The same bounded reader handles settings POST replies.
204 empty responses are recognized; JSON-bearing GET/POST bodies must contain a
complete JSON object. Other successful settings POST responses need valid HTTP
framing/status, not a new application-specific JSON schema.

JSON preprocessing bounds nesting to 12 and string-token count to 512, rejects
raw/decoded NUL and trailing garbage. Settings update batches must be arrays with
at most the existing MAX_SETTINGS=50 entries. Every address/value must be a
nonempty string, fully parseable and fit the existing 16-bit API (signed negative
values down to -32768 remain supported). The whole batch is validated before any
setter runs, so a malformed later item cannot leave a partially applied batch.
Invalid batches now fail instead of silently dropping/truncating items. Actual
processModbusSettingsFromServer mapping and control behavior were not altered.

## Owner/recovery behavior

app_logging_tasks.c calls the TX pump while active and waits for it to finish
before reading/processing responses. The 200-second overall attempt limit retains
the existing timer-ISR fallback; the ISR still only signals the main loop. DNS
has a 10-second phase limit, other normal setup phases 30 seconds, receiving has
a 15-second no-progress limit, and the existing settle-wait phases retain their
longer allowance. Time arithmetic supports monotonic millisecond wrap.

Failure/network loss closes only logging's socket, discards its partial I/O,
stops the timer and releases only its owned logging lock. Failed work is retained
as full-log/settings-only retry intent. Delays are 5, 10, 20, 40, then 60 seconds,
with 60-second retries thereafter until recovery. Offline waiting doesn't consume
new timer events or open sockets; repeated down notifications don't multiply
failures. A retry forces a fresh query for the logging hostname, not global cache
clearing. Success resets the backoff. Regular logging timers are otherwise unchanged.

A complete response that forbids reuse leads to a new TCP/TLS connection for the
next GET/settings POST, preserving the job and its lock. It does not restart or
replay an already successful realtime POST within that attempt. Actual retries
after ambiguous delivery may still create duplicate server-side records; exactly-
once semantics require a server-supported idempotency contract, not TCP alone.

Overview-required flags survive failed sends and are cleared only after a complete
successful overview response. A display-change revision prevents acknowledging a
later edit with an earlier upload. A failed send cannot fall through into waiting
for confirmation. Startup/interface checks defer to the shared manager instead
of requiring every inactive secondary interface to be ready. Logging requests an
SNTP refresh only when no usable timestamp is available.

UART markers: `Logging transaction completed successfully`,
`Logging recovery: retry in ... ms`, `Logging phase timeout: state=...`,
`Logging I/O failed; closing and retrying`, and `Logging HTTP failure: ...`.
Socket allocation diagnostics still report actual occupied sockets, free/total/
largest TCP/IP heap. Headers/auth tokens are not printed by the new send path.

## Verification

- tests/logging_http: partial/blocked sends, 8 KB piece, stall/wrap/backend errors;
  every byte prefix of framed replies, mixed-case headers, lengths/chunks/trailers/
  EOF, malformed/conflicting framing and error statuses. Python's standard HTTP
  decoder independently validates the generated chunked request.
- tests/logging_stream: actual extracted producers/receiver plus real HTTP pump
  and cJSON, 17-device upload through a simulated 4 KB window, short writes and
  backpressure, settings CSV against a captured legacy layout, decoded responses,
  rejection of malformed whole batches and negative 16-bit setting values.
- tests/logging_recovery: complete unchanged application source against hardware
  doubles; retry/backoff, forced DNS refresh, 5-minute cable loss, lock release,
  blocked-send waiting, read timeout, server-requested reconnect, pending overview
  retries and later display edits.
- Existing logging socket ownership/1000-close cycles and shared-network tests.

These doubles don't emulate the real Harmony heap or full TLS record processing.
Strict XC32 compilation passes for the changed modules; preexisting generated
callback casts/unused helpers are suppressed in verification only. Full-link
verification uses existing project objects with recompiled changed modules and
actual 4096-byte TCP initialization, retaining heap=160000 and an explicit
8192-byte minimum stack check. Programmer ROM origin remains 0x0 in this artifact;
no linker/bootloader configuration was changed and nothing was flashed.
Memory summary: program 474839 bytes, static data 80155 bytes. The 181989 bytes
outside static data still need to cover heap/stack; they are not free after heap.
This is roughly 11.6 KB less static data than the prior compact S2 bench build.
A full Clean and Build in MPLAB is still required for the chosen device configuration.

## Hardware acceptance

1. Keep the existing dummy CEM and UART capture; Clean and Build, add existing
   logging_http.c/.h if the IDE view hasn't refreshed, then program the bench unit.
2. Confirm S2 PASS and successful normal uploads in the actual logging backend,
   including a full realtime cycle and settings-only cycle. UART success alone is
   HTTP acknowledgement, not proof of the database contents.
3. Disconnect Ethernet for five minutes, reconnect and observe retained logging
   work retry automatically. Check for successful upload without resetting the unit,
   no steady allocation failures and continued S2 recovery.
4. Repeat short cable interruptions during handshake/upload/response. Confirm no
   lock remains held, no partial settings batch is applied, and other tasks continue.
5. Check overview round-trip after a controlled display setting change. Only use
   a suitable bench setting; do not trigger resets or unsafe heat-pump commands.
6. Leave both services running and monitor errors, backend data and heap behavior.
   These runtime/production-service checks have not been performed by the agent.
