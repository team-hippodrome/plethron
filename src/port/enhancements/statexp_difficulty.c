// Stat Exp difficulty (hippodrome-utils docs/specs/STATEXP_DIFFICULTY.md): the player sets the lowest and highest stat
// experience of the rental Pokémon and of the opponent (COM) Pokémon, each Vanilla (the default: Zero's own values),
// one of the five presets (Casual, Easy, Medium, Hard, Kaizo: fifths of 65535) or Custom (enhancements.h,
// hippodrome/statexp_difficulty.h). A range clamps each of the five stat exp values into it, then the stats are worked
// out again with Zero's own formula (Pokemon_CalcStat) at the Pokémon's level. DVs are never changed. Compiled with
// the game code.
//
// Where it applies:
// - rentals: the Free Battle / tournament entry screen's rental tables (the 73 rentals of both divisions) and its
//   8 preset teams, each time the battle setup screen loads them from the ROM (FreeBattle_RunSetupScreen), so the
//   rental cards already show the new stats. A changed setting takes effect the next time that screen opens.
// - opponents: every tournament COM team as it is loaded (TournamentOpponent_LoadTemplate), and the team the player
//   gives the COM in Free Battle (FreeBattle_CopyTeamToRoster, player 5). A Free Battle COM team picked from the
//   rentals already has the rental setting; an opponent setting other than Vanilla is applied on top of it.
// 1P's own Game Boy Pokémon are never changed.
#include <stdio.h>
#include <stdlib.h>

#include "global.h"
#include "game_state.h"
#include "gb_data.h"

#include "enhancements.h"
#include "battle_enhancements.h"

#include <hippodrome/statexp_difficulty.h>

static s32 PortEnh_SeLog(void) {
    static s32 sLog = -1;
    if (sLog < 0) {
        const char* v = getenv("PORT_ENH_LOG");
        sLog = (v != NULL && *v != '\0' && *v != '0');
    }
    return sLog;
}

static void PortEnh_SeApply(BattleMon* mon, const hu_se_setting* setting) {
    u16 oldMaxHp = mon->maxHp;
    s32 species = mon->species.id;

    if (species == 0 || species > 151) {
        return;
    }
    hu_se_apply(setting, mon->statExp);
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

void PortEnh_StatExpDifficultyTeam(BattleMon* mons, s32 count, s32 group, const char* what) {
    hu_se_setting setting;
    s32 i;

    Port_Enh_StatExpDifficulty(group, &setting);
    if (!hu_se_active(&setting)) {
        return;
    }
    for (i = 0; i < count; i++) {
        PortEnh_SeApply(&mons[i], &setting);
        if (PortEnh_SeLog()) {
            fprintf(stderr,
                    "[statexp-difficulty] %s %s %d: species %d L%d dvs %04X stat exp %u/%u/%u/%u/%u -> HP %u Atk %u "
                    "Def %u Spd %u Spc %u\n",
                    hu_se_mode_name(setting.mode), what, (int)i, mons[i].species.id, mons[i].level, mons[i].dvs,
                    mons[i].statExp[0], mons[i].statExp[1], mons[i].statExp[2], mons[i].statExp[3],
                    mons[i].statExp[4], mons[i].maxHp, mons[i].attack, mons[i].defense, mons[i].speed,
                    mons[i].special);
        }
    }
}
