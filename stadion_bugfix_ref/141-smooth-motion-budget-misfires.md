# Smooth motion picture budget misfired: 2160p menus and slow GPUs held at one picture per game frame

- **Category:** timing-pacing
- **Zero status:** already ported (plethron `src/port/interp.cpp` is identical to stadion's current one)
- **stadion:** [PR #141](https://github.com/team-hippodrome/stadion/pull/141), commit(s) `e68cd65`
- **stadion files:** `src/port/interp.cpp`, `src/port/launcher.cpp`

## Symptom
M5 MacBook Air: menus did not reach 60 FPS at 2160p although the GPU had room. Intel HD 520: title at ~20 FPS at any
setting (v0.0.12 showed 60).

## Root cause
The #136 budget fired on its own side effects:
1. A long picture delays the window thread's RSP tasks, so the next retrace is a few ms late; `PumpInterpolation`
   dropped in-between pictures and `PicturesAllowed` renewed the hold at 4 ms of clock lag, so it never recovered.
2. "3 drops in a row" was never reset by a completed series: it meant "3 drops ever".
3. One late picture (a shader build, a load) started a hold.

## Fix in stadion
Lag tolerance of one refresh (`PORT_INTERP_LAG_MS`); drop counter reset per completed series and only counting due
pictures; a hold needs a second late picture within one second (`NoteLatePicture`). `PORT_INTERP_OLD_BUDGET=1` restores
the old rules.

## Zero / plethron
Present: `src/port/interp.cpp:128-140` (`NoteLatePicture`, 60-tick repeat rule), :772 (`PORT_INTERP_LAG_MS`).
Nothing to do. (The launcher "taller than this display" hint in the same PR is cosmetic.)

## How to detect
`PORT_INTERP_LOG=1` logs each dropped picture and why; `tools/interp_check/pacing.sh` with `PORT_DEBUG_DRAW_MS=14`
should show all pictures drawn and 0 freezes.
