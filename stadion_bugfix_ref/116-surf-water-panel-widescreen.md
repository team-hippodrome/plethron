# Widescreen: Surf's camera-facing water panel kept its 4:3 width (visible edges in 16:9)

- **Category:** rendering
- **Zero status:** already ported (plethron b1ea1c8: `SurfTerrainGrid_BuildCameraBounds` calls `Port_WidenViewSpan`)
- **stadion:** [PR #116](https://github.com/team-hippodrome/stadion/pull/116), commit(s) `cb86887` (merge `d6fc25e`)
- **stadion files:** `patches/game/20-fragment-battle_engine.patch` (battle_engine_347330.c `TerrainGrid_BuildCameraBounds`), `src/port/Engine.cpp`, `src/port/sdk/port_compat.h`

## Symptom
In 16:9, Surf's water panel ended short of the picture's sides with hard rectangular edges.

## Root cause
The panel is a quad facing the camera whose half-width is computed from the 4:3 field of view:
`sp94 = perspective.unk_0C * |tan| * fovScale * 1.2f`. The widescreen projection shows 4/3 more horizontally.

## Fix in stadion
```c
sp94 = spB4->perspective.unk_0C * var_fs0 * sp48 * 1.2f;
sp94 = Port_WidenViewSpan(sp94); // PORT: covers the current camera's horizontal view
```
`Port_WidenViewSpan(span)` returns `span * 4/3` when `Port::AspectSetting() == 1`, else `span`.

## Zero / plethron
- Helper present: `src/port/Engine.cpp:873`, declared in `src/port/StadionPort.h:119`.
- Bug present: `game/src/fragments/battle/stat_ribbon_surf_grid.c:342` `SurfTerrainGrid_BuildCameraBounds`; line 408
  `sp94 = spB4->perspective.unk_0C * var_fs0 * sp48 * 1.2f;` has no `Port_WidenViewSpan` (grep in `game/src` finds
  no caller). Add the call right after line 408.

## Fixed in plethron (b1ea1c8)
`game/src/fragments/battle/stat_ribbon_surf_grid.c`: `sp94 = Port_WidenViewSpan(sp94);`. Checked in 16:9 with the title demo battle forced to Lapras using Surf (gdb, as plethron/dig-softlock/dig.gdb): the panel's side edges are gone from the full-screen water shots; 4:3 is unchanged.

## How to detect
Play every water/full-screen move in 16:9 and look for edges (stadion's `tools/widescreen_review.py` makes
before/after pairs). Grep the battle effects for camera-facing quads sized from `perspective`/FOV values.
