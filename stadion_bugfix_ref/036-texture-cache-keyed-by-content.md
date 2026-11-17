# Texture cache keyed by content: RAM textures re-uploaded every frame (battle stutter)

- **Category:** performance
- **Zero status:** already ported (in the libultraship fork at plethron's pin 4148f93)
- **stadion:** [PR #36](https://github.com/team-hippodrome/stadion/pull/36), commit(s) `7896bd2` (reached main through #41; follow-up to #34, see `034c-...`)
- **stadion files:** `patches/libultraship.patch` (`src/fast/interpreter.cpp` ImportTexture, `src/fast/backends/gfx_opengl.cpp`)

## Symptom
Battle frames stalled in `glTexImage2D` on the AYN Thor (Android, GLES): hundreds of present lines with a draw time over 20 ms per 75 s replay.

## Root cause
A RAM texture's `loaded_texture.addr` is its TMEM snapshot, a fresh buffer for nearly every load, and the texture cache key included that address. About 55 cache misses a frame (33,000 uploads per 600 frames) for only 558 distinct texture contents in the whole replay.

## Fix in stadion
`ImportTexture`: snapshotted RAM textures are keyed by the content hash plus a `decode_hash` of everything the decoders read (load/line sizes, tile bounds, clamp/mask, HD scale, palette, filtering). Resource/replacement textures keep the address key. GL backend: a recycled texture id re-uploaded at the same size uses `glTexSubImage2D`. Uploads dropped to 200-700 per 600 frames.

## Zero / plethron
Inherited through the fork: `libultraship/src/fast/interpreter.cpp:1881-1909` (the PORT comment and `key.decode_hash = d;`) and `libultraship/src/fast/backends/gfx_opengl.cpp:719` (`glTexSubImage2D` on same-size re-upload) at submodule pin 4148f93 (`git -C /home/user/lus grep decode_hash 4148f93`).

## How to detect
Off-CPU profile showing time in `glTexImage2D`/`ImportTexture*`; count uploads per frame vs distinct texture hashes. `PORT_TRACE_PACING=1` shows `[present]` lines with long draw times.
