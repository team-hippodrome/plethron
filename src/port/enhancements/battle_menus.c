// Two enhancements on the battle's command menus (hippodrome-utils docs/specs), both off by default. Compiled with the
// game code (they read the battle structs and draw with the battle HUD's own panels).
//
// Mid-battle move description (MIDBATTLE_MOVE_DESCRIPTION): in the fight menu, holding a D-pad direction shows the
// move on the C button of that direction (Up = C-Up, ...): its type, power, accuracy and description, as the てもち
// summary's moves page has them. The window sits under the side's menu bar (or under the move list while R is held).
//
// Order queueing (ORDER_QUEUEING): while the turn plays out, Z opens a copy of the command menu: A the moves, B the
// party, then the game's own pick buttons (moves: the C buttons; party: C-Up / C-Right / C-Down with three brought,
// C-Up C-Left B C-Right C-Down A with six) queue the order, L goes back a step and Z closes it and drops the order.
// When the side's next command menu comes up the order is entered for it on the COM's timing: the menu opens on frame
// 30 and the order is picked on frame 90 by pressing the same buttons the player would, so the game checks it as
// usual. Z in that time takes it back. An order that can no longer be given (the Pokémon fainted, is asleep, frozen
// or locked into a move, the move has no PP left or is disabled, the party member fainted) is dropped.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "global.h"
#include "fragments/battle/battle.h"
#include "game_state.h"
#include "variables.h"

#include "StadionPort.h"
#include "text_lookup.h"
#include "enhancements.h"
#include "battle_enhancements.h"
#include "enh_draw.h"

#include <hippodrome/move_description.h>
#include <hippodrome/order_queue.h>
#include <hippodrome/window_anim.h>

extern f32 D_FLT_82199EB0; // the command phase's frame timer (BattleMenu_TickCommandTimer)
// D_8218B008 / D_8218B014 (battle.h, s8): the menus' gradient colours, 3 bytes per controller port
#define MENU_TOP(port) ((Color_RGB8*)(&D_8218B008 + (port) * 3))
#define MENU_BOTTOM(port) ((Color_RGB8*)(&D_8218B014 + (port) * 3))
void BattleMenu_DrawCommandMenu(ActivePokemon* arg0, s16 arg1, s16 arg2, s32 arg3);
void BattleMenu_DrawMovePanel(ActivePokemon* arg0, s16 arg1, s16 arg2, s32 arg3);
void BattleMenu_DrawSwitchPanel(ActivePokemon* arg0, s16 arg1, s16 arg2, s32 arg3);
void BattleMenu_DrawRosterPanel(TeamRoster* roster, s16 x, s16 y, s32 color_index);
void BattleAnim_PlaySoundEffect1(void); // the battle menus' cursor sound
void BattleAnim_PlaySoundEffect2(void); // their confirm sound
void BattleAnim_PlaySoundEffect3(void); // their back sound
s32 Type_ToTableIndex(s32 type);

typedef struct TypeName { // D_80066AD8: the 16 type names and their colours (as the てもち summary draws them)
    const char* str;
    u8 r, g, b, a;
} TypeName;
extern TypeName D_80066AD8[16];

#define BM_BTN_A 0x8000
#define BM_BTN_B 0x4000
#define BM_BTN_Z 0x2000
#define BM_BTN_DUP 0x0800
#define BM_BTN_DDOWN 0x0400
#define BM_BTN_DLEFT 0x0200
#define BM_BTN_DRIGHT 0x0100
#define BM_BTN_L 0x0020
#define BM_BTN_R 0x0010
#define BM_BTN_CUP 0x0008
#define BM_BTN_CDOWN 0x0004
#define BM_BTN_CLEFT 0x0002
#define BM_BTN_CRIGHT 0x0001

