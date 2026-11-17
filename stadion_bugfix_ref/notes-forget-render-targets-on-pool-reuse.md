# Memory reused after a render target still served stale GPU pixels (black HUD labels): tell the renderer when a pool block is handed out again

- **Category:** rendering
- **Zero status:** already ported (plethron 5134413: MainPool_AllocFromPool and MemPool_Alloc call Port_GfxForgetRenderTargets)
- **stadion:** no PR (third ares round, Mewtwo battle, before PR #1; history squashed into `be110fe`)
- **stadion files:** `game/src/memory_main.c`, `src/port/StadionPort.h`, libultraship `src/fast/interpreter.cpp`

## Symptom
Mewtwo battle: the player's HUD labels ("TRAINER", the Pokémon's name) were black boxes. Earlier, the same mechanism dropped letters from fonts.

## Root cause
The renderer remembers RDRAM ranges that were used as render-to-texture targets and serves texture loads from inside them from the GPU framebuffer. A target is recognised by a 16-byte marker/stamp at its start. The font set was loaded into memory that had earlier been a full-screen render target; the font started further into the block, so the marker survived and glyph loads were answered with the old GPU picture.

## Fix in stadion
`main_pool_alloc_from_pool` reports every block it hands out (queued, applied on the render thread):
```c
if (addr != NULL) { // PORT: a reused block is no longer a GPU render target (see interpreter.cpp)
    Port_GfxForgetRenderTargets(addr, size);
}
```
(`game/src/memory_main.c:72-74`; implementation in the libultraship fork, `interpreter.cpp`.)

## Zero / plethron
- Renderer side inherited: `Port_GfxForgetRenderTargets` is defined in the lus fork at plethron's pin (`libultraship src/fast/interpreter.cpp:273` @4148f93) and declared in `src/port/StadionPort.h:99`.
- **Needed:** nothing in plethron calls it (`grep -rn Port_GfxForgetRenderTargets game/src` is empty). Zero's `MainPool_AllocFromPool` (`game/src/memory_main.c:59-85`) returns `addr` without the call. Add the same two lines before `return addr;`. Zero renders to textures (VS screen model icons, menu cards, `poke_icon.c`), so the same stale-target reuse can happen.

## How to detect
`PORT_TEXLOG=1` logs `[rtt] bind` for texture loads served from a render target; a bind on an address that now holds a font/texture loaded by the CPU is this bug. Symptom: text or icons drawn as black/old picture after a scene that used render targets.
