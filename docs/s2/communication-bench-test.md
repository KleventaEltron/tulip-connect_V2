# S2 communication bench test (no pump control)

This milestone provides an opt-in device test and local PC CEM. It tests S2 Connect
HTTPS authentication/token rotation, WebSocket upgrade, S2 JSON exchange and
correlated responses. Nothing here activates FRBC or calls pump/control setters.
Default firmware remains OFF and performs no bench token writes or connections.

## What is exchanged

Both peers send Handshake, using the current v1.0.0 JSON profile also selected by
the HTTPS session. The CEM sends HandshakeResponse. The diagnostic RM then sends
ResourceManagerDetails for a clearly named **simulated** resource, advertising
only NOT_CONTROLABLE. It provides one synthetic 0 W ELECTRIC.POWER.L1 sample with
UTC timestamp; this is not read from the heat pump. The test server acknowledges
these messages, selects NOT_CONTROLABLE, then deliberately attempts to select
FILL_RATE_BASED_CONTROL. The device responds INVALID_CONTENT to that unsupported
selection. It never advertises FRBC or receives/executes an FRBC instruction.

ReceptionStatus correlates subject_message_id with the original message. It is
never acknowledged again. The firmware and server independently print PASS after
the expected positive/negative exchanges. Arbitrary text echo is not the pass
criterion. This diagnostic subset is not a full S2 RM or interoperability certification.

## Files and ownership

- src/files/s2/s2_test_messages.c/.h: bounded parser, handshake/response queue,
  diagnostics, simulated resource/measurement and rejection of control messages.
  Reception callback only queues replies; transport APIs are called outside it.
- src/app_s2_comm_test.c/.h: opt-in bench owner, first provisioning of an EMPTY
  journal, real SAME54 backend, session coordinator and message dispatch.
- src/files/s2/s2_comm_test_config.h: TULIP_S2_COMM_TEST_ENABLE defaults to 0.
- tools/s2_comm_test/server.py: local TLS/HTTPS/WebSocket CEM and fixture generator.
- tests/s2_messages: C tests, pinned official schemas and loopback integration test.

main calls the empty/default bench hooks after normal S2 initialization/tasks.
The existing session coordinator now accepts optional ipv4Override as an explicitly
provisioned route. TLS still verifies the configured hostname and CA; only DNS
lookup is bypassed. This makes the first LAN test possible without changing DNS,
mDNS, hosts files or logging settings. Zero preserves the existing DNS behavior.
No shared stack or logging configuration changes are made.

The simulated resource is isolated from actual heat-pump state. There are no
pump/valve/relay/Modbus controller dependencies in the message engine or bench owner.
This means S2 messages cannot change the pump through this test. The normal
firmware still runs its existing autonomous pump control.

## First hardware run

Use a bench unit for the first run. This uses the already reserved EEPROM token
ranges; it does not reserve more EEPROM. Physical power-cut/sector-reallocation
qualification and whole-main-loop storage timing remain pending. Avoid interrupting
power during initial credential provisioning in this first communication test.
Do not treat this as a field release.

From the inner `tulip-connect_V2` project directory, on the PC:

1. Install Python dependencies (Python 3.10+):

   `python -m pip install -r tools/s2_comm_test/requirements.txt`

2. Generate a private bench identity and certificate for the PC's reachable IPv4:

   `python tools/s2_comm_test/server.py init --ip 192.168.1.50`

   Replace that address with the actual PC address. This creates private files in
   tools/s2_comm_test/local and src/files/s2/s2_comm_test_local.h. Both are ignored
   by Git. No secret tokens are printed. Generated certificates last 30 days.
   The generated identity is NOT automatically trusted by any production CEM.
   Init refuses to overwrite an existing identity/header; retain them for reruns.

3. Start the server on that PC address:

   `python tools/s2_comm_test/server.py serve --bind 192.168.1.50`

   The default bind without --bind is loopback only and cannot accept the board.
   The board must be able to reach TCP port 8443 on the PC; allow that connection
   in the PC firewall if necessary. This tool does not change firewall rules.

4. In src/files/s2/s2_comm_test_config.h set TULIP_S2_COMM_TEST_ENABLE to 1.
   Reopen the MPLAB project if its file view is stale, then Clean and Build using
   the correct application/bootloader configuration and program the bench unit.
   This change deliberately activates the test; no such change is enabled in the
   supplied code. Do not separately activate manual APP_S2 session injection.

5. Keep the normal Ethernet network and valid SNTP clock available. Certificate
   time validation is enforced. The test waits for network and time, checks actual
   EEPROM configuration (currently SBLK=1/PSZ=3), and loads the journal. Only a
   completely EMPTY journal is provisioned with the generated initial token.
   Different pairing, invalid records and storage failures stop without erasing
   settings or replacing unknown credentials. A paired/occupied store needs an
   explicit migration/reset decision; there is no automatic erase button here.

