# Vulkan backend allocated new command buffers every frame and never freed them (OOM crash after long sessions)

- **Category:** rendering
- **Zero status:** already ported (libultraship 4148f93 has `AcquireCommandBuffer`)
- **stadion:** [PR #128](https://github.com/team-hippodrome/stadion/pull/128), commit(s) `b8324d7`
- **stadion files:** `patches/libultraship.patch` (`gfx_vulkan.h`, `gfx_vulkan.cpp`)

## Symptom
On a Windows Intel HD iGPU, after a long GB Tower session, `vkAllocateCommandBuffers` returned
`VK_ERROR_OUT_OF_HOST_MEMORY` and the game crashed, with growing lag before that.

## Root cause
Every frame allocated fresh command buffers from its frame slot's pool (one per framebuffer drawn, one per
synchronous readback, one for uploads: ~2 per game frame). `vkResetCommandPool` resets buffers but does not free
them, so each pool grew for the whole run (16,840 buffers over 8,200 frames).

## Fix in stadion
Each `FrameSlot` keeps its command buffers and hands them out again after the pool reset
(`GfxRenderingAPIVK::AcquireCommandBuffer`). `PORT_VK_STATS=1` prints allocation counts.

## Zero / plethron
plethron pins libultraship `4148f93` (`git ls-tree HEAD libultraship`). That commit contains the fix:
`include/fast/backends/gfx_vulkan.h:275` declares `AcquireCommandBuffer()`, `src/fast/backends/gfx_vulkan.cpp:419`
defines it and `:615` uses it for uploads. Nothing to do.

## How to detect
`PORT_VK_STATS=1` on a long run (allocation count must stay flat). Generally: any `vkAllocate*` in a per-frame path
without a matching free/reuse.
