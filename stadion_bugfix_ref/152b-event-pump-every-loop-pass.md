# Window loop pumped OS events and polled pads on every ~1 ms pass (85 % of the window thread's time)

- **Category:** performance
- **Zero status:** needed (plethron `PumpFrame` pumps unconditionally; see also `157-...`)
- **stadion:** [PR #152](https://github.com/team-hippodrome/stadion/pull/152), commit(s) `08e38c1`
- **stadion files:** `src/port/Engine.cpp`, `src/port/os_devices.cpp`

## Symptom
Window thread busy mostly in the Cocoa event pump (about 7x the cost of drawing the game); wasted CPU/battery.

## Root cause
`main.cpp`'s window loop calls `PumpFrame` about once per millisecond while it waits for the game, and every call ran
`Port_ProcessControllerInitRequests()`, `sWindow->HandleEvents()` and the window/display lookup. The clock only takes
the pads at a retrace (`LatchPads`).

## Fix in stadion
`os_devices.cpp` records `gLastRetraceNs` at each retrace (both clocks) and exports `Port_NextRetraceNs()`.
`PumpFrame` returns early unless it is within 1.5 ms before the next retrace (and has not pumped for it yet) or the
last pump was 4 ms ago (later changed to 33 ms + mid-frame pump in #157). Interpolation and RSP tasks still run every
pass. Cup replay 19.3 G → 16.4 G instructions.

## Zero / plethron
Missing: `src/port/Engine.cpp:772` `PumpFrame` goes straight to `Port_ProcessControllerInitRequests();
sWindow->HandleEvents();` (:785-786) with no rate limit; `Port_NextRetraceNs` / `gLastRetraceNs` do not exist
(`grep` finds nothing); retrace sites are `os_devices.cpp:1114-1115` (`gTick = ++tick; LatchPads(tick);`) and :1244 in `port_osCreateViManager`.
plethron `Engine.cpp` is identical to stadion 766c25d, so stadion `08e38c1` + `704db9d` apply nearly as-is.

## How to detect
Sample the window thread (Instruments / `perf record`): `PumpFrame`/`HandleEvents` above `RunGameFrame`. Check pad
freshness at the retrace after the change (pre-retrace pump must stay).
