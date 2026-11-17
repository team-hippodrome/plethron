# Menus' rounded boxes: dark lines at their pieces' edges when scaled (texrect samples past a wrapping tile)

- **Category:** rendering
- **Zero status:** already ported (libultraship fork at pin 4148f93)
- **stadion:** [PR #46](https://github.com/team-hippodrome/stadion/pull/46), commit(s) `842364d`
- **stadion files:** `patches/libultraship.patch` (`src/fast/interpreter.cpp`, `GfxDpTextureRectangle`)

## Symptom
In fullscreen (non-integer scale, e.g. ~6.9x), thin dark lines around the corner pieces of the menus' rounded boxes. Not at integer scales; only with three-point or linear filtering.

## Root cause
The boxes are textured rectangles: 16x16 IA4 corner textures on tiles that wrap with a 32-texel period. Filtered edge pixels sample half a texel past the texture; because the tile wraps, they blend in the transparent opposite side. The N64 samples texel centres at 1:1 and never reaches past the edge.

## Fix in stadion
`GfxDpTextureRectangle`: a rectangle whose texture coordinates stay within the tile (texture shown once) is drawn with the tile clamped (cms/cmt) for that draw; tiled patterns keep wrapping. `PORT_NO_RECT_CLAMP=1` restores the old behaviour. `PORT_TEXLOG`'s settile line prints cms/cmt/masks/maskt.

## Zero / plethron
Present: `libultraship/src/fast/interpreter.cpp:4100-4110` at pin 4148f93 (`sNoRectClamp`, `savedCms`/`savedCmt`). Zero's menus use the same UI frame code (rounded windows with corner textures), so they benefit.

## How to detect
Dark/bright hairlines on 2D boxes only at non-integer window scales; toggle the texture filter to "none" - if the lines vanish it is edge sampling. Compare with `PORT_NO_RECT_CLAMP=1`.
