# Android APK build failed: NDK's -Werror=format-security not undone by the game's -Wno-error

- **Category:** platform-build
- **Zero status:** already ported (plethron 8298280: -Wno-error=format-security in STADION_GAME_WARNINGS)
- **stadion:** [PR #153](https://github.com/team-hippodrome/stadion/pull/153), commit(s) `851df50`
- **stadion files:** `CMakeLists.txt`

## Symptom
v0.0.15 release run failed on Android only (20 errors); macOS, Windows, Linux built.

## Root cause
The NDK adds `-Werror=format-security`; a blanket `-Wno-error` does not undo an explicit `-Werror=<group>`. The errors
were `sprintf(buf, Text_GetString(...))` (non-literal format, no arguments) in two demo-scene files.

## Fix in stadion
`-Wno-error=format-security` added to `STADION_GAME_WARNINGS`.

## Zero / plethron
`CMakeLists.txt:266-279` `STADION_GAME_WARNINGS` has `-Wno-error` but no `-Wno-error=format-security`. A grep of
`game/src` for single-line `sprintf(x, y)` with a non-literal format and no further arguments found none (Zero's
`sprintf` calls with variable formats, e.g. `game/src/gb_data.c:224`, pass arguments, which this warning ignores).
Multi-line calls were not checked exhaustively. Adding the flag is a one-line safeguard before the first Android
release.

## How to detect
Build with `-Wformat-security` (clang) and look for warnings in `game/`; an Android CI build.
