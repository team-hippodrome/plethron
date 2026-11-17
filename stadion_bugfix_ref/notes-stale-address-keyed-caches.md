# Caches and "already swapped" flags keyed by address go stale when a file is reloaded at the same address

- **Category:** rendering
- **Zero status:** already ported (descriptor cache cleared with the display-list cache; Zero's swap-once markers are content-based; plethron `0f590be` extends it to replaced model blobs)
- **stadion:** no PR ("Fixed in this round" text tables; third ares round Gallery photos; before PR #1, history squashed into `be110fe`). The animated-texture frame-record case (Rapidash's flames) is PR #123, covered separately.
- **stadion files:** `game/src/text_system.c`, `src/port/gfx_translate.cpp`, `game/src/memmap.c`

## Symptom
- Transfer Pak screen crashed in `Font_Printf`: a text table reloaded at an old address was taken as already byte-swapped and used unswapped.
- Gallery: 2nd and later photos drew the sky and cliffs with the previous photo's textures.

## Root cause
Converters that byte-swap in place or translate once and cache by host address assume an address always holds the same data. Scenes reload files into the same buffers, so the cache returns the previous file's result (or skips the swap of the new one).

## Fix in stadion
- `text_system.c:22-46`: whether a table is swapped is decided from its contents (count small, first offset just past the table), not from an address cache.
- `gfx_translate.cpp`: the texture-descriptor cache of `Port_ConvertDesc` is cleared in `Port_GfxFlushCache` together with the translated display lists; segment/fragment map changes call `Port_GfxFlushCache` (`memmap.c`).

## Zero / plethron
Already ported: `src/port/gfx_translate.cpp:40-55` (`gDescCache`, `gPaletteDescCache` cleared in the flush) and `:495-516` ("dropped with the other translations (Port_GfxFlushCache): a blob reloaded at the same address holds other..."); `game/src/memmap.c:446,500,606,658` call `Port_GfxFlushCache` ("from stadion"). Swap-once markers in Zero are content-based: `memmap.c:36-45` (`PORT_FRAGMENT_SWAPPED` in `inst[1]`), `game/src/libnumus/player.c:631-649` (song header detected by value). plethron's own `0f590be` "Drop translated display lists when a model blob is replaced in memory" covers the same class for model blobs. Zero's text system differs from Stadium 1's (`text_renderer.c`), no address-keyed swap there.

## How to detect
Wrong content only on the second visit or second item of a screen; grep `std::unordered_map<const void*` caches in `src/port` and check each has an invalidation on reload; grep in-place swaps guarded by a static "done" flag or address set.
