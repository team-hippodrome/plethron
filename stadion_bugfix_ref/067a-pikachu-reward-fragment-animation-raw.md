# Pikachu Surf reward crash: animation in a data fragment read as native (not recognised as raw ROM data)

- **Category:** endianness
- **Zero status:** check (Zero's raw test does not cover segment/fragment-mapped data)
- **stadion:** [PR #67](https://github.com/team-hippodrome/stadion/pull/67), commit(s) `e621388`
- **stadion files:** `patches/game/02-core.patch` (`src/model_animation.c`: `ANIM_RAW` -> `Port_IsRawData`), `PORTING_NOTES.md`

## Symptom
After the Pikachu reward taught Surf, the follower models switched to animation 9 and the port segfaulted in `ModelAnim_EvaluateScaleChannel` (fault address 0x8811208e).

## Root cause
The followers' animation (`D_8C2011C4`) lies in the data fragment `_798CD0` (linked at 0x8C200000, fragment-mapped), not in an archive blob. The accessors decided raw-ness with `Port_VIsRaw` (blobs only), read the big-endian header as native (0x8C201188 came out as 0x8811208C) and followed it.

## Fix in stadion
`#define ANIM_RAW(p) Port_IsRawData(p)`: blobs plus segment- and fragment-mapped ROM data, as for geo layouts.

## Zero / plethron
Zero decides raw-ness in `game/src/anim_raw.h:20-22`: `Anim_IsRaw(p) = Port_VIsRaw(p) || ZLift_IsRaw(p)` (blobs and lifted byte data), used e.g. at `game/src/geo_render.c:211`. It does not include the segment-mapped ranges that `Port_IsRawData` (`src/port/gfx_translate.cpp:564`, via `InRaw` at `:79-95`) covers. Zero has a Pikachu reward (`game/src/fragments/pikachu_reward/`); check whether its (or any other) model animations come from segment-mapped data that is neither a registered blob nor lifted. If so, switch `Anim_IsRaw` to `Port_IsRawData`.

## How to detect
Crash in an animation evaluator with a fault address that looks like a byte-swapped 0x8xxxxxxx address; log `Anim_IsRaw` vs `Port_IsRawData` disagreements for each bound `TransformAnimData`.
