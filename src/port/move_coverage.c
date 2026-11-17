// Port-only support for tools/move_coverage (every battle Pokémon using every move it can have, run on the port and
// on ares): compiled with the game code (it reads the battle structs) but kept in the port, so game/ holds only decomp
// code.
//
// PORT_DUMP_MOVESETS=<file>: after the resident data and the battle engine's data are lifted from the ROM, writes for
// every species of the Free Battle table (D_80064C5C, flag bit 0) its base data and each move Stadium Zero's own
// legality check (Battle_CheckMovesetLegality: start moves, level-up moves, TM/HM, and those of its pre-evolutions)
// accepts, with the lowest level that accepts it, then exits. Nothing of the ROM goes into the repository: the
// generator reads this file from the user's ROM at run time.
//
// The battle planner (PORT_PLANNER_IN / PORT_PLANNER_OUT, src/port/os_devices.cpp) gets the battle state from
// Port_MoveCoverageState below at each command phase.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "global.h"
#include "fragments/battle/battle.h"
#include "gb_data.h"
#include "zero_lift.h"

void Battle_CheckMovesetLegality(ActivePokemon* arg0);

static int LiftImageIndex(const char* name) {
    for (uint32_t i = 0; i < gZLiftImageCount; i++) {
        if (strcmp(gZLiftImages[i].name, name) == 0) {
            return (int)i;
        }
    }
    return -1;
}

static int IsLegal(ActivePokemon* ap, int dex, int move, int level) {
    memset(&ap->state, 0, sizeof(ap->state));
    ap->state.runtime.species = (u8)dex;
    ap->state.runtime.level = (u8)level;
    ap->state.runtime.moves[0] = (u8)move;
    Battle_CheckMovesetLegality(ap);
    return ap->state.moveSlotIllegality[0] == 0;
}

void Port_DumpMovesets(const char* path) {
    int battle = LiftImageIndex("fragment012"); // the battle engine (its learnset tables)
    if (battle < 0) {
        fprintf(stderr, "[movesets] no battle engine image\n");
        exit(1);
    }
    ZLift_LoadImage(battle);
    FILE* f = fopen(path, "w");
    if (f == NULL) {
        fprintf(stderr, "[movesets] cannot write %s\n", path);
        exit(1);
    }
    ActivePokemon* ap = calloc(1, sizeof(ActivePokemon));
    fprintf(f, "# Stadium Zero battle species and the moves its legality check accepts (PORT_DUMP_MOVESETS)\n");
    fprintf(f, "# M <move> <effect> <power> <type> <accuracy> <pp>\n");
    for (int m = 1; m <= 165; m++) {
        MoveData* md = &gMoveData[m - 1];
        fprintf(f, "M %d %d %d %d %d %d\n", m, md->effect, md->power, md->type, md->accuracy, md->pp);
    }
    fprintf(f, "# S <dex> <gen1 index> <flags> <hp> <atk> <def> <spd> <spc> <type1> <type2> <catch rate> <growth>\n");
    fprintf(f, "# L <dex> <move> <lowest legal level>\n");
    for (int dex = 1; dex <= 151; dex++) {
        u8 flags = D_80064C5C[dex - 1].cupEligibilityFlags;
        if (!(flags & 1)) {
            continue;
        }
        SpeciesData* sd = &gSpeciesData[dex - 1];
        fprintf(f, "S %d %d %d %d %d %d %d %d %d %d %d %d\n", dex, Species_ToInternalIndex(dex), flags, sd->baseHitPoints,
                sd->baseAttack, sd->baseDefense, sd->baseSpeed, sd->baseSpecial, sd->type1, sd->type2, sd->catchRate,
                sd->growthRate);
        for (int m = 1; m <= 165; m++) {
            for (int level = 1; level <= 100; level++) {
                if (IsLegal(ap, dex, m, level)) {
                    fprintf(f, "L %d %d %d\n", dex, m, level);
                    break;
                }
            }
        }
    }
    free(ap);
    fclose(f);
    fprintf(stderr, "[movesets] written to %s\n", path);
}

static int Side(ActivePokemon* ap) {
    return ap == sSide1ActivePokemon ? 0 : 1;
}

// one line per side: "S <side> <field>=<value> ...", then "R <side> <slot> <species> <hp> <status>" per party member
int Port_MoveCoverageState(char* out, int size) {
    int n = 0;
    ActivePokemon* sides[2] = { sSide1ActivePokemon, sSide2ActivePokemon };
    for (int s = 0; s < 2; s++) {
        ActivePokemon* ap = sides[s];
        if (ap == NULL) {
            continue;
        }
        BattleMonRuntime* rt = &ap->state.runtime;
        n += snprintf(out + n, size - n,
                      "S %d sp=%d hp=%d mhp=%d st=%d vol=%d imm=%d uniq=%d conf=%d sub=%d m=%d,%d,%d,%d pp=%d,%d,%d,%d "
                      "ri=%d faint=%d opt=%d cmd=%d t=%d,%d lv=%d tr=%d disabled=%d\n",
                      s, rt->species, rt->hp, rt->maxHp, rt->statusCondition, rt->volatileStatusFlags,
                      rt->immobilizedReason, rt->uniqueStatusFlags, rt->confusionTurnsLeft, rt->substituteHp,
                      rt->moves[0], rt->moves[1], rt->moves[2], rt->moves[3], rt->pp[0], rt->pp[1], rt->pp[2],
                      rt->pp[3], ap->state.rosterIndex, ap->state.faintState, ap->state.selectedCommandOption,
                      ap->state.selectedCommand, rt->type1, rt->type2, rt->level, ap->state.transformSpecies,
                      rt->disabledMove);
        if (ap->teamRoster != NULL) {
            for (int i = 0; i < 6 && i < ap->teamRoster->numberOfPokemonBrought; i++) {
                BattleMon* mon = &ap->teamRoster->pokemonPicked[i];
                n += snprintf(out + n, size - n, "R %d %d %d %d %d\n", s, i, mon->species.id, mon->hp,
                              mon->statusCondition);
            }
        }
        if (n >= size) {
            return size - 1;
        }
    }
    return n;
}

void Port_PlannerEvent(const char* kind, int side, int move); // (os_devices.cpp)

// a move animation starts (BattleAnim_PlayMoveEffectScript / BattleAnim_PlayChargingMoveEffectScript)
void Port_SyncEvent(const char* name);

void Port_MoveCoverageAnim(ActivePokemon* user, s32 move, int charging) {
    if (Side(user) == 0) {
        Port_SyncEvent("anim1"); // tools/screen_diff sync:anim1 (tools/ares_ref hooks the same two functions)
    }
    Port_PlannerEvent(charging ? "charge" : "anim", Side(user), move);
}