static const u16 kMoveButton[4] = { BM_BTN_CUP, BM_BTN_CRIGHT, BM_BTN_CDOWN, BM_BTN_CLEFT }; // slot -> C button
static const u16 kSwitch3[3] = { BM_BTN_CUP, BM_BTN_CRIGHT, BM_BTN_CDOWN };                  // party slot -> button
static const u16 kSwitch6[6] = { BM_BTN_CUP, BM_BTN_CLEFT, BM_BTN_B, BM_BTN_CRIGHT, BM_BTN_CDOWN, BM_BTN_A };

static u32 sFrame; // counts the battle's HUD frames

// A port string: its translation, else the original language's kana, else English (as the memory screen's).
static const char* Str(const char* id, const char* kana, const char* en) {
    const char* t = Port_Text_LookupId(id);
    if (t != NULL && *t != '\0') return t;
    return (Port_Text_GetLanguage() == PORT_LANG_ORIGINAL) ? kana : en;
}

static s32 SideOf(ActivePokemon* a) {
    return BattleScene_GetParticipantSideIndex(a) != 0;
}

static s32 BattleIsDemo(void) { // the attract demos and the showcase modes (as battle_enhancements.c)
    u8 m = gSessionContext.mode;
    return m == 3 || m == 5 || m == 6 || m >= 16;
}

static s32 IsHumanSide(ActivePokemon* a) {
    return !BattleIsDemo() && a != NULL && a->teamRoster != NULL && !(a->teamRoster->isHuman & 2) && a->side->tagTeamSize == 1U;
}

static s32 ColorIndex(ActivePokemon* a) { // the side's menu colour (its controller port), as the HUD picks it
    return a->side->teamMember[a->thumbnail.shownTeamMemberIndex]->controllerPort;
}

static s32 MoveUsable(ActivePokemon* a, s32 slot) { // a move the C button can pick (BattleMenu_HandleMoveButtons)
    u8 m = a->state.runtime.moves[slot];
    return m != 0 && m < 0xA6;
}

static s32 MoveLegal(ActivePokemon* a, s32 slot) { // and that BattleMenu_TryConfirmMove takes
    u8 flags = a->state.runtime.disabledMoveState;
    if (!MoveUsable(a, slot) || !(a->state.runtime.pp[slot] & 0x3F)) {
        return 0;
    }
    return !((flags & 0xF) && ((flags >> 4) == slot));
}

static s32 SwitchLegal(ActivePokemon* a, s32 slot) { // a party member BattleMenu_HandleSwitchButtons would send in
    TeamRoster* team = a->side->teamMember[0];
    return slot >= 0 && slot < team->numberOfPokemonBrought && team->pokemonPicked[slot].hp != 0 &&
           (u32)slot != (u32)a->state.rosterIndex;
}

// =============================================================================================================
// Mid-battle move description
// =============================================================================================================

static hu_move_desc sDesc[2];
static u32 sDescFrame[2]; // sFrame + 1 of the last frame the side's fight menu read its buttons
static s32 sDescInit;

void PortEnh_MoveMenuInput(ActivePokemon* a) {
    static const int8_t slot_of[4] = { 0, 1, 2, 3 }; // up, right, down, left: the C buttons' slots
    s32 s;
    u16 held;
    u8 dirs = 0;
    u8 usable = 0;
    s32 i;

    if (!Port_Enh_MoveDescriptionsEnabled() || !IsHumanSide(a) || a->state.controller == NULL) {
        return;
    }
    s = SideOf(a);
    if (!sDescInit) {
        hu_move_desc_init(&sDesc[0], 5, 3);
        hu_move_desc_init(&sDesc[1], 5, 3);
        sDescInit = 1;
    }
    held = a->state.controller->buttonDown;
    if (held & BM_BTN_DUP) dirs |= HU_DPAD_UP;
    if (held & BM_BTN_DRIGHT) dirs |= HU_DPAD_RIGHT;
    if (held & BM_BTN_DDOWN) dirs |= HU_DPAD_DOWN;
    if (held & BM_BTN_DLEFT) dirs |= HU_DPAD_LEFT;
    for (i = 0; i < 4; i++) {
        if (MoveUsable(a, i)) usable |= 1 << i;
    }
    if (hu_move_desc_update(&sDesc[s], dirs, slot_of, usable)) {
        BattleAnim_PlaySoundEffect1();
    }
    sDescFrame[s] = sFrame + 1;
}

