# ROM and blob data are big-endian: tags, headers, records, descriptors and byte lists read natively on the host

- **Category:** endianness
- **Zero status:** already ported (the asset layer is "from stadion"; Zero's data symbols are lifted per type with swapping)
- **stadion:** no PR (Stage 3b/3c asset layer, "reaches the battle", per-species records, ares rounds; before PR #1, history squashed into `be110fe`). Later ROM-table fixes with PR numbers (#12, #13, #16, #61, ...) are covered separately.
- **stadion files:** `game/src/memmap.c`, `game/src/display_object_textures.c`, asset archive / jpeg_stream, `src/port/gfx_translate.cpp`, session.c, battle_engine, `tools/gen_word_arrays.py`, `CMakeLists.txt`

## Symptom
No asset recognised as compressed (`'PERS'` compare), every rental Pokémon L100 (EXP read little-endian; the cup's level cap blocked the third pick and looked like broken C buttons), empty portrait boxes (per-species camera records), the field stage fogged white (s16 near/far), balloons drawn as squares and `[timg] bad address 0x20020` (animated-texture descriptors indexed with the host struct size instead of the 12-byte ROM stride), battle move effects missed or `gMoveEffectHandlers[-1]` crash (effect lists declared as u32 words but checked byte by byte), the intro sky streaked yellow/green and the battle wipe blocky (C arrays of u16/u32 texels little-endian on the host; arm64 `_MergedGlobals` hid ~40 of them from the registry).

## Root cause
Everything the N64 reads from ROM or from C word arrays is in big-endian memory order; the host loads are little-endian. Records also have N64 strides that differ from host `sizeof`.

## Fix in stadion
`PORT_BE32` tag compares; headers swapped once with a marker (`Memmap_NormalizeFragmentHeader`, `inst[1]`); BinArchive header = two BE u16; `Port_BattleMonFromBE`; per-species records swapped after DMA; `Port_DescAt` (ROM stride); effect lists as byte arrays in N64 order; word arrays swapped into TMEM (`tools/gen_word_arrays.py` registration wrappers) and `-mno-global-merge`.

## Zero / plethron
Already ported: `game/src/asset_archive.c:201,253` and `display_object_textures.c:183` (`PORT_BE32(...) == 'PERS'`), `game/src/memmap.c:36-45` (`Memmap_NormalizeFragmentHeader`, "from stadion"), `display_object_textures.c:71,290,368` (BE model tables, blob entries, resource tables "from stadion"), `game/src/geo_render.c:1655,1685` (`Port_DescAt(..., sizeof(GeoTexture))`), animation data through `anim_raw.h` (`geo_render.c:211-277`), `CMakeLists.txt:194-218` (`gen_word_arrays.py`) and `:347-348` (`-mno-global-merge` on arm64). Zero's own data symbols (e.g. the effect lists `D_8218ABF0` used by `Battle_IsByteInArray`, `fragments/battle/damage.c:204,225`) are declared `u8[]` and filled by the data lifter, which swaps scalars by declared type (`src/port/zero_lift.h:1-4`, `tools/zero/gen_lift.py`): a wrong declared element type there is the Zero form of this bug.

## How to detect
`tools/run_replays.py` BADADDR (`[timg] bad`, `[dma-bad]` lines); values that are absurd (L100, huge counts); `PORT_VALIDATE_DL=1`; for Zero, compare the lifter's declared type of a symbol with how the code indexes it (u32 table read via `u8*`, etc.); `-Wincompatible-pointer-types` without `-w` finds word arrays passed as byte pointers.
