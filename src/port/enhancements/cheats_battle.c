// Battle cheats (cheats.h, GitHub issue #97): the game side. Compiled with the game code, since it reads the battle
// structs, but kept in the port, so game/ holds only decomp code. The battle calls these hooks (cheats_battle.h); each
// returns the game's own behaviour unless a cheat is on (Port_CheatPlayers).
//
// A cheat is set per player (controller port). The player of an active Pokémon is the trainer who brought it: the
// tag-team member Battle_InitPokemonRuntimeState takes it from (side->teamMember[state.menuTeamMemberIndex]), and only
// a human trainer (isHuman 1): the COM never gets a player's cheats.
//
// Everything here changes the running battle only: the active Pokémon's battle state (state.runtime) and, for HP and
// status, its entry in the battle's roster copy (teamRoster->pokemonPicked), which the HUD follows. Neither is the
// N64 save or a Game Boy cartridge.
#include <stdio.h>

#include "global.h"
#include "fragments/battle/battle.h"
#include "gb_data.h"
#include "game_state.h"

#include "cheats.h"
#include "cheats_battle.h"

void Battle_ApplyParalysisSpeedCut(ActivePokemon* arg0);
void Battle_ApplyBurnAttackCut(ActivePokemon* arg0);

// one line on stderr each time a cheat changes what happens (test runs check them; a battle prints a handful)
#define CHEAT_LOG(...) fprintf(stderr, "[cheats] " __VA_ARGS__)

static s32 Cheat_Side(ActivePokemon* ap) {
    return ap == sActivePokemon[1];
}

static TeamRoster* Cheat_Owner(ActivePokemon* ap) {
    BattleSide* side;
    u32 member;

    if (ap == NULL || ap->side == NULL) {
        return NULL;
    }
    side = ap->side;
    member = ap->state.menuTeamMemberIndex;
    if (member >= 2 || member >= side->tagTeamSize) {
        member = 0;
    }
    return side->teamMember[member];
}

// The scripted battles (the opening, the title screen's demo battle, mode 6) set both sides up as human trainers
// (Session_InitTwoTrainers with modes 5, 3 and 6): they are not a player's, so they get no cheats.
static s32 Cheat_ScriptedBattle(void) {
    return gSessionContext.mode == 3 || gSessionContext.mode == 5 || gSessionContext.mode == 6;
}

static s32 Cheat_IsPlayer(TeamRoster* trainer) {
    return trainer != NULL && trainer->isHuman == 1 && !Cheat_ScriptedBattle();
}

// the cheat is on for the trainer who brought this active Pokémon
static s32 Cheat_OnFor(s32 cheat, ActivePokemon* ap) {
    s32 players = Port_CheatPlayers(cheat);
    TeamRoster* owner;

    if (players == 0) {
        return 0;
    }
    owner = Cheat_Owner(ap);
    return Cheat_IsPlayer(owner) && ((players >> (owner->controllerPort & 3)) & 1);
}

// the cheat is on for any player on this side (the opponent's effects: who they battle may be either tag partner)
static s32 Cheat_OnForSide(s32 cheat, ActivePokemon* ap) {
    s32 players = Port_CheatPlayers(cheat);
    s32 i;

    if (players == 0 || ap == NULL || ap->side == NULL) {
        return 0;
    }
    for (i = 0; i < 2 && i < ap->side->tagTeamSize; i++) {
        TeamRoster* trainer = ap->side->teamMember[i];
        if (Cheat_IsPlayer(trainer) && ((players >> (trainer->controllerPort & 3)) & 1)) {
            return 1;
        }
    }
    return 0;
}

// the active Pokémon whose battle state this is; NULL for the COM AI's scratch copies, which are left alone
static ActivePokemon* Cheat_Active(BattleMonRuntime* rt) {
    s32 i;

    for (i = 0; i < 2; i++) {
        if (rt != NULL && sActivePokemon[i] != NULL && &sActivePokemon[i]->state.runtime == rt) {
            return sActivePokemon[i];
        }
    }
    return NULL;
}

