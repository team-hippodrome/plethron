// Stat Exp difficulty (hippodrome-utils docs/specs/STATEXP_DIFFICULTY.md): the player sets a lowest (Min) and a highest
// (Max) stat experience for the rental Pokémon and for the opponent (COM) Pokémon, each Vanilla (the default: that
// bound leaves Zero's values alone), one of the five presets (Casual, Easy, Medium, Hard, Kaizo; the other way round
// for rentals, so Kaizo rentals get the least) or Custom (enhancements.h, hippodrome/statexp_difficulty.h). Each of the
// five stat exp values starts from Zero's own, is raised to Min and lowered to Max, then the stats are worked out again
// with Zero's own formula (Pokemon_CalcStat) at the Pokémon's level. DVs are never changed. Compiled with the game code.
//
// Tournaments only (Free Battle is never changed), as each tournament battle loads its COM team
// (TournamentOpponent_LoadTemplate):
// - opponents: that COM team, copied fresh from the ROM's template each time.
// - rentals: the player's rentals (species.isRental == 1, set by the tournament entry screen). Each one first gets
//   back Zero's own stat exp from the rental table (Port_FreeBattleVanillaRentals), so a setting changed between two
//   launches (a continued tournament) is applied to the game's values, not on top of the old setting. The entry
//   screen's rental cards show Zero's own stats, as in Free Battle.
// 1P's own Game Boy Pokémon are never changed.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static void PortEnh_SeApply(BattleMon* mon, const hu_se_setting* setting, hu_se_side side) {
    u16 oldMaxHp = mon->maxHp;
    s32 species = mon->species.id;

    if (species == 0 || species > 151) {
        return;
    }
    hu_se_apply(setting, side, mon->statExp);
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

static void PortEnh_SeLogMon(const char* setting, const char* what, s32 i, const BattleMon* m) {
    if (PortEnh_SeLog()) {
        fprintf(stderr,
                "[statexp-difficulty] %s %s %d: species %d L%d dvs %04X stat exp %u/%u/%u/%u/%u -> HP %u Atk %u Def %u "
                "Spd %u Spc %u\n",
                setting, what, (int)i, m->species.id, m->level, m->dvs, m->statExp[0], m->statExp[1], m->statExp[2],
                m->statExp[3], m->statExp[4], m->maxHp, m->attack, m->defense, m->speed, m->special);
    }
}

static void PortEnh_SeName(const hu_se_setting* s, hu_se_side side, char* out, size_t n) {
    u16 lo, hi;
    hu_se_bounds(s, side, &lo, &hi);
    snprintf(out, n, "%s..%s (%u..%u)", hu_se_preset_name(s->min.preset), hu_se_preset_name(s->max.preset), lo, hi);
}

void PortEnh_StatExpDifficultyTeam(BattleMon* mons, s32 count, s32 group, const char* what) {
    hu_se_side side = group == PORT_SE_RENTAL ? HU_SE_RENTAL : HU_SE_OPPONENT;
    hu_se_setting setting;
    char name[64];
    s32 i;

    Port_Enh_StatExpDifficulty(group, &setting);
    if (!hu_se_active(&setting)) {
        return;
    }
    PortEnh_SeName(&setting, side, name, sizeof(name));
    for (i = 0; i < count; i++) {
        PortEnh_SeApply(&mons[i], &setting, side);
        PortEnh_SeLogMon(name, what, i, &mons[i]);
    }
}

const u8* Port_FreeBattleVanillaRentals(void); // free_battle_setup_5.c

// The rental table entry `mon` was copied from: 73 rentals, then the 8 preset teams' 6 each.
static const BattleMon* PortEnh_VanillaRental(const BattleMon* mon) {
    const u8* seg = Port_FreeBattleVanillaRentals();
    s32 i;

    if (seg == NULL) {
        return NULL;
    }
    for (i = 0; i < 73 + 8 * 6; i++) {
        const BattleMon* r = i < 73 ? (const BattleMon*)(seg + i * 0x50)
                                    : (const BattleMon*)(seg + 0x16D0 + ((i - 73) / 6) * 0x1F0 + 0x10 + ((i - 73) % 6) * 0x50);
        if (r->species.id == mon->species.id && r->level == mon->level && r->dvs == mon->dvs && r->otId == mon->otId &&
            memcmp(r->moves, mon->moves, sizeof(mon->moves)) == 0) {
            return r;
        }
    }
    return NULL;
}

static void PortEnh_SeRentals(BattleMon* mons, s32 count, const hu_se_setting* setting, const char* name,
                              const char* what) {
    s32 i;

    for (i = 0; i < count; i++) {
        BattleMon* m = &mons[i];
        const BattleMon* v;

        if (m->species.isRental != 1) {
            continue;
        }
        v = PortEnh_VanillaRental(m);
        if (v == NULL) {
            if (PortEnh_SeLog()) {
                fprintf(stderr, "[statexp-difficulty] %s %d: species %d not found in the rental table, kept\n", what,
                        (int)i, m->species.id);
            }
            continue;
        }
        memcpy(m->statExp, v->statExp, sizeof(m->statExp));
        PortEnh_SeApply(m, setting, HU_SE_RENTAL); // (works the stats out again even while Vanilla)
        PortEnh_SeLogMon(name, what, i, m);
    }
}

void PortEnh_StatExpDifficultyTournament(TeamRoster* com, ExtendedRosterInfo* comInfo) {
    hu_se_setting setting;
    char name[64];
    TeamRoster* p1 = &gSessionContext.trainerSlots[0];
    ExtendedRosterInfo* p1Info = &gSessionContext.rosterInfo[0];

    (void)comInfo;
    if (gSessionContext.mode == 0) {
        return; // Free Battle (never loads a template, but just in case)
    }
    PortEnh_StatExpDifficultyTeam(com->pokemonPicked, 6, PORT_SE_OPPONENT, "tournament COM");

    Port_Enh_StatExpDifficulty(PORT_SE_RENTAL, &setting);
    PortEnh_SeName(&setting, HU_SE_RENTAL, name, sizeof(name));
    if (p1Info->numberOfPokemonCanBePicked <= 6) {
        PortEnh_SeRentals(p1Info->battleMon, p1Info->numberOfPokemonCanBePicked, &setting, name, "tournament rental");
    }
    if (p1->numberOfPokemonBrought <= 6) {
        PortEnh_SeRentals(p1->pokemonPicked, p1->numberOfPokemonBrought, &setting, name, "tournament rental (picked)");
    }
}

void PortEnh_OpponentStatExpBounds(s32 forThisBattle, u16* lo, u16* hi) {
    hu_se_setting setting;
    u8 m = gSessionContext.mode;

    *lo = 0;
    *hi = 65535;
    if (forThisBattle && (m == 0 || m == 3 || m == 5 || m == 6 || m >= 16)) {
        return; // Free Battle and the scripted battles keep Zero's own stat exp
    }
    Port_Enh_StatExpDifficulty(PORT_SE_OPPONENT, &setting);
    hu_se_bounds(&setting, HU_SE_OPPONENT, lo, hi);
}
