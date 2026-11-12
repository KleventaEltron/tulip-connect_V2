# S2 token journal (storage-independent stage)

Added `src/files/s2/s2_token_store.c/.h`; registered in both MPLAB configurations.
No application calls, global instance, EEPROM address reservation, flash writes,
SD writes or network behavior are introduced by this stage.

The session engine needs durable old+pending tokens before confirmation, then
only the new token after confirmation. Supply S2_TOKEN_STORE_Persist as its
storage callback and the initialized store as user. Load the journal on restart
before starting a session. A successful empty snapshot (count zero) represents
an explicit cleared credential set; physical secure erase is a separate concern.

## Record and recovery

Two dedicated 568-byte slots (1136 bytes total), encoded independently of C
padding/size_t. Each includes format magic, little-endian generation, 32-byte
pairing binding, count and two NUL-terminated token fields, CRC32 and commit byte.
Offsets: magic 0, generation 4, binding 8, count 40, token fields 41 and 298,
reserved 555..558, CRC 559..562, reserved 563..566, commit 567.
The owner supplies a stable binding identifying the paired CEM/trust/resource.
The module does not derive that identity, establish trust or validate S2 base64;
the existing session engine validates protocol tokens. Records are plaintext.
CRC detects accidental damage; it does not provide authentication or encryption.

Writes target the older/unused slot: invalidate commit and sync; write payload
and sync; write commit and sync; read back. The previous newest slot is untouched.
A failed callback can mean old or new committed tokens; the next operation reads
both slots again. Generation comparison handles wrap; equal/half-range ambiguity
fails closed. With no valid records only two fully erased (0xff) slots are EMPTY.
Unrecognized/corrupted/binding-mismatched data returns INVALID rather than being
silently overwritten. Provisioning must handle first-write interruption explicitly.

## Hardware backend integration still required

The read/write/sync interface explicitly requires ordered durable synchronization
and failure isolation between slots. An interrupted write may affect the target
range but must never damage the other slot. Read failures must be reported.
The original EEPROM helpers do not provide this contract: writes return void, wait loops
are unbounded and no durable completion result is exposed. Two logical EEPROM
ranges are not automatically independent hardware failure domains. Two FAT files
on the logging SD card are not automatically power-cut safe either.

Before connecting this to firmware: validate actual SmartEEPROM fuse allocation,
bootloader/update preservation, block-level power-loss behavior, settings/reset
ownership, wear and bounded write latency; implement the adapter with an error
path and real flush semantics; then perform physical power-cut tests. Do not
merely return true from a RAM cache or an unchecked write. This module is a tested
journal algorithm, not yet a claim of durable storage on installed hardware.

The record scratch buffer uses 1136 bytes of automatic storage; there is no heap
allocation. Future integration must account for call-chain stack and main-loop
latency. No tokens are printed. Temporary record buffers are explicitly wiped.

## Verification

`python tests/s2_token_store/run_tests.py --cc <compiler>`

Actual C module: 1142 byte-interruption/restart cases including previously
committed slot reuse, all three sync failure points, unknown pairing, read error,
invalid input, protected-byte corruption, and recovery after uncertain commit.
The simulated medium writes immediately and reports injected failures; these
checks do not model a real EEPROM controller or filesystem. Strict host warnings
and XC32 SAME54 compilation pass. Full MPLAB linking/hardware testing is pending.

A concrete SAME54 adapter is now implemented; see token-store-same54.md. It is
not yet called by the application. Its physical power-loss behavior and
bootloader preservation still need qualification before connecting it to
session rotation.
