# S2 prerequisite: SAME54 entropy and certificate clock audit

## Finding

Target preprocessing with the actual XC32 include/configuration flags confirmed
that wolfSSL selected the MICROCHIP_PIC32 seed fallback, which calls srand with
SYS_TIME_CounterGet and emits rand bytes. The SAME54 hardware TRNG port files
present in the tree were not the selected seed implementation. Source-file
presence alone therefore did not establish the actual entropy path.

## Implemented

- src/files/crypto_random.c/.h: TULIP_CryptoSeed, returning zero on success or
  minus one on failure; TULIP_CryptoRandom is a bool adapter for S2 callbacks.
- src/files/crypto_random_hw.h: SAME54 APBC/TRNG register access using DFP masks.
- src/config/default/config.h: CUSTOM_RAND_GENERATE_SEED selects the hardware
  helper, overriding the timer/rand fallback without editing vendor random.c.
- SYS_RANDOM_CryptoBlockGet now returns zero and clears the output if
  CRYPT_RNG_BlockGenerate fails, instead of unconditionally reporting success.

The helper enables the TRNG APBC clock and peripheral, waits for DATARDY, then
reads DATA. DATA read automatically acknowledges DATARDY on SAME54. It leaves
TRNG enabled between calls, does not reset it per request and does not enable an
interrupt or standby operation. Calls are main-loop only and non-reentrant.
There is no heap allocation, timer dependency, pseudo-random fallback or watchdog
clearing. A call accepts at most 1024 bytes, with a shared 100000 unsuccessful-poll
budget for the entire request. This is an iteration bound, not a calibrated time.
A timeout clears all requested bytes, including bytes already generated, and
returns failure. Invalid arguments are rejected before hardware access.

This shared wolfSSL seed hook also changes the entropy source for logging TLS.
Its context, certificate policy, socket flow, TCP buffers and packet protocol are
unchanged. Cryptographic randomness is available to future S2 WebSocket nonces
and masks through TULIP_CryptoRandom; it is not wired to a live S2 connection yet.
The hardware mechanism is not a claim of statistical certification or a complete
entropy-health-test implementation. Real hardware behavior remains to be tested.

## Clock finding

The compiled certificate-time function wc_Time uses pic32_time, which reads
TCPIP_SNTP_UTCSecondsGet. An S2 adapter should wait for valid synchronized UTC
before negotiating certificate-verified TLS. No clock override or certificate
validation bypass has been added. The logging provider still uses its existing
SSL_VERIFY_NONE policy; the future S2 adapter must have its own verified context.
The entropy hook is complete; that TLS/socket adapter and application task remain
pending, as do pairing/session initiation and durable trust/token handling.

## Verification

    python tests/crypto_random/run_tests.py --cc gcc

Zig cc can also be selected. Tests compile the production entropy helper with
hardware-access doubles, exercising delayed readiness, byte/remainder boundaries,
maximum length, no-ready timeout, partial-output clearing, invalid arguments and
subsequent recovery. The same runner extracts SYS_RANDOM_CryptoBlockGet unchanged
and tests RNG failure/lock failure reporting. These tests do not emulate analog
entropy or measure physical peripheral timing.

The helper, actual wolfSSL random.c, crypto wrapper, system RNG adapter, NET_PRES
TLS glue and logging module compile for ATSAME54P20A with XC32 v4.60. Preprocessed
random.c is checked to select TULIP_CryptoSeed and exclude the old timer fallback.
Both MPLAB configurations include the new production source and headers.
Full firmware linking and hardware verification still need a Clean and Build.
If the IDE view is stale, reopen or add existing crypto_random.c, crypto_random.h
and crypto_random_hw.h under the existing files folders.

Before the next integration step, boot the board repeatedly and confirm regular
logging TLS connections/settings updates still work. Check Ethernet reconnection
and normal heat-pump responsiveness. Hardware seeding failure must produce a TLS
failure rather than hang the main loop. Do not print generated seed/key bytes
in production diagnostics. No S2 resource control or connection is enabled.

## Primary references

- SAME54 product/device documentation: https://www.microchip.com/en-us/product/ATSAME54P20A
- SAM D5x/E5x TRNG initialization: https://onlinedocs.microchip.com/oxy/GUID-F5813793-E016-46F5-A9E2-718D8BCED496-en-US-15/GUID-DE057F93-840F-4F37-A469-CF1D70E81F58.html
- Family data sheet: https://ww1.microchip.com/downloads/en/DeviceDoc/SAM_D5xE5x_Family_Data_Sheet_DS60001507F.pdf
- Family errata: https://ww1.microchip.com/downloads/aemDocuments/documents/MCU32/ProductDocuments/Errata/SAM-D5x-E5x-Family-Silicon-Errata-and-Data-Sheet-Clarification-DS80000748.pdf
