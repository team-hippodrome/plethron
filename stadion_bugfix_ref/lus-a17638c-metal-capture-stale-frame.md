# Metal screenshots showed the previous capture point's frame (or black), not the frame just drawn

- **Category:** rendering
- **Zero status:** needed (libultraship bump; plethron's replay/screen-diff tools rely on `PORT_SHOT_AT` captures)
- **stadion:** [PR #156](https://github.com/team-hippodrome/stadion/pull/156), commit(s) `d1b0c65` (libultraship `a17638c`, fork PR #7)
- **stadion files:** `libultraship` (`src/fast/backends/gfx_metal.cpp`)

## Symptom
`PORT_SHOT_AT` / `PORT_SCREENSHOT_STOP_ONLY` on Metal produced scenes hundreds of frames off, or black; Metal vs
OpenGL captures differed in 68-93 % of pixels. Replay checks compared the wrong frames.

## Root cause
`ReadFramebufferToCPU(0)` returned the pixels of a blit requested by the previous call (run in the EndFrame after it);
data not read in the next frame was dropped as stale. Fine for a caller reading every frame, wrong after a gap.

## Fix in stadion
Deferred data is used only if its blit ran in the latest EndFrame; otherwise the screen texture is copied now on the
frame's command queue and waited for.

## Zero / plethron
Pin `4148f93`: `src/fast/backends/gfx_metal.cpp:892-911` (deferred blit, "ReadFramebufferToCPU next frame") and
`:1687`. plethron captures through it at `src/port/Engine.cpp:522` and :609, used by `PORT_SHOT_AT` (Engine.cpp:548)
and by `tools/run_replays.py`, `tools/screen_diff/run.py`, `tools/frame_sweep.py`. Metal-only; bump to `8556b99`.

## How to detect
Capture the same frame on Metal and OpenGL and diff; captures after a gap that match an earlier capture.
