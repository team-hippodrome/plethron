# Hyper Beam / Ice Beam strands lost their wave: a never-written local folded to 0 by clang

- **Category:** game-logic
- **Zero status:** already ported (plethron 5134413: `spA8 = 0` in HyperBeamIceBeamTrail_UpdateSlot)
- **stadion:** [PR #55](https://github.com/team-hippodrome/stadion/pull/55), commit(s) `22b0571`
- **stadion files:** `patches/game/20-fragment-battle_engine.patch` (`battle_engine_357C30.c`, now `game/src/fragments/battle_engine/battle_engine_hyper_beam_ice_beam_trails.c`)

## Symptom
Hyper Beam and Ice Beam strands left the user's mouth flat instead of wavy (effect_p90 135 vs ares).

## Root cause
`BattleAnim_SixSlotTrail_UpdateSlot` (now `HyperBeamIceBeamTrail_UpdateSlot`) reads a local `spA8` (each strand node's wave phase) that it never writes. In the ROM it is `lw s7,168(sp)` after `addiu sp,-176`: on the N64 whatever an earlier call left in that stack slot. clang treats the read as undefined and folded every `sinf(((spA8 + r) % n) * 2pi / n)` to `sinf(0)`.

## Fix in stadion
```c
s32 spA8 = 0; // PORT: never set in the original; the N64 reads a stale stack word
```
Any fixed value looks the same (it only shifts an already random phase).

## Zero / plethron
Still uninitialised: `game/src/fragments/battle/hyper_beam_ice_beam_trail.c:220` (`s32 spA8; //!bug never set before it is read below...`), read at `:240`, `:242`, `:243` in the same `__sinf((((spA8 + temp_s3) % temp_s2) * 6.2831854820251465) / temp_s2)` expressions. Zero's comment at `:216` says it ran for Ice Beam and Hyper Beam in the move sweep. Fix: initialise to 0 with a PORT comment.

## How to detect
`clang -Wuninitialized -Wsometimes-uninitialized` over `game/src/fragments/battle` (stadion found no other case in its battle code); grep for `//!bug` comments about unset locals. At runtime: effects that should wobble or scatter look perfectly regular.
