# Gym Leader Castle crash: Battle_EstimateMoveDamage read past a move table the decomp had split in three

- **Category:** game-logic
- **Zero status:** not applicable (Zero's announcer does not use Battle_EstimateMoveDamage or this table)
- **stadion:** [PR #33](https://github.com/team-hippodrome/stadion/pull/33), commit(s) `5c24a88`
- **stadion files:** `patches/game/02-core.patch` (`src/3D140_2.c`)

## Symptom
Gym Leader Castle runs crashed with the audio manager overwritten nearby (found under ASan from recorded runs).

## Root cause
One rodata table (effect, power, type for moves 1-165) was split by the decomp into `D_800780B4[204]`,
`D_80078180[96]` and `D_800781E0[196]`. `Battle_EstimateMoveDamage` indexes it as one array; on the N64 it ran on into
the next parts, on a host the arrays need not be adjacent.

## Fix in stadion
Merged into one `u8 D_800780B4[204 + 96 + 196]` with the other two parts as comments.

## Zero / plethron
In plethron `Battle_EstimateMoveDamage` appears only as stale prototypes (`game/src/misc_stubs.c:89`,
`audio_manager.c:72`, `audio_sequence_queue.c:74`), with no definition (`grep` for a non-prototype line finds none,
also not in /home/user/pokestadium0/src). Zero's announcer (`game/src/audio_commands_4.c`,
`Announcer_CommentOnMove` etc.) queues literal line ids. Zero data is lifted per symbol (`zero_lift_data.c`), so the
same class (one table indexed across symbol boundaries) can appear elsewhere; plethron `1b51011` was such a case.

## How to detect
ASan global-buffer-overflow reports; in plethron, lifted symbols sized smaller than the index range the code uses
(compare `ZLIFT_STORAGE(sym, size)` in `src/port/generated/zero_lift_data.c` with the loops that read them).
