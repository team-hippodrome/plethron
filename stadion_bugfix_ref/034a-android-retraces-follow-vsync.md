# Android stutter: retraces on a free clock drifted against the panel (59.35 Hz), frames landed on uneven refreshes

- **Category:** timing-pacing
- **Zero status:** already ported (`src/port/pacing_android.cpp`)
- **stadion:** [PR #34](https://github.com/team-hippodrome/stadion/pull/34), commit(s) `df7b9ac`, `889d43c`
- **stadion files:** `src/port/pacing_android.cpp`, `src/port/os_devices.cpp`

## Symptom
On the AYN Thor ~10% of frames in 30 fps scenes were 16 ms or >= 50 ms apart; 44-84% of retraces off the 60 Hz grid.

## Root cause
(1) The panel reports 60 Hz but refreshes at ~59.35 Hz; the free 16.67 ms retrace clock beat against it.
(2) Frames whose draw took 15 ms vs 18 ms were shown on different refreshes, alternating 16/50 ms.

## Fix in stadion
AChoreographer vsync timestamps feed `RetraceSchedule` (as CVDisplayLink on macOS); `Port_BeforePresent` sets
`eglPresentationTimeANDROID` so each frame appears two refreshes after its retrace (`PORT_PRESENT_REFRESHES`).

## Zero / plethron
`src/port/pacing_android.cpp:1-2` (vsync source for `RetraceSchedule`), `:77-85` (AChoreographer callbacks),
`:103-120` (`Port_BeforePresent`, `eglPresentationTimeANDROID`).

## How to detect
`adb shell dumpsys SurfaceFlinger --latency <layer>` or `PORT_TRACE_PACING=1` (via `port_env.txt` on Android):
present-to-present intervals not equal to 2 refreshes in a 30 fps scene.
