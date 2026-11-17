# Lab PC list sort by the Pokémon column compared a record's first word as one s32

- **Category:** endianness
- **Zero status:** not applicable (Zero's list sorts compare typed fields)
- **stadion:** [PR #61](https://github.com/team-hippodrome/stadion/pull/61), commit(s) `375cdea`
- **stadion files:** `patches/game/20-fragment-lab_pc_list.patch` (`lab_pc_list_1E46B0.c` `LabPCList_CompareBySpeciesGroup`, `LabPCList_SpeciesGroupKey`)

## Symptom
Sorting the Lab PC list by the Pokémon column gave another order than the N64 (not seen at run time; found by the union/cast sweep).

## Root cause
`LabPCList_CompareBySpeciesGroup` compared a record's first word (species at byte 0x01, u16 at 0x02) as one s32; on a little-endian host that sorted by the u16 first.

## Fix in stadion
The key is built from the fields in the N64's order (`LabPCList_SpeciesGroupKey`).

## Zero / plethron
Zero's いちらん sorts compare named fields: `game/src/fragments/lab_pokemon_list/pokemon_list_9.c:280` `LabPCList_CompareBySpeciesNumber` (`a->mon.species.id - b->mon.species.id`), `:206` `LabPCList_CompareById` (`mon.otId`); there is no species-group comparator (`grep -rn -i SpeciesGroup game/src` is empty).

## How to detect
`tools/endian_union_sweep.py` "pointer casts to a wider type"; comparators that subtract `*(s32*)` of a record.
