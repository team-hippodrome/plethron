# Widescreen: flickering strip at the 2D menus' right edge

- **Category:** rendering
- **Zero status:** already ported (libultraship fork at pin 4148f93)
- **stadion:** [PR #37](https://github.com/team-hippodrome/stadion/pull/37), commit(s) `fa1463b`
- **stadion files:** `patches/libultraship.patch` (`src/fast/interpreter.cpp`, `GfxDpSetColorImage` margin clear)

## Symptom
In 16:9, a thin strip at the right edge of the 4:3 picture flickered on the main menu and other full-screen-backdrop menus (columns alternated black / old content every frame).

## Root cause
The menus' scissor stops ~2 native pixels short of the right edge, so the game never draws the last columns. On the N64 they keep the framebuffer's one-time clear; on the port each rotating framebuffer kept whatever an earlier scene left there, and the per-frame margin clear only covered the area outside the 4:3 picture. (#40 later found the real reason for the 2-pixel gap: the 2D lock's 4:3 area was truncated, see `040-...`.)

## Fix in stadion
The margin clear also covers the picture's outermost 4 low-resolution pixels on each side, before the game draws, so anything drawn there still shows. Nothing changes at 4:3.

## Zero / plethron
Present: `libultraship/src/fast/interpreter.cpp:4480-4485` at pin 4148f93 ("also the picture's outermost 4 (low-resolution) pixels ..."), later refined to only frames that repaint the whole screen (`sBandClearThisRun`).

## How to detect
Screenshot consecutive frames (`PORT_SCREENSHOT_DIR`) of a static menu at 16:9 and diff the columns next to the pillarbox: values that alternate between frames mean a buffer that is not redrawn.
