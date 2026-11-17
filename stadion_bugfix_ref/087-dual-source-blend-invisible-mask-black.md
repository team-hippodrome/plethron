# Rock moves drew a black quad on OpenGL/Vulkan: invisible depth masks kept an opaque blend alpha

- **Category:** rendering
- **Zero status:** already ported (pinned libultraship GLSL shaders zero the alpha before both output paths)
- **stadion:** [PR #87](https://github.com/team-hippodrome/stadion/pull/87), commit(s) `3bc68fb` (squashed into `2db362c` locally)
- **stadion files:** `patches/libultraship.patch` (src/fast/shaders/opengl/default.shader.glsl, src/fast/shaders/vulkan/default.shader.glsl)

## Symptom
Strength, Rock Throw, Rock Slide: a large opaque black quad over the arena on OpenGL and Vulkan; Metal/D3D11 fine.

## Root cause
The move's special render flag submits an invisible depth-only mask (`o_invisible`). In the GLSL dual-source path
the first output's alpha (coverage) was zeroed, but `texel.a = 0.0` was only applied in the non-dual branch, so the
second output (`vOutColor1`, which drives RGB blending) stayed opaque and replaced the framebuffer colour with black.

## Fix in stadion
Move `@if(o_invisible) texel.a = 0.0; @end` before the `@if(dual_source)` / `@if(dual_out)` split in both templates.

## Zero / plethron
libultraship 4148f93: `src/fast/shaders/opengl/default.shader.glsl:290-293` ("Depth-only masks preserve colour and
coverage") zeroes `texel.a` before `@if(dual_source)` at :294; `src/fast/shaders/vulkan/default.shader.glsl:294`
does the same before `@if(dual_out)`. Zero's battle uses the same moves and renderer, so it is covered.

## How to detect
Compare backends on any effect that uses invisible/depth-only draws (`o_invisible`); a black rectangle on GL/Vulkan
only points at the dual-source path. Grep shader templates for output writes that differ between the dual-source
and single-output branches.
