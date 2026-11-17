# Android: ~15% CPU in the unwinder capturing stacks on every blocking receive

- **Category:** performance
- **Zero status:** already ported (`src/port/port_platform.h:93-117`, `src/port/os_thread.cpp:388`)
- **stadion:** [PR #34](https://github.com/team-hippodrome/stadion/pull/34), commit(s) `59ce668`
- **stadion files:** `src/port/port_platform.h`, `src/port/os_thread.cpp`

## Symptom
High app CPU on Android (68-70% before the PR's changes) with much of it in `dl_iterate_phdr`.

## Root cause
The port records where a game thread blocks (for the stall report) with `backtrace`/the unwinder. On Android the
unwinder looks up each frame's unwind tables via `dl_iterate_phdr`, under the scheduler lock, on every blocking
`osRecvMesg`.

## Fix in stadion
`Port_BacktraceFast`: on arm64 Android walk the x29 frame-record chain (strip PAC bits from LR, stop on non-monotonic
or misaligned frames); the crash handler keeps the full unwinder.

## Zero / plethron
`src/port/port_platform.h:98` defines `Port_BacktraceFast` (fallback `#define` at `:117`), used at
`src/port/os_thread.cpp:388` (`tlsSelf->blockedDepth = Port_BacktraceFast(...)`).

## How to detect
`simpleperf` / `perf` on device: `dl_iterate_phdr`, `_Unwind_*` or `backtrace` in the top symbols.
