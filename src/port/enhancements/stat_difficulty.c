// DV and Stat Exp difficulty (hippodrome-utils docs/specs/DV_STATEXP_DIFFICULTY.md): the player sets the lowest and
// highest DVs and stat experience of the rental Pokémon and of the opponent (COM) Pokémon, each vanilla (the
// default: Zero's own values) or a custom range (enhancements.h, hippodrome/stat_difficulty.h). A custom range clamps
// each DV and stat exp value into it, then the stats are worked out again with Zero's own formula (Pokemon_CalcStat)
// at the Pokémon's level. Compiled with the game code.
//
// Where it applies:
// - rentals: the Free Battle / tournament entry screen's rental tables (the 73 rentals of both divisions) and its
//   8 preset teams, each time the battle setup screen loads them from the ROM (FreeBattle_RunSetupScreen), so the
//   rental cards already show the new stats. A changed setting takes effect the next time that screen opens.
// - opponents: every tournament COM team as it is loaded (TournamentOpponent_LoadTemplate), and the team the player
//   gives the COM in Free Battle (FreeBattle_CopyTeamToRoster, player 5). A Free Battle COM team picked from the
//   rentals already has the rental range; a custom opponent range is applied on top of it.
// Party Pokémon from a Game Boy cart are never changed for 1P.
#include <stdio.h>
#include <stdlib.h>

#include "global.h"
#include "game_state.h"
#include "gb_data.h"

#include "enhancements.h"
#include "battle_enhancements.h"

#include <hippodrome/stat_difficulty.h>

static s32 PortEnh_SdLog(void) {
    static s32 sLog = -1;
    if (sLog < 0) {
        const char* v = getenv("PORT_ENH_LOG");
        sLog = (v != NULL && *v != '\0' && *v != '0');
    }
    return sLog;
}

static void PortEnh_SdApply(BattleMon* mon, const hu_sd_range* range) {
    u16 oldMaxHp = mon->maxHp;
    s32 species = mon->species.id;

    if (species == 0 || species > 151) {
        return;
    }
    mon->dvs = hu_sd_apply_dvs(range, mon->dvs);
    hu_sd_apply_stat_exp(range, mon->statExp);
    mon->maxHp = Pokemon_CalcStat(STAT_HP, species, mon->statExp[0], mon->level, mon->dvs);
    mon->attack = Pokemon_CalcStat(STAT_ATTACK, species, mon->statExp[1], mon->level, mon->dvs);
    mon->defense = Pokemon_CalcStat(STAT_DEFENSE, species, mon->statExp[2], mon->level, mon->dvs);
    mon->speed = Pokemon_CalcStat(STAT_SPEED, species, mon->statExp[3], mon->level, mon->dvs);
    mon->special = Pokemon_CalcStat(STAT_SPECIAL, species, mon->statExp[4], mon->level, mon->dvs);
    // a team is made at full HP: keep it full (and never above the new maximum)
    if (mon->hp >= oldMaxHp || mon->hp > mon->maxHp) {
        mon->hp = mon->maxHp;
    }
}

void PortEnh_StatDifficultyTeam(BattleMon* mons, s32 count, s32 group, const char* what) {
    hu_sd_range range;
    s32 i;

    Port_Enh_StatDifficulty(group, &range);
    if (!hu_sd_active(&range)) {
        return;
    }
    for (i = 0; i < count; i++) {
        PortEnh_SdApply(&mons[i], &range);
        if (PortEnh_SdLog()) {
            fprintf(stderr,
                    "[stat-difficulty] %s %d: species %d L%d dvs %04X stat exp %u/%u/%u/%u/%u -> HP %u Atk %u Def %u "
                    "Spd %u Spc %u\n",
                    what, (int)i, mons[i].species.id, mons[i].level, mons[i].dvs, mons[i].statExp[0],
                    mons[i].statExp[1], mons[i].statExp[2], mons[i].statExp[3], mons[i].statExp[4], mons[i].maxHp,
                    mons[i].attack, mons[i].defense, mons[i].speed, mons[i].special);
        }
    }
}
