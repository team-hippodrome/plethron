# Intermittent frame-0 crash: osContInit on the game thread raced the window thread's pad polling

- **Category:** input
- **Zero status:** already ported (`port_osContInit` marshals to the window thread; Zero calls `osContInit` through the macro)
- **stadion:** [PR #98](https://github.com/team-hippodrome/stadion/pull/98), commit(s) `83d7baa` (squashed into `2db362c` locally)
- **stadion files:** `src/port/os_devices.cpp`, `src/port/Engine.cpp`, `src/port/sdk/port_compat.h`

## Symptom
Occasional crash at the very first frame in `Controller::ReadToOSContPad`.

## Root cause
libultraship's `osContInit` (reached from the game thread) builds the control deck and writes the CVar settings
map while the window thread already polls pads (`deck->WriteToPad` in `PumpFrame`); unsynchronised std::map
mutation and partially installed mappings.

## Fix in stadion
`#define osContInit port_osContInit`: the call queues a request with a `std::promise` and waits;
`Port_ProcessControllerInitRequests()` runs it on the window thread at the top of `PumpFrame` before polling. The
game thread keeps its virtual scheduler token while waiting, so the replay clock/RNG are unchanged.

## Zero / plethron
`src/port/sdk/port_compat.h:172-173` (macro), `src/port/os_devices.cpp:488` (`port_osContInit`) and `:503`
(`Port_ProcessControllerInitRequests`), `src/port/Engine.cpp:785` (called in `PumpFrame`). Zero's
`game/src/controller.c:130` calls `osContInit(...)`, which the macro redirects.

## How to detect
Any libultraship call made from a game thread that touches SDL, CVars or the control deck. Run many cold starts
(stadion did 30 per backend); TSan on the port shows it.
