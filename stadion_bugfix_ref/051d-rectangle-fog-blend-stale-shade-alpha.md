# Rectangles fogged by a stale shade alpha (Pokédex AREA label boxes painted in fog colour)

- **Category:** rendering
- **Zero status:** already ported (libultraship fork at pin 4148f93)
- **stadion:** [PR #51](https://github.com/team-hippodrome/stadion/pull/51), commit(s) `dfe6826`
- **stadion files:** `patches/libultraship.patch` (`src/fast/interpreter.cpp`, `GfxSpTri1`)

## Symptom
The AREA map's label boxes were pale (painted in the fog colour).

## Root cause
A rectangle has no shade on the RDP, so a blender that fogs by shade alpha leaves it unchanged. The interpreter used a stale per-vertex alpha of 0xFF for rectangles and fully fogged them.

## Fix in stadion
In `GfxSpTri1`, `is_rect` triangles get fog factor 0 when the blend uses shade alpha (combined with #49's `fog_by_fog_alpha` branch).

## Zero / plethron
Present: `libultraship/src/fast/interpreter.cpp:3051-3054` at pin 4148f93 (`} else if (is_rect) { ... mBufVbo[mBufVboLen++] = 0.0f;`). Zero draws its habitat labels the same way over the fogged Kanto map (`pokedex_2.c` area labels).

## How to detect
2D boxes drawn while a fog render mode is still active look tinted/washed out; check `is_rect` paths whenever shade-derived values feed blending.
