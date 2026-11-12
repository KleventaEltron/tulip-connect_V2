# SAME54 token-storage adapter

Added `src/files/s2/s2_token_store_same54.c/.h` and its small register-access
header `s2_token_store_same54_hw.h`. Both MPLAB configurations include the source.
This is the concrete SmartEEPROM implementation behind the existing journal IO.
No application initialization/calls or EEPROM writes have been enabled yet.

## Layout and checks

`eeprom.h` now reserves logical ranges [1024,2048) and [2048,3072) for S2.
Each 568-byte journal record uses the beginning of its reserved range. These
ranges don't overlap current named application settings (through byte 146) or
the known bootloader-version field at 60. This source tree does not contain the
installed bootloader implementations, so their preservation/ownership of these
ranges remains unverified. This is a logical EEPROM layout, not an application
flash relocation; no linker, bootloader offset, fuse or User Row changes occur.

Initialization is read-only and checks runtime SBLK=1 / PSZ=3 (4096-byte virtual
SmartEEPROM), automatic sector reallocation, unlocked access, controller status,
and SYS_TIME availability. Other fuse combinations are rejected, not silently
reconfigured. A busy controller can time out. IO callbacks restrict all access
to the two 568-byte records; compile-time checks bound the reserved ranges.

Reads/writes wait for SmartEEPROM idle and command-ready. Writes skip unchanged
bytes, clear only the SEEWRC completion flag, wait for AHB-write completion,
check controller errors and compare the byte read back. The backend leaves
shared WMODE unchanged. Sync checks LOAD, issues SEEFLUSH if necessary, and waits
for LOAD to drain with BUSY clear and command-ready. It does not assume a new
SEEWRC interrupt is emitted by SEEFLUSH. ADDRE, PROGE, LOCKE, ECCDE, NVME and
SEESOVF are treated as errors; they are preserved rather than cleared away.
No automatic erase, reallocation command, unlock or reset is performed.

A callback has a 1000 ms elapsed-time deadline and a 1,000,000-poll fallback if
the timer stalls. This is synchronous; a whole journal save uses multiple
callbacks and could take several seconds near fault timeouts. It must not run
from an interrupt and is not yet scheduled in the control loop. A memory-mapped
access can itself stall in hardware; software polling is not a hard real-time
bound for a hung bus. Measure healthy/sector-reallocation latency before wiring
this into the controller. Any NVM writer/interrupt handler that clears completion
or errors must be excluded during the operation. Existing settings helpers are
unchanged. Disable prevents further adapter IO without touching stored bytes.

## Integration and qualification still pending

Intended ownership: session owner -> S2_TOKEN_STORE -> this adapter -> SAME54 NVM.
The owner will initialize the adapter, initialize the journal with pairing
identity, load saved tokens and pass journal Persist as the session callback.
Only actual successful pairing may provision the initial credentials. Invalid
records are not automatically erased. A torn first provisioning write needs an
explicit pairing recovery path; a zero-count tombstone is not secure erasure.

The slots occupy different logical pages, but SmartEEPROM automatically remaps
physical pages and reallocates whole sectors. Neither logical separation nor
readback proves the journal's power-loss contract on actual silicon. Qualify
silicon errata/revision, BOD behavior, interruptions during sector reallocation,
reset and installed-bootloader updates, then measure wear/latency and run power
cuts on a spare board before live token rotation. The SD card is not involved.
This step implements the hardware backend but does not establish production
power-cut durability or activate S2.

## Verification

`python tests/s2_token_store_same54/run_tests.py --cc <compiler>`

Actual adapter + actual journal with a simulated controller: 1142 interrupted-
write/restart cases in buffered/unbuffered modes, untouched settings/padding,
unsupported/locked/error states, busy and frozen-clock timeouts, failed readback,
failed flush, invalid ranges, unchanged-byte wear avoidance and disabled IO.
The model does not represent physical page tearing/sector recovery, bus stalls
or installed bootloader behavior. Existing standalone journal and HTTPS tests
are separate regressions. XC32 compilation checks the actual SAME54 register
names and SYS_TIME APIs. Full firmware linking and real-hardware qualification
are pending.

References checked 2026-11-11:
- Microchip SAM D5x/E5x datasheet, sections 25.6.8.4 and 25.6.8.6-7:
  https://ww1.microchip.com/downloads/aemDocuments/documents/MCU32/ProductDocuments/DataSheets/SAM-D5x-E5x-Family-Data-Sheet-DS60001507.pdf
- Existing project SAME54 DFP register definitions and NVMCTRL PLIB.
