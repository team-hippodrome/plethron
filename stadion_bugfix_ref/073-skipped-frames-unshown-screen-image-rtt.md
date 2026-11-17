# Skipped frames ignored render-to-texture into an unshown screen-sized image (Bonus stickers on black)

- **Category:** rendering
- **Zero status:** already ported (libultraship fork at pin 4148f93; `Port_ViFrameBufferState` in src/port)
- **stadion:** [PR #73](https://github.com/team-hippodrome/stadion/pull/73), commit(s) `e1a8e29`
- **stadion files:** `patches/libultraship.patch` (`src/fast/interpreter.cpp`: `IsUnshownScreenImage`, `TrackSegmentsOnly`, `FrameRepaintsScreen`)

## Symptom
Bonus stickers screen showed 5 of 16 background thumbnails on black (ares: all 16 on white), at `PORT_SPEED > 1` or when the window was not ready.

## Root cause
The game fills a 640x480 image white, draws one thumbnail into it per frame and draws the image back as 480 one-line textures. `GfxDpSetColorImage` treated a screen-sized image the game never shows as a render target, but the state pass for frames that are not drawn (`TrackSegmentsOnly`) only counted widths other than 320/640 as render-to-texture, so it skipped those frames and their draws never reached the image.

## Fix in stadion
One test `IsUnshownScreenImage(width, address)` (`Port_ViFrameBufferState == 0`) used by the draw path, `TrackSegmentsOnly` (draw such frames) and `FrameRepaintsScreen` (an off-screen fill is not a screen repaint).

## Zero / plethron
Present: `libultraship/src/fast/interpreter.cpp:4329-4333` (`IsUnshownScreenImage`), used at `:4343`, `:6791` and `:7161` at pin 4148f93; the port side `src/port/os_devices.cpp:413` (`Port_ViFrameBufferState`).

## How to detect
A screen that renders correctly with `PORT_RENDER_EVERY=1` but not at `PORT_SPEED=max` (or under load): draws lost in skipped frames. Compare frame-skipping and every-frame screenshots.
