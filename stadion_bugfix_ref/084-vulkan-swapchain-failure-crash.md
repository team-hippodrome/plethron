# Vulkan: a failed swapchain creation crashed in the driver instead of falling back

- **Category:** platform-build
- **Zero status:** already ported (pinned libultraship checks swapchain calls; plethron main.cpp catches `Fast::VulkanFatalError`)
- **stadion:** [PR #84](https://github.com/team-hippodrome/stadion/pull/84), commit(s) `ca8c241` (squashed into `2db362c` locally)
- **stadion files:** `patches/libultraship.patch` (gfx_vulkan.cpp), `src/port/main.cpp`, `src/port/Engine.cpp`, `src/port/launcher.cpp`

## Symptom
Windows 7 / Intel: after Play, `vkCreateSwapchainKHR` returned -1, then an access violation in `igvk64.dll`.

## Root cause
The Vulkan backend's `VK_CHECK` only logged failures, so the code went on with a null swapchain handle. Surface
format UNDEFINED, unsupported image usages and composite-alpha modes were also not handled.

## Fix in stadion
Surface queries, swapchain creation, image enumeration and image-view creation are checked and throw; required
image usages are validated, optional sampled usage requested only when supported, an advertised composite-alpha
mode selected. The port catches the error, shows the failing call/result, saves OpenGL as the renderer (window
config and launcher preference) and exits without driver teardown. Test hook `PORT_VK_FAIL_SWAPCHAIN_AT=1|2|3`.

## Zero / plethron
- libultraship 4148f93: `src/fast/backends/gfx_vulkan.cpp:753` reads `PORT_VK_FAIL_SWAPCHAIN_AT`.
- `src/port/main.cpp:535` `catch (const Fast::VulkanFatalError& e)` (generalised later by stadion #124), and
  `src/port/Engine.cpp:1076` skips driver teardown after a failed swapchain.

## How to detect
`PORT_VK_FAIL_SWAPCHAIN_AT=1` (or `PORT_DEBUG_VK_OOM=<n>`) must give a dialog and an OpenGL restart, not a crash.
Grep Vulkan code for result codes that are only logged.
