# DXGI/DX11: startup crash from uninitialised window-backend and renderer members

- **Category:** platform-build
- **Zero status:** already ported (libultraship fork at pin 4148f93)
- **stadion:** [PR #42](https://github.com/team-hippodrome/stadion/pull/42), commit(s) `6f0467e`
- **stadion files:** `patches/libultraship.patch` (`include/fast/backends/gfx_dxgi.h`, `gfx_window_manager_api.h`, `gfx_direct3d_common.h`, `src/fast/backends/gfx_dxgi.cpp`)

## Symptom
Releases v0.0.4/v0.0.5 crashed at startup on some Windows machines (DX11): access violation inside the window procedure during `ShowWindow`, RAX `0x2020202020202020`, R15 `0x1C` (`WM_ACTIVATEAPP`).

## Root cause
`GfxWindowBackendDXGI` is created with `new` and its constructor only set the shared_ptrs; plain members were left uninitialised. `ShowWindow` sends `WM_ACTIVATEAPP` before `SetKeyboardCallbacks` has set `mOnAllKeysUp`; the handler tested `mOnAllKeysUp != nullptr` and called heap garbage (eight spaces). `WM_SETFOCUS` read an uninitialised `mIsMouseCaptured`; the base class's `mFullScreen` could be garbage `true` and skip "start fullscreen". `GfxRenderingAPIDX11` had the same issue (`mCoordBufferSize` garbage -> depth readback buffer never created). Most machines get zeroed pages, so few players saw it.

## Fix in stadion
Default member initialisers for every plain member (pointers `nullptr`, bools `false`, numbers `0`, arrays `{}`) in the DXGI backend, the window-manager base class and the DX11 renderer. `ThrowIfFailed` logs the HRESULT before throwing.

## Zero / plethron
Present at the fork pin: `libultraship/include/fast/backends/gfx_dxgi.h:97` (`void (*mOnAllKeysUp)(void) = nullptr;`), `:100` (`bool mIsMouseCaptured = false;`), `include/fast/backends/gfx_window_manager_api.h:51` (`bool mFullScreen = false;`), `include/fast/backends/gfx_direct3d_common.h:210` (`size_t mCoordBufferSize = 0;`), `src/fast/backends/gfx_dxgi.cpp:1125-1128` (`ThrowIfFailed` logs `SPDLOG_CRITICAL`). Note: `include/fast/backends/gfx_metal.h:296` still declares `size_t mCoordBufferSize;` without an initialiser (Metal, not part of this fix).

## How to detect
grep headers of heap-allocated classes for members without `=`/`{}`; run under a debug heap that fills with a pattern (MSVC debug CRT `0xCD`, or `MALLOC_PERTURB_` on Linux) to make garbage visible.
