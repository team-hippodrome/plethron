// Stadium Zero's rules for trainer memory (hippodrome-utils docs/specs/TRAINER_MEMORY.md): which trainer a tournament
// battle is against, and what the memory screen may show of each one. Compiled with the game code.
#pragma once
#include "global.h"
#include "game_state.h"

#include <hippodrome/trainer_memory.h>

// Zero's cups as trainer memory keys: `cup` is the session mode (1 Lv. 50-55, 2 Lv. 1-30), `division` the Lv. 1-30
// ball cup (0 Poké Ball .. 3 Master Ball; 0 in Lv. 50-55), `trainer` the round (Lv. 1-30: each cup's trainers come
// in a fixed order) or the Nintendo Cup '97 finalist (Lv. 50-55: 0-14, met in random order).
#define ZERO_TM_CUP_L50 1
#define ZERO_TM_CUP_L30 2
#define ZERO_TM_L30_ROUNDS 8
#define ZERO_TM_L50_FINALISTS 15

hu_tm_key ZeroTm_Key(s32 cup, s32 division, s32 trainer);
// The key of the trainer the session's current round is against; 0 outside the tournaments (Free Battle has no
// memory: its COM teams are not fixed trainers).
s32 ZeroTm_KeyForSession(const SessionContext* session, hu_tm_key* key);
void ZeroTm_RecordName(hu_tm* tm, hu_tm_key key, const char* name);

// What the memory screen shows of one trainer.
typedef struct ZeroTmView {
    s32 reached;                // the player got to this trainer: its name is shown
    s32 teamCount;              // Pokémon shown (0 = the team is unknown)
    const hu_tm_trainer* own;   // this trainer's record (moves seen, speeds), or NULL
    const hu_tm_trainer* team;  // the record whose team is shown: own, or the Poké Ball Cup's
} ZeroTmView;

// Zero's Lv. 1-30 cups have the same Pokémon for each round's trainer in every cup (only stats and moves differ), so
// having fought a round's trainer in the Poké Ball Cup shows its Pokémon in the other cups as well, but not their
// moves or speeds (Julius, 2026-11-21).
ZeroTmView ZeroTm_View(const hu_tm* tm, s32 cup, s32 division, s32 trainer);
