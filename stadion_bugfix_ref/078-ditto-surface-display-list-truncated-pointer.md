# Ditto drew as a flat white blob: its conditional-segment callback cut the display-list pointer to 4 bytes

- **Category:** abi-64bit
- **Zero status:** not applicable (Zero's callback tables are converted to u32 segment words, not host pointers, so the 4-byte read is the whole value)
- **stadion:** [PR #78](https://github.com/team-hippodrome/stadion/pull/78), commit(s) `7a8b278` (squashed into `2db362c` locally)
- **stadion files:** `patches/game/20-fragment-particle_data_library.patch` (particle_data_library_2557A0.c), `PORTING_NOTES.md`

## Symptom
Ditto rendered flat white (rental detail panel, VS grid, battle) instead of pink-purple, on every screen.

## Root cause
`DisplayList_BuildConditionalSegment(Gfx*, s32* address)` read the address list that stadion had converted to
host pointers (`Port_PdlWords`) through an `s32*`. `gSPDisplayList(gfx++, *address)` therefore emitted only the low
4 bytes of a 64-bit host pointer (`de 09900e30`); the renderer skipped the bogus call, so Ditto's surface list
(G_TEXTURE_GEN, TEXEL0 x PRIM x SHADE and its texture) never ran and the node's PRIM x SHADE material drew white.

## Fix in stadion
Take the typed table and call the full pointer:
```c
void DisplayList_BuildConditionalSegment(Gfx* gfx, DisplayListAddresses* addresses) {
    if (D_8006F09C->textureMode == 0) {
        gSPDisplayList(gfx++, addresses->segments[0]);
    }
```

## Zero / plethron
Zero's equivalent is `ModelCallback_BuildDefaultModeList(Gfx*, s32* address)` in
`game/src/fragments/model_callbacks/model_callbacks_3.c:39` (comment at :47: "Ditto's model data uses it"). It still
reads `*address` through an `s32*`, but its table comes from `ModelCallback_HostAddresses`
(`model_callbacks.c:39-54`), which fills `static u32 sWords[4][16]` with the big-endian words as `u32` segment
addresses, and `DisplayListAddresses.segments` is `u32[2]` (`model_callbacks.h:18-21`). A 4-byte read gets the whole
word, so the stadion bug cannot happen here. (Changing the parameter to `DisplayListAddresses*` would be cosmetic.)

## How to detect
Any `s32*`/`u32*` that walks a table which the port converted to host pointers. Grep for
`gSPDisplayList(gfx++, *` and check the element type of the pointer. Runtime: a `G_DL` with a 32-bit operand that
is not a segmented address (top byte not 0x00-0x0F) is skipped silently; model parts that lose their material
draw white or flat.