// Draws the description window of side `s` (its frame scaled by `size` about its middle, contents at `fill`).
static void DrawMoveDescription(ActivePokemon* a, s32 s, s32 slot, f32 size, f32 fill) {
    const f32 W = 200.0f, H = 58.0f;
    s32 move = a->state.runtime.moves[slot];
    s32 port = ColorIndex(a);
    f32 x = 96.0f, y;
    f32 w = W * size, h = H * size;
    MoveData* data;
    TypeName* type;
    char buf[64];
    const char* desc;
    u8 alpha = (u8)(0xFF * fill);

    if (s == 0) {
        y = (a->state.showingCommandOptions == 1) ? 15.0f + 56.0f + 4.0f : 15.0f + 18.0f + 3.0f;
    } else {
        x = (a->state.showingCommandOptions == 1) ? 25.0f : 98.0f;
        y = (a->state.showingCommandOptions == 1) ? 166.0f - 4.0f - H : 206.0f - 3.0f - H;
    }
    if (w < 2.0f || h < 2.0f || move <= 0 || move > 165) {
        return;
    }
    // the battle menus' own gradient in the side's colour, then a thin light border
    Battle_DrawGradientRectWrapper((s32)(x + (W - w) * 0.5f), (s32)(y + (H - h) * 0.5f), (s32)w, (s32)h,
                                   MENU_TOP(port), MENU_BOTTOM(port));
    Enh_Begin();
    {
        f32 fx = x + (W - w) * 0.5f, fy = y + (H - h) * 0.5f;
        Enh_Rect(fx - 1.0f, fy - 1.0f, w + 2.0f, 1.0f, 0xE8, 0xE8, 0xF8, 0xFF);
        Enh_Rect(fx - 1.0f, fy + h, w + 2.0f, 1.0f, 0xE8, 0xE8, 0xF8, 0xFF);
        Enh_Rect(fx - 1.0f, fy, 1.0f, h, 0xE8, 0xE8, 0xF8, 0xFF);
        Enh_Rect(fx + w, fy, 1.0f, h, 0xE8, 0xE8, 0xF8, 0xFF);
    }
    if (alpha == 0) {
        Enh_End();
        return;
    }
    data = &gMoveData[move - 1];
    type = &D_80066AD8[Type_ToTableIndex(data->type) & 15];
    {
        // タイプ／ <type>   いりょく／ <power>   めいちゅう／ <accuracy>%: the summary's labels and values, in three
        // columns (squeezed to fit, as translated labels are on the summary)
        const f32 th = 8.5f, ty = y + 4.0f;
        const f32 col[4] = { x + 6.0f, x + 78.0f, x + 136.0f, x + W - 5.0f };
        f32 tx;
        tx = col[0] + Enh_Text(col[0], ty, th, 40.0f, Str("SUMMARY_TYPE", "タイプ／", "Type/"), 0xFF, 0xC8, 0xC8,
                               alpha);
        Enh_Text(tx, ty, th, col[1] - 3.0f - tx, Port_Text_Display(type->str), type->r, type->g, type->b, alpha);
        tx = col[1] + Enh_Text(col[1], ty, th, 40.0f, Str("SUMMARY_POWER", "いりょく／", "Power/"), 0xFF, 0xC8,
                               0xC8, alpha);
        if (data->power >= 2) {
            snprintf(buf, sizeof buf, "%d", data->power);
        } else {
            snprintf(buf, sizeof buf, "---");
        }
        Enh_Text(tx, ty, th, col[2] - 3.0f - tx, buf, 0xFF, 0xFF, 0xFF, alpha);
        tx = col[2] + Enh_Text(col[2], ty, th, 40.0f, Str("SUMMARY_ACCURACY", "めいちゅう／", "Accuracy/"), 0xFF,
                               0xC8, 0xC8, alpha);
        snprintf(buf, sizeof buf, "%d%%", (((data->accuracy * 100) / 255) + 2) / 5 * 5);
        Enh_Text(tx, ty, th, col[3] - tx, buf, 0xFF, 0xFF, 0xFF, alpha);
    }
    // the description: its own line breaks (the translations are reflowed for the summary's box, which is as wide)
    desc = Port_Text_Display(D_8006A9D4[move - 1].description);
    {
        f32 ly = y + 17.0f;
        char line[256];
        while (desc != NULL && *desc != '\0' && ly < y + H - 9.0f) {
            const char* nl = strchr(desc, '\n');
            size_t n = (nl != NULL) ? (size_t)(nl - desc) : strlen(desc);
            if (n >= sizeof line) n = sizeof line - 1;
            memcpy(line, desc, n);
            line[n] = '\0';
            Enh_Text(x + 6.0f, ly, 10.0f, W - 12.0f, line, 0xFF, 0xFF, 0xFF, alpha);
            ly += 12.5f;
            desc = (nl != NULL) ? nl + 1 : NULL;
        }
    }
    Enh_End();
}

