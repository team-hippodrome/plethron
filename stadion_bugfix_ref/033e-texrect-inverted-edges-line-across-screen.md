# Line across the battle screen: texture rectangles with inverted edges were drawn

- **Category:** rendering
- **Zero status:** already ported (fork at 4148f93, `interpreter.cpp:3962`)
- **stadion:** [PR #33](https://github.com/team-hippodrome/stadion/pull/33), commit(s) `2ebf969`
- **stadion files:** `patches/libultraship.patch` (`src/fast/interpreter.cpp`, `GfxDpTextureRectangle`)

## Symptom
When the battle menu slid off the left edge after choosing a move, a white/green line crossed the whole picture for a
few frames.

## Root cause
The HP bar's negative x wrapped around in the RDP's 10.2 coordinates, giving a rectangle whose left edge lies right of
its right edge (e.g. 1010..34). The RDP draws nothing for it; Fast3D drew the span between.

## Fix in stadion
```cpp
if (lrx <= ulx || lry <= uly) {
    return;
}
```
at the top of `Interpreter::GfxDpTextureRectangle`.

## Zero / plethron
`git -C /home/user/lus show 4148f93:src/fast/interpreter.cpp` line 3962 has the check. Zero's battle menus
slide the same way and use the same renderer.

## How to detect
Full-width horizontal streaks during slide-outs; `PORT_RECTLOG` (rect logging in the fork) shows rectangles with
`lrx < ulx`.
