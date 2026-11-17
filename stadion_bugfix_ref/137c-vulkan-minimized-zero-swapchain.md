# Vulkan crashed when the window was minimized (zero-sized swapchain)

- **Category:** rendering
- **Zero status:** already ported (libultraship 4148f93 contains fork commit 1a0d7d5)
- **stadion:** [PR #137](https://github.com/team-hippodrome/stadion/pull/137), commit(s) `e1481ad` (libultraship `1a0d7d5`, fork PR #1)
- **stadion files:** `libultraship` (`src/fast/backends/gfx_vulkan.cpp`, `include/fast/backends/gfx_vulkan.h`)

## Symptom
On Windows, minimizing the game with the Vulkan renderer crashed it.

## Root cause
A minimized window's surface reports `currentExtent` 0x0; the backend created a zero-sized swapchain, which Vulkan
forbids.

## Fix in stadion
No frame is started, recorded or presented while the surface has no area; the swapchain is recreated on restore.
Debug hook `PORT_DEBUG_MINIMIZE=<frame>:<restore frame>`.

## Zero / plethron
plethron's pin `4148f93` includes it: `src/fast/backends/gfx_vulkan.cpp:1351` (comment), :1396-1398
(`SDL_WINDOW_MINIMIZED` → no frame), :1416. Nothing to do.

## How to detect
Minimize/restore on Windows with `-DLUS_ENABLE_VULKAN=ON`; macOS/MoltenVK never reports a zero extent, so it does
not reproduce there.
