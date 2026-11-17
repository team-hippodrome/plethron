# Pokédex AREA and Lab PC Items/See List: crashes from N64 struct layouts and undersized buffers

- **Category:** abi-64bit
- **Zero status:** already ported (Zero's own typing of the same screens)
- **stadion:** [PR #51](https://github.com/team-hippodrome/stadion/pull/51), commit(s) `8d70d3a` (head `de60de6`)
- **stadion files:** `patches/game` (`src/fragments/pokedex/pokedex_2190D0.c`, Lab PC list/items fragments)

## Symptom
Screens only a Game Boy cartridge opens crashed: the Pokédex AREA map and the Lab PC Items / See List.

## Root cause
Decomp declarations sized for the N64: a 1-byte `char unk_00[1]` field (+ padding) that `sprintf` fills with the area label ("area unknown"), and `unk_D_88826CF0` declared as one pointer plus padding where the code indexes five per-location pointers (pointers are 8 bytes on the host, so the padding no longer covers them). Other struct strides assumed 4-byte pointers.

## Fix in stadion
`pokedex_2190D0.c:21` `char unk_00[0x2D]` (was `[1]` + pad), `:38` `unk_04[5]` pointer array, plus Lab struct/stride fixes ("PORT64" markers).

## Zero / plethron
Zero's Pokédex area labels are already a real buffer: `game/src/fragments/pokedex/pokedex_2.c:684-687` (`Label8A104B00 { char text[0x2E]; s16 width; }`), filled by `sprintf` at `pokedex_2.c:757-825`. Zero's Lab list structs are typed for the host: `game/src/fragments/lab_pokemon_list/lab_pokemon_list.h:179-195` (`LabPCBox`, `LabPCEntry`, "PORT: typed so the host layout follows the pointers"), `pokemon_list_10.c:42` (`LabPCList_InitSourceBoxes`, "were N64 strides of 0x20 and 0x58"). One leftover N64 stride in the same screens is a real bug: see `070-lab-pc-box-pointer-list-read-through-s32.md`.

## How to detect
grep for `char unk..\[1\]` fields that are `sprintf`/`strcpy` targets, and for `[1]` + `char unkNN[...]` pairs that hide arrays of pointers. ASan build (`-DPORT_ASAN=ON` in stadion) on the route.
