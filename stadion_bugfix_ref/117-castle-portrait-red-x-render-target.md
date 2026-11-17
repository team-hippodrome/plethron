# Defeated trainer's portrait vanished: the red X was blended onto an empty GPU render target

- **Category:** rendering
- **Zero status:** not applicable (Castle badge carousel only; the libultraship mechanism `gDPPortPreserveColorImage` is inherited)
- **stadion:** [PR #117](https://github.com/team-hippodrome/stadion/pull/117), commit(s) `6b81e18`, `60a4061`, `cb86b26` (merge `220f9ec`)
- **stadion files:** `patches/game/20-fragment-battle_prep.patch`, `patches/libultraship.patch` (interpreter RTT init, shader caches), `src/port/sdk/port_compat.h`, `tools/rtt_check.py`

## Symptom
In Gym Leader Castle, a defeated trainer's portrait showed only the red X on black.

## Root cause
The game loads the portrait into an RDRAM image with the CPU and then renders the X into that image as a colour
image. The renderer created a fresh GPU render target for the address and never uploaded the CPU pixels, so the
blend went onto an empty texture.

## Fix in stadion
The game marks such draws: `gDPPortPreserveColorImage(gDisplayListHead++)` (G_NOOP 0x51) before the colour image;
the interpreter then uploads the existing RGBA16 pixels before the first draw. Only marked images are uploaded
(the first broad version cost 37 extra uploads per battle). The same PR adds disk caches for compiled shaders
(D3D bytecode, GL program binaries, Vulkan SPIR-V) and fixes Metal readback quantization.

## Zero / plethron
libultraship 4148f93 handles 0x51 (`src/fast/interpreter.cpp:315`, `:4782`). Zero's game code sets colour images
only in `game/src/gfx_rect.c:97,127,131,152,158` (frame/depth buffer setup); no fragment renders into a CPU-loaded
picture, so no marker is needed. Revisit if a Zero screen draws on top of a CPU-written image (cup grid crosses,
photo frames).

## How to detect
An image the CPU fills (`bcopy`/decompress into a buffer) and later passed to `gDPSetColorImage`: the GPU result
lacks the CPU content. stadion's `tools/rtt_check.py` reads the render target back.
