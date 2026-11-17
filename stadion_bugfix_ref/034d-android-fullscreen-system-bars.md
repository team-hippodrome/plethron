# Android: status and navigation bars covered the picture (window not fullscreen)

- **Category:** platform-build
- **Zero status:** already ported (`src/port/Engine.cpp:253-258`)
- **stadion:** [PR #34](https://github.com/team-hippodrome/stadion/pull/34), commit(s) `c014159`
- **stadion files:** `src/port/Engine.cpp`

## Symptom
On Android the system status and navigation bars sat over the game picture.

## Root cause
A plain SDL window leaves the activity in normal mode; only a fullscreen SDL window puts SDLActivity into immersive
mode.

## Fix in stadion
On Android force `Window.Fullscreen.Enabled` and windowed (desktop) fullscreen, so the display keeps its mode.

## Zero / plethron
`src/port/Engine.cpp:253-258` (`#if defined(__ANDROID__)`: `config->SetBool("Window.Fullscreen.Enabled", true)`,
`gSdlWindowedFullscreen` 1).

## How to detect
Run the APK and check the top/bottom edges; `adb shell dumpsys window | grep -i immersive`.
