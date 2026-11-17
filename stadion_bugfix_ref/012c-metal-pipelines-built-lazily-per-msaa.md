# Metal hitches: every new shader built a pipeline for every MSAA level

- **Category:** performance
- **Zero status:** already ported (libultraship fork at 4148f93 has `PipelineFor`)
- **stadion:** [PR #12](https://github.com/team-hippodrome/stadion/pull/12), commit(s) `7cbfc13`
- **stadion files:** `patches/libultraship.patch` (`src/fast/backends/gfx_metal.cpp`, `include/fast/backends/gfx_metal.h`)

## Symptom
Visible hitches on macOS whenever a scene used a shader combination for the first time.

## Root cause
`GfxRenderingAPIMetal::CreateAndLoadNewShader` pre-built a `MTL::RenderPipelineState` for every supported MSAA
level (typically 4), so each new shader cost several synchronous pipeline compiles while a draw waited.

## Fix in stadion
Keep the retained `pipeline_descriptor` in `ShaderProgramMetal`; build only the window's MSAA level at creation and
other levels on first use via `PipelineFor(prg, msaaLevel)`, called from `DrawTriangles`.
`PORT_TRACE_PACING=1` logs `[shader] ... built in N ms`.

## Zero / plethron
`git -C /home/user/lus show 4148f93:src/fast/backends/gfx_metal.cpp`: `PipelineFor` defined at line 561, used at
486 (creation) and 817 (draw). Inherited through the submodule pin.

## How to detect
`PORT_TRACE_PACING=1` and look for `[shader]` lines with large build times or `[present]` gaps > 40 ms that line up
with them.
