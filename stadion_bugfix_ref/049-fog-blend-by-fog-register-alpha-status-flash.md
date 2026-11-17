# Status flashes drew flat silhouettes: fog blend used shade alpha instead of the fog register's alpha

- **Category:** rendering
- **Zero status:** already ported (libultraship fork at pin 4148f93)
- **stadion:** [PR #49](https://github.com/team-hippodrome/stadion/pull/49), commit(s) `9da3139`
- **stadion files:** `patches/libultraship.patch` (`src/fast/interpreter.cpp`, `GfxSpTri1`)

## Symptom
When a move inflicts a status (Flamethrower burn, Ice Beam freeze, Dragon Rage, Night Shade...), the target turned into a solid, unshaded fog-coloured silhouette instead of a tint that keeps shading.

## Root cause
The flashes use `GeoRender_SetFogColor`: fog colour plus render mode `CLR_FOG * A_FOG + CLR_IN * (1 - A_FOG)` (`G_RM_FOG_PRIM_A`), a constant blend by the fog colour's alpha. The interpreter always used the vertex fog factor (shade alpha) whenever the blender source was `CLR_FOG`; with `G_FOG` off that is the lit model's alpha, 255.

## Fix in stadion
When the blender's alpha input is `G_BL_A_FOG`, blend by `fog_color.a` instead of shade alpha.

## Zero / plethron
Present: `libultraship/src/fast/interpreter.cpp:2632` (`bool fog_by_fog_alpha = blend_src == G_BL_CLR_FOG && ((mRdp->other_mode_l >> 26) & 3) == G_BL_A_FOG;`) and `:3047-3050` at pin 4148f93. Zero uses the same blend: `GeoRender_ApplyEntryFog` (`game/src/geo_render.c:683-695`) emits the entry's fog colour and ORs `0xC4000000` into the render mode, i.e. P=CLR_FOG, A=A_FOG (`G_RM_FOG_PRIM_A`), so its per-model fog tints depend on this fix.

## How to detect
Battle cross-check against ares (`tools/battle_sweep.py`, `effect_p90` per move); visually: any effect that turns a shaded model into a flat solid colour. Decode the other_mode_l blender bits when a tint looks flat.
