# Texture rectangles copying the shown frame buffer onto itself painted stale RDRAM (black/noise on OpenGL)

- **Category:** rendering
- **Zero status:** already ported (plethron 4db7329: libultraship pin 95a0f1f; no Zero trigger known, the fix is in place if one shows up)
- **stadion:** [PR #156](https://github.com/team-hippodrome/stadion/pull/156), commit(s) `d1b0c65` (libultraship `89d2976`, fork PR #7)
- **stadion files:** `libultraship` (`src/fast/interpreter.cpp`)

## Symptom
Gallery, enlarged photo on OpenGL: black and random-coloured noise over the top-left quarter; screenshots differed
from run to run. Metal was fine.

## Root cause
The game copies the frame buffer being shown onto itself each frame (its RDRAM loaded as a 320-wide image in strips,
drawn at the same pixels with the colour image at the same address). On N64 that is a no-op; on the port the picture
lives on the GPU and that RDRAM holds old data.

## Fix in stadion
`IsFrameBufferSelfCopy`: skip a texrect on a main frame buffer when its source is the colour image's own RDRAM at
exactly the destination pixel (same width, RGBA16, 1:1, no flip, offset 0) and colour/alpha come out as the texel
(copy mode, or 1-cycle TEXEL0). `PORT_DRAW_FB_SELF_COPIES=1` draws them as before.

## Zero / plethron
Pin `4148f93` has no such check in `Interpreter::GfxDpTextureRectangle` (`src/fast/interpreter.cpp:3944`). Zero has no
Gallery; whether another Zero screen self-copies is unknown. Comes free with the bump to `8556b99` (#155/#156 measured
no change elsewhere).

## How to detect
OpenGL-only black/noise patches on 2D screens that differ between runs; set `PORT_DRAW_FB_SELF_COPIES=1` after the bump
to see whether a screen hits the path.
