# OpenGL ES: Poké Ball send-out burst grey and blocky (dual-source blending off)

- **Category:** rendering
- **Zero status:** already ported (fork at 4148f93, `gfx_opengl.cpp:251-360`)
- **stadion:** [PR #34](https://github.com/team-hippodrome/stadion/pull/34), commit(s) `d96fdd2`
- **stadion files:** `patches/libultraship.patch` (`src/fast/backends/gfx_opengl.cpp`)

## Symptom
On Android the send-out burst (and other effects that read the screen back / blend by a second colour) came out grey
and blocky; macOS GL with `PORT_GL_NO_DUAL=1` showed the same.

## Root cause
The port's blender emulation uses dual-source blending (a second fragment output as blend factor). On GLES it was
disabled, so those combiner/blender modes fell back to a wrong approximation.

## Fix in stadion
Enable dual-source blending on GLES when `GL_EXT_blend_func_extended` is present (Adreno, Mali, PowerVR), with
`#extension GL_EXT_blend_func_extended : require` in the shader prelude.

## Zero / plethron
`git -C /home/user/lus show 4148f93:src/fast/backends/gfx_opengl.cpp`: extension check at lines 251-265, prelude at
333, `dual_out` at 353-360.

## How to detect
Compare effect frames GL vs Metal/Vulkan; force `PORT_GL_NO_DUAL=1` on desktop to reproduce the fallback.