static ActivePokemon* Cheat_Foe(ActivePokemon* ap) {
    if (sActivePokemon[0] == ap) {
        return sActivePokemon[1];
    }
    if (sActivePokemon[1] == ap) {
        return sActivePokemon[0];
    }
    return NULL;
}

static BattleMon* Cheat_RosterMon(ActivePokemon* ap) {
    TeamRoster* owner = Cheat_Owner(ap);
    s32 slot = ap->state.rosterIndex;

    if (owner == NULL || slot < 0 || slot >= 6) {
        return NULL;
    }
    return &owner->pokemonPicked[slot];
}

void PortCheat_BattleFrame(void) {
    s32 i;

    for (i = 0; i < 2; i++) {
        ActivePokemon* ap = sActivePokemon[i];
        BattleMonRuntime* rt;
        BattleMon* mon;

        if (ap == NULL || ap->side == NULL) {
            continue;
        }
        rt = &ap->state.runtime;
        mon = Cheat_RosterMon(ap);
        if (Cheat_OnFor(PORT_CHEAT_INFINITE_HP, ap) && ap->state.faintState == 0 && rt->hp != 0 &&
            rt->hp < rt->maxHp) {
            // (a Pokémon at 0 HP is fainting: it is not brought back, so Self-Destruct and Explosion still end it)
            CHEAT_LOG("Infinite HP: side %d HP %d -> %d\n", i, rt->hp, rt->maxHp);
            rt->hp = rt->maxHp;
            if (mon != NULL && mon->hp != 0) {
                mon->hp = mon->maxHp;
            }
        }
        if (Cheat_OnFor(PORT_CHEAT_NO_STATUS, ap) &&
            (rt->statusCondition != 0 || (rt->immobilizedReason & 0x80) || (rt->uniqueStatusFlags & 0x01))) {
            CHEAT_LOG("Status always good: side %d status %02X confused %d cured\n", i, rt->statusCondition,
                      (rt->immobilizedReason & 0x80) != 0);
            if (rt->statusCondition & 0x40) {
                // paralysis quartered the battle Speed (Battle_ApplyParalysisSpeedCut): give it back
                rt->speed = rt->speed * 4 > 999 ? 999 : rt->speed * 4;
            }
            if (rt->statusCondition & 0x10) {
                // burn halved the battle Attack (Battle_ApplyBurnAttackCut)
                rt->modifiedAttack = rt->modifiedAttack * 2 > 999 ? 999 : rt->modifiedAttack * 2;
            }
            rt->statusCondition = 0;
            rt->immobilizedReason &= ~0x80; // confused (Gen 1 battle status 1, bit 7)
            rt->confusionTurnsLeft = 0;
            rt->uniqueStatusFlags &= ~0x01; // badly poisoned (Toxic)
            rt->toxicCounter = 0;
            if (mon != NULL) {
                mon->statusCondition = 0;
            }
        }
    }
}

s32 PortCheat_ShieldDamage(BattleMonRuntime* target) {
    ActivePokemon* ap = Cheat_Active(target);

    if (!Cheat_OnFor(PORT_CHEAT_INFINITE_HP, ap)) {
        return 0;
    }
    CHEAT_LOG("Infinite HP: side %d took no damage (HP %d/%d)\n", Cheat_Side(ap), target->hp, target->maxHp);
    return 1;
}

s32 PortCheat_KeepPp(s32 side) {
    if (side < 0 || side >= 2 || !Cheat_OnFor(PORT_CHEAT_INFINITE_PP, sActivePokemon[side])) {
        return 0;
    }
    CHEAT_LOG("Infinite PP: side %d spent no PP\n", side);
    return 1;
}

