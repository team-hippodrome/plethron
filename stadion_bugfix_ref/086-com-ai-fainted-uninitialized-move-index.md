# COM AI read an uninitialized selected-move index for a fainted Pokemon (out-of-bounds crash)

- **Category:** game-logic
- **Zero status:** not applicable (Zero's COM AI has no per-candidate move-score records or `BattleAI_ScoreAttackVsSwitch`)
- **stadion:** [PR #86](https://github.com/team-hippodrome/stadion/pull/86), commit(s) `980088c` (squashed into `2db362c` locally)
- **stadion files:** `patches/game/20-fragment-battle_engine.patch` (battle_engine_361050.c), `tools/test_battle_ai_fainted.py`

## Symptom
Windows access violation in `BattleAI_ScoreAttackVsSwitch`; registers showed move index 167 used on a 4-entry table.

## Root cause
`BattleAI_BuildAllCandidateMoveScores` skips fainted Pokemon, leaving their selected-move byte (`unk_B4`)
uninitialized. The doubles switch-scoring path then indexed `arg5[...].unk_04[unk_B4]` for the fainted current
Pokemon. On N64 the stale byte read harmless RAM; on the host it read past the allocation.

## Fix in stadion
Gate the lookup on current HP: `if ((sp40->unk_0C > 0) && (arg5[...].unk_04[arg5[...].unk_B4].unk_0C >= 0xFB))`.
The routine already forces the switch score to 0xFF for a fainted Pokemon, so behaviour is unchanged.

## Zero / plethron
Zero's COM AI lives in `game/src/fragments/battle/com_ai.c`, `com_ai_2.c`, `com_ai_3.c`, `com_ai_move_choice.c`
(`BattleAi_ChooseTurnAction` at com_ai_move_choice.c:337, `BattleAi_ChooseSwitch` at com_ai_2.c:1632). It is a
different, simpler design (ComBattler state, pick helpers). Grepping `0xFB`/`0x400000`/`ScoreAttackVsSwitch` in those
files finds nothing; there is no per-candidate score table indexed by a stored move byte.

## How to detect
Look for arrays filled only for living/valid entries and later indexed by a byte from the skipped entries. An ASan
build (the stadion tool compiles one function with stubs) catches it; at runtime a crash in AI scoring right after
a faint in doubles.
