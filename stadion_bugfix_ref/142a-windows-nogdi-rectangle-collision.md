# Windows build: wingdi.h's Rectangle() collided with the game's Rectangle type

- **Category:** platform-build
- **Zero status:** already ported (plethron `CMakeLists.txt:283-286` defines NOGDI for `stadium_game`)
- **stadion:** [PR #142](https://github.com/team-hippodrome/stadion/pull/142), commit(s) `e2e5573`
- **stadion files:** `CMakeLists.txt`

## Symptom
mingw-w64 build of the vendored decomp failed: conflicting declarations of `Rectangle`.

## Root cause
`<windows.h>` pulls in `wingdi.h`, which declares a `Rectangle()` function; the decomp has `typedef struct Rectangle`.

## Fix in stadion
`target_compile_definitions(stadium_game PRIVATE NOGDI)` on WIN32 (game code only; the port layer keeps GDI).

## Zero / plethron
Present at `CMakeLists.txt:283-286`. Zero has the same type (`game/include/variables.h:893` `typedef struct Rectangle`).
Nothing to do.

## How to detect
Windows/mingw compile errors naming a Win32 API (`Rectangle`, `Polygon`, `GetObject`, `min`/`max`) in game code: add
`NOGDI` / `NOMINMAX` / `WIN32_LEAN_AND_MEAN` to the game target, not the port target.
