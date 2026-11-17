# OpenGL on Linux: shader preprocessor dropped the rest of the shader after a taken @elseif; game picture upside down

- **Category:** rendering
- **Zero status:** already ported (libultraship fork at 4148f93)
- **stadion:** no PR ("Linux and Windows", before PR #1; history squashed into `be110fe`)
- **stadion files:** libultraship `src/fast/backends/gfx_opengl.cpp`, `src/fast/shaders/opengl/default.shader.glsl`

## Symptom
- Linux: render-to-texture UI built in earlier frames (Game Pak Check panels, menu buttons behind the Event Battle dialog) missing; macOS's OpenGL backend failed to link shaders.
- Linux with the window at the render size (640x480): the game shown upside down in the window, while screenshots looked right.

## Root cause
- The prism shader preprocessor drops the rest of the shader after a taken `@elseif` branch (or an `@if` nested in an `@else`), so the alpha shaders lost their tail.
- Game framebuffers were created without Y inversion when the window matched the render size; screenshots read back bottom-up and hid it.

## Fix in stadion
Fragment outputs chosen by flat flags set in `gfx_opengl.cpp` (`dual_out`, `single_out`) instead of `@elseif` chains; buffers inverted whenever they are shown as a texture, readback reverses rows of non-inverted buffers. Compatibility context (GLSL 1.30) kept on Linux with `glBindFragDataLocationIndexed`. Debug: `PORT_GL_FBLOG=1`, `PORT_GL_DUMP_SHADERS=<dir>`, `PORT_GL_NO_DUAL=1`.

## Zero / plethron
Already ported via fork 4148f93: `src/fast/backends/gfx_opengl.cpp:360-361` (`dual_out` flags), `:995-998` (invertY, `PORT_GL_FBLOG`); `default.shader.glsl` uses the flags.

## How to detect
`PORT_GL_DUMP_SHADERS=<dir>` and check the shader ends where expected; look at the real window (an X screen grab), not only port screenshots.
