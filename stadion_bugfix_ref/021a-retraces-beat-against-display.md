# Uneven frame pacing: free-running VI clock woke late and beat against the display refresh

- **Category:** timing-pacing
- **Zero status:** already ported (`RetraceSchedule` in `src/port/os_devices.cpp:957`, `src/port/pacing_mac.mm`)
- **stadion:** [PR #21](https://github.com/team-hippodrome/stadion/pull/21), commit(s) `ae382b4`
- **stadion files:** `src/port/os_devices.cpp`, `src/port/pacing_mac.mm`, `src/port/pacing.h`, `src/port/Engine.cpp`

## Symptom
30 fps scenes alternated 1 and 3 refreshes per frame (judder), worse at 120 Hz; 25-35% of retraces off the 1/60 s grid.

## Root cause
1. macOS coalesced the clock thread's sleeps: retraces up to 7.5 ms late, then caught up (7 ms / 27 ms gaps).
2. The free 16.67 ms clock drifted through the display's refresh, so frames finishing near a vblank landed one
   refresh early or late.

## Fix in stadion
`RetraceSchedule`: when the display runs at a whole multiple of 60 Hz, a retrace comes 0.5 ms after every n-th vblank
(CVDisplayLink, smoothed); otherwise the free clock as before. The clock thread gets a Mach time-constraint policy.
libultraship's limiter caps at 64 fps while paced. `PORT_TRACE_PACING` prints retrace/drawn/shown statistics.

## Zero / plethron
`src/port/os_devices.cpp:957` (`class RetraceSchedule`, used at `:1045` and `:1230`), `src/port/pacing_mac.mm`
(CVDisplayLink, `thread_policy_set(... THREAD_TIME_CONSTRAINT_POLICY ...)` at `:149-155`), 64 fps cap note at
`src/port/Engine.cpp:735-742`.

## How to detect
`PORT_TRACE_PACING=1`: summaries every 10 s with p95/p99 and the share off the 1/60 s grid.
