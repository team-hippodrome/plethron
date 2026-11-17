# Windows launcher was blurry/small on high-DPI displays (no DPI awareness)

- **Category:** platform-build
- **Zero status:** already ported (plethron's manifest declares PerMonitorV2; libultraship DXGI fix is at the pin)
- **stadion:** [PR #105](https://github.com/team-hippodrome/stadion/pull/105), commit(s) `819accd` (merge `0ce51de`)
- **stadion files:** `src/port/pokestadium.manifest`, `CMakeLists.txt`, `src/port/launcher.cpp`, `patches/libultraship.patch` (gfx_dxgi.cpp WM_DPICHANGED)

## Symptom
On a 150% Windows display the windowed launcher was bitmap-scaled (blurry) and opened at an unreadable size on
DirectX.

## Root cause
The exe manifest had no `dpiAware`/`dpiAwareness`, so Windows scaled the window. The DXGI backend's WM_DPICHANGED
handler also overwrote its own position/size fields from the suggested rectangle before the resize was processed.

## Fix in stadion
Manifest: `<dpiAware>true/pm</dpiAware>` and `<dpiAwareness>PerMonitorV2, PerMonitor</dpiAwareness>` (and the .rc
depends on the manifest so it rebuilds); WM_DPICHANGED only calls `SetWindowPos`; launcher font atlas has several
raster sizes and renders at native density.

## Zero / plethron
`src/port/pokestadium.manifest:5-6` has both elements. libultraship 4148f93 `src/fast/backends/gfx_dxgi.cpp:406-409`
handles WM_DPICHANGED with `SetWindowPos` only.

## How to detect
Run at 150%/200% scaling and compare a launcher screenshot with the expected pixel size; `GetDpiForWindow` at
runtime. Check the manifest after any change to the .rc.