s32 PortCheat_ForceMiss(BattleMonRuntime* user, BattleMonRuntime* target) {
    ActivePokemon* t = Cheat_Active(target);

    // only a move aimed at the other side (not a Pokémon's own, which have no target here)
    if (t == NULL || Cheat_Active(user) != Cheat_Foe(t) || !Cheat_OnFor(PORT_CHEAT_FOE_NEVER_HITS, t)) {
        return 0;
    }
    CHEAT_LOG("Opponent never hits: side %d's move missed\n", !Cheat_Side(t));
    return 1;
}

s32 PortCheat_SureHit(BattleMonRuntime* user) {
    ActivePokemon* ap = Cheat_Active(user);

    if (!Cheat_OnFor(PORT_CHEAT_NEVER_MISS, ap)) {
        return 0;
    }
    CHEAT_LOG("Your moves never miss: side %d skipped the accuracy roll\n", Cheat_Side(ap));
    return 1;
}

s32 PortCheat_MoveOrder(s32 first_side) {
    s32 first0 = Cheat_OnFor(PORT_CHEAT_MOVE_FIRST, sActivePokemon[0]);
    s32 first1 = Cheat_OnFor(PORT_CHEAT_MOVE_FIRST, sActivePokemon[1]);

    s32 side = first_side;

    if (first0 && !first1) {
        side = 0;
    } else if (first1 && !first0) {
        side = 1;
    }
    if (first0 || first1) {
        CHEAT_LOG("Always your turn first: side %d acts first (the game picked %d)\n", side, first_side);
    }
    return side;
}

// Gen 1 stat formula (pokered CalcStat): ((base + DV) * 2 + ceil(sqrt(stat exp)) / 4) * level / 100 + 5, the square
// root capped at 255. Perfect: DV 15, stat exp 65535 (63); minimum: DV 0, stat exp 0.
static u16 Cheat_Stat(u32 base, s32 perfect, u32 level) {
    u32 v = ((base + (perfect ? 15 : 0)) * 2 + (perfect ? 63 : 0)) * level / 100 + 5;

    return v > 999 ? 999 : (u16)v;
}

void PortCheat_InitRuntime(ActivePokemon* ap) {
    BattleMonRuntime* rt;
    SpeciesData* base;
    s32 perfect;
    s32 minimum;

    if (ap == NULL) {
        return;
    }
    rt = &ap->state.runtime;
    if (rt->species < 1 || rt->species > 151) {
        return;
    }
    perfect = Cheat_OnFor(PORT_CHEAT_PERFECT_STATS, ap);
    minimum = Cheat_OnForSide(PORT_CHEAT_FOE_MIN_STATS, Cheat_Foe(ap)) && Cheat_Owner(ap) != NULL;
    if (perfect == minimum) { // neither, or both (a player ticked both against each other: leave the game's stats)
        return;
    }
    base = &gSpeciesData[rt->species - 1];
    rt->unmodifiedAttack = rt->modifiedAttack = Cheat_Stat(base->baseAttack, perfect, rt->level);
    rt->unmodifiedDefense = rt->modifiedDefense = Cheat_Stat(base->baseDefense, perfect, rt->level);
    rt->unmodifiedSpeed = rt->speed = Cheat_Stat(base->baseSpeed, perfect, rt->level);
    rt->unmodifiedSpecial = rt->modifiedSpecial = Cheat_Stat(base->baseSpecial, perfect, rt->level);
    Battle_ApplyParalysisSpeedCut(ap);
    Battle_ApplyBurnAttackCut(ap);
    CHEAT_LOG("%s: mode %d side %d species %d L%d Atk %d Def %d Spd %d Spc %d\n",
              perfect ? "Perfect stats" : "Opponent minimum stats", gSessionContext.mode, Cheat_Side(ap), rt->species, rt->level, rt->unmodifiedAttack, rt->unmodifiedDefense, rt->unmodifiedSpeed,
              rt->unmodifiedSpecial);
}
