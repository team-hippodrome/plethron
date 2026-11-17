# Lab PC TM/HM list: count as bytes >> 4 and comparator through an N64-offset view (crash opening the list)

- **Category:** abi-64bit
- **Zero status:** already ported (Zero's usable-item list counts entries and compares typed fields)
- **stadion:** [PR #68](https://github.com/team-hippodrome/stadion/pull/68), commit(s) `c3a8fca`
- **stadion files:** `patches/game/20-fragment-lab_pc_list.patch` (`LabPCList_BuildTmHmList`, `LabPCList_CompareTmHmEntries`)

## Symptom
Crash in `LabPCList_CompareTmHmEntries` <- `PointerList_Sort` <- `LabPCList_BuildTmHmList` as soon as the list screen opened with TMs/HMs, vitamins or PP Up in the item box.

## Root cause
The builder counted entries as the byte difference `>> 4` (16-byte N64 entries, 24 bytes on the host): count 1.5x too high, the sort compared empty entries. The comparator read the entries through a struct whose fields only land on the TM number/source with 4-byte pointers.

## Fix in stadion
Count entries (`var_s1 - arg1`); compare through the real entry type (by number, then by source).

## Zero / plethron
Zero's equivalent (`LabPCList_BuildUsableItemList`) counts entries: `game/src/fragments/lab_pokemon_list/pokemon_list_10.c:372` (`list->count = var_s1 - entries;`), and its comparator reads the typed `TmEntry8910A21C` fields: `pokemon_list_10.c:310-311` (`LabPCList_CompareUsableItems`, "PORT: typed"); the entry type is declared for the host at `lab_pokemon_list.h:198-204`.

## How to detect
grep for pointer differences shifted by a constant (`>> 2`, `>> 3`, `>> 4`) and for comparators/accessors that take `void*` and read through an unrelated struct.
