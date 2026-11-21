// Side summary bars and speed estimates in Stadium Zero's battles (hippodrome-utils: docs/specs/PARTY_SUMMARY_BARS.md,
// SPEED_STAT_ESTIMATE.md), and what a tournament battle teaches the trainer memory (TRAINER_MEMORY.md).
//
// Compiled with the game code, since it reads the battle structs, but kept in the port, so game/ holds only decomp
// code. The battle calls four hooks (battle_enhancements.h): PortEnh_BattleBegin when it allocates its participants,
// PortEnh_TurnOrder once Battle_StartTurn knows who acts first, PortEnh_MoveStart when a move's presentation starts
// (after its "X used Y" message) and PortEnh_BattleFrame each frame it draws its HUD. Everything else is read from
// the battle's own state each frame, so nothing the game does changes.
//
// The cards are drawn outside the game's picture, in the window's room beside it (summary_bars.cpp): 1P's party on
// the left, the opponent's on the right. This file publishes what they show each frame (Port_Enh_PublishBattle).
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "global.h"
#include "fragments/battle/battle.h"
#include "gb_data.h"
#include "game_state.h"
#include "moves.h"
#include "variables.h"

#include "text_lookup.h"
#include "enhancements.h"
#include "battle_enhancements.h"
#include "zero_trainer_memory.h"

#include <hippodrome/summary_card.h>
#include <hippodrome/trainer_memory.h>

extern SpeciesInfo D_80064C5C[];


static hu_side sSides[2];
static s8 sCardOfPicked[2][6]; // pokemonPicked index -> card (party) index
static TeamRoster* sRosters[2];
static s32 sNeedInit;
static s32 sReady;
static s32 sViewer = -1;   // the side whose own Pokémon are fully known (1P against the COM), -1 in multiplayer
static s32 sPendingSlot[2]; // the move slot each side committed this turn, revealed when the move is presented
static s32 sSaved;
static s32 sHasKey;
static hu_tm_key sKey;
static s32 sTransformed[2];

