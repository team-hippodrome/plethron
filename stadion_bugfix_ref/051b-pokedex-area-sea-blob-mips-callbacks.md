# Pokédex AREA map: black sea (geo callbacks were MIPS code inside the model blob)

- **Category:** rendering
- **Zero status:** not applicable (Zero's Kanto map callbacks are C code in fragment 030, already lifted)
- **stadion:** [PR #51](https://github.com/team-hippodrome/stadion/pull/51), commit(s) `dfe6826`
- **stadion files:** `patches/game` (`pokedex_2190D0.c` `Pokedex_AreaSeaCallback0/1`, `jpeg_stream.c` blob-code table, fragment loader), `src/port` (`Port_ResolveCode`, `Port_VLinkOf`)

## Symptom
The Pokédex AREA map's sea was black.

## Root cause
The area model's two sea nodes have geo callbacks at blob addresses (0x8C000118, 0x8C000214) that are MIPS code in the ROM model blob; there is no C version, so the nodes stayed empty.

## Fix in stadion
Host versions `Pokedex_AreaSeaCallback0/1` that step the blob's wave counters and build the scrolling tile-size list; `Port_ResolveCode` maps a blob's pseudo address to its link address and looks up blob code in a table keyed by fragment ROM; `Fragment_LoadAndRelocateFromRomRange` records which ROM a slot holds.

## Zero / plethron
Zero's habitat map is fragment 030 with C callbacks: `game/src/fragments/pokedex_kanto_map/pokedex_kanto_map.c:51` (`KantoMap_SeaGeoCallback`) and `:81` (`KantoMap_RiversGeoCallback`), lifted in plethron 0a6e51b ("Pokédex habitat screen: lift the Kanto map"). Nothing to port.

## How to detect
A geo node whose callback pointer resolves into a data blob instead of a host function: log unresolved callback addresses in the geo renderer (stadion: `Port_ResolveCode`); visually, empty parts of a model that should animate.