static void MoveDescriptionFrame(void) {
    s32 s;
    if (!sDescInit) {
        return;
    }
    for (s = 0; s < 2; s++) {
        ActivePokemon* a = sActivePokemon[s];
        s32 slot;
        if (a == NULL) continue;
        if (sDescFrame[s] != sFrame + 1 && sDescFrame[s] != sFrame) {
            // the fight menu did not read its buttons this frame: it is gone, the window closes
            hu_move_desc_close(&sDesc[s]);
        }
        slot = hu_move_desc_shown(&sDesc[s]);
        if (slot < 0 || sBattleScene.scene->mode != 1 || a->state.selectedCommandOption == 3) {
            if (slot >= 0 && a->state.selectedCommandOption == 3) hu_move_desc_reset(&sDesc[s]); // picked: gone
            continue;
        }
        if (s == 0) gDPPortAnchorLeft(gDisplayListHead++);
        else gDPPortAnchorRight(gDisplayListHead++);
        DrawMoveDescription(a, s, slot, hu_window_anim_size(&sDesc[s].anim), hu_window_anim_fill(&sDesc[s].anim));
        gDPPortAnchorEnd(gDisplayListHead++);
    }
}

// =============================================================================================================
// Order queueing
// =============================================================================================================

static hu_order_queue sQueue[2];
static hu_window_anim sQueueWin[2];
static u8 sQueueShown[2];   // the hu_oq_state the window shows (it keeps showing the last one while it closes)
static u8 sLive[2];          // the side's command menu is up (the order is being entered or the player picks)
static u32 sCommandFrame[2]; // sFrame + 1 of the last frame Battle_ChooseActions asked this side for its action
static s32 sQueuedRoster[2]; // the Pokémon (rosterIndex) the order was given for
static u16 sInjected[2];     // buttons pressed for the player this frame (taken back after the command input)
static s32 sQueueInit;
static s32 sCommandSeen;     // a command phase has come up in this battle (no queueing during the opening)

static void QueueInit(void) {
    s32 s;
    for (s = 0; s < 2; s++) {
        hu_oq_init(&sQueue[s]);
        hu_window_anim_init(&sQueueWin[s], 6, 4);
        sLive[s] = 0;
        sCommandFrame[s] = 0;
        sQueuedRoster[s] = -1;
        sQueueShown[s] = HU_OQ_HIDDEN;
    }
    sCommandSeen = 0;
    sQueueInit = 1;
}

void PortEnh_BattleMenusBegin(void) {
    QueueInit();
    if (sDescInit) {
        hu_move_desc_reset(&sDesc[0]);
        hu_move_desc_reset(&sDesc[1]);
    }
}