// PORT_ENH_LOG=1: what the enhancements learn, on stderr (tests)
static void EnhLog(const char* fmt, ...) {
    static s32 on = -1;
    va_list ap;
    if (on < 0) on = getenv("PORT_ENH_LOG") != NULL;
    if (!on) return;
    va_start(ap, fmt);
    fprintf(stderr, "[enh] ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
}

static s32 IsCom(const TeamRoster* r) {
    return (r->isHuman & 2) != 0; // the COM's roster has isHuman 2 or 3
}

static s32 BattleIsDemo(void) {
    return gSessionContext.mode >= SESSION_MODE_ATTRACT_DEMO_BATTLE;
}

void PortEnh_BattleBegin(void) {
    sNeedInit = 1;
    sReady = 0;
    sSaved = 0;
}

// Same Pokémon: the fields a battle does not change.
static s32 SameMon(const BattleMon* a, const BattleMon* b) {
    return a->species.id == b->species.id && a->otId == b->otId && a->dvs == b->dvs && a->exp == b->exp &&
           memcmp(a->nickname, b->nickname, sizeof(a->nickname)) == 0 && a->level == b->level;
}

static hu_status StatusOf(u8 condition, u8 toxic) {
    if (condition & 0x07) return HU_STATUS_SLEEP;
    if (condition & 0x20) return HU_STATUS_FREEZE;
    if (condition & 0x10) return HU_STATUS_BURN;
    if (condition & 0x40) return HU_STATUS_PARALYSIS;
    if (condition & 0x08) return toxic ? HU_STATUS_TOXIC : HU_STATUS_POISON;
    return HU_STATUS_NONE;
}

static u8 BaseSpeed(u8 species) {
    return (species >= 1 && species <= 151) ? gSpeciesData[species - 1].baseSpeed : 0;
}

static void InitSide(s32 s) {
    TeamRoster* r = sActivePokemon[s]->teamRoster;
    ExtendedRosterInfo* info = r->rosterInfo;
    BattleMon* party = (info != NULL) ? info->battleMon : r->pokemonPicked;
    s32 count = (info != NULL) ? info->numberOfPokemonCanBePicked : r->numberOfPokemonBrought;
    s32 brought = r->numberOfPokemonBrought;
    u8 used = 0;
    u8 picked_mask = 0;
    s32 i, j;

    if (count <= 0 || count > 6) count = brought;
    if (brought <= 0 || brought > count) brought = count;
    sRosters[s] = r;
    hu_side_init(&sSides[s], count, brought, s == sViewer ? HU_REVEAL_ALL : HU_REVEAL_AS_USED);
    for (i = 0; i < count; i++) {
        const BattleMon* m = &party[i];
        u16 moves[4];
        for (j = 0; j < 4; j++) moves[j] = m->moves[j];
        hu_side_set_mon(&sSides[s], i, m->species.id, m->level, m->hp, m->maxHp, moves, BaseSpeed(m->species.id),
                        s == sViewer ? m->speed : 0);
    }
    // which party Pokémon each brought one is
    for (i = 0; i < 6; i++) sCardOfPicked[s][i] = -1;
    for (i = 0; i < brought; i++) {
        for (j = 0; j < count; j++) {
            if (!(used & (1 << j)) && SameMon(&r->pokemonPicked[i], &party[j])) break;
        }
        if (j == count) { // (not expected) fall back to the species
            for (j = 0; j < count; j++) {
                if (!(used & (1 << j)) && r->pokemonPicked[i].species.id == party[j].species.id) break;
            }
        }
        if (j < count) {
            used |= 1 << j;
            sCardOfPicked[s][i] = (s8)j;
            picked_mask |= 1 << j;
        }
    }
    if (s == sViewer) {
        hu_side_set_picked(&sSides[s], picked_mask);
    }
}

// The trainer memory of the opponent's side, and what the save already knows about its speeds.
static void InitMemory(void) {
    s32 hidden = (sViewer == 0) ? 1 : -1;
    const hu_tm_trainer* t;
    s32 i;

    sHasKey = (hidden >= 0) && ZeroTm_KeyForSession(&gSessionContext, &sKey);
    if (!sHasKey) {
        return;
    }
    {
        TeamRoster* r = sRosters[hidden];
        ExtendedRosterInfo* info = r->rosterInfo;
        BattleMon* party = (info != NULL) ? info->battleMon : r->pokemonPicked;
        hu_tm_team_entry team[6];
        s32 n = sSides[hidden].count;
        for (i = 0; i < n; i++) {
            s32 k;
            team[i].species = party[i].species.id;
            team[i].level = party[i].level;
            for (k = 0; k < 4; k++) team[i].moves[k] = party[i].moves[k];
        }
        // reaching the battle means the VS screen showed this trainer and its team
        hu_tm_record_team(Port_Enh_Memory(), sKey, team, n);
        ZeroTm_RecordName(Port_Enh_Memory(), sKey, (const char*)info->fullName);
    }
    if (!Port_Enh_TrainerMemoryEnabled()) {
        return; // the memory is still kept, but this battle starts from scratch
    }
    t = hu_tm_find(Port_Enh_Memory(), sKey);
    for (i = 0; i < sSides[hidden].count; i++) {
        hu_card* c = &sSides[hidden].cards[i];
        const hu_tm_mon* m = hu_tm_find_mon(t, c->species);
        s32 k;
        if (m == NULL) continue;
        if (m->flags & HU_TM_MON_HAS_SPEED) {
            c->speed = hu_speed_intersect(c->speed, m->speed);
            c->transform_speed = c->speed;
        }
        for (k = 0; k < 4; k++) {
            if (hu_tm_move_seen(m, c->moves[k])) c->revealed |= 1 << k;
        }
    }
}

static void Init(void) {
    s32 com0 = IsCom(sActivePokemon[0]->teamRoster), com1 = IsCom(sActivePokemon[1]->teamRoster);

    sViewer = (!com0 && com1) ? 0 : (com0 && !com1) ? 1 : -1;
    InitSide(0);
    InitSide(1);
    sPendingSlot[0] = sPendingSlot[1] = -1;
    sTransformed[0] = sTransformed[1] = 0;
    InitMemory();
    if (sHasKey) {
        Port_Enh_SaveMemory(); // the team and name (writes only when something changed)
    }
    EnhLog("battle: viewer %d, sides %d/%d (brought %d/%d), memory key %s", (int)sViewer, sSides[0].count,
           sSides[1].count, sSides[0].brought, sSides[1].brought, sHasKey ? "yes" : "no");
    if (sHasKey) {
        EnhLog("memory key: cup %d division %d round %d trainer %d", sKey.cup, sKey.division, sKey.round, sKey.trainer);
    }
    sNeedInit = 0;
    sReady = 1;
}

static s32 ActiveCard(s32 s) {
    s32 p = sActivePokemon[s]->state.rosterIndex;
    return (p >= 0 && p < 6) ? sCardOfPicked[s][p] : -1;
}

// How many times the paralysis cut has been applied to the active Pokémon's speed since it was last recomputed: the
// difference between its stage-applied raw stat and its live speed. Every cut happens on screen (paralysis, then each
// stat change of the opponent: Gen 1's quirk), so this is information the player has.
static hu_speed_mods ModsOf(const BattleMonRuntime* rt) {
    hu_speed_mods m = hu_speed_mods_none();
    u16 v;
    m.stage = (s8)(rt->statStages[2] - 7);
    if (rt->statusCondition & 0x40) {
        v = hu_apply_stage(&hu_gb_speed_rules, rt->unmodifiedSpeed, m.stage);
        while (v > rt->speed && m.paralysis_cuts < 8) {
            v = v >> 2;
            if (v == 0) v = 1;
            m.paralysis_cuts++;
        }
    }
    return m;
}

static void Poll(void) {
    s32 s, p;
    for (s = 0; s < 2; s++) {
        TeamRoster* r = sRosters[s];
        hu_side* side = &sSides[s];
        s32 active = ActiveCard(s);
        const BattleMonRuntime* rt = &sActivePokemon[s]->state.runtime;

        if (active >= 0 && !(side->cards[active].flags & HU_CARD_F_ACTIVE)) {
            hu_side_send_out(side, active);
            sTransformed[s] = 0;
            if (sHasKey && s != sViewer) {
                hu_tm_record_sent_out(Port_Enh_Memory(), sKey, side->cards[active].species, side->cards[active].level);
            }
        }
        for (p = 0; p < r->numberOfPokemonBrought && p < 6; p++) {
            s32 c = sCardOfPicked[s][p];
            const BattleMon* bm = &r->pokemonPicked[p];
            u16 hp;
            hu_status st;
            if (c < 0) continue;
            if (c == active) {
                hp = rt->hp;
                st = StatusOf(rt->statusCondition, rt->toxicCounter != 0);
            } else {
                hp = bm->hp;
                st = StatusOf(bm->statusCondition, 0);
            }
            hu_side_set_hp(side, c, hp);
            hu_side_set_status(side, c, st);
            if (hp == 0) {
                hu_side_faint(side, c);
            }
        }
        if (active >= 0) {
            s32 transformed = (rt->uniqueStatusFlags & 8) != 0;
            hu_side_set_substitute(side, active, rt->substituteHp != 0);
            if (transformed && !sTransformed[s]) {
                // the copied Speed: the viewer knows its own exactly; the opponent's is its estimate
                s32 o = !s;
                s32 oc = ActiveCard(o);
                hu_speed_range copied = (oc >= 0) ? hu_card_shown_speed(&sSides[o].cards[oc]) : side->cards[active].speed;
                if (oc >= 0 && o != sViewer) copied = sSides[o].cards[oc].speed;
                hu_side_transform(side, active, (u8)sActivePokemon[s]->state.transformSpecies, copied);
            } else if (!transformed && sTransformed[s]) {
                hu_side_transform(side, active, side->cards[active].species, side->cards[active].speed);
            }
            sTransformed[s] = transformed;
        }
        hu_side_tick(side);
    }
}

// Zero's priority brackets (Battle_DetermineMoveOrder): Quick Attack first, Counter last, everything else by Speed.
static s32 MovePriority(u8 move) {
    if (move == QUICK_ATTACK) return 1;
    if (move == COUNTER) return -1;
    return 0;
}

void PortEnh_TurnOrder(s32 first_side) {
    s32 s;
    if (!sReady) return;
    for (s = 0; s < 2; s++) {
        const ActivePokemonState* st = &sActivePokemon[s]->state;
        s32 slot = st->selectedMoveIndex;
        sPendingSlot[s] = -1;
        if (slot >= 0 && slot < 4 && st->runtime.usedMove != 0 && st->runtime.usedMove != 0xFF &&
            st->runtime.moves[slot] == st->runtime.usedMove && !(st->runtime.uniqueStatusFlags & 8)) {
            sPendingSlot[s] = slot;
        }
    }
    EnhLog("turn: side %d first (moves %u / %u, speed %u / %u)", (int)first_side,
           sActivePokemon[0]->state.runtime.usedMove, sActivePokemon[1]->state.runtime.usedMove,
           sActivePokemon[0]->state.runtime.speed, sActivePokemon[1]->state.runtime.speed);
    // the speed estimate: only against the COM, when Speed decided the order
    if (sViewer >= 0) {
        s32 h = !sViewer;
        const BattleMonRuntime* mine = &sActivePokemon[sViewer]->state.runtime;
        const BattleMonRuntime* theirs = &sActivePokemon[h]->state.runtime;
        s32 c = ActiveCard(h);
        if (c >= 0 && mine->usedMove != 0 && mine->usedMove != 0xFF && theirs->usedMove != 0 &&
            theirs->usedMove != 0xFF && MovePriority(mine->usedMove) == MovePriority(theirs->usedMove) &&
            !(mine->uniqueStatusFlags & 8) && !(theirs->uniqueStatusFlags & 8)) {
            hu_card* card = &sSides[h].cards[c];
            if (hu_speed_observe(&card->speed, first_side == h ? HU_MOVED_FIRST : HU_MOVED_SECOND, mine->speed,
                                 &hu_gb_speed_rules, ModsOf(theirs))) {
                EnhLog("speed estimate: side %d card %d now %u-%u", (int)h, (int)c, card->speed.min, card->speed.max);
                if (sHasKey) {
                    hu_tm_record_speed(Port_Enh_Memory(), sKey, card->species, card->speed);
                }
            }
        }
    }
    if (sHasKey) {
        Port_Enh_SaveMemory(); // last turn's moves and speed (a battle can end by surrender or a reset)
    }
}

void PortEnh_MoveStart(void) {
    s32 s;
    s32 c;
    s32 slot;
    if (!sReady) return;
    s = sBattleScene.scene->actingSide & 1;
    slot = sPendingSlot[s];
    c = ActiveCard(s);
    if (slot < 0 || c < 0) return;
    hu_side_move_used(&sSides[s], c, slot);
    EnhLog("move: side %d card %d slot %d move %u", (int)s, (int)c, (int)slot, sSides[s].cards[c].moves[slot]);
    if (sHasKey && s != sViewer) {
        hu_tm_record_move(Port_Enh_Memory(), sKey, sSides[s].cards[c].species, sSides[s].cards[c].moves[slot]);
    }
    sPendingSlot[s] = -1;
}

// ---- the view the summary bars draw (summary_bars.cpp, on the render thread) ----

static const char* SpeciesName(u8 species) {
    if (species < 1 || species > 151) return "";
    return Port_Text_Display(D_80064C5C[species - 1].name);
}

static const char* MoveName(u16 move) {
    if (move < 1 || move > 165) return "";
    return Port_Text_Display(D_8006A9D4[move - 1].name);
}

static void Publish(void) {
    PortEnhBattleView v;
    s32 s, i, k;
    memset(&v, 0, sizeof(v));
    v.viewer = sViewer;
    for (s = 0; s < 2; s++) {
        const BattleMonRuntime* rt = &sActivePokemon[s]->state.runtime;
        v.sides[s] = sSides[s];
        for (i = 0; i < sSides[s].count; i++) {
            const hu_card* c = &sSides[s].cards[i];
            hu_speed_range sr = hu_card_shown_speed(c);
            v.names[s][i] = SpeciesName(c->species); // the name stays while transformed (the model changes)
            for (k = 0; k < 4; k++) {
                v.moves[s][i][k] = MoveName(c->moves[k]);
            }
            // speed as the turn order compares it now: stage and paralysis applied to the active one
            if (c->flags & HU_CARD_F_ACTIVE) {
                if (s == sViewer) {
                    sr = hu_speed_exact(rt->speed);
                } else if (!(rt->uniqueStatusFlags & 8)) {
                    sr = hu_speed_shown(sr, &hu_gb_speed_rules, ModsOf(rt));
                }
            }
            v.speed[s][i] = sr;
        }
    }
    Port_Enh_PublishBattle(&v);
}

void PortEnh_BattleFrame(void) {
    s32 bars = Port_Enh_SummaryBarsEnabled();
    if (BattleIsDemo() || (!bars && !Port_Enh_TrainerMemoryEnabled())) {
        sReady = 0;
        return;
    }
    if (sNeedInit || !sReady || sRosters[0] != sActivePokemon[0]->teamRoster ||
        sRosters[1] != sActivePokemon[1]->teamRoster) {
        Init();
    }
    Poll();
    if (bars) {
        Publish();
    }
    if (sBattleScene.scene->winner != 0 && !sSaved) {
        Port_Enh_SaveMemory();
        sSaved = 1;
    }
}

