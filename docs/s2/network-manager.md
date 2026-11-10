# Network manager - step 2

The manager owns shared Ethernet TCP/IP startup, readiness and recovery. Logging
owns its presentation socket, DNS/SNTP requests and transaction timer. S2 is not
yet connected. No heat-pump control or production image offsets are changed.

## Startup and task order

1. SYS_Initialize initializes drivers, SYS_TIME, presentation and applications.
   It leaves sysObj.tcpip invalid instead of starting the generated default stack.
2. main initializes the manager once after SYS_Initialize.
3. The I2C task supplies its verified EUI with NETWORK_MANAGER_SetHardwareEui.
   The manager copies it and preserves the existing EUI-to-Ethernet-MAC mapping.
4. The manager starts the configured Harmony stack from its main-loop task.
   The original network defaults and module table remain in use.
5. The logging INIT state registers its cleanup callback before opening sockets.
   Logging waits for manager readiness before acquiring its transaction lock.

main polls the manager after SYS_Tasks. Harmony advances stack initialization in
SYS_Tasks on subsequent iterations. TCPIP_STACK_Task safely ignores the invalid
module object while waiting for the hardware EUI or a retry deadline.

## Status and ownership

NETWORK_MANAGER_GetStatus returns state, raw Harmony IPv4 address, revision and
stackRestarts. NETWORK_MANAGER_IsReady reports the last sampled readiness.
READY requires a ready stack, an up and linked eth0 interface, and a ready,
nonzero IPv4 address. It does not guarantee DNS, Internet or CEM availability.
The address clears when readiness is lost. Revision changes with state/address;
stackRestarts counts actual recovery teardowns. Both reset at manager Initialize.
The existing logging reset-counter field reports the low eight bits of that
restart count, retaining its previous field width.

All manager APIs run in the cooperative main loop, not interrupts. Interface
handles are reacquired each poll. Consumers must register a down callback before
opening sockets. Up to four distinct callbacks are supported; registration
returns false when full. Callbacks run synchronously on readiness loss, IPv4
changes and before teardown, while the stack still exists. They must release
owned sockets, return promptly and tolerate repeated notifications. Socket use
must stop until readiness returns. Callback registration/lifecycle mutation from
inside a callback is unsupported.

| Responsibility | Owner |
| --- | --- |
| EUI acquisition and validation | I2C |
| Ethernet stack configuration/startup/teardown/retry | Network manager |
| Link and address observation | Network manager |
| Logging socket and transaction cleanup | Logging |
| Future S2 sockets and sessions | S2 application |

## Recovery policy

- Failed stack initialization: retry after 5 seconds.
- Stack error: notify clients, deinitialize in the main loop, retry after 5 seconds.
- Stack initialization, missing interface or interface-down state lasting
  30 seconds: notify clients, deinitialize, retry after 5 seconds.
- Linked interface waiting for a usable IPv4 address for 120 seconds: recover
  the stack and retry after 5 seconds.
- Cable absent: notify clients on readiness loss, keep the stack alive, and let
  Harmony handle link return. Do not repeatedly reset for an unplugged cable.
- Logging server/DNS/SNTP/transaction timeout: clean up logging only. This does
  not tear down the shared stack or interrupt a future S2 connection.

Deadlines use the 64-bit SYS_TIME counter and frequency to avoid truncating long
timeouts at the SAME54 clock frequency. TC2's callback only sets a volatile flag.
Logging consumes that flag in its task, stops the timer, closes its socket and
releases its own lock. Each new transaction resets the timer counter. Logging
cleanup never unlocks a mutex owned by the SD-card task. DNS/open failures now
follow the normal close path so they do not leave a transaction timer running.

## Verification

    python tests/network_manager/run_tests.py --cc gcc

A Zig executable may also be supplied. Host tests compile the real manager with
Harmony doubles and exercise hardware-ID waiting/MAC configuration, initialization
failure and retry, readiness prerequisites, address changes, repeated link loss,
client notification before teardown, startup/address deadlines, replacement
handles after recovery and 64-bit counter wrap. The clock double runs at 120 MHz.

Hardware checks before proceeding:

1. Clean/build, boot with Ethernet connected, and verify the expected MAC,
   DHCP address, regular logging and settings exchange.
2. Boot unplugged, then reconnect; verify logging resumes and restart count does
   not grow merely because the cable is absent.
3. Unplug during an active logging transaction, reconnect, and verify the next
   transaction succeeds without a stale socket or logging lock.
4. Block the logging server while retaining LAN access. After the transaction
   timeout, verify logging returns to idle and the stack remains available.
5. Disable DHCP with the cable connected. Verify address-timeout recovery and
   eventual reconnection when DHCP returns; also check SD-card logging continues.

Host tests and target object compilation cannot verify PHY/DHCP timing, actual
NET_PRES cleanup or concurrent SD-card transaction behavior on hardware.

## MPLAB / MCC

This step adds no further production files. Keep the existing network_manager.c
and .h registered in MPLAB. The generated initialization.c intentionally defers
stack startup; MCC regeneration may restore TCPIP_STACK_Init and must be reviewed.
There must be exactly one application owner of TCPIP_STACK_Initialize and
TCPIP_STACK_Deinitialize: network_manager.c. The generated network configuration
and module table are still used by that owner. No linker/bootloader offset changes
are needed.
