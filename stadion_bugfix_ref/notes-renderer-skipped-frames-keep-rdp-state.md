# Skipped or dropped frames lost state the N64 keeps across frames (stale segments, missing texture loads, menus missing their one-time backgrounds)

- **Category:** rendering
- **Zero status:** already ported (libultraship fork at plethron's pin 4148f93 contains all of it)
- **stadion:** no PR ("Renderer: state that persists across frames", ares tours, Linux/Windows section; before PR #1, history squashed into `be110fe`; libultraship changes then lived in `patches/libultraship.patch`, now in the fork)
- **stadion files:** libultraship `src/fast/interpreter.cpp`, `src/fast/Fast3dWindow.cpp`

## Symptom
- Intermittent intro crash around frame 488 (~1 in 4 runs): a stale segment pointed into a reused buffer.
- Missing texture loads after a skipped frame (wrong textures on the next frame).
- Menus that draw their static background only in the first frames of a screen (order screen: Poké Ball background, computer panel, labels missing).
- DXGI paced by wall clock and dropped frames: what the game draws only once was lost.
- A freed display-list buffer that already held asset data was executed; G_VTX with a garbage address crashed.

## Root cause
The N64 keeps the RSP segment table and the RDP tile/TMEM state across tasks, and the frame buffer keeps its picture between frames. The port skipped whole display lists of frames it did not draw (render divisor at speed, libultraship's frame limiter), and cleared/recreated framebuffers.

## Fix in stadion
- `Interpreter::TrackSegmentsOnly`: walks skipped frames and applies segment writes and texture-state commands (SETTIMG/SETTILE/SETTILESIZE/LOADBLOCK/LOADTILE/LOADTLUT); frames that render to a texture, or draw over the previous picture (no full-screen fill) with a changed amount of drawing, are drawn anyway (`PORT_SKIP_INCREMENTAL=1`, `PORT_TRACE_FILL=1`).
- One persistent GPU framebuffer per N64 frame-buffer address; only depth is cleared at frame start.
- The port never drops a frame (`PORT_ALLOW_FRAME_DROP=1` restores the old behaviour).
- Invalid F3DEX2 opcodes end the current list; bad G_VTX addresses are skipped with a `[badvtx]` dump (user space is 48-bit).

## Zero / plethron
Already ported via the fork pinned at 4148f93: `src/fast/interpreter.cpp:6728` (`TrackSegmentsOnly`), `:6821` (`PORT_SKIP_INCREMENTAL`), `:5065-5068` (`[badvtx]`, 48-bit check), `src/fast/Fast3dWindow.cpp:256` (`PORT_ALLOW_FRAME_DROP`) and `:333` (skipped frames tracked). plethron's `src/port/Engine.cpp` uses `TrackSegmentsOnly`.

## How to detect
Pictures that differ between `PORT_SPEED=1` and fast runs, or that lose a static background after the first frames; `PORT_TRACE_FILL=1`; an intermittent crash in the renderer on a garbage display list.
