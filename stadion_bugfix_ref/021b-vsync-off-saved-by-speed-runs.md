# Vsync stayed off after a PORT_SPEED run (saved into the config)

- **Category:** timing-pacing
- **Zero status:** already ported (`src/port/Engine.cpp:213-228`)
- **stadion:** [PR #21](https://github.com/team-hippodrome/stadion/pull/21), commit(s) `ae382b4`
- **stadion files:** `src/port/Engine.cpp`

## Symptom
After any `PORT_SPEED>1` run (e.g. the replay runner), every later normal-speed run from that folder presented without
vsync: ~30% of intervals off the 1/60 s grid.

## Root cause
`PORT_SPEED>1` turned `gVsyncEnabled` off so the speed-up is not capped, and the config was saved with that value,
indistinguishable from a player's choice.

## Fix in stadion
Keep the player's value in `gPort.VsyncSaved` while overridden and restore it on the next normal-speed run.

## Zero / plethron
`src/port/Engine.cpp:213-228`: `gPort.VsyncOverridden` / `gPort.VsyncSaved` save and restore around
`Port_Speed() > 1.0`.

## How to detect
Diff the saved config (`pokestadium.cfg.json` or plethron's equivalent) before/after a debug run; any debug override
written through `cvars` and then `Save()` is suspect.
