# Widescreen: a battle stage whose sky is drawn as full-screen rectangles was treated as a 2D screen (boxed in 4:3)

- **Category:** rendering
- **Zero status:** already ported (plethron 6ecd2d8 emits `gDPPort3DScene` in Zero's `Battle_DrawStageBackdrop`)
- **stadion:** [PR #125](https://github.com/team-hippodrome/stadion/pull/125), commit(s) `7f69456` (merge `1a881f8`)
- **stadion files:** `patches/game/20-fragment-battle_engine.patch` (`Battle_DrawStageBackdrop`), `patches/libultraship.patch` (interpreter 2D-screen check), `src/port/sdk/port_compat.h`, `tools/widescreen_review.py`

## Symptom
In 16:9 the Elite Four room and the rival's room (stage files 15, 16) were drawn only inside the 4:3 area.

## Root cause
Their sunset sky is the only stage backdrop drawn as a texture (two full-screen rectangles) before any 3D. The
interpreter's 2D-screen check (`s2DRectArea`) saw a frame that starts with screen-sized rectangles, took it for a
menu and locked it to 4:3.

## Fix in stadion
The backdrop first emits `gDPPort3DScene` (G_NOOP type 0x53), which sets `sSeen3DThisRun` so the frame counts as
3D. `PORT_NO_3D_SCENE_MARK=1` ignores the marker.

## Zero / plethron
In Zero more stages have a sky (tournament rounds, finals, attract battle). plethron's head commit `6ecd2d8` adds
`gDPPort3DScene(gDisplayListHead++);` at `game/src/fragments/battle/stage.c:186`; the pinned libultraship handles the
marker (`src/fast/interpreter.cpp:330-331` at 4148f93).

## How to detect
Run every stage in 16:9 and check that 3D fills the width. Any 3D scene whose display list begins with full-screen
texture rectangles (backdrops, skies, photos behind models) needs the marker. `PORT_NO_2D_LOCK=1` disables the lock
for comparison.
