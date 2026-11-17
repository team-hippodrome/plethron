# First-entry save stall: every FlashRAM page write rewrote the save file (with backups)

- **Category:** performance
- **Zero status:** check (Zero saves to SRAM; plethron writes the whole .sra with a backup snapshot on every changed DMA write)
- **stadion:** [PR #106](https://github.com/team-hippodrome/stadion/pull/106), commit(s) `153a418` (merge `7c408e8`)
- **stadion files:** `src/port/os_devices.cpp` (`FlashSave`, `Port_Begin/EndFlashBankWrite`), `patches/game/02-core.patch` (save_data.c `Save_FlushBank`, `Save_EnsureBankLoaded`)

## Symptom
A pause the first time a mode touched the save (Stadium hub entry).

## Root cause
The FlashRAM emulation persisted the whole 128 KiB file (atomic write plus backup rotation since #104) after every
page program; a bank flush programs many pages (primary and backup copy), so one game save meant dozens of file
writes on the game thread.

## Fix in stadion
`Save_FlushBank` and `Save_EnsureBankLoaded` bracket their writes with `Port_BeginFlashBankWrite()` /
`Port_EndFlashBankWrite()`; inside the bracket `FlashSave()` only marks dirty, and the end writes once.

## Zero / plethron
The flash bracket is inherited (`src/port/os_devices.cpp:150-153`, `:228-235`) but Zero does not use FlashRAM.
Zero's SRAM path: `os_devices.cpp:323-340` writes each DMA block and calls `SramSave()` when the block changed;
`SramSave()` (:208) goes through `Port::WriteProtectedSave` (backup copy into `<save>.backups`,
`launcher_reliability.cpp:152-169`). `Storage_WriteSram` (`game/src/rsp.c:91`) issues one DMA per request, and
`save_data.c:66` notes both footers are written, so one game save is at least two file writes. Measure before acting.

## How to detect
`PORT_TRACE_FLASH=1` logs loads; add a counter/timer around `SramSave()` and save once in game. More than one
file write per in-game save, or a write taking several ms on the game thread, is this class.
