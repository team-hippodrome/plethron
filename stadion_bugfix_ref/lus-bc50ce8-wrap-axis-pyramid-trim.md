# Scrolled wrapping textures squashed for a few frames (mipmap-style tile trim applied to a wrapping axis)

- **Category:** rendering
- **Zero status:** already ported (plethron 4db7329: libultraship pin 95a0f1f, the fork's main)
- **stadion:** [PR #155](https://github.com/team-hippodrome/stadion/pull/155) (bump that brought fork PR #4), commit(s) libultraship `bc50ce8`
- **stadion files:** `libultraship` (`src/fast/interpreter.cpp`)

## Symptom
In battles the sleep-Z sprite (32x64 sheet, `maskt` 6) was trimmed to 62/56/50/44 rows for 4 game frames at fixed
counter values: visibly squashed Zs.

## Root cause
A wrapping axis (mask set, not CLAMP) repeats every 2^mask texels; hardware only reads `uls/ult` for it. Games scroll
such textures with `gDPSetTileSize` and let `lrs/lrt` drift at another rate, so the tile region computed from them
drifts. When it drifted into the "pyramid-like" ratio window (loaded 1x to 1.625x of rendered) the importer trimmed
the texture to the tile region.

## Fix in stadion
`IsWrappingAxis(mask, clampMode, tileSize)`; the pyramid trim is skipped on a wrapping axis (`pyramidLike && !wrapS`)
in `ImportTextureRgba16/Rgba32/Ci4/Ci8` and in `GfxSpTri1`'s UV size. HD replacements and CLAMP axes keep the clamp.

## Zero / plethron
plethron pins `4148f93`, which still has the unconditional trim: `src/fast/interpreter.cpp:1111` (`pyramidLike`),
`:1127` `if ((isHd || pyramidLike || clampS) && ...)`, same at :1210, :1533 and `GfxSpTri1` :2896. Zero has the same
effect: `game/src/fragments/battle/anim_draw.c:121` "Draws a sleep-move Z mark ... 32x64 I4 texture". Comes with a
bump of the libultraship submodule to `8556b99` (or later).

## How to detect
`PORT_TEXLOG=1` (libultraship `interpreter.cpp:74`) texture logs showing an imported height that changes
for a few frames on a scrolled texture; frame-by-frame comparison against ares on sleep moves.
