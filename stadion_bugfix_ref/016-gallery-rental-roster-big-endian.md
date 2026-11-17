# Gallery rental viewer read its roster unswapped (huge count, byte-swapped stats)

- **Category:** endianness
- **Zero status:** not applicable (Zero has no Gallery; its rental rosters are swapped in free_battle_setup_5.c)
- **stadion:** [PR #16](https://github.com/team-hippodrome/stadion/pull/16), commit(s) `c8f1528` (game part: michiiik/pokestadium#299)
- **stadion files:** `game/src/fragments/gallery_rental_viewer/gallery_rental_viewer_2AD910.c` (today)

## Symptom
Found by static audit: the Gallery's rental viewer would read a huge Pokémon count and byte-swapped stats/experience.

## Root cause
Trainer-data file 0x1A (count, then `BattleMon` records) is big-endian ROM data. Stadium 1's Rental_LoadRoster
swaps it in place once; the Gallery path loaded the same file without swapping.

## Fix in stadion
After `BinArchive_GetFile(..., 0x1A)`, if the count looks unswapped (> 0x1000) swap it and convert each record with
`Port_BattleMonFromBE` (the buffer can be handed out again, so the count decides).

## Zero / plethron
Zero has no Gallery mode. Its rental/preset teams are converted by `Port_SwapBattleMon`
(`game/src/fragments/free_battle_setup/free_battle_setup_5.c:765`, applied at `:793` and `:802`).

## How to detect
Audit every `BinArchive_GetFile` result that is cast to a struct with 16/32-bit fields: is it swapped once, and is
the "already swapped" check robust when the archive returns a cached buffer? Runtime: absurd counts or stats.
