# Launcher text blurry on HiDPI (Retina) screens: fonts rasterized at point size

- **Category:** rendering
- **Zero status:** already ported (`src/port/launcher.cpp` rasterizes fonts at the window's pixel density)
- **stadion:** [PR #18](https://github.com/team-hippodrome/stadion/pull/18), commit(s) `6e2d9f4`
- **stadion files:** `src/port/launcher.cpp`

## Symptom
On a Retina Mac all launcher text, including button labels, looked blurry.

## Root cause
The window and drawable were already full resolution (ImGui framebuffer scale 2), but ImGui 1.91 rasterizes each font
once and the launcher's fonts were built at their point size (13-32 px), so every glyph was stretched 2x.

## Fix in stadion
Rasterize the fonts at `SDL_GetWindowPixelDensity` times their size and draw them at 1/density
(`io.FontGlobalScale` while the launcher shows, reset for the game). No change on 1x screens.

## Zero / plethron
`src/port/launcher.cpp:892-901`: `mPixelDensity = std::max(1.0f, SDL_GetWindowPixelDensity(sdl))` and the font set
nearest `mScale * mPixelDensity` is chosen (a later evolution of the same fix); positions are snapped to device pixels
at `:1083-1084`.

## How to detect
Screenshot the launcher on a 2x display and zoom; or log `ImGui::GetIO().DisplayFramebufferScale` against the font's
rasterized size.
