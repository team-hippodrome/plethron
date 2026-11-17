# Closing the window crashed: main returned while game threads still ran, static destructors destroyed a mutex in use

- **Category:** platform-build
- **Zero status:** already ported (`src/port/main.cpp:522-528`)
- **stadion:** no PR (German Yellow / ares round, user report; before PR #1, history squashed into `be110fe`)
- **stadion files:** `src/port/main.cpp`

## Symptom
After an 80-minute session, closing the window ended with "mutex lock failed: Invalid argument" and abort (3/3 with `PORT_QUIT_AT`).

## Root cause
`main` returned after the window loop; the C++ static destructors ran while the game's host threads (audio, scheduler, ...) were still running, and one of them locked a destroyed mutex.

## Fix in stadion
End the process with `_exit(0)` after the loop, as the SIGTERM handler already did; saves are written through on every flash / Transfer Pak write, so nothing is lost. `PORT_QUIT_AT=<frame>` sends the window-close event for testing.

## Zero / plethron
Already ported: `src/port/main.cpp:522-528` (comment and `_exit(0)`), signal handlers at `:192-193`. Zero's SRAM is written through on every DMA write (`src/port/os_devices.cpp:332-338` calls `SramSave()` when the bytes change), so `_exit` loses nothing.

## How to detect
`PORT_QUIT_AT=<frame>` on a replay, repeated; any shutdown path that returns from `main` or calls `exit()` while game threads live.
