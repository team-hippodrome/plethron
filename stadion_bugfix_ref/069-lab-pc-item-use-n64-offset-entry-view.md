# Lab PC list: using an item crashed (chosen item read through an N64-offset entry view)

- **Category:** abi-64bit
- **Zero status:** already ported (Zero's item-use code reads the typed entry and deck)
- **stadion:** [PR #69](https://github.com/team-hippodrome/stadion/pull/69), commit(s) `9b08157`
- **stadion files:** `patches/game/20-fragment-lab_pc_list.patch`, `patches/game/02-core.patch` (`table_view.h` `WordList`)

## Symptom
SIGSEGV in `LabPCList_TeachTmMove` when using an item from the list (See List -> Pikachu -> Use item -> HP UP -> Yes).

## Root cause
The item menus hold `unk_func_8830867C_02C_1A8` entries, but several functions read them through a struct with the TM number at 0x08, only right with 4-byte pointers: on the host the read got the box pointer, so every item took the TM path. `LabPCList_ConsumeUsedTmItem` read the box's item list at the N64 offset 0x0C (0x10 on the host) and truncated the pointer to s32; the vitamin crossfade widget stored 8-byte values into 4-byte fields.

## Fix in stadion
Read entries as the real type, use the real box for the item list (`WordList` data is a pointer), index the replaced move by field, fix the widget fields.

## Zero / plethron
Zero's equivalents are typed: `game/src/fragments/lab_pokemon_list/lab_pokemon_list.h:198-204` (`TmEntry8910A21C`: item pointer, deck pointer, code, source, reason), and `LabPCList_ConsumeUsedItem` (`pokemon_list_10.c:497-510`) removes from the deck's word list with a `_Static_assert` that the deck's `unk_0C/unk_10/unk_14` match `WordList` on the host, `WordList.items` is a pointer (`game/src/table_view.h:79-83`, "PORT: was s32").

## How to detect
grep for struct views that differ from the producer's type (`(SomeOtherStruct*)entry`), `(s32)` casts of pointers, and stores into fields narrower than the value (`-Wshorten-64-to-32`, in plethron's strict group list in `CMakeLists.txt`).
