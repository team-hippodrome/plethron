# Crash after winning a cup: badge award address tables read unswapped and as 8-byte pointers

- **Category:** endianness
- **Zero status:** not applicable (Zero's badge award has no ROM address tables; its own badge-award crash was a different bug, fixed in plethron 1b51011)
- **stadion:** [PR #13](https://github.com/team-hippodrome/stadion/pull/13) (also described in [PR #12](https://github.com/team-hippodrome/stadion/pull/12)), commit(s) `89df906`
- **stadion files:** `game/src/fragments/battle_prep/battle_prep_39ED60.c` (today; then a submodule bump)

## Symptom
After a won cup the game crashed in TmemLoad with `[timg] bad address 0x40160202 from w1=0xf000000` (pointer
0x4018020240160202), at frame 20464 of the cup-win recording.

## Root cause
`BattlePrepBadgeAward_DrawBadgeIcon` / `...IconAlt` / `...Sparkle` set segment 0x0F from three tables of big-endian
32-bit segmented addresses in ROM data. The port indexed them as host pointers (8 bytes, native order), so two
swapped entries were fused into one wild 64-bit address.

## Fix in stadion
Index as 4-byte entries and swap:
```c
gSPSegment(gDisplayListHead++, 0x0F,
           (uintptr_t)Memmap_GetSegmentVaddr(PORT_BE32(*(s32*)(temp_v0->unk_1C + tmp * 4))));
```

## Zero / plethron
Zero's badge award is `Tournament_PlayBadgeAward` (`game/src/fragments/vs_scene/vs_scene.c:~1019`), driven by lifted
curve tables `D_82202C1C` / `D_82202C3C` (`vs_scene.c:55-56`); the lifter (`src/port/zero_lift.h`) widens and swaps
pointers at load. It crashed for a different reason (a table lifted as one pointer), fixed by plethron `1b51011`.
No segment-0x0F badge tables exist in Zero.

## How to detect
`[timg] bad address` / `[dma-bad]` in the log (stadion's run_replays flags these as BADADDR). Grep for
`*(s32*)(... + i * 4)` vs `((uintptr_t*)...)[i]` on data that came from ROM.
