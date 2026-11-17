# Smooth motion on slow GPUs froze then jumped ahead (in-between pictures did not fit a game frame)

- **Category:** timing-pacing
- **Zero status:** already ported (plethron `src/port/interp.cpp` is identical to stadion's current one)
- **stadion:** [PR #136](https://github.com/team-hippodrome/stadion/pull/136), commit(s) `a8eecf2`, `3f31beb`
- **stadion files:** `src/port/interp.cpp`, `src/port/Engine.cpp`, `src/port/Engine.h`

## Symptom
Ceremonies and the Castle ending "fast-forwarded"; with a simulated slow GPU (`PORT_DEBUG_DRAW_MS=25`) the cup-clear
ceremony had 92 freezes over 50 ms and 37 jumps in 15 s with Smooth motion, ~1 and 0 without.

## Root cause
Every in-between picture is a full draw. When two no longer fit in one 33 ms game frame, game time lags and the
catch-up (`PumpRspTasks`) skips whole game frames.

## Fix in stadion
A picture budget: after an overrun (picture finished after the next game frame was due, first picture took two
refreshes, drops in 3 frames in a row) the next frames get one picture each, with a hold that doubles per overrun
(2 s to 5 min). Also: scene cuts shown exactly, recorded matrices pinned to their first-drawing value, resubmitted
frames not blended backwards. Debug: `PORT_INTERP_BUDGET=0`, `PORT_INTERP_IMMEDIATE=late`, `PORT_INTERP_LOG=2/3`.

## Zero / plethron
`src/port/interp.cpp` in plethron has the same md5 as stadion HEAD. Budget: `struct Budget` at `interp.cpp:111`,
explanation :102-110, `BudgetDisabled` :121; scene cut :446. Hooked up for Zero in plethron `3de3b4e`. Nothing to do.

## How to detect
`PORT_DEBUG_DRAW_MS=25` + `tools/interp_check/pacing.sh` (present in plethron) counts freezes and jumps.
