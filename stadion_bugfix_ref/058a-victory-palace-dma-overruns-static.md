# Victory Palace completion scene crash: a 0xBC0-byte DMA overran its buffer into the next statics

- **Category:** abi-64bit
- **Zero status:** not applicable (Zero has no Victory Palace / completion scene)
- **stadion:** [PR #58](https://github.com/team-hippodrome/stadion/pull/58), commit(s) `d0de90d`
- **stadion files:** `patches/game` (completion scene fragment: `Completion_LoadModels`, `Completion_InitCamera`), `PORTING_NOTES.md`

## Symptom
Segfault in `Completion_InitCamera` (geo root was garbage) when every Victory Palace statue was collected.

## Root cause
`Completion_LoadModels` DMAs a 0xBC0-byte animation record into `D_86B0EBE8[0xA60]`. On the N64 it continues into the next bss array `D_86B0F648[0x180]`, which the code then reads through `*D_86B0F648`; on the host the linker placed other statics there, which the DMA overwrote.

## Fix in stadion
Both arrays became one block, so the record lands where the code reads it.

## Zero / plethron
Zero has no Victory Palace (pokestadium0 `docs/layout.md` fragment list). The class matters: other 0xBC0-record DMAs in stadion (intro, gallery camera, battle engine) use 0xBE0-byte buffers. Check Zero's battle camera record DMAs for a buffer smaller than the transfer whose tail lands in the following static.

## How to detect
ASan build on the route (stadion: `-DPORT_ASAN=ON`); grep DMA calls (`DMA_`/`osPiStartDma`/`Util_*Dma*`) and compare the transfer size with the destination array size; adjacent bss arrays read through the second one right after a DMA into the first.
