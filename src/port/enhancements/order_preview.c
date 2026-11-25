// The side summary bars on the order screen (hippodrome-utils docs/specs/PARTY_SUMMARY_BARS.md), the screen before a
// battle where each trainer picks which Pokémon to bring (battle_order_select.c). The same cards as in the battle
// (summary_bars.cpp draws them; card_models.c their models), filled from the two parties the screen shows:
//
// - The viewer's own side (1P against the COM) shows everything, as in battle. Its picks light up as they are made;
//   the others keep the Unknown look until the picks are confirmed, then turn Not Picked.
// - The other side shows what the battle would start with: no moves, the species' speed range, and, with trainer
//   memory on, what the save remembers of this trainer (the same reveal as at the battle's start). Its picks stay
//   hidden (Unknown), as the COM's are on the N64.
// - Two human players: neither side's moves or picks are shown (moves only appear as they are used).
//
// Nothing here changes the game: it reads the screen's panels each frame (PortEnh_OrderFrame) and publishes a view.
// No bars for the scripted battles (the title demo, the opening) or when the bars are off.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "global.h"
#include "fragments/battle_order_select/battle_order_select.h"
#include "gb_data.h"
#include "game_state.h"
#include "variables.h"

#include "text_lookup.h"
#include "enhancements.h"
#include "battle_enhancements.h"
#include "zero_trainer_memory.h"
#include "card_models.h"

#include <hippodrome/summary_card.h>
#include <hippodrome/trainer_memory.h>

extern SpeciesInfo D_80064C5C[];

static hu_side sSides[2];
static const OrderSelectPanel* sPanel[2]; // the panel each side's cards were made from
static s32 sViewer = -1;
static s32 sModels;

static s32 IsCom(const TeamRoster* r) {
    return (r->isHuman & 2) != 0; // the COM's roster has isHuman 2 or 3
}

// The scripted battles (battle_enhancements.c, BattleIsDemo): no bars.
static s32 SessionIsDemo(void) {
    u8 m = gSessionContext.mode;
    return m == 3 || m == 5 || m == 6 || m >= 16;
}

static const char* MoveName(u16 move) {
    if (move < 1 || move > 165) return "";
    return Port_Text_Display(D_8006A9D4[move - 1].name);
}

// The side's first panel (a tag team has two; Zero's played modes have one per side).
static const OrderSelectPanel* SidePanel(const OrderSelectPanel* panels, s32 count, s32 side) {
    s32 i;
    for (i = 0; i < count; i++) {
        if (panels[i].sidebarTeam == side && panels[i].roster != NULL && panels[i].candidates != NULL) {
            return &panels[i];
        }
    }
    return NULL;
}

// What trainer memory knows of the opponent (as the battle's InitMemory reveals it at the start).
static void ApplyMemory(s32 hidden) {
    hu_tm_key key;
    const hu_tm_trainer* t;
    s32 i, k;

    if (!Port_Enh_TrainerMemoryEnabled() || !ZeroTm_KeyForSession(&gSessionContext, &key)) {
        return;
    }
    t = hu_tm_find(Port_Enh_Memory(), key);
    for (i = 0; i < sSides[hidden].count; i++) {
        hu_card* c = &sSides[hidden].cards[i];
        const hu_tm_mon* m = hu_tm_find_mon(t, c->species);
        if (m == NULL) continue;
        if (m->flags & HU_TM_MON_HAS_SPEED) { // taken to this battle's Stat Exp clamp first
            u16 lo, hi;
            u8 sp = c->species;
            PortEnh_OpponentStatExpBounds(1, &lo, &hi);
            hu_speed_range stored;
            if (hu_tm_mon_speed(m, (sp >= 1 && sp <= 151) ? gSpeciesData[sp - 1].baseSpeed : 0, lo, hi, &stored)) {
                c->speed = hu_speed_intersect(c->speed, stored);
            }
            c->transform_speed = c->speed;
        }
        for (k = 0; k < 4; k++) {
            if (hu_tm_move_seen(m, c->moves[k])) c->revealed |= 1 << k;
        }
    }
}

