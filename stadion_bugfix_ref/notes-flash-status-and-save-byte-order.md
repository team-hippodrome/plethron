# Saves: wrong flash status constants dropped every page after the first; save image written in host byte order

- **Category:** saves
- **Zero status:** not applicable (Zero saves to SRAM, not FlashRAM); the byte-order half is already ported (plethron `ec82138`)
- **stadion:** no PR (third ares round, before PR #1; history squashed into `be110fe`)
- **stadion files:** `src/port/sdk/port_compat.h`, `game/src/save_data.c`, `game/src/save_data.h`

## Symptom
Only the first page of each save bank was ever written (cup/Castle progress lost); a pointer field in `GameOptions` moved the progress array from 0x0C to 0x18; save files were not interchangeable with ares or the save editor (footer written as "OPEK").

## Root cause
`port_compat.h` defined `FLASH_STATUS_ERASE_OK/ERROR` as 0x08/0x04 instead of libultra's 0/-1, so every page program after the first "failed" and was retried and abandoned. RAM banks are host-native structs and were written raw.

## Fix in stadion
libultra's constants; `GameOptions` without the host-sized pointer; `save_data.c` swaps every multi-byte record field at the bank boundary (`Save_AllocateBank` after reading, `Save_FlushBank` programs a swapped copy).

## Zero / plethron
- Constants already right: `src/port/sdk/port_compat.h:56-57` (`FLASH_STATUS_ERASE_OK 0`, `ERROR -1`), though Zero does not use flash.
- Zero uses SRAM; plethron `ec82138` "Keep Zero's SRAM save in N64 byte order and move the launcher to it", with `src/port/zero_sram.cpp` (record kinds and order detection, incl. "the byte order of earlier port builds"); `src/port/os_devices.cpp:327-338` swaps each SRAM DMA between host-order records and the N64-order file. Save-struct layout: check that no Zero save record holds a pointer (same class as `GameOptions`).

## How to detect
`PORT_TRACE_FLASH=1` / `PORT_TRACE_SAVE=1` in stadion; a save written by the port must be byte-identical to an emulator's save for the same actions.
