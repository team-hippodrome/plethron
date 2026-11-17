# Steady audio pitch: playback-rate PI control oscillated (wobbling pitch)

- **Category:** audio
- **Zero status:** already ported (inherited `src/port/audio_out.cpp`)
- **stadion:** [PR #47](https://github.com/team-hippodrome/stadion/pull/47), commit(s) `63b56a8`
- **stadion files:** `src/port/audio_out.cpp`

## Symptom
Testers on Windows: audio "slowdown (freq problems)", sound effects at a different rate. Measured on macOS with `PORT_TRACE_AUDIO_OUT=1`: playback ratio 0.983-1.016 (about +/-26 cents), queue 42-108 ms. The game's own sound matched ares to the sample (`PORT_AUDIO_DUMP` vs `ARES_REF_WAV`).

## Root cause
The PI control of the SDL stream's playback ratio stepped per buffer with a strong integral term: an underdamped loop (omega ~0.65/s, damping ~0.05), i.e. a ~10 s oscillation of the pitch.

## Fix in stadion
PI control on the fill level smoothed over 0.5 s with gains per second (omega ~0.1/s, zeta ~0.8, P term capped at +/-0.5 %); playback starts once ~70 ms are queued and again after a stall drains the queue. Ratio now settles at 1.0031-1.0035.

## Zero / plethron
Present: `src/port/audio_out.cpp:186-191` (the comment "a PI control around the target, on the level smoothed over ~0.5 s", `kKp = 0.16, kKi = 0.01, kSmoothing = 0.5`) and the refill-after-stall logic at `:196-200`.

## How to detect
`PORT_TRACE_AUDIO_OUT=1` and plot the ratio over 40 s: any periodic swing is a control-loop problem, not game audio. Compare `PORT_AUDIO_DUMP` against an emulator recording to separate game-side from output-side pitch errors.