static void Log(s32 s, const char* what) {
    if (getenv("PORT_ENH_LOG") != NULL) {
        fprintf(stderr, "[order-queue] side %d: %s (state %d slot %d, frame %u)\n", (int)s, what,
                (int)sQueue[s].state, (int)sQueue[s].slot, (unsigned)sFrame);
    }
}

static void Drop(s32 s, const char* why) {
    if (sQueue[s].state != HU_OQ_HIDDEN || sQueue[s].entering) {
        Log(s, why);
    }
    hu_oq_cancel(&sQueue[s]);
    sQueuedRoster[s] = -1;
}

// Whether the queued order can still be given (the game's own rules for its command menu: BattleScene_CollectPlayerAction).
// `at_menu`: the side's command menu is coming up now (the lock checks only mean something then: during the turn the
// battle sets and clears those flags while it plays the moves out).
static s32 OrderLegal(ActivePokemon* a, ActivePokemon* foe, s32 s, s32 at_menu) {
    hu_order_queue* q = &sQueue[s];
    if (a->state.faintState == 0x10 || a->state.faintState == 0x11 || a->state.runtime.hp == 0) {
        return 0; // fainted: the next menu is the forced replacement
    }
    if (sQueuedRoster[s] >= 0 && (s32)a->state.rosterIndex != sQueuedRoster[s]) {
        return 0; // a different Pokémon is out now
    }
    if (at_menu && ((a->state.runtime.volatileStatusFlags & 0x60) || (a->state.runtime.immobilizedReason & 0x12))) {
        return 0; // the second turn of Fly, Dig, Solar Beam..., a rampage or recharging: no menu at all this turn
    }
    if (q->state == HU_OQ_SWITCH_QUEUED) {
        return SwitchLegal(a, q->slot);
    }
    if (q->state == HU_OQ_MOVE_QUEUED) {
        if (at_menu && ((a->state.runtime.statusCondition & 0x27) || (a->state.runtime.immobilizedReason & 0x21) ||
                        (foe->state.runtime.immobilizedReason & 0x20))) {
            return 0; // asleep, frozen, locked into a move or held by a trapping move: Fight picks no move
        }
        return MoveLegal(a, q->slot);
    }
    return 1;
}

void PortEnh_PlayerCommandBegin(ActivePokemon* a, ActivePokemon* foe) {
    s32 s;
    hu_order_queue* q;
    Controller* c;
    hu_oq_action act;

    if (!Port_Enh_OrderQueueEnabled() || !IsHumanSide(a) || a->state.controller == NULL) {
        return;
    }
    if (!sQueueInit) QueueInit();
    s = SideOf(a);
    q = &sQueue[s];
    c = a->state.controller;
    sCommandFrame[s] = sFrame + 1;
    sCommandSeen = 1;
    sInjected[s] = 0;
    if (a->state.faintState == 0x10) {
        Drop(s, "fainted: dropped");
        sLive[s] = 1;
        return;
    }
    if (foe->state.faintState == 0x10) {
        return; // the other side sends in a replacement: this side's menu is not up, the turn is not over yet
    }
    if (!sLive[s]) {
        // the moves have played out and this side's command menu comes up
        sLive[s] = 1;
        if (hu_oq_queued(q) && !OrderLegal(a, foe, s, 1)) {
            Drop(s, "illegal at the command menu: dropped");
        } else {
            if (!hu_oq_queued(q) && q->state != HU_OQ_HIDDEN) Log(s, "not queued at the command menu: dropped");
            hu_oq_resolution_end(q);
            if (q->entering) Log(s, "entering");
        }
    }
    if (!q->entering) {
        return;
    }
    if (c->buttonPressed & BM_BTN_Z) {
        hu_oq_z(q); // taken back: the player picks as usual
        BattleAnim_PlaySoundEffect3();
        Log(s, "Z: taken back");
        return;
    }
    act = hu_oq_command_frame(q);
    switch (act) {
        case HU_OQ_OPEN_FIGHT:
        case HU_OQ_OPEN_SWITCH:
            if (a->state.selectedCommandOption == 0) {
                sInjected[s] = (act == HU_OQ_OPEN_FIGHT) ? BM_BTN_A : BM_BTN_B;
            } else if (a->state.selectedCommand != (u32)((act == HU_OQ_OPEN_FIGHT) ? 1 : 2)) {
                Drop(s, "the player opened another menu: dropped");
            }
            break;
        case HU_OQ_PICK: {
            s32 slot = q->slot;
            s32 is_switch = a->state.selectedCommand == 2;
            if (a->state.selectedCommandOption == 3) {
                break; // the player picked by themselves
            }
            if (is_switch && a->state.selectedCommandOption == 2 && SwitchLegal(a, slot)) {
                sInjected[s] = (a->side->teamMember[0]->numberOfPokemonBrought == 3) ? kSwitch3[slot % 3] : kSwitch6[slot % 6];
                Log(s, "picked the party member");
            } else if (!is_switch && a->state.selectedCommand == 1 && a->state.selectedCommandOption == 1 &&
                       MoveLegal(a, slot)) {
                sInjected[s] = kMoveButton[slot & 3];
                Log(s, "picked the move");
            } else {
                Log(s, "the menu could not take it: dropped");
            }
            sQueuedRoster[s] = -1;
            break;
        }
        default:
            break;
    }
    if (sInjected[s] != 0) {
        c->buttonPressed |= sInjected[s];
        c->buttonDown |= sInjected[s];
    }
}

