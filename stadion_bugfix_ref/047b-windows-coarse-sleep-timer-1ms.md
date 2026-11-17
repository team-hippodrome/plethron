# Windows: game clock slept on the 15.6 ms timer tick (retraces in bursts, audio ran short)

- **Category:** timing-pacing
- **Zero status:** already ported (inherited `src/port/pacing.cpp`)
- **stadion:** [PR #47](https://github.com/team-hippodrome/stadion/pull/47), commit(s) `63b56a8`
- **stadion files:** `src/port/pacing.cpp`, `CMakeLists.txt` (links `winmm`)

## Symptom
On Windows, presents 30-50 ms apart in bursts; the playback-rate control hit its 3 % limit. Simulated on macOS with sleeps rounded to 15.625 ms: frames more than 40 ms apart went from 9 to 161 per 40 s. Wine's precise `Sleep` hid it.

## Root cause
The lockstep clock's `sleep_until` and its 20 us idle polls become `Sleep(ms)` in the MSVC/mingw C++ runtime, which ends on the 15.6 ms system tick. SDL3 no longer raises the timer resolution; LUS's frame limiter had its own high-res timer, the port's clock did not.

## Fix in stadion
`Port_PreciseTimingThread` calls `timeBeginPeriod(1)` once and raises the clock thread's priority (`THREAD_PRIORITY_HIGHEST`); link `winmm`.

## Zero / plethron
Present: `src/port/pacing.cpp:41-50` (`timeBeginPeriod(1)`, `SetThreadPriority(..., THREAD_PRIORITY_HIGHEST)`), `CMakeLists.txt:413` (`target_link_libraries(pokestadium PRIVATE winmm)`).

## How to detect
`PORT_TRACE_PACING=1` on real Windows (not Wine): present intervals clustered at multiples of ~15.6 ms. grep for `sleep_for`/`sleep_until`/`Sleep(` on threads that pace frames or audio.
