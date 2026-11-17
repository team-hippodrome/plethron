# Widescreen: Hypnosis' swirl (camera-facing effect grids) stopped short of the right edge

- **Category:** rendering
- **Zero status:** needed (helper `Port_WidenEffectGridX` is inherited; Zero's three grid inits do not call it)
- **stadion:** [PR #120](https://github.com/team-hippodrome/stadion/pull/120), commit(s) `ce24d57` (merge `1c00cf4`)
- **stadion files:** `patches/game/20-fragment-battle_engine.patch` (battle_engine_34E1A0.c `Trail40_Init`, battle_engine_34F1A0.c `Trail40Variant_InitPool`, battle_engine_3502D0.c `Trail40GridAlt_Init`), `src/port/Engine.cpp`, `src/port/sdk/port_compat.h`

## Symptom
In 16:9 the Hypnosis swirl (and other moves using these grids) ended before the right edge, with streaks where the
generated texture coordinates ran out.

## Root cause
The grids' nodes span x = i*9 - 72 for i in 0..15, i.e. -72..63: 0.94 of the 4:3 half-width on the right. A 16:9
picture needs 4/3 of it.

## Fix in stadion
```c
var_v0->unk_00 = Port_WidenEffectGridX((i * 9.0f) - 72.0f); // PORT: covers a widescreen picture
```
`Port_WidenEffectGridX(x)` = `x * 1.45f` when `Port::AspectSetting() == 1`, else `x` (4:3 unchanged).

## Zero / plethron
- Helper present: `src/port/Engine.cpp:880`, declared `src/port/StadionPort.h:124`.
- Not called (grep in `game/src` finds nothing). Zero's counterparts still set `position.x = (i * 9.0f) - 72.0f;`:
  - `game/src/fragments/battle/hypnosis_screech_trail.c:44` (`HypnosisScreechTrail_Init`, = Trail40_Init)
  - `game/src/fragments/battle/sing_trail.c:71` (`SingTrail_InitPool`, = Trail40Variant_InitPool)
  - `game/src/fragments/battle/trail40_grid.c:47` (`Trail40GridAlt_Init`)
  Wrap each in `Port_WidenEffectGridX(...)`.

## How to detect
`grep -rn "\* 9.0f) - 72.0f" game/src` for the grids; generally any effect whose geometry is laid out in fixed
screen-space units at 4:3. Review moves in 16:9 with stadion's `tools/widescreen_review.py` approach.
