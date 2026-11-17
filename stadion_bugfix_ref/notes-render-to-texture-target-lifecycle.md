# Render-to-texture targets: empty viewports, wrong row/column offsets, passes out of order, markers in RDRAM, images taken for frame buffers

- **Category:** rendering
- **Zero status:** already ported (libultraship fork at 4148f93 and the identical `Port_ViFrameBufferState` in `src/port/os_devices.cpp`)
- **stadion:** no PR (Status "boots into the main menu", "Comparing with ares", third ares round VS screen / Gallery / GB Tower; before PR #1, history squashed into `be110fe`). Related numbered fixes (#79 black frame, #139 OpenGL texture cache, #156) are covered separately.
- **stadion files:** libultraship `src/fast/interpreter.cpp`, `gfx_metal.cpp`; `src/port/os_devices.cpp`

## Symptom
Menu cards duplicated/clipped; Game Pak check cards 2-4 empty; main-menu card strips and the warning bar shifted; VS screen grid cells empty, the first picture drawn into the depth buffer; Gallery enlarge showed noise then a 2x quarter; GB Tower loading-screen party pictures with coloured dots top left.

## Root cause
- A new GPU target starts with an empty viewport/scissor, but Fast3D's cached viewport skipped setting it when the size matched the previous target.
- `LoadTextureTile` with `uls > 0` from inside a target needs a column offset (and strips a row offset).
- Metal: the first readback of a new framebuffer cannot wait, so later passes moved to the readback queue ran before the first pass, whose fresh-texture clear then wiped the image; switching back to an existing target did not cancel a pending target creation (`sPendingOffscreenAddr`).
- Every 320/640-wide colour image was taken for a frame buffer, even images the game never shows.
- The 16-byte render-target marker written into RDRAM showed up when the CPU copied the image.

## Fix in stadion
Viewport/scissor reset on every target switch; column/row offsets; RTT framebuffers start on the readback queue (`SetFramebufferReadback`); pending creation cancelled; targets read back as big-endian RGBA5551 (`RttReadback`) with the marker kept as `RttInfo::stamp` instead of in memory; an image never passed to `osViSwapBuffer` is a render target (`Port_ViFrameBufferState`, `PORT_NO_VIFB_CHECK=1`) and a 640-wide one is drawn in 640x480 space.

## Zero / plethron
Already ported: fork 4148f93 `src/fast/interpreter.cpp:3358` (`SetFramebufferReadback`), `:4332` (frame buffer vs. RTT via `Port_ViFrameBufferState`), `gfx_metal.cpp:1784`; plethron `src/port/os_devices.cpp:413` (`Port_ViFrameBufferState`). The game-side companion (forgetting targets when memory is reused) is NOT in Zero: see `notes-forget-render-targets-on-pool-reuse.md`.

## How to detect
`PORT_TEXLOG=1` (`[rtt] bind`, `[rtt] fill clear`), `PORT_TRACE_ICON=1`, `PORT_NO_RTT_READBACK=1` / `PORT_NO_RTT_SAMPLE=1` to bisect; `tools/rtt_check.py` in plethron's tools.
