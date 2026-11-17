# Slow GPUs: game time fell behind real time (slow motion, sound queue starving)

- **Category:** timing-pacing
- **Zero status:** already ported (lag make-up and frame skipping are in plethron's os_devices.cpp)
- **stadion:** [PR #118](https://github.com/team-hippodrome/stadion/pull/118), commit(s) `a79c60e`, `5063329` (merge `abc9e51`)
- **stadion files:** `src/port/os_devices.cpp`, `src/port/interp.cpp`, `src/port/audio_out.cpp`, `src/port/Engine.cpp`, `src/port/launcher.cpp`

## Symptom
Intel HD 520 (Windows): low frame rate and stuttering sound; the game ran in slow motion.

## Root cause
In lockstep mode a retrace waits for the window thread to draw the game's frame, and the game produces sound per
game frame. When drawing took longer than real time the clock restarted after one late retrace, so game time (and
audio production, ~70% of the device rate) fell behind and the queue ran dry repeatedly. Smooth motion added an
in-between draw per frame on the same thread.

## Fix in stadion
Retraces make up a lag of up to 100 ms (`kMaxLagNs`); while the clock lags by more than a retrace, up to 2 game
frames in a row are tracked but not drawn (`WantSkipWhileBehind`, `gPort.FrameSkip`, `PORT_NO_FRAME_SKIP=1`);
in-between frames are dropped while behind; the audio queue target grows (to 120 ms) after a non-stall underrun.
`PORT_DEBUG_DRAW_MS=<ms>` simulates a slow GPU. Plus a launcher Graphics panel.

## Zero / plethron
`src/port/os_devices.cpp:955` `kMaxLagNs`, `:973` max-lag clamp, `:1178` `Port_ClockLagNs`, `:1530`
`WantSkipWhileBehind`, `:1574` its use; `src/port/Engine.cpp:666` `DebugDrawDelay`. `interp.cpp` and `audio_out.cpp`
are byte-identical to stadion's.

## How to detect
`PORT_DEBUG_DRAW_MS=25 PORT_TRACE_PACING=1`: `[pace] ... skipped to catch up` lines and no repeated audio refills;
game speed near 100%.
