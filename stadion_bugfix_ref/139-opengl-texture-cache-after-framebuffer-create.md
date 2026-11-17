# OpenGL: only the first render-to-texture target got its texture (cup grid crosses missing)

- **Category:** rendering
- **Zero status:** already ported (libultraship 4148f93 contains fork commit e0a4b28)
- **stadion:** [PR #139](https://github.com/team-hippodrome/stadion/pull/139), commit(s) `212cf39` (libultraship `e0a4b28`, fork PR #2)
- **stadion files:** `libultraship` (`src/fast/backends/gfx_opengl.cpp`)

## Symptom
Poké Cup trainer grid on OpenGL: only the first beaten trainer got the red cross; the others were drawn without it.

## Root cause
`SelectTexture` skips `glBindTexture` when `mLastBoundTextures` already names the texture. `CreateFramebuffer` and
`UpdateFramebufferParameters` bound the new target's colour texture and then texture 0 behind that cache. From the
second render target in one frame on, the cross texture was a cache hit, so the draw sampled texture 0.

## Fix in stadion
Both functions rebind the texture the cache records for the active unit (`RebindCachedTexture`).

## Zero / plethron
plethron's pin `4148f93`: `src/fast/backends/gfx_opengl.cpp:949` (comment), :965 and :1041
(`RebindCachedTexture(mLastBoundTextures, mLastActiveTexture)`). Zero draws trainer portraits/badges the same way, so
it benefits; nothing to do.

## How to detect
Several render-to-texture draws in one frame that look right on Metal/Vulkan but blank on OpenGL. Grep backends for
raw `glBindTexture` calls that bypass a binding cache.