6. Observe the UART console and PC console. Expected firmware progress:

   `S2 COMM TEST: starting simulated NOT_CONTROLABLE resource`

   `S2 COMM TEST PASS: handshake, resource details, simulated measurement and rejected control acknowledged; TX=... RX=...`

   The PC independently prints `S2 COMM TEST PASS: both directions, ...`.
   Seeing only a socket connection or only TX is not sufficient.

The device's existing coordinator status exposes detailed failure categories.
A blocked connection also prints its error/TLS/HTTP status (no credentials).
The message test has a 30-second deadline while WebSocket is open. The PC server
is a short bench fixture and closes idle connections after 35 seconds; the device
may then retry with a fresh session. Full long-running heartbeat policy remains
separate work.

Keep config.json, state.json, certificates, the private key and generated header
from the same identity together. The server persists rotated tokens across normal
restarts, and the device resumes them from EEPROM. Recreating only one side's
identity causes authentication/binding failures by design. Never delete a real
unit's token store merely to make this test pass. A torn first provisioning save
requires explicit recovery; unknown EEPROM records are not overwritten.

After testing, set TULIP_S2_COMM_TEST_ENABLE back to 0 and rebuild. Persisted lab
credentials remain in the reserved token slots; disabling does not erase them.
Generated private material is ignored by Git and should remain local.

## Memory

No bench workspace is allocated in the OFF build. The enabled bench uses the
compact limits in src/files/s2/s2_limits.h: 640-byte WebSocket messages,
512-byte HTTP/upgrade headers, 128-byte TLS I/O chunks, 1024-byte HTTPS JSON,
768-byte HTTP requests, 1024-byte trusted CA and three 640-byte reply slots.
These limits cover the generated fixture; they are not future FRBC payload limits.
The normal/OFF profile retains the original larger protocol limits.

HTTPS and WebSocket use a single retained CA copy in the channel workspace.
The enabled SAME54 work context is 15,272 bytes and the diagnostic messages
context is 2,264 bytes, plus small backend/bookkeeping fields: roughly 18 KB total.
This replaces the original roughly 60 KB bench allocation, which did not fit
alongside the project's configured 160,000-byte heap. No heap reduction or EEPROM
allocation change was made. Clean and Build is required whenever the profile is
changed because context layouts differ between the ON/OFF builds.

A full link using the existing project objects and recompiled compact S2 sources
passed with a 160,000-byte heap and an explicit 8192-byte minimum stack check.
The verification memory summary reported 91,790 bytes static data and 170,354
bytes outside that static allocation (the latter must cover heap and stack; it
is not additional free space after the heap). The ROM origin stayed at the
programmer build's existing 0x0. No image was flashed and no project heap, linker
script or bootloader settings were altered. Hardware peak heap/stack use still
needs observation during simultaneous logging and S2 TLS.

## Verification performed

The C test executable links only the diagnostic handler and cJSON. It checks state
ordering, invalid messages/duplicate fields/decoded NUL, response loops, queue
bounds, entropy failure and rejection of control selection. An interactive mode
then drives that same C code against the actual local PC server over verified
TLS, HTTPS token rotation and masked WebSocket frames. The outbound C messages
and server replies are validated against pinned official JSON schemas. Both peers
confirmed PASS. No pump module is linked into this executable.

Run with the installed compiler:

`python tests/s2_messages/run_tests.py --cc <C-compiler>`

The server portion requires the Python dependencies above. Certificate generation
and tokens in this automated test stay in a temporary directory, the listener binds
127.0.0.1 only, and is shut down afterward. Nothing is flashed to hardware.
Both disabled and enabled device branches compile with strict XC32 SAME54 checks;
the enabled compile uses a temporary artifact fixture, leaving the project flag OFF.
Existing coordinator/application/HTTPS/session/TLS regressions remain relevant.
Actual board TLS interoperability, logging concurrency and EEPROM power-cut behavior
must still be confirmed on hardware.

Official references checked 2026-11-12:
- https://github.com/flexiblepower/s2-json (pinned commit and schema hashes in tests/s2_messages/spec/UPSTREAM.json)
- https://docs.s2standard.org/model-reference/Common/Handshake/
- https://docs.s2standard.org/model-reference/Common/ReceptionStatus/
- https://docs.s2standard.org/model-reference/Common/ControlType/

NOT_CONTROLABLE is the standard spelling. v1.0.0 is the only offered/accepted
message-version string in this test; no beta/older version fallback is enabled.

## Regression profiles

General WebSocket/TLS/HTTP regression runners explicitly compile the normal
profile, independently of the bench flag selected in the developer's firmware.
The diagnostic C/PC integration runner explicitly uses the compact profile.
The coordinator can exercise both profiles:

`python tests/app_s2_session/run_tests.py --cc <compiler>`

`python tests/app_s2_session/run_tests.py --cc <compiler> --bench`

Only s2_limits.h is a new MPLAB header for the memory fix; no extra C source
needs to be added. The compact certificate limit is checked against the fixture.
