# Allocations, clears and strides written as N64 byte counts overflow host structs (Gfx is 16 bytes, pointers 8)

- **Category:** abi-64bit
- **Zero status:** already ported (plethron replaced the literal sizes it found with sizeof / GFX counts; keep auditing new code)
- **stadion:** no PR (Stage 2-3 boot, "Status: boots into the main menu", second/third ares rounds; before PR #1, history squashed into `be110fe`)
- **stadion files:** `game/src/gfx_buffer.h` (`GFX_N64_BYTES`), `game/src/main_pool.c`, `game/src/memory_main.c`, particle_data_library, Lab PC, GB Tower, `src/port/os_thread.cpp`

## Symptom
- Moltres' wings drawn as two lines across the screen / frames all yellow: a per-frame list allocated as 0x50 bytes (10 N64 commands) but written as 9 host commands (0x90) overran into the wing joint's matrix.
- Every scene-graph node overlapped the arena state (`MainPoolState` header 0x10 on the N64, 0x20 on the host).
- Font glyph lookup broken (`Font_MapGlyphCode` relied on the 8-byte `Gfx`).
- Lab PC crashes: pointer lists allocated with 4 bytes per entry, record matrices laid out with N64 sizes (0x10/0x1C), TableView over box banks with stride 0x10 (host 0x18); GB Tower thread structs allocated with N64 sizes; Lab screen pool (`mem_pool_try_init(0x18000)`) too small.
- `Util_Malloc(0x2C)`-style literal sizes smaller than the 64-bit structs.

## Root cause
The decomp keeps the compiler's constant byte counts (`Util_Malloc(0x2C)`, `Gfx_AllocDisplayList(0x50)`, `bzero(p, 0x20)`, `i * 0x48`), which are only right for ILP32 with 8-byte `Gfx`.

## Fix in stadion
`#define GFX_N64_BYTES(n) ((n) / 8 * (s32)sizeof(Gfx))` used at 24 display-list allocations; `sizeof(T)` for struct allocations and clears; array indexing instead of byte strides; the audio heap and some pools doubled. Found with `PORT_DRAWLOG=1` `[mtxdl]/[mtxwhere]`, `PORT_DLALLOC_AT=<hex>` and ASan.

## Zero / plethron
Already ported across Zero's code, e.g. `game/src/font.c:536,558` (`sizeof(Gfx) * 20` "was 0xA0"), `game/src/game_state.c:933`, `fragments/model_callbacks/model_callbacks_2.c:42,75`, `fragments/particle_data/particle_data_2.c:43,79,117`, `fragments/vs_trainer_panel/vs_trainer_panel.c:92,164`; struct sizes in `game/src/dma.c:27-87` (`sizeof(StorageRequest)`, "was 0x2C"), `fragments/free_battle_setup/free_battle_setup_5.c:603`, `game/src/geo_render.c:615`; strides in `game/src/pokemon_stats_2.c:197,1166`, `game/src/poke_icon.c:100`; arena header `game/src/main_pool.c:52` (0x20). Remaining literal `Gfx_AllocDisplayList(0x40/0x140/0xA0)` calls (`fragments/battle/hypnosis_screech_trail.c:279`, `petal_dance_trail.c:276`, `model_renderer_callbacks.c:233,251`, `model_callbacks_4.c:268`) allocate `Mtx`/`Vtx` space, whose size is the same on both, so they are fine.

## How to detect
- grep `Gfx_AllocDisplayList(0x`, `Util_Malloc(0x`, `bzero(.*, 0x`, `\* 0x[0-9A-F]+\]` / `+ i \* 0x` and compare with the host `sizeof`.
- `PORT_DRAWLOG=1` (G_MTX whose data are display-list commands), ASan build.
