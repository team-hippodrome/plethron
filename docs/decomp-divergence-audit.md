# Decomp divergence audit

[Back to the README](../README.md)

Audit of everything the port changed in the vendored game code relative to the decomp it came from, after the sound
fix (PR #22) found 31 sound calls that an earlier 64-bit cleanup had silently deleted. Date: 2026-11-16. Port: this
branch (on top of PR #22). Decomp: pokestadium0 `35d7a61`, the commit in [game/UPSTREAM.md](../game/UPSTREAM.md).

## Result

Apart from the 31 calls PR #22 restored, nothing was deleted without a reason. Every lost statement, call, branch or
constant in `game/src` is a port change with a host-side replacement, and the 732 offset-to-field rewrites that
could be checked automatically all land on the right N64 offsets. The audit found one leftover from stadion in the
launcher. This PR fixes it.

| Check | Functions / sites | Unjustified |
| --- | --- | --- |
| Decomp functions or files missing from the port | 0 of 5,214 functions, 614 files | 0 |
| Functions that differ from the decomp at all | 949 | - |
| ... with fewer statements, assignments, branches or calls (read one by one) | 84 | 0 (31 calls already restored by #22) |
| ... with more `return`s (early outs the port added) | 31 | 0 |
| Statements that differ only in a constant | 4 (after hex floats and `NULL`/`0`) | 0 |
| Decomp globals a function no longer names (views re-indexed) | 50 | 0 |
| `M2C_FIELD` / raw-offset reads rewritten as fields, offsets checked on a MIPS32 BE layout | 732 | 0 |
| `#if 0`, commented-out calls, `PORT_`-guarded code that is off in the build | 0 | 0 |
| Headers (`game/include`) and `game/lib/ultralib` | 115 and 15 removed lines | 0 |
| Constants in `src/port` copied from stadion | see below | 1, fixed here |

How the functions were compared: both trees are tokenized without comments, strings and preprocessor lines, and
every name goes through pokestadium0's `symbol_addrs.txt` (`func_8003834C` and its real name are the same symbol),
so renames don't count as changes. Per function, the decomp's statements, assignments, control keywords, calls,
numbers and identifiers are counted. Wherever the port has fewer, the function was diffed and read by hand. The scripts
are in [tools/decomp_audit](../tools/decomp_audit/README.md) and can be rerun after every decomp sync.

## The 84 functions that lost statements or calls

All of them fall into these groups (one representative each; the full list is the `structure` section of the
script's report):

- **In-place relocation replaced by host tables.** On the N64 the game turns 32-bit offsets inside loaded blobs into
  pointers in place, which cannot hold a 64-bit pointer. `Block_RelocateSelf` now only swaps the header and sets the
  flag (its users read the table offsets through `AUDIO_FX_TABLE`/`AUDIO_FX_ENTRY`). `Audio_RelocateSoundBank`
  builds host wave tables in `PortA_RelocateSoundBank` (wave base + ROM base unless `0xFF......`, loop and book
  offsets, as the original). `func_80037508` relocates songs with `Audio_PortRelocateSong`. In
  `Audio_LoadSoundData`, the eager `Block_RelocateSelf` calls on the FX blocks `D_800B41A4`/`D_800B41A8` are gone
  because every user (`func_8003821C`, `func_800383DC`) relocates lazily, exactly like the ROM does when the flag is
  clear. `D_800B41A0` is relocated where its tables are read.
- **Model data read in big-endian.** `ModelAnim_SetAnimation`, `ModelAnim_SetEventTrack` and
  `ModelAnim_GetAnimationRecord` call the bank function through `Port_ModelAnimBank` (it runs the blob's MIPS entry
  through `Port_CallBlobEntry`) instead of `getAnimationBank`. `PackedBits_ReadSigned` callers use
  `Anim_PackedBitsReadSigned`, which reads ROM curves in N64 byte order. `PackedBits_ReadSigned` itself builds the
  same 32-bit pair without the union. `Model_SetMaterialColor` writes the numeric `0xRRGGBBAA` instead of
  `Color_SetRGBA`'s byte-wise write (that was the cyan fog). `GeoNode_SetCallback` resolves callbacks with
  `Port_ResolveGeoCallback`. `Memmap_SetSegments` passes host pointers, not 29-bit physical addresses.
  `MtxF_ToFixed` builds the fixed-point words Fast3D reads. None of these is a deletion behind the model spikes.
- **YUV images drawn as RGBA.** `Ui_DrawFramedYuvImage`, `TitleScreen_DrawBackground` and
  `MonSummary_DrawMoveBackdrop` swap `gDPLoadTextureBlockYuv` + the YUV combiner for RGBA16, because the host JPEG
  task decodes to RGBA16.
- **Typed structs instead of byte offsets.** About 50 functions (geo layout commands, `func_8003A2B8`,
  `Audio_StopAllSoundEffects`, the Lab list and Free Battle screens, `BattleAi_*`) read fields where m2c had
  `M2C_FIELD`, raw `p + 0x2B` pointers or `p += 0x148` strides. Temporaries and the pointer bumps they needed are gone.
  Their offsets are checked below.
- **Decomp scaffolding dropped.** `// FAKE` permuter temps, karodo (`if (entry) {}`), stack padding and stale m2c
  arguments (`func_800023C4(size, 0, stale_a2)`) were removed where the function was retyped.
- **Other SDK or host replacements.** `__osLeoInterrupt` calls the stubbed `osEPiRawStartDma`, since there is no 64DD.
  `_decodeChunk` uses `n_aLoadBufferKind` (sample data stays in N64 byte order). `GbEmu_RunFrame` composes the GB
  Player frame on the host (`Port_GbPlayerPresent`) instead of submitting the GB Player's RSP task.
  `Battle_SimpleStringFormat` and `ActivePokemon_ShowCaption` forward their arguments through
  `Battle_SimpleStringVformat`: on the N64 the variadic arguments reached the callee in registers, so the ROM can
  call it without passing them, and a host can't.
- **The fragment's own audio driver** (`AudioLoad_Init`, `AudioHeap_*`, `AudioPlayback_*` in
  `fragments/audio_and_gb_emulator`): retyped and cleared with `memset`. This driver has no caller (the game plays
  sound through libnaudio/libnumus).

## Early returns the port added (31)

- 13 `if (!gAudioPortReady) return` guards on the sound entry points (`Audio_PlaySystemSound`, `func_8003834C`, ...).
  They only fire before the sound data is loaded or with `PORT_NO_AUDIO=1`.
- Fragment and archive loading: `Fragment_Load` returns the statically linked entry (`PORT_STATIC_FRAGMENTS`),
  `Memmap_RelocateFragment` skips relocation for linked code, and `BinArchive_GetFile` and `Memmap_SetFragmentMap`
  guard against a failed open or a bad slot id.
- `LeoCJCreateLeoManager` reports the 64DD as absent, as an N64 without the drive does.
- Text and menus: `Font_Printf` and `Font_PrintfToDisplayList` return early through the Unicode text path,
  `Options_DrawMenu` through the language option, and `Registration_RunFrame` and `LabPCList_RunFrame` now pass on
  the result the N64 left in `$v0`.

## Constants (4)

`func_8003A2B8` clears the channel from `sample` to the end of the host struct (was bytes 0x34 to 0x148).
`Registration_BuildTeamPointers` steps one `RegisteredTeam` (0x220 bytes on the N64). `BattleHud_DrawLevelNumber` and
`BattleHud_DrawHpFractionText` enlarge `sprintf` buffers that overflowed into stack padding on the N64. The remaining
number changes are m2c's float bit patterns (`0x3F800000`) written as float literals, checked equal bit for bit.

## Re-indexed views (50)

The decomp reads several tables through symbols a few bytes before or after them (`D_8006A9CC[move].name` = the move
table at `D_8006A9D4` minus 8, `D_8006B18B[move * 6]` = `gMoveData` minus 5, `D_80064C44` = the species table minus
0x18, the GB Player's `D_8115623C`/`D_81156261` inside `D_811561C8[]`). With 8-byte pointers those views no longer
line up, so the port indexes the real table: `D_8006A9D4[move - 1].name`, `gMoveData[move - 1].effect`,
`D_80064C5C[species - 1]`. Each one was checked against the element size (0xC, 6, 0x1C, 0xA4 on the N64).

## Offset-to-field rewrites (732 checked)

The decomp has 1,349 `M2C_FIELD` reads, and the port still has 198. For the rewritten ones,
`tools/decomp_audit/offsets.py` pairs each `M2C_FIELD(p, T*, K)` (nested ones too) with the port's `p->a.b`
(`p->a->b`) in the same aligned statement and asserts with clang on a 32-bit big-endian MIPS target that each field
starts at the decomp's offset. 732 pairs could be matched. Their only differences are on purpose:

- `Particle.scratchA` / `Particle.param1` read at 0xAB / 0xA7: the low byte of an s16 on the big-endian N64. The
  port casts the field to `u8`.
- `BattleAi_CountUnfainted` (offset relative to `ComSide`, the port walks `battlers[]`: same address),
  `BattleMenu_HandleSwitchButtons` (0x85C is `side`, the port reads `side->teamMember[]`) and
  `BattleScene_InitTurnPresentation` (the script guessed the variable's type wrong; the field is at 0x28).

The other ~420 rewrites are spelled in ways the script can't pair (cast chains like `*((s32 *) (((s8 *) p) + 8))`,
pointer arithmetic, renamed locals). They were part of the hand-read diffs above wherever a function lost
statements, but they are not mechanically proven. `tools/layout_check.py` covers the related class of two struct
views of the same memory.

## Outside the game code

- **`src/port/stubs.c`** stands in for the decomp's handwritten assembly and for SDK internals the host doesn't
  need: the 64DD (`__LeoBootGame3`, `osEPiRawStartDma`), the Controller Pak (`__osContRamRead/Write` report an
  error; the game's `contpfs`/`gbpak*` copies in `game/src/libultra` are not built, and the Transfer Pak is
  `transfer_pak.cpp`), the old libaudio synthesizer (`alSynNew`, `_doModFunc`; the game uses libnaudio) and
  `gGbMemoryMap` (zero-filled: the only read, `GbMem_ReadIoRegister(0xFF26)`, computes NR52 from the APU state and
  never reads the array; `gb_core.c` keeps its own GB memory). No game logic lives there.
- **Files copied from stadion.** Every hex constant that `src/port` shares with stadion was checked: the ROM size and
  SHA-1 are Zero's (`rom_check.h`), the resampler table is found by signature (PR #22), hardware handles and
  FPU/flash ids are libultra's. One Stadium 1 value was left: **the launcher read the trainer's name, ID and checksum
  from GB saves at the Western offsets only** (checksum at 0x3523, ID at 0x2605), so Japanese carts, Zero's own,
  never showed their trainer. Fixed here: for a cart whose header says Japan, it checks the checksum at 0x3594 and
  reads the ID at 0x25FB (`GbSaveMainData.playerId`, SRAM 0x2580 + 0x7B). Japanese names are kana and the launcher's
  font has no kana, so only the ID is shown for them.
- **`game/lib/ultralib`**: `alSeqFileNew` converts the big-endian file to the host layout. `alBnkfNew` still truncates
  host pointers to `s32`, but nothing calls it (Zero uses libnaudio's banks). Left as is and noted here.
- **History.** plethron's history before `90ea528` was squashed, so the commit that deleted the sound calls (`3c185a5`
  in the sound thread's notes) is no longer in the repository. The comparison above is against the decomp itself, so
  it covers every commit, squashed or not. The 60 later commits touching `game/` were scanned for call statements
  removed without a replacement: all were rewrites of the same call.

## Rerunning it

See [tools/decomp_audit/README.md](../tools/decomp_audit/README.md). After a decomp sync, rerun both scripts and read
any new `structure` or `FAIL` lines.
