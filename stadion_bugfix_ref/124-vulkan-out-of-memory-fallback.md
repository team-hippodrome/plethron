# Vulkan: driver out-of-memory crashed in the next call; now falls back to OpenGL

- **Category:** platform-build
- **Zero status:** already ported (pinned libultraship throws `Fast::VulkanFatalError`; plethron main.cpp catches it)
- **stadion:** [PR #124](https://github.com/team-hippodrome/stadion/pull/124), commit(s) `7642937` (merge `353179c`)
- **stadion files:** `patches/libultraship.patch` (gfx_vulkan), `src/port/Engine.cpp`, `src/port/main.cpp`

## Symptom
Windows, Intel HD (`igvk64.dll`): `vkAllocateCommandBuffers` returned `VK_ERROR_OUT_OF_HOST_MEMORY`, then the
next call (`vkBeginCommandBuffer` with a null handle) crashed in the driver.

## Root cause
`VK_CHECK` only logged errors, so code continued with null handles.

## Fix in stadion
`VK_CHECK` throws `Fast::VulkanFatalError` for errors that leave the device unusable (out of host/device memory,
device lost, init failed), on the drawing thread; main handles it like a failed swapchain (084): OpenGL for the next
start, dialog naming the call, exit. `PORT_DEBUG_VK_OOM=<n>` forces it.

## Zero / plethron
libultraship 4148f93: `include/fast/backends/gfx_vulkan.h:47` `class VulkanFatalError`,
`src/fast/backends/gfx_vulkan.cpp:422-423` `PORT_DEBUG_VK_OOM`. plethron `src/port/main.cpp:535` catches it.

## How to detect
`PORT_DEBUG_VK_OOM=1` must end in the dialog and an OpenGL restart. Grep backends for results that are logged and
ignored.
