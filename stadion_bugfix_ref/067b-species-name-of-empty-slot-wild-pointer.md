# Event Battle with a short party: species 0's name read record -1 (wild pointer)

- **Category:** game-logic
- **Zero status:** already ported (guard present in Zero's `Text_CopySpeciesName`)
- **stadion:** [PR #67](https://github.com/team-hippodrome/stadion/pull/67), commit(s) `e621388`
- **stadion files:** `patches/game/11-gameboy.patch` (`src/gb_data.c` `Text_CopySpeciesName`)

## Symptom
Segfault in `HAL_Strcpy` on the Event Battle select screen with a cartridge party of fewer than 6 Pokémon.

## Root cause
`EventBattle_InitTrainerCard` reads 6 party entries per side; past the party count the species is 0, and `Text_CopySpeciesName(0)` read the name pointer of record -1. On the N64 that word is the start of species 151's name ("MEW\0" -> unmapped 0x4D455700); on the host a wild pointer.

## Fix in stadion
Species outside 1..152 copy an empty name (matches the empty nickname, so the slot shows as an ineligible cell).

## Zero / plethron
Present: `game/src/gb_data.c:1343-1351` (`if ((arg1 <= 0) || (arg1 > ARRAY_COUNT(D_8006FF00))) { arg0[0] = '\0'; return arg0; }`), with the same PORT comment.

## How to detect
Table lookups indexed by `id - 1` where id can be 0 for empty slots; ASan on screens that iterate a fixed 6 slots over a shorter party.
