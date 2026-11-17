# Launcher crashed when hovering Smooth motion (ImGui multi-viewport tooltip window on Metal)

- **Category:** platform-build
- **Zero status:** already ported (`src/port/Engine.cpp:249-251`, `src/port/launcher.cpp:887`)
- **stadion:** [PR #24](https://github.com/team-hippodrome/stadion/pull/24), commit(s) `f19e029`
- **stadion files:** `src/port/Engine.cpp`, `src/port/launcher.cpp`

## Symptom
Hovering the Smooth motion option crashed the app (signal 11 in `ImGui_ImplMetal_CreateWindow` inside
`ImGui::UpdatePlatformWindows`).

## Root cause
libultraship enables ImGui multi-viewports by default. The tooltip was wider than the space left in the launcher
window, so ImGui made it a separate OS window, and the Metal backend crashed creating it.

## Fix in stadion
`gEnableMultiViewports = 0` at startup. Follow-up: with viewports off libultraship's "Main Game" window covered the
launcher (black window), so the launcher calls `ImGui::BringWindowToDisplayFront` every frame.

## Zero / plethron
`src/port/Engine.cpp:251` sets `gEnableMultiViewports` 0; `src/port/launcher.cpp:887` and `:1413` bring the launcher
window to the front.

## How to detect
Force wide tooltips/popups near the window edge on macOS; grep `gEnableMultiViewports` at init.
