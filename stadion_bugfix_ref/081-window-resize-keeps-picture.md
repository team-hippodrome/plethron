# Window resize blacked out menus that draw their background only once

- **Category:** rendering
- **Zero status:** already ported (libultraship fork at plethron's pin 4148f93 has `resize_keep.h` and the backend changes)
- **stadion:** [PR #81](https://github.com/team-hippodrome/stadion/pull/81), commit(s) `bd5f7aa` (squashed into `2db362c` locally)
- **stadion files:** `patches/libultraship.patch` (fast/backends/resize_keep.h, gfx_metal, gfx_opengl, gfx_vulkan, gfx_direct3d11)

## Symptom
Resizing the window on Free Battle's rule screen turned the background black; only the parts redrawn every frame
(text, cursor) stayed until the next screen.

## Root cause
A size change recreates every game frame buffer's colour texture (`Interpreter` -> `UpdateFramebufferParameters`)
and the first pass into a fresh texture clears it to black (`sFreshColorFbs`). N64-style menus draw their static
background only in a screen's first frames and then only repaint what changes, so the picture was lost.

## Fix in stadion
Every backend copies the old picture into the new texture, uniformly scaled so the 4:3 picture keeps its place
(shared rectangle in `fast/backends/resize_keep.h`): Metal draws a quad sampling the old texture, OpenGL copies
aside and blits back after the depth renderbuffer is resized, Vulkan `vkCmdBlitImage`, D3D11 draws a quad and
restores all context state. `PORT_NO_RESIZE_KEEP=1` restores the old behaviour; `PORT_DEBUG_RESIZE=f:WxH,...`
simulates a drag.

## Zero / plethron
plethron pins libultraship 4148f93 (`git ls-tree HEAD libultraship`). At that commit
`include/fast/backends/resize_keep.h` exists and line 15 reads `PORT_NO_RESIZE_KEEP`. Nothing game-side is needed:
Zero's menus benefit automatically.

## How to detect
Resize the window on any static menu; with `PORT_NO_RESIZE_KEEP=1` it goes black, without it it must not.
`PORT_DEBUG_RESIZE=3000:1100x700` reproduces it in a replay.
