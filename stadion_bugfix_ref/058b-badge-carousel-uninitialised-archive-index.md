# Badge carousel after a Free Battle: uninitialised archive index read a wild trainer record

- **Category:** game-logic
- **Zero status:** not applicable (Zero has no battle_prep badge carousel)
- **stadion:** [PR #58](https://github.com/team-hippodrome/stadion/pull/58), commit(s) `d0de90d`
- **stadion files:** `patches/game` (`src/fragments/battle_prep/battle_prep_397B10.c`, `BattlePrep_LoadBadgeTextures`)

## Symptom
Crash at the rules confirmation of a Game Pak Free Battle after a battle had been played.

## Root cause
`BattlePrep_LoadBadgeTextures` sets its archive index `sp44` only for session modes 1-8; for Free Battle (mode 0) it is an uninitialised stack slot (in the original too). After a battle (`roundOrDemoId > 0`) the trainer records indexed by it were read: a wild pointer whenever the stack held the wrong value.

## Fix in stadion
```c
s16 sp44 = -1; // PORT: modes without a badge set
...
temp_s5 = sp44 >= 0 ? BinArchive_GetFile(arg1, sp44) : NULL; // neutral texture
```

## Zero / plethron
Zero has no battle_prep fragment; `grep -ril badge game/src` finds only `tournament_badge_cap.c` (geo callbacks, no archive index), `battle_order_select.c` (trainer badge drawing) and `vs_scene.c`. Nothing reads an archive by a mode switch without a default there.

## How to detect
`clang -Wsometimes-uninitialized` (switch without default assigning a local later used as an index); ASan/MSan on mode-dependent screens.
