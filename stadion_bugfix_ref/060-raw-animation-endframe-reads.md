# Raw big-endian animation headers read natively: endFrame 256x too large (Dream Eater's sparkles missing)

- **Category:** endianness
- **Zero status:** needed (four raw `endFrame` reads remain in Zero's battle code)
- **stadion:** [PR #60](https://github.com/team-hippodrome/stadion/pull/60), commit(s) `19bb77a`
- **stadion files:** `patches/game/02-core.patch` (`model_animation.c` `ModelAnim_GetEndFrame`, `model_animation_events.c` `ModelAnim_GetEventEndFrame`), `patches/game/20-fragment-battle_engine.patch` (`battle_engine_315D50.c`, `battle_engine_2EC3C0.c`, `battle_engine_309ED0.c`), minigame patches

## Symptom
Dream Eater's nine star sparkles never appeared; Ekans minigame Digletts stayed in their holes; Drowzee/Dynamo poses wrong.

## Root cause
Model animation headers loaded from ROM blobs stay big-endian; the accessors in model_animation*.c swap, but code elsewhere read `transformAnim.data->endFrame` / `eventTrack.data->endFrame` directly: 17 read as 4352. `BattleAnim_BindModelAnimReversed` started the stars thousands of frames past the end of their animation.

## Fix in stadion
New `ModelAnim_GetEndFrame` (`TAD_END`) and `ModelAnim_GetEventEndFrame` (`ETD_END`), used at every outside read: `BattleAnim_BindModelAnimReversed`, `BattleAnim_TrySetModelEventTrackFromRow`, `BattleScene_RunRepeatedHitHpTransition`, and the minigames.

## Zero / plethron
Zero has the accessor macro `ANIM_END(d)` (`game/src/anim_raw.h:39`, raw-aware) and uses it inside geo_render.c/geo_node.c/model_animation_events.c, but these reads bypass it:
- `game/src/fragments/battle/anim_effect_queue.c:939` `BattleAnim_BindModelAnimReversed`: `arg0->transformAnim.data->endFrame - 1` (the Dream Eater bug; Zero's comment at `:933` says it ran only for Dream Eater).
- `game/src/fragments/battle/move_animation.c:1977` `BattleAnim_TrySetModelEventTrackFromRow`: `eventTrack.data->endFrame`.
- `game/src/fragments/battle/phases.c:73` `BattleScene_ReactionMultiHitEnd` (multi-hit HP transition): `M2C_FIELD(...transformAnim.data, u16*, 0xA)`.
- `game/src/fragments/battle/turn_actions.c:1130` `BattleAnim_AlignIdleLoopToFrameCount`: `transformAnim.data->endFrame` (Zero-specific).
- Debug only: `game/src/fragments/battle/menu_panels.c:1328,1384,1424` (sprintf of endFrame).
Per `anim_raw.h:5-9`, animation data of models loaded from ROM blobs keeps its N64 byte order, so the Pokémon-model reads (phases.c, move_animation.c, turn_actions.c) are raw; the battle-effect asset read in anim_effect_queue.c is raw if the effect asset file is a blob (confirm with `Anim_IsRaw`). Replace with `ANIM_END(x.data)` (or a `ModelAnim_GetEndFrame` helper).

## How to detect
`grep -rn -E 'endFrame|M2C_FIELD\(.*(transformAnim|eventTrack)\.data' game/src` outside the animation accessors; any header field of a raw blob read through a typed struct. Runtime: a log of endFrame values > 0x100 for short animations.
