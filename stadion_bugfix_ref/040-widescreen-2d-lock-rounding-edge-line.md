# Widescreen: bright line at the picture's right edge (2D lock area truncated), fade flicker around widened menus

- **Category:** rendering
- **Zero status:** already ported (libultraship fork at pin 4148f93; port side in src/port/os_devices.cpp)
- **stadion:** [PR #40](https://github.com/team-hippodrome/stadion/pull/40), commit(s) `ffe31a3`
- **stadion files:** `patches/libultraship.patch` (`src/fast/interpreter.cpp`), `src/port/os_devices.cpp`

## Symptom
1. At the darkest point of the hub-map -> Free Battle fade, a bright vertical line at the right edge of the 4:3 picture (seen on the AYN Thor, 640x360).
2. With #38's widened menu backdrops: fades covered only the 4:3 middle, and the CPU-drawn white frames between screens were shown 4:3 between widescreen frames (width jumps / flicker).

## Root cause
1. The 2D lock's 4:3 area was computed from floats and truncated: 79.99998 became 79, so the area was columns 79-559 instead of 80-560 and a 2D screen never redrew the last column, which kept an older frame.
2. Full-screen single-colour fades were held by the 4:3 lock; CPU frames had no knowledge of the previous frame's width.

## Fix in stadion
`lroundf` for the 4:3 area; full-screen one-colour fades pass the 2D lock (`sWideFade`); a single-colour CPU frame after a frame that filled the whole picture is shown full width (`Port_LastFrameFilledWide`). `PORT_NO_WIDE_PATTERN=1` turns the widening off.

## Zero / plethron
Present: `libultraship/src/fast/interpreter.cpp:2601-2607` (rounded `area43`, comment about 79.99998), `:374` `sWideFade`, `:381` `Port_LastFrameFilledWide`; port side `src/port/os_devices.cpp:1408,1441` uses it for CPU frames.

## How to detect
Look for float-to-int truncation of viewport/scissor rectangles (`(int16_t)` casts of float expressions) in the interpreter; at 16:9 inspect the picture's edge columns in a fade frame by frame.
