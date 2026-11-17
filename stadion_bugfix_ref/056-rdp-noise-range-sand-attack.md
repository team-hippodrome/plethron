# RDP NOISE combiner input only covered [0.5, 1) (Sand-Attack's cloud smooth and bright)

- **Category:** rendering
- **Zero status:** already ported (libultraship fork at pin 4148f93)
- **stadion:** [PR #56](https://github.com/team-hippodrome/stadion/pull/56), commit(s) `b78a0f8`
- **stadion files:** `patches/libultraship.patch` (`gfx_metal_shader.cpp`, `gfx_opengl.cpp`, `gfx_direct3d11.cpp`; Vulkan reuses the GL formulas)

## Symptom
Sand-Attack's sand cloud was smooth and bright instead of grainy and darker; Poison Powder and Sleep Powder slightly off.

## Root cause
The cloud colour is a per-pixel random blend between env and prim colour (`NOISE` combiner input). Every backend mapped the shaders' `random()` with `(r + 1) / 2`, as if it returned [-1, 1], but `random()` is `fract(...)`, already [0, 1). The noise only covered [0.5, 1), pulled toward the brighter primitive colour.

## Fix in stadion
Use `random()` as it is in each backend's `RAND_NOISE`.

## Zero / plethron
Present at pin 4148f93: `libultraship/src/fast/backends/gfx_opengl.cpp:114`, `gfx_metal_shader.cpp:33`, `gfx_direct3d11.cpp:1477` all `#define RAND_NOISE "random(...)"` with no `(+1)/2`. Zero uses the same input: `game/src/fragments/battle/particle_gfx.c:310` (`gDPSetCombineLERP(..., NOISE, 0, ENV_ALPHA, ...)`).

## How to detect
grep shader generators for remapping of a function's output whose range you have not checked; compare a noise effect's mean brightness with an emulator (ares/paraLLEl-RDP).
