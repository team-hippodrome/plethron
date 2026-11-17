# Smoke behind "LOSE" was garbled (table of big-endian segmented addresses read unswapped)

- **Category:** endianness
- **Zero status:** not applicable (Zero's WIN/LOSE board has no puff animation reading a ROM address table)
- **stadion:** [PR #12](https://github.com/team-hippodrome/stadion/pull/12), commit(s) `7cbfc13` (game part: michiiik/pokestadium#297)
- **stadion files:** `game/src/fragments/battle_prep/battle_prep_3A0010.c` (today; was a submodule change)

## Symptom
After giving up a cup battle, the smoke puffs behind LOSE showed whatever texture had been loaded before.

## Root cause
`BattlePrepStarBurst_UpdateSparkSlot` picks the puff frame from a table of segmented addresses inside ROM data
(`D_2023240`). Read natively, 0x02021A40 became 0x401A0202, which resolved to a wrong segment.

## Fix in stadion
```c
arg0->unk_0C = Memmap_GetSegmentVaddr(PORT_BE32(((s32*)Memmap_GetSegmentVaddr(D_2023240))[(arg0->unk_04 / 2)]));
```

## Zero / plethron
Zero's result board (`game/src/fragments/vs_results_board/vs_results_board.c`) and VS scene contain no
`Memmap_GetSegmentVaddr` reads of address tables (`grep -n GetSegmentVaddr` on vs_results_board, vs_scene,
tournament_badge_cap and cup_clear finds only `Memmap_GetFragmentVaddr` in vs_scene.c:217-223). Zero's lifted
data (`src/port/zero_lift.h`) is swapped and widened at load, so the class only applies to raw ROM asset files.

## How to detect
`grep -rn "Memmap_GetSegmentVaddr(\*\|GetSegmentVaddr(((s32\*)" game/src` and check each table source is lifted or
swapped. Runtime: libultraship logs `[timg] bad address` for wild texture addresses.
