# Rental team picks: a flag tested through an s32 union over four bytes (same Pokémon picked twice)

- **Category:** endianness
- **Zero status:** not applicable (no such union in Zero's Free Battle/rental code; the sweep tool is in plethron)
- **stadion:** [PR #61](https://github.com/team-hippodrome/stadion/pull/61), commit(s) `375cdea`
- **stadion files:** `patches/game` (`gallery_rental_viewer.h` `unk_D_838067F0_0168_0000`, fragment 61, rental_team_selection), `tools/endian_union_sweep.py`

## Symptom
In a Free Battle with a Game Pak party, Mew could be added to the team twice.

## Root cause
The rental team header overlays bytes `unk_000..unk_003` with `s32 raw`; the code sets/tests `raw & 0x80000000` ("slot being replaced") and `raw & 7`. On the N64 bit 31 is `unk_000`'s top bit; on a little-endian host it was the species byte, so species >= 128 (Mew 151) was never "already chosen".

## Fix in stadion
On little-endian hosts the four bytes are declared in reverse order inside the union, so `raw` keeps the N64 value and every use stays unchanged.

## Zero / plethron
`python3 -I tools/endian_union_sweep.py` in plethron lists 10 unions with narrow fields under a wide member in Zero (geo_render AnimFixedFrame, BinArchive, PokeIconSource, audio Stereo, Color_RGBA16*, Vec2s_s32, ...), none in `game/src/fragments/free_battle_setup/` and no `raw & 0x80000000` flag test outside audio. The same class was already fixed in Zero's `PokeIconSource` (`game/src/poke_icon.h:27-35`, halves swapped). The sweep's "sensitive" uses (82) are the place to look if a pick/flag misbehaves.

## How to detect
`tools/endian_union_sweep.py` (both repos): unions overlaying narrow fields with a wide member, and casts reading a field through a wider type. Runtime: behaviour that depends on species/ids >= 128.
