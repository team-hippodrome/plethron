# Widescreen margins flickered (each rotating frame buffer kept an older scene there)

- **Category:** rendering
- **Zero status:** already ported (libultraship fork at plethron's pin 4148f93 has the margin clear)
- **stadion:** [PR #6](https://github.com/team-hippodrome/stadion/pull/6), commit(s) `1938cd3`
- **stadion files:** `patches/libultraship.patch` (`src/fast/interpreter.cpp`), `PORTING_NOTES.md`

## Symptom
In a window wider than 4:3 the areas left and right of the picture flickered during the trailer and menu transitions
(e.g. a Clefairy from the trailer next to the title screen every third frame). All backends.

## Root cause
The port keeps one GPU render target per N64 frame buffer (the game rotates two or three). 3D scenes draw into the
widescreen margins, 2D screens do not, so each target kept whatever an earlier scene left in its margins, and the
margins changed as the buffers took turns.

## Fix in stadion
In `Interpreter::GfxDpSetColorImage`, the first main frame buffer selected in a `Run` gets two black fill rectangles
outside the 4:3 picture (only if the window is wider than 4:3); 3D scenes redraw on top. `sMarginsClearedThisRun` is
reset at `Interpreter::Run`. `PORT_NO_MARGIN_CLEAR=1` restores the old behaviour.

## Zero / plethron
Present in the libultraship submodule pinned by plethron (`git -C /home/user/lus show 4148f93:src/fast/interpreter.cpp`):
`sMarginsClearedThisRun` at line 325, the clear at lines 4464-4490 (already the later `reach`/band variant, see 015
and stadion #37/#40), reset at line 7199. Zero's menus go through the same interpreter path.

## How to detect
Run at 16:9 through a 3D scene into a 2D menu and watch the side bars; `PORT_NO_MARGIN_CLEAR=1` shows the old
behaviour for comparison.
