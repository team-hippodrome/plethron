# Event pump's 4 ms floor still cost twice the game's drawing; replaced by 33 ms floor + mid-frame pump

- **Category:** performance
- **Zero status:** already ported (plethron PR #54: stadion 704db9d as is; real-time keyboard input still reaches the game)
- **stadion:** [PR #157](https://github.com/team-hippodrome/stadion/pull/157), commit(s) `704db9d`
- **stadion files:** `src/port/Engine.cpp`

## Symptom
After #152, profiling showed `PumpFrame` at 1,101 samples against 472 in `RunGameFrame` (cup replay).

## Root cause
The "at least every 4 ms" fallback alone meant ~250 Cocoa pumps per second at ~0.3 ms each. Simply raising it would
let pads be a frame old when the pre-retrace pump is missed during a long draw (42 of 2,400 retraces).

## Fix in stadion
Floor 4 ms → 33 ms; keep the pump shortly before each retrace; add a pump halfway through a frame that has had none
since the last retrace:
```cpp
bool due = now - sLastPumpNs >= 33000000;
if (!due && retrace != 0) {
    const int64_t period = (int64_t)(16666667.0 / Port_Speed());
    const int64_t last = retrace - period;
    if (retrace - now <= 1500000 && sLastPumpNs < retrace - 2000000) due = true;
    else if (sLastPumpNs < last && now - last >= period / 2) due = true;
}
```
PumpFrame samples −50 %, window thread busy −27 %, pad snapshot fresh for 2,391/2,400 retraces.

## Zero / plethron
Missing, together with #152's rate limit: `src/port/Engine.cpp:772-786` (no `sLastPumpNs`, no `Port_NextRetraceNs`).
Port `152b` and this in one change.

## How to detect
Same as `152b`: window-thread profile; temporary instrumentation of pad snapshot age at `LatchPads`.