void PortEnh_PlayerCommandEnd(ActivePokemon* a) {
    s32 s;
    if (!sQueueInit || a == NULL || a->state.controller == NULL) {
        return;
    }
    s = SideOf(a);
    if (sInjected[s] != 0) {
        a->state.controller->buttonPressed &= ~sInjected[s];
        a->state.controller->buttonDown &= ~sInjected[s];
        sInjected[s] = 0;
    }
}

// The queue's buttons while the turn plays out (the side's command menu is not up).
static void QueueInput(ActivePokemon* a, ActivePokemon* foe, s32 s) {
    hu_order_queue* q = &sQueue[s];
    u16 p = a->state.controller->buttonPressed;
    s32 changed = 0, back = 0;
    s32 i;

    if (q->entering) {
        return;
    }
    if (p & BM_BTN_Z) {
        back = q->state != HU_OQ_HIDDEN;
        changed = hu_oq_z(q);
        if (q->state == HU_OQ_HIDDEN) sQueuedRoster[s] = -1;
    } else if (q->state == HU_OQ_SWITCH_SELECT) {
        // the switch menu's own buttons: with six brought B and A pick party members too
        s32 brought = a->side->teamMember[0]->numberOfPokemonBrought;
        for (i = 0; i < ((brought == 3) ? 3 : 6); i++) {
            u16 b = (brought == 3) ? kSwitch3[i] : kSwitch6[i];
            if ((p & b) && SwitchLegal(a, i)) {
                changed = hu_oq_pick(q, i);
                break;
            }
        }
        if (!changed && (p & BM_BTN_L)) {
            back = changed = hu_oq_l(q);
        }
    } else if (q->state == HU_OQ_MOVE_SELECT) {
        for (i = 0; i < 4; i++) {
            if ((p & kMoveButton[i]) && MoveLegal(a, i)) {
                changed = hu_oq_pick(q, i);
                break;
            }
        }
        if (!changed && (p & BM_BTN_L)) {
            back = changed = hu_oq_l(q);
        }
    } else if (p & BM_BTN_A) {
        changed = hu_oq_a(q);
    } else if (p & BM_BTN_B) {
        changed = hu_oq_b(q);
    } else if (p & BM_BTN_L) {
        back = changed = hu_oq_l(q);
    }
    if (hu_oq_queued(q)) {
        sQueuedRoster[s] = (s32)a->state.rosterIndex;
    }
    if (changed) {
        if (back) BattleAnim_PlaySoundEffect3();
        else BattleAnim_PlaySoundEffect2();
        Log(s, "menu");
    }
    if (hu_oq_queued(q) && !OrderLegal(a, foe, s, 0)) {
        if (getenv("PORT_ENH_LOG") != NULL) {
            fprintf(stderr, "[order-queue] faint %d hp %d roster %d/%d move %d pp %02x disabled %02x\n",
                    a->state.faintState, a->state.runtime.hp, (int)a->state.rosterIndex, (int)sQueuedRoster[s],
                    q->slot >= 0 ? a->state.runtime.moves[q->slot & 3] : -1,
                    q->slot >= 0 ? a->state.runtime.pp[q->slot & 3] : -1, a->state.runtime.disabledMoveState);
        }
        Drop(s, "became illegal: dropped");
    }
}

