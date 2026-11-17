# Battle AI crash (unoptimized builds): `ptr->unk_57 * 0` read through an unset pointer

- **Category:** game-logic
- **Zero status:** not applicable (Zero's COM AI has no BattleAI_SelectBestScoredMove; no `ptr->... * 0` pattern)
- **stadion:** [PR #34](https://github.com/team-hippodrome/stadion/pull/34), commit(s) `313e457`
- **stadion files:** `patches/game/20-fragment-battle_engine.patch` (`src/fragments/battle_engine/battle_engine_361050.c`)

## Symptom
Unoptimized builds crashed at frame 1232 of `boot_battle_move` when the COM chose a move.

## Root cause
A decomp leftover in `BattleAI_SelectBestScoredMove`: `sp70 = ptr->unk_57 * 0;` with `ptr` not yet set. Optimizers
drop the load (result is 0); `-O0` keeps it and dereferences garbage.

## Fix in stadion
`sp70 = 0; // PORT: was ptr->unk_57 * 0 with ptr not yet set`

## Zero / plethron
`grep -rn "\* 0;" game/src` finds two hits, both harmless: `game/src/gb_audio.c:199`
(`D_800B3FE8[arg0] * 0`, a static array) and `game/src/game_state.c:610` (an expression on a global). Zero's COM AI
(`game/src/fragments/battle/com_ai.c`, `com_ai_2.c`) has no `0x80000002` scoring sentinel / SelectBestScoredMove.

## How to detect
`grep -rn "\* 0;" game/src` and check each operand is valid at that point; build Debug (`-O0`) with ASan/UBSan and run
battle routes.
