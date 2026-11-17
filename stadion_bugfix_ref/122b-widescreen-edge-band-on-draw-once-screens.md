# Widescreen: screens drawn once (rental list) lost their outer 4 px columns to the margin clear

- **Category:** rendering
- **Zero status:** already ported (libultraship fork at 4148f93 clears the edge band only on full repaints)
- **stadion:** [PR #122](https://github.com/team-hippodrome/stadion/pull/122), commit(s) `70ba60b` (merge `d045f77`)
- **stadion files:** `patches/libultraship.patch` (src/fast/interpreter.cpp)

## Symptom
In 16:9 the rental list looked 8 pixels narrower than the screen before it, with a black band at each 4:3 edge.

## Root cause
The widescreen margin clear also blacked out the picture's outermost 4 pixels each frame (to hide stale columns).
Screens that paint their background once and then only what changes never redrew those columns.

## Fix in stadion
The band is cleared only on frames that repaint the whole screen (`sBandClearThisRun`).

## Zero / plethron
libultraship 4148f93 `src/fast/interpreter.cpp:326` (`sBandClearThisRun`: "the frame repaints the whole screen")
and :4483 / :7200. No game change needed.

## How to detect
In 16:9, compare a draw-once menu's width with the previous screen; screenshot the 4:3 edges over several frames.