// The order on a bar: "Next: <move>" or "Next: <Pokémon>", with the buttons that take it back.
static void DrawQueuedBar(ActivePokemon* a, s32 s, f32 x, f32 y, f32 fill) {
    hu_order_queue* q = &sQueue[s];
    const f32 W = 200.0f;
    u8 alpha = (u8)(0xFF * fill);
    char line[128];
    const char* what = "";
    s32 port = ColorIndex(a);

    Battle_DrawGradientRectWrapper((s32)x, (s32)y, (s32)W, 18, MENU_TOP(port), MENU_BOTTOM(port));
    if (alpha == 0) return;
    if (q->state == HU_OQ_MOVE_QUEUED || sQueueShown[s] == HU_OQ_MOVE_QUEUED) {
        s32 m = (q->slot >= 0) ? a->state.runtime.moves[q->slot & 3] : 0;
        what = (m > 0 && m <= 165) ? Port_Text_Display(D_8006A9D4[m - 1].name) : "";
    } else {
        TeamRoster* team = a->side->teamMember[0];
        if (q->slot >= 0 && q->slot < 6) what = Port_Text_Display((const char*)team->pokemonPicked[q->slot].nickname);
    }
    snprintf(line, sizeof line, "%s %s", Str("PORT_QUEUE_NEXT", "つぎ：", "Next:"), what);
    Enh_Begin();
    Enh_Text(x + 6.0f, y + 4.0f, 10.0f, 120.0f, line, 0xFF, 0xFF, 0xFF, alpha);
    {
        const char* hint = Str("PORT_QUEUE_HINT", "L：もどる  Z：やめる", "L: Back  Z: Cancel");
        f32 tw = Enh_TextWidth(8.0f, hint);
        if (tw > 70.0f) tw = 70.0f;
        Enh_Text(x + W - 6.0f - tw, y + 5.0f, 8.0f, 70.0f, hint, 0xE0, 0xE0, 0xF0, alpha);
    }
    Enh_End();
}

