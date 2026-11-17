# Pointers kept in u32/s32 fields, casts and return values are cut to 32 bits on a 64-bit host

- **Category:** abi-64bit
- **Zero status:** already ported (Zero's equivalents are widened; strict per-file ratchet in place)
- **stadion:** no PR (Stages 1-3, "Fixed in this round", second/third ares rounds, Linux bring-up; before PR #1, history squashed into `be110fe`)
- **stadion files:** many; tools `tools/widen_ptr_casts.py`, `tools/widen_addr_tables.py`, `tools/ptr_warnings.sh`, `cmake/strict_sources.txt`

## Symptom
- Boot: RSP thread crashed in `osEPiStartDma` (`OSIoMesg.dramAddr` built from u32 fields).
- Stadium carousel callbacks crashed (`DisplayNodeHeader.callbackArg` was s32 holding a display list).
- GB Tower crashed once the noise channel played (`GbApu_Reset` took the noise table pointer as s32).
- Linux only: badge carousel texture load crashed (`BattlePrep_Align64PoolBlock` returned a pointer as s32: sign-extended `0xffffffff...`; on macOS the cut value happened to look like an N64 address). `Display_GetWorkerStatus` same pattern.
- GLC portraits empty, mini-game instruction pictures as stripes, Lab info-card icon noise: picture addresses in s32.

## Root cause
On the N64 pointers, `int`, `long` and `u32` are all 32 bits; the decomp freely stores addresses in integer fields, tables of `u32` words, and casts `(u32)ptr`. On LP64/LLP64 hosts the upper half is dropped. Which value the cut produces depends on the heap address, so a bug may hide on one OS and crash on another.

## Fix in stadion
Fields/params/returns that hold addresses became `uintptr_t`/`intptr_t`/typed pointers (tagged `PORT64` or `PORT:`), ~370 casts widened by script, ~120 address tables widened to `uintptr_t`. Later a per-file `-Werror` ratchet (`STADION_STRICT_GROUPS`).

## Zero / plethron
Already ported for the counterparts that exist in Zero: `game/include/global.h:92` (`intptr_t callbackArg; // PORT ... (was s32)`), `game/src/fragments/audio_and_gb_emulator/gb_apu.c:909` (`GbApu_Reset(u8* arg0) // PORT: arg0 is a pointer (was s32)`), `game/src/geo_render.c:917-923` (pool/state pointers), `game/src/libnumus/player.c:625` (`SoundBank*`, "was s32"). The strict ratchet is in `CMakeLists.txt:292-304` with `cmake/strict_sources.txt`. No Zero counterpart of `BattlePrep_Align64PoolBlock` / `Display_GetWorkerStatus` was found by name.

## How to detect
`tools/ptr_warnings.sh` (both repos) for compiler-visible cuts; compile with `-Wshorten-64-to-32 -Wpointer-to-int-cast -Wint-conversion` (grep `error:` too: `-Wint-conversion` is an error hidden by flags); ASan; `[timg] bad` / `[dma-bad]` lines in replays (BADADDR in `tools/run_replays.py`). Test on Linux and Windows too: heap addresses differ from macOS.
