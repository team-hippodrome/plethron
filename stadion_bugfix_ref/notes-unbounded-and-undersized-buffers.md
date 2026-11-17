# Buffers the N64 overran harmlessly crash on the host (unterminated GB names, char[1] sprintf targets, 6-entry locals written 7-8 times)

- **Category:** abi-64bit
- **Zero status:** check (Zero's `Text_UntranscodeName` is the unbounded JP form; whether a real input lacks the 0x50 terminator was not proven)
- **stadion:** no PR (Stage 3 "fortified libc", third ares round long battle / Lab AREA, Linux bring-up; before PR #1, history squashed into `be110fe`)
- **stadion files:** `game/src/gb_data.c`, `game/src/fragments/battle_engine/battle_engine_com_decision_logic.c`, Lab PC / Pokédex sources

## Symptom
- Long battle under ASan / crashes: `Text_UntranscodeName` ran past an empty Transfer Pak slot's zeroed name field.
- Pokédex AREA: signal 5 (fortified `sprintf` trap) writing "AREA UNKNOWN" into a `char[1]` field.
- Linux only: crash at frame 504 of `PORT_BOOT=battle`, the saved return address overwritten.
- Early boot: fortified libc (`__sprintf_chk`, `__bzero_chk`) aborted on small original buffers.

## Root cause
The N64 code wrote past declared sizes into memory that was unused on the N64 (stack homes, struct padding, the next field). Host stack frames and heap layouts differ, and fortified libc checks object sizes, so the same writes hit live data or trap.
- `Text_UntranscodeName` loops `while (*src != 'P')` with no bound; a field without the 0x50 terminator runs on.
- `BattleAI_InsertRankedCandidateGroup` wrote 7-8 entries into 6-entry / 3x3 locals.
- Struct fields declared `[1]` plus padding for data that is really longer.

## Fix in stadion
- `gb_data.c:1282-1285` (US path): `char* end = arg1 + 11; while (*arg1 != 'P' && arg1 < end)`. The JP branch (`gb_data.c:1196-1213`) was left unbounded.
- `battle_engine_com_decision_logic.c:3995` and `:4148`: locals sized to the entries really used (`s32 unk_00[8]`, `s32 sp94[8]`).
- AREA: `unk_D_88826A88.unk_00` and `unk_D_88826CF0.unk_04` given real sizes; sprintf/bzero targets enlarged.

## Zero / plethron
- `game/src/gb_data.c:109-121`: Zero's `Text_UntranscodeName` is the unbounded JP form (`while (*arg1 != 'P')`), writing two bytes per character. Callers: `gb_data.c:172` (`Text_UntranscodeFixedName`, terminates its own copy, safe) and `fragments/cartridge_check/cartridge_check_options.c:898` (`identity.name`, filled from the cached 8-byte identity, `pokemon_stats_2.c:1155`). A bound like stadion's (name field length) costs nothing; add it.
- Related, Zero-side, not a stadion fix: `pokemon_stats_2.c:565-566` initialises an empty port's identity with `*(u64*)&identity = 0x500000000000;`. On the N64 that puts 0x50 in `playerName[0]` (empty name); on a little-endian host the 0x50 lands in `playerName[3]`, so an empty port shows three glyphs of table entry 0. stadion has the same line (`pokemon_stats.c:2108`), unfixed. Check the cartridge-check port panels with an empty pak.
- Battle AI locals: Zero's COM AI is different code (`fragments/battle/com_ai*.c`); no 1:1 counterpart of `BattleAI_InsertRankedCandidateGroup` was found. Run ASan on battle routes.

## How to detect
- `cmake -DPORT_ASAN=ON` (plethron `CMakeLists.txt:513-516`) plus `tools/asan_run.sh` on replays; stadion's routes all ran ASan-clean after these fixes.
- Linux/Windows-only crashes with a corrupted return address: look for locals the loop writes past.
- Grep: `while \(\*\w+ != 'P'\)`, struct fields `[1]` followed by padding, `sprintf` into fixed fields.