static void DrawQueue(ActivePokemon* a, s32 s) {
    hu_window_anim* w = &sQueueWin[s];
    f32 size = hu_window_anim_size(w), fill = hu_window_anim_fill(w);
    // under the side's status box during the turn (side 0 top left, side 1 bottom right, as their menus)
    f32 x = (s == 0) ? 96.0f : 25.0f, y = (s == 0) ? 56.0f : 120.0f;
    f32 H;
    s32 port = ColorIndex(a);
    u8 shown = sQueueShown[s];

    switch (shown) {
        case HU_OQ_SHOWING: H = 18.0f; break;
        case HU_OQ_MOVE_SELECT: H = 56.0f; break;
        case HU_OQ_SWITCH_SELECT: H = (a->side->teamMember[0]->numberOfPokemonBrought == 3) ? 50.0f : 104.0f; break;
        default: H = 18.0f; break;
    }
    if (s != 0) y = 200.0f - H;
    if (fill <= 0.0f) {
        // the window grows from its middle (then its contents fill in; closing the other way round)
        f32 w2 = 200.0f * size, h2 = H * size;
        if (w2 >= 2.0f && h2 >= 2.0f) {
            Battle_DrawGradientRectWrapper((s32)(x + (200.0f - w2) * 0.5f), (s32)(y + (H - h2) * 0.5f), (s32)w2,
                                           (s32)h2, MENU_TOP(port), MENU_BOTTOM(port));
        }
        return;
    }
    switch (shown) {
        case HU_OQ_SHOWING:
            BattleMenu_DrawCommandMenu(a, (s16)x, (s16)y, port);
            break;
        case HU_OQ_MOVE_SELECT:
            BattleMenu_DrawMovePanel(a, (s16)x, (s16)y, port);
            break;
        case HU_OQ_SWITCH_SELECT:
            if (a->side->teamMember[0]->numberOfPokemonBrought == 3) {
                BattleMenu_DrawRosterPanel(a->side->teamMember[0], (s16)x, (s16)y, port);
            } else {
                BattleMenu_DrawSwitchPanel(a, (s16)x, (s16)y, port);
            }
            break;
        default:
            DrawQueuedBar(a, s, x, y, 1.0f);
            break;
    }
    if (fill < 1.0f) {
        // filling: the game's panel shows through its own colour as it comes in
        Color_RGB8* c = MENU_TOP(port);
        Enh_Begin();
        Enh_Rect(x, y, 200.0f, H + 1.0f, c->r, c->g, c->b, (u8)(0xFF * (1.0f - fill)));
        Enh_End();
    }
    // a tab above the window says what it is for
    {
        const char* title = Str("PORT_QUEUE_TITLE", "つぎの めいれい", "Next order");
        f32 tw = Enh_TextWidth(8.0f, title);
        Enh_Begin();
        Enh_Rect(x + 4.0f, y - 11.0f, tw + 8.0f, 11.0f, 0x00, 0x00, 0x00, (u8)(0xA0 * fill));
        Enh_Text(x + 8.0f, y - 10.0f, 8.0f, 0.0f, title, 0xFF, 0xFF, 0xFF, (u8)(0xFF * fill));
        Enh_End();
    }
}

static void OrderQueueFrame(void) {
    s32 s;
    if (!sQueueInit) {
        QueueInit();
    }
    for (s = 0; s < 2; s++) {
        ActivePokemon* a = sActivePokemon[s];
        ActivePokemon* foe = sActivePokemon[!s];
        hu_order_queue* q = &sQueue[s];
        s32 resolving;
        if (a == NULL || foe == NULL || !IsHumanSide(a) || a->state.controller == NULL) {
            continue;
        }
        if (sCommandFrame[s] != sFrame + 1 && sCommandFrame[s] != sFrame) {
            if (sLive[s] && q->entering) {
                Drop(s, "the command menu went before the order was entered: dropped");
            }
            sLive[s] = 0; // the side is not being asked for its action: the turn plays out
        }
        resolving = sCommandSeen && !sLive[s] && sBattleScene.scene->winner == 0 && sBattleScene.scene->mode != 1;
        if (resolving) {
            QueueInput(a, foe, s);
        } else if (!sLive[s] && q->state != HU_OQ_HIDDEN && !q->entering && sBattleScene.scene->winner != 0) {
            Drop(s, "the battle is over: dropped");
        }
        if (hu_oq_visible(q) && resolving) {
            sQueueShown[s] = q->state;
            hu_window_anim_set(&sQueueWin[s], 1);
        } else {
            hu_window_anim_set(&sQueueWin[s], 0);
        }
        hu_window_anim_tick(&sQueueWin[s]);
        if (hu_window_anim_visible(&sQueueWin[s])) {
            if (s == 0) gDPPortAnchorLeft(gDisplayListHead++);
            else gDPPortAnchorRight(gDisplayListHead++);
            DrawQueue(a, s);
            gDPPortAnchorEnd(gDisplayListHead++);
        }
    }
}

// =============================================================================================================

void PortEnh_BattleMenusFrame(void) {
    sFrame++;
    if (Port_Enh_MoveDescriptionsEnabled()) {
        MoveDescriptionFrame();
    }
    if (Port_Enh_OrderQueueEnabled()) {
        OrderQueueFrame();
    }
}
