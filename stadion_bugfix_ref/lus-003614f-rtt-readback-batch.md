# Each render-to-texture readback waited for the GPU on its own (up to 8 ms hitches at scene changes)

- **Category:** performance
- **Zero status:** already ported (plethron 4db7329: libultraship pin 95a0f1f, the fork's main)
- **stadion:** [PR #155](https://github.com/team-hippodrome/stadion/pull/155), commit(s) `649c4fa` (libultraship `003614f`, fork PR #6)
- **stadion files:** `libultraship` (`src/fast/interpreter.cpp`, backends `gfx_metal.cpp`, rendering API header)

## Symptom
Hitches at scene changes that draw several off-screen pictures (cup portraits, VS screen): 7.9 ms in one frame.

## Root cause
When the game leaves an off-screen target, `RttReadback` copied it back to RDRAM immediately, a GPU readback with
its own wait (~0.5 ms however small). Six to eight per frame stacked up.

## Fix in stadion
Backend API `BeginReadFramebuffer` / `FinishReadFramebuffer`. Readbacks begin when the game leaves a target and
finish together at frame end, or earlier when the frame reads one of those buffers (texture image, TMEM load,
ImportTexture source, target lookup), draws into it again, or creates a target over it. Stamp check unchanged.
`PORT_RTT_READBACK_SYNC=1` restores per-readback finishing. 7.9 ms → 0.6 ms.

## Zero / plethron
Pin `4148f93` reads back synchronously: `Interpreter::RttReadback` at `src/fast/interpreter.cpp:4298`, called at
:4358 and :4371. Zero has the VS screen with trainer portraits (`docs/layout.md` in pokestadium0: fragments
`vs_scene`, `vs_trainer_panel`; plethron `9968e3f`). Bump to `8556b99`.

## How to detect
`PORT_TRACE_FRAMETIME` frame times at scene changes; count readbacks per frame.