static void InitSide(s32 s, const OrderSelectPanel* p) {
    s32 count = p->candidateCount;
    s32 i, j;

    if (count <= 0 || count > HU_SIDE_MAX) count = 0;
    sPanel[s] = p;
    hu_side_init(&sSides[s], count, count, s == sViewer ? HU_REVEAL_ALL : HU_REVEAL_AS_USED);
    for (i = 0; i < count; i++) {
        BattleMon* m = &p->candidates[i];
        u8 sp = m->species.id;
        u16 moves[4];
        for (j = 0; j < 4; j++) moves[j] = m->moves[j];
        hu_side_set_mon(&sSides[s], i, sp, m->level, m->hp, m->maxHp, moves,
                        (sp >= 1 && sp <= 151) ? gSpeciesData[sp - 1].baseSpeed : 0, s == sViewer ? m->speed : 0);
        if (s != sViewer) { // Stat Exp difficulty: what the opponent's stat exp can be in this battle
            u16 lo, hi;
            PortEnh_OpponentStatExpBounds(1, &lo, &hi);
            if (lo != 0 || hi != 65535) {
                sSides[s].cards[i].speed = hu_speed_initial_se(
                    (sp >= 1 && sp <= 151) ? gSpeciesData[sp - 1].baseSpeed : 0, m->level, lo, hi);
                sSides[s].cards[i].transform_speed = sSides[s].cards[i].speed;
            }
        }
        if (sModels) {
            PortEnh_CardModelsLoad(s, i, m);
        }
    }
}

// The viewer's picks as they are made: picked cards are Picked, the rest Unknown until the picks are confirmed.
static void UpdatePicks(s32 s) {
    const OrderSelectPanel* p = sPanel[s];
    hu_side* side = &sSides[s];
    u8 mask = 0;
    s32 i;

    for (i = 0; i < p->pickCount && i < 6; i++) {
        if (p->pickedSlots[i] >= 0 && p->pickedSlots[i] < side->count) mask |= 1 << p->pickedSlots[i];
    }
    if (p->state != 0 && mask != 0) { // the confirm prompt or ready: the picks are made
        hu_side_set_picked(side, mask);
        return;
    }
    for (i = 0; i < side->count; i++) {
        hu_card* c = &side->cards[i];
        if (mask & (1 << i)) {
            c->flags |= HU_CARD_F_PICK_KNOWN | HU_CARD_F_PICKED;
        } else {
            c->flags &= (u8) ~(HU_CARD_F_PICK_KNOWN | HU_CARD_F_PICKED);
        }
    }
}

static void Publish(void) {
    PortEnhBattleView v;
    s32 s, i, k;
    memset(&v, 0, sizeof(v));
    v.viewer = sViewer;
    for (s = 0; s < 2; s++) {
        v.sides[s] = sSides[s];
        for (i = 0; i < sSides[s].count; i++) {
            const hu_card* c = &sSides[s].cards[i];
            v.names[s][i] = Port_Text_Display((const char*)sPanel[s]->candidates[i].nickname);
            for (k = 0; k < 4; k++) {
                v.moves[s][i][k] = MoveName(c->moves[k]);
                v.moveTypes[s][i][k] = PortEnh_MoveHuType(c->moves[k]);
            }
            v.speed[s][i] = hu_card_shown_speed(c);
            v.modelReady[s][i] = (u8)PortEnh_CardModelReady(s, i);
            v.modelSpecies[s][i] = (u8)PortEnh_CardModelSpecies(s, i);
        }
    }
    Port_Enh_PublishBattle(&v);
}

void PortEnh_OrderFrame(OrderSelectPanel* panels, s32 count) {
    const OrderSelectPanel* p0;
    const OrderSelectPanel* p1;
    s32 s;

    if (!Port_Enh_SummaryBarsEnabled() || SessionIsDemo() || panels == NULL) {
        sPanel[0] = sPanel[1] = NULL;
        return;
    }
    p0 = SidePanel(panels, count, 0);
    p1 = SidePanel(panels, count, 1);
    if (p0 == NULL || p1 == NULL) {
        return;
    }
    if (p0 != sPanel[0] || p1 != sPanel[1] || p0->candidates != sPanel[0]->candidates ||
        p1->candidates != sPanel[1]->candidates) {
        s32 com0 = IsCom(p0->roster), com1 = IsCom(p1->roster);
        sViewer = (!com0 && com1) ? 0 : (com0 && !com1) ? 1 : -1;
        sModels = 1;
        PortEnh_CardModelsFree();
        InitSide(0, p0);
        InitSide(1, p1);
        if (sViewer >= 0) {
            ApplyMemory(!sViewer);
        }
    }
    if (sViewer >= 0) {
        UpdatePicks(sViewer);
    }
    for (s = 0; s < 2; s++) {
        hu_side_tick(&sSides[s]);
    }
    Publish();
    {
        PortEnhCardBattle none[2];
        memset(none, 0, sizeof(none));
        none[0].active = none[1].active = -1;
        none[0].faintAnim = none[1].faintAnim = -1;
        none[0].sleepAnim = none[1].sleepAnim = -1;
        PortEnh_CardModelsDraw(sSides, none);
    }
}

void PortEnh_OrderEnd(void) {
    // the models were made from this screen's model archive; the battle loads its own
    if (sPanel[0] != NULL || sModels) {
        PortEnh_CardModelsFree();
    }
    sPanel[0] = sPanel[1] = NULL;
    sModels = 0;
}
