# Smooth motion stretched limbs across the screen after a hitch (blended from a frame many retraces old)

- **Category:** rendering
- **Zero status:** already ported (`src/port/interp.cpp:692-697`, `kMaxGap = 4`)
- **stadion:** [PR #35](https://github.com/team-hippodrome/stadion/pull/35), commit(s) `d7bdac9`
- **stadion files:** `src/port/interp.cpp`, `PORTING_NOTES.md`

## Symptom
With Smooth motion on, a Clefairy limb streaked over the bottom-right corner during the title's fall (blended from
20 retraces back).

## Root cause
Over a long gap many of a model's matrices turned past the 60-degree interpolation cut (drawn as built) while the ones
skinned to them were still blended, so the skin between them stretched.

## Fix in stadion
`Port::RunGameFrame` blends only when the previous game frame is at most 4 retraces old:
```cpp
const uint64_t kMaxGap = 4;
if (cur && gPrev && !exact && tick > gPrevTick && tick - gPrevTick <= kMaxGap) { ... }
```

## Zero / plethron
`src/port/interp.cpp:695-697` (plus a serial check), active since Zero feeds frames to the interpolator
(plethron `3de3b4e`).

## How to detect
`PORT_INTERPOLATE=1 PORT_INTERP_IMMEDIATE=1 PORT_RENDER_EVERY=1` and step through hitches/scene starts; stretched
geometry only on in-between frames points to interpolation.
