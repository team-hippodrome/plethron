# Code that indexes past one global into the next (N64 link order) reads or writes unrelated host memory

- **Category:** abi-64bit
- **Zero status:** already ported (plethron 915a208 lays byte-layout neighbours out as one N64-shaped block; an ASan build over all 13 screen-diff routes found one overrun, `ModelScrollStrip.textures[frame & 7]` read past the 6-entry host copy in the intro, fixed in 66ed03b by keeping 8 entries; the rerun is clean. `gMoveData[usedMove - 1]` never fired on these routes)
- **stadion:** no PR (Stage 3c ASan, "Fixed in this round", third ares round ASan passes; before PR #1, history squashed into `be110fe`)
- **stadion files:** `tools/merge_vtx_arrays.py`, `game/src/gb_data.c`, `game/include/variables.h`, `game/src/fragments/battle_prep/battle_prep_394120.c`, battle scene tick slots, `widget_toolkit_2.c`, `clefairy_game.c`, `sushi_game_144300.c`, `gb_tower_emulator_7F9A0.c`

## Symptom
Crashes and corrupted pictures that depend on the build: recolouring the cup-select banner overwrote the display list behind it; a long battle crashed at frame 21622 (a tick-callback slot held garbage); Sushi-Go-Round crashed after 5+ plates; ASan reports in the intro camera, team-selection scale tables, type icons.

## Root cause
The decomp split what the ROM lays out contiguously into separate globals, but the code still indexes across the boundary: vertex arrays consumed with a count spanning two `Vtx` symbols (`gsSPVertex(D_84B12498, 32, 0)` on a 31-entry array), the sine/cosine table split in two, a 3-entry table read at index 3-4, a list relying on the next table's first word as NULL terminator, `gMoveData[move - 1]` for empty move slots (move 0 reads element -1 = zero tail of the previous table), a tick-slot array written at index 7 after its loop, `D_84B19910[400]` written in a 400-entry array. On the N64 these hit known neighbouring bytes; host globals are placed independently.

## Fix in stadion
Keep the N64 contents, make them one object: `tools/merge_vtx_arrays.py` merged 28 vertex runs (`#define D_x (D_first + n)`); one `gSineTable[0x1400]`; tables merged with their ROM successor; `gMoveDataWithZeroth[166]` with `#define gMoveData (&gMoveDataWithZeroth[1])` (`variables.h:996`, `gb_data.c:930`); `D_84B19910[401]`; spare 8th tick slot; explicit NULL terminators; negative indices treated as inactive.

## Zero / plethron
- Zero's data mostly lives in the decomp's assembly; plethron lifts each referenced symbol into its own host array (`src/port/zero_lift.h:61` `ZLIFT_STORAGE`, `tools/zero/gen_lift.py`), sized to the gap to the next N64 symbol, so overruns within that gap are covered, and the tool's `DECLARED_ONLY` list handles symbols whose tail is other data.
- Not covered: reads before a symbol. `gMoveData` is lifted as its own 990-byte array (`src/port/generated/zero_lift_data.c:67`), and Zero indexes `gMoveData[x - 1]` in `game/src/gb_data.c:491` (guarded by `moves[i] != 0`), `fragments/battle/damage.c:598-603` (`usedMove - 1`), `fragments/battle/damage_estimate.c:180`, `fragments/battle/com_ai_move_choice.c:429`. If `usedMove` can be 0 there, the host reads 6 bytes before the array (whatever the linker put there; ASan would flag it). Check with ASan; stadion's `gMoveDataWithZeroth` pattern is the fix.
- C-defined arrays in Zero's `game/src` (models, display lists) follow the same rule; `tools/merge_vtx_arrays.py` exists in plethron's `tools/`.

## How to detect
ASan build (`-DPORT_ASAN=ON`, plethron `CMakeLists.txt:515`) over the replay routes; `[badvtx]` / `@bad-dl` renderer logs; grep `\[\w+ - 1\]` on tables indexed by ids that can be 0.
