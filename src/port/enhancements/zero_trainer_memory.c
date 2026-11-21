// Stadium Zero's trainer memory rules (zero_trainer_memory.h).
#include "zero_trainer_memory.h"

hu_tm_key ZeroTm_Key(s32 cup, s32 division, s32 trainer) {
    hu_tm_key k;
    k.game = HU_GAME_STADIUM_ZERO;
    k.cup = (u8)cup;
    k.division = (u8)division;
    k.round = 0;
    k.trainer = (u8)trainer;
    return k;
}

s32 ZeroTm_KeyForSession(const SessionContext* session, hu_tm_key* key) {
    s32 round = session->roundOrDemoId - 1;
    if (round < 0 || round >= 8) {
        return 0;
    }
    switch (session->mode) {
        case ZERO_TM_CUP_L50: // TournamentOpponent_SetUpLv50Round keeps the finalist it picked here
            *key = ZeroTm_Key(ZERO_TM_CUP_L50, 0, session->opponentHistory[0][round]);
            return session->opponentHistory[0][round] < ZERO_TM_L50_FINALISTS;
        case ZERO_TM_CUP_L30: // TournamentOpponent_SetUpLv1To30Round: sCupFirstOpponent[cup] + round
            if (session->modeSelection < 0 || session->modeSelection > 3) {
                return 0;
            }
            *key = ZeroTm_Key(ZERO_TM_CUP_L30, session->modeSelection, round);
            return 1;
    }
    return 0;
}

void ZeroTm_RecordName(hu_tm* tm, hu_tm_key key, const char* name) {
    hu_tm_record_name(tm, key, name);
}

ZeroTmView ZeroTm_View(const hu_tm* tm, s32 cup, s32 division, s32 trainer) {
    ZeroTmView v;
    v.own = hu_tm_find(tm, ZeroTm_Key(cup, division, trainer));
    v.team = NULL;
    v.reached = (v.own != NULL) && (v.own->flags & HU_TM_REACHED);
    v.teamCount = 0;
    if (v.own != NULL && (v.own->flags & HU_TM_TEAM_KNOWN)) {
        v.team = v.own;
    } else if (cup == ZERO_TM_CUP_L30 && division != 0) {
        const hu_tm_trainer* poke_ball = hu_tm_find(tm, ZeroTm_Key(cup, 0, trainer));
        if (poke_ball != NULL && (poke_ball->flags & HU_TM_TEAM_KNOWN)) {
            v.team = poke_ball;
        }
    }
    if (v.team != NULL) {
        v.teamCount = v.team->team_count;
    }
    return v;
}
