# Widened full-screen clear stopped at x=512 on 640x480 screens (stale panels left at the right)

- **Category:** rendering
- **Zero status:** already ported (libultraship 4148f93 scales the margin with the native width)
- **stadion:** [PR #129](https://github.com/team-hippodrome/stadion/pull/129), commit(s) `8b4bdec`
- **stadion files:** `patches/libultraship.patch` (`src/fast/interpreter.cpp`)

## Symptom
Trade Machine box screen (640x480): pieces of an earlier panel ("/30", PP numbers) stayed visible in the right fifth
of the screen, in 4:3 and 16:9.

## Root cause
`Interpreter::GfxDpFillRectangle` widens a full-screen fill to cover widescreen margins, but used a fixed
-1024..2048 (10.2 fixed point = -256..512 px), which only suits 320x240. A 640-wide clear stopped at x 512.

## Fix in stadion
The margin is now a multiple of the native width (identical numbers at 320x240).

## Zero / plethron
Zero has no Trade Machine, but any 640x480 screen with a full-screen fill would hit it. The fix is in plethron's pinned
libultraship `4148f93`: `src/fast/interpreter.cpp:4248-4255` ("PORT: a margin of the native width on every side").
Nothing to do.

## How to detect
Leftover pixels in the right part of a hi-res (640x480) screen that the game clears with a fill rect; compare against
ares with `tools/screen_diff/`.
