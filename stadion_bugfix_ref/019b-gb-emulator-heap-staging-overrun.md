# GB Tower exit crash: emulator staged compressed data ~80 KB past its heap block (pool header clobbered)

- **Category:** abi-64bit
- **Zero status:** not applicable (Zero's GB Player copies its emulator data uncompressed into the start of its block; no staging copy past the end)
- **stadion:** [PR #19](https://github.com/team-hippodrome/stadion/pull/19), commit(s) `09b97fc` (game part: michiiik/pokestadium#304)
- **stadion files:** `game/src/fragments/gb_tower_emulator/gb_tower_emulator_7F9A0.c` (`GbEmu_InitSession`, today)

## Symptom
Leaving the GB Tower emulator crashed in the next `main_pool_pop_state` (with the self-destroy bug of 019a fixed).

## Root cause
`GbEmu_InitSession` decompresses fragment1's data from a staging copy placed after the carved area
(`sGbEmuHeapCursor + 0x80000`), about 0x14700 bytes past the end of its `main_pool_alloc` block. On the N64 that only
overwrote free memory and the free-list header, whose garbage read as a low address and was skipped. On a 64-bit
host the same garbage read as a high address and the pool pop jumped through it.

## Fix in stadion
`sGbEmuHeapStart = main_pool_alloc(GB_EMU_HEAP_SIZE + 0x20000, 0);` so the block covers the staging copy.

## Zero / plethron
Zero's GB Player entry `GbEmu_Init` (`game/src/fragments/audio_and_gb_emulator/gb_emu_screen.c:320-334`) allocates
`(D_0015A9B0 - D_000DA9B0) + 0x158000`, fills its start with `func_80003824` (= `Dma_WriteChunks`, a plain copy,
per the prototype at `:305`) and uses the rest as the 0x158000 cartridge area; the CPU core is the host
`src/port/gb_core.c`. No decompress-from-staging step was found. If a pool crash ever appears on GB Player exit,
check `GbEmu_LoadCartridge` writes stay inside 0x158000.

## How to detect
Pool corruption shows up at the next pop/push (`main_pool_pop_state`). Run under ASan (`tools/asan_run.sh`) or add a
canary after large pool blocks; look for code writing at `heap + fixed offset` beyond the allocated size.
