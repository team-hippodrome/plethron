# Texgen: lookat kept as floats instead of the RSP's s0.7 bytes (Sing overlay flickers between two looks)

- **Category:** rendering
- **Zero status:** already ported (libultraship fork at pin 4148f93)
- **stadion:** [PR #62](https://github.com/team-hippodrome/stadion/pull/62), commit(s) `cf12a5d` (originally `082d2e0`)
- **stadion files:** `patches/libultraship.patch` (`src/fast/interpreter.cpp`, `Interpreter::GfxSpVertex`)

## Symptom
During Sing, the swirling full-screen overlay jumped between two looks on alternate frames and leaned green; ares stays steady.

## Root cause
The overlay is a rippling plane drawn with `G_TEXTURE_GEN` on a 32x32 texture at scale 0x8000 (wraps ~16 times), so a one-texel texgen error recolours the screen. The RSP keeps transformed lookat directions as signed bytes (s0.7); libultraship kept floats, and lookat X's y component (difference of two nearly equal terms) ran a sawtooth (-0.0059, -0.0002, -0.0032...).

## Fix in stadion
After `CalculateNormalDir`, `current_lookat_coeffs` are floored to 1/128 steps and clamped at 127/128 (as the RSP's arithmetic shift does). Light directions stay float. Floor matched best (round 56.5, trunc 60.6, float 49 vs floor 33.1 effect score).

## Zero / plethron
Present: `libultraship/src/fast/interpreter.cpp:2243-2252` at pin 4148f93 (`fminf(floorf(mRsp->current_lookat_coeffs[a][b] * 128.0f), 127.0f) / 128.0f`). Zero enables texgen in the geo renderer (`game/src/geo_render.c:830` `gSPSetGeometryMode(..., G_TEXTURE_GEN)`), and the move effects share Stadium 1 code.

## How to detect
Texgen/environment-mapped effects that alternate frame to frame while the camera is still; battle cross-check `effect_p90` for texgen moves (Sing, Barrier, Light Screen, Reflect).
