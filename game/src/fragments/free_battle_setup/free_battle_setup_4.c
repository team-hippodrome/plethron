/* Fixed-layout split (from FreeBattle_EnterEntryScreenState); original translation-unit boundaries remain unconfirmed. */

#include "global.h"
#include "m2c_macros.h"
#include "free_battle_setup.h"
#include "game_state.h"


// PORT: the decomp called these without a prototype (IDO allows it; clang does not).
u16 ScrollGrid_UpdateCursor(ScrollGrid *menu);
u16 ScrollList_UpdateCursor(ScrollList* menu);

typedef struct {
    /* 0x00 */ u8 id;
    /* 0x01 */ char unk01[0x4F];
} Record50;
typedef struct {
    /* 0x00 */ Record50* records;
    /* 0x04 */ char unk04[0x4];
    /* 0x08 */ s32 count;
} RecordTable;
typedef struct {
    /* 0x00 */ RecordTable* table;
    /* 0x04 */ u16* flags;
} FlagOwner;
typedef struct Color82001CCC {
    u8 c[4];
} Color82001CCC;
typedef struct {
    /* 0x00 */ TeamMonSlot** slots;
    /* 0x04 */ u16* flags;
} SlotFlags;

s32 GridMenu_UpdateCursor(GridMenu* arg0); // PORT: matches the definition
void FreeBattle_SetScreenState(FreeBattleScreen*, s32, s32);
extern s32 D_8200AC58[];
void Font_BeginTranslucentTextRendering(void);
void Font_SetActive(s32, s32);
void Font_EndTexturedTextRendering(void);
void Gfx_SetEnvColor(u8, u8, u8, u8);
void Font_Printf(s32 x, s32 y, const char* fmt, ...);
extern M2C_UNK D_800734A8;
void Menu_ClearItemFlags(FlagOwner*);
extern Gfx D_80060270[];
void func_80029028(s32, s32, s32, s32, Color82001CCC);
extern Color82001CCC D_82008F5C;
extern Color82001CCC D_82008F58;

extern char D_82009C50[];
extern char D_82009C6C[];
extern char D_82009C84[];
void FreeBattle_InitLevelSumWarning(FreeBattleLevelSumWarning* window, s32 x, s32 y, Controller* controller); // PORT: matches the definition
void FreeBattle_InitRentalConfirm(FreeBattleRentalConfirm* window, s32 x, s32 y, s32 has_third_button, Controller* controller); // PORT: matches the definition
void ScrollGrid_SyncCursor(FlagOwner*);

void FreeBattle_ClearIndices(s32* indices, s32 start, s32 count); // PORT: no prototype in the decomp
void FreeBattle_InitConfirmWindow(FreeBattleMonStatusWindow* window, s32 x, s32 y, BattleMon* mon, char* prompt, Controller* controller); // PORT: no prototype in the decomp
void FreeBattle_MarkPickedRentals(FlagOwner* grid, TeamMonSlot* slots, s32 except_species); // PORT: no prototype in the decomp
/**
 * Enters an entry screen state and picks its panel: 1 the team list, 2 the duplicate warning,
 * 3/7/9 a Pokémon's confirm window, 4 the shortage warning, 5 the level-sum warning, 6 and 8 the
 * rental grid, 10 the entry confirm, 12 the text box.
 * Seen in play: runs when 「エントリーポケモンのせんたく」 opens in state 1 (the team list).
 * Seen in play (2026-11-14): state 2 with 「おなじポケモンが ２たいいます」, 3 with 「このポケモンにしますか？」,
 * 5 with the 155 level-sum warning and 8 with the rental grid (state read at +4 in RAM), and 10
 * with 「このデータでエントリーします。」 (FreeBattle_InitRentalConfirm hit). Not seen: 4, 6, 7, 9, 12.
 */
void FreeBattle_EnterEntryScreenState(FreeBattleEntryScreen* arg0) {
    Window* panel = NULL;
    s32 color;

    switch (arg0->state) {
        case 1:
            if (!(arg0->stateFlags & 2) && arg0->teamChanged != 0) {
                FreeBattle_ClearIndices(D_8200AC58, (arg0->teamList.cursor * 6) % 24, 6);
                arg0->teamChanged = 0;
            }
            panel = &arg0->window;
            break;
        case 2:
            panel = &arg0->duplicateWarning.window;
            break;
        case 5:
            if (!(arg0->stateFlags & 2)) {
                FreeBattle_InitLevelSumWarning(&arg0->levelSumWarning, 0xA6, 0x105, arg0->controller);
            }
            panel = &arg0->levelSumWarning.window;
            break;
        case 4:
            panel = &arg0->shortageWarning.window;
            break;
        case 8:
            if (!(arg0->stateFlags & 2)) {
                if (arg0->team.slots[arg0->slotMenu.cursor].flags != 0) {
                    color = -1;
                } else {
                    color = arg0->team.slots[arg0->slotMenu.cursor].mon.species.id;
                }
                FreeBattle_MarkPickedRentals((FlagOwner*)&arg0->rentalGrid, arg0->team.slots, color);
                ScrollGrid_SyncCursor((FlagOwner*)&arg0->rentalGrid);
            }
            panel = &arg0->window;
            break;
        case 6:
            Menu_ClearItemFlags((FlagOwner*)&arg0->rentalGrid);
            panel = &arg0->window;
            break;
        case 3:
            FreeBattle_InitConfirmWindow(&arg0->statusWindow, 0x69, 0x7D, &arg0->team.slots[arg0->slotMenu.cursor].mon, D_82009C50, arg0->controller); // PORT: the decomp computed &mon as nickname - 0x30 (a matching fake)
            panel = &arg0->statusWindow.window;
            break;
        case 7:
            FreeBattle_InitConfirmWindow(&arg0->statusWindow, 0x69, 0x7D, &arg0->team.slots[arg0->slotMenu.cursor].mon, D_82009C6C, arg0->controller); // PORT: the decomp computed &mon as nickname - 0x30 (a matching fake)
            panel = &arg0->statusWindow.window;
            break;
        case 9:
            FreeBattle_InitConfirmWindow(&arg0->statusWindow, 0x69, 0x7D, &((BattleMon*)arg0->rentalGrid.items->rows)[arg0->rentalGrid.cursor], D_82009C84, arg0->controller);
            panel = &arg0->statusWindow.window;
            break;
        case 10:
            FreeBattle_InitRentalConfirm(&arg0->entryConfirm, 0x7A, 0x10B, arg0->ruleCategory != 0, arg0->controller);
            panel = &arg0->entryConfirm.window;
            break;
        case 12:
            panel = &arg0->textBox.window;
            break;
    }
    arg0->view = panel;
}


/**
 * Sets the entry screen's state (and flags); without flag 1 it switches at once through
 * FreeBattle_EnterEntryScreenState, otherwise it defers the switch.
 * Seen in play: runs when 「エントリーポケモンのせんたく」 opens.
 * Seen in play (2026-11-13) ran when the エントリーポケモンのせんたく screen opened and when its rows were
 * picked.
 */
void FreeBattle_SetEntryScreenState(FreeBattleEntryScreen* entry, s32 state, s32 flags) {
    entry->stateFlags = flags;
    if (flags & 1) {
        entry->view->animTimer = 7;
        entry->pendingState = state;
        return;
    }
    entry->state = state;
    entry->pendingState = 0;
    FreeBattle_EnterEntryScreenState(entry); // PORT: the decomp left a0 (entry) in place
}

void ScrollList_SetCursor(ScrollList*, u32);

/**
 * Puts the entry list cursor on the row for the current entry mode, picks that mode's pick buffer
 * (D_800AAF50) and enters entry state 1.
 * Seen in play: runs when 「エントリーポケモンのせんたく」 opens.
 * @unverified Name from the code.
 */
void FreeBattle_InitEntryListCursor(FreeBattleEntryScreen* entry) {
    s32 sel = entry->teamList.cursor;
    s32 mode = entry->player;
    s32 i;
    s32 count;
    u8** list;
    PointerList* obj;

    if (mode != 5) {
        obj = (PointerList*)entry->teamList.items; // PORT: the team list's items are a PointerList
        count = obj->count;
        list = (u8**)obj->items;
        for (i = 0; i < count && (*list)[0] == 0 && (*list)[1] == 0x20; i++, list++) {
            if (mode - 1 == (*list)[2]) {
                sel = i;
                break;
            }
        }
    }
    ScrollList_SetCursor(&entry->teamList, sel);
    mode = entry->player;
    if (mode == 5) {
        entry->controller = &gControllers[0];
    } else {
        entry->controller = &gControllers[mode - 1]; // PORT: was D_800AAF50 + (mode - 1) * 0x24 (the N64 sizeof(Controller))
    }
    entry->teamChanged = 0;
    entry->teamList.controller = entry->controller;
    entry->rentalGrid.controller = entry->controller;
    FreeBattle_SetEntryScreenState(entry, 1, 0);
}

extern char D_82009C98[];
void FreeBattle_DrawMonCell(s32 x, s32 y, s32* entry, s32 frame, s32 flags, intptr_t icon_cache); // PORT: matches the definition
void FreeBattle_DrawTeamRow(s32 x, s32 y, u8* item, s32 index, s32 flags, intptr_t picks); // PORT: matches the definition
void Window_SetRect(Window* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);
void ScrollList_Init(ScrollList* arg0, ByteMatrix* arg1, u16* arg2, s32 arg3, MenuDrawItemFunc arg4, s32 arg5, s32 arg6); // PORT: matches the definition
void ScrollGrid_Init(ScrollGrid*, ByteMatrix*, u16*, s32, s32, MenuDrawItemFunc, s32, s32); // PORT: was (s8*, s8*, s8*, M2C_UNK, ...)
void func_80027410(ByteMatrix* arg0, void* arg1, s32 arg2, s32 arg3, s32 arg4); // PORT: matches TableView_Initialize
void PokeIcon_ResetSlots(PokeIconRenderer* arg0);
void FreeBattle_InitEntryListCursor(FreeBattleEntryScreen*);

/**
 * Initializes the slot-menu object at entry (the layout other functions in this file
 * access as SlotMenu):
 *   - sets up the base window at +0x14 via Window_SetRect (x, y, size 0x220 x 0x158),
 *   - zeroes +0x00/+0x38/+0x3C, stores 0x801 at +0x28, &D_82009C98 at +0x34 and teams at +0xC4,
 *   - creates the sub-object at +0x40/+0x84 with ScrollList_Init (passing &FreeBattle_DrawTeamRow),
 *   - sets bit 0x10000 at +0x60,
 *   - fills the first 0x28 words of D_8200AC58 with -1,
 *   - stores 1 at +0x738, picks at +0x73C and the mode ruleCategory at +0x740, and calls FreeBattle_InitEntryListCursor,
 *   - when ruleCategory != 0, builds the record view at +0x684 (source D_8200ACFC if ruleCategory == 1,
 *     otherwise D_8200AD00, with different dimensions), links it through ScrollGrid_Init
 *     at +0x6E4/+0x694 (passing &FreeBattle_DrawMonCell) and sets bit 0x10000 at +0x704,
 *   - finally calls PokeIcon_ResetSlots on D_8200AC50.
 * The meaning of x..ruleCategory beyond what is stored above is not established.
 * Seen in play: runs as 「エントリーポケモンのせんたく 1Pせんたくちゅう」 opens.
 */
void FreeBattle_InitEntrySelect(FreeBattleEntryScreen* entry, s32 x, s32 y, PointerList* teams, FreeBattleTeam** picks, s32 rule_category) {
    ByteMatrix* var_s1;

    Window_SetRect(&entry->window, x, y, 544, 344);
    entry->window.animTimer = 0;
    entry->window.state = 0;
    entry->window.frame.style = 0x801;
    entry->window.title = D_82009C98;
    entry->teams = teams;
    ScrollList_Init(&entry->teamList, (ByteMatrix*)teams, entry->teamListFlags, 3, FreeBattle_DrawTeamRow, 532, 100);
    entry->teamList.options = entry->teamList.options | 0x10000;
    entry->unk_00 = 0;
    FreeBattle_ClearIndices(D_8200AC58, 0, 40);
    entry->player = 1;
    entry->playerTeams = picks;
    entry->ruleCategory = rule_category;
    FreeBattle_InitEntryListCursor(entry);
    if (rule_category != 0) {
        if (rule_category == 1) {
            var_s1 = &entry->rentalItems;
            func_80027410(var_s1, D_8200ACFC, 0x50, 40, 40);
        } else {
            var_s1 = &entry->rentalItems;
            func_80027410(var_s1, D_8200AD00, 0x50, 33, 33);
        }
        ScrollGrid_Init(&entry->rentalGrid, var_s1, entry->rentalItemFlags, 4, 3, (MenuDrawItemFunc)FreeBattle_DrawMonCell, 132, 50);
        entry->rentalGrid.options = entry->rentalGrid.options | 0x10000;
    }
    PokeIcon_ResetSlots(D_8200AC50);
}

extern Gfx D_80073450[];
extern u8 D_05000008[];
extern char D_82009CC8[];
void Gfx_DrawTextureRgba16(s32, s32, s32, s32, u8*, s32, s32);
void Ui_DrawPrimColorRect(s32, s32, s32, s32, s32, s32, s32, s32);

/**
 * Emits the display list D_80073450 and the DECALRGBA combiner, then calls
 * Gfx_DrawTextureRgba16 at (x, y) with D_05000008 as its texture argument (width
 * is passed straight through as the third argument), calls Ui_DrawPrimColorRect with
 * (x + 0xE, y, 0x84, 0x14, ...) and then
 * prints the string D_82009CC8 at (x + 0x10, y + 2) in the color
 * stored at D_800734A8 using font 4.
 * What this draws is not yet established.
 * Seen in play: draws the 「レンタルポケモン」 title bar of the rental grid.
 */
void FreeBattle_DrawRentalTitle(s32 x, s32 y, s32 width) {
    gSPDisplayList(gDisplayListHead++, D_80073450);
    gDPSetCombineMode(gDisplayListHead++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    Gfx_DrawTextureRgba16(x, y, width, 20, D_05000008, 8, 0);
    Ui_DrawPrimColorRect(x + 14, y, 132, 20, 0, 0, 0, 0xFF); // #000000
    Font_BeginTranslucentTextRendering();
    Gfx_SetEnvColor(((u8*)&D_800734A8)[0], ((u8*)&D_800734A8)[1], ((u8*)&D_800734A8)[2], ((u8*)&D_800734A8)[3]);
    Font_SetActive(4, 0);
    Font_Printf(x + 0x10, y + 2, D_82009CC8);
    Font_EndTexturedTextRendering();
}

extern struct FrameTextures D_800735C4; // PORT: typed so the lifter widens its texture pointers (was raw bytes)
extern Color82001CCC D_82009098[];
extern char D_82009CF0[];
void Ui_DrawTexturedFrameDecal(s32, s32, s32, s32, char*);

/**
 * Draws a labeled box using the menu object entry (layout not established; it
 * reads the words at 0x14, 0x18 and the mode index at 0x738). Calls
 * Ui_DrawTexturedFrameDecal at (word 0x14 + 0x127, word 0x18 - 0x12) with size 0xA6 by 0x14
 * and D_800735C4, emits D_80060270 and fills a 0xA2 by 0x10 rectangle at
 * (+0x129, -0x10) with the color D_82009098[mode - 1]. Then prints the string
 * D_82009CF0 at (+0x12A, -0x10) in the color D_800734A8, font 4, passing
 * D_820090A8[mode] as the extra argument.
 * Seen in play: draws 「1Pせんたくちゅう」 next to the entry screen title.
 */
void FreeBattle_DrawChoosingLabel(FreeBattleEntryScreen* entry) {
    Ui_DrawTexturedFrameDecal(entry->window.frame.x + 295, entry->window.frame.y - 18, 166, 20, &D_800735C4);
    gSPDisplayList(gDisplayListHead++, D_80060270);
    func_80029028(entry->window.frame.x + 297, entry->window.frame.y - 0x10, 162, 0x10, D_82009098[entry->player - 1]);
    Font_BeginTranslucentTextRendering();
    Gfx_SetEnvColor(((u8*)&D_800734A8)[0], ((u8*)&D_800734A8)[1], ((u8*)&D_800734A8)[2], ((u8*)&D_800734A8)[3]);
    Font_SetActive(4, 0);
    Font_Printf(entry->window.frame.x + 298, entry->window.frame.y - 0x10, D_82009CF0, D_820090A8[entry->player]);
    Font_EndTexturedTextRendering();
}

typedef struct {
    /* 0x00 */ s32* table;
    /* 0x04 */ s32 unk_04;
    /* 0x08 */ s32 unk_08;
    /* 0x0C */ s32 unk_0C;
} TableArgs;

extern s32 D_8200AC58[];
void ScrollGrid_DrawItems(ScrollGrid* menu, s32 x, s32 y, s32 mask, intptr_t arg4); // PORT: matches the definition

/**
 * Draws using the menu object entry (layout not established; it reads the words
 * at 0x14, 0x18 and 0x1C as position/size values): calls FreeBattle_DrawRentalTitle at
 * (+6, +0x88) with the third argument being word 0x1C minus 0xC, emits the
 * display list D_80060270 and fills a 2 by 0xB6 rectangle with color D_820090C0
 * at (+6, +0x9C). Then builds a TableArgs {D_8200AC58, 0x18, 0x10, 0} and calls
 * ScrollGrid_DrawItems on the sub-object at &entry->rentalGrid at (+8, +0xAC) with a mode
 * word of 0xFED, or 0xFEF when cursorStyle is 1, or 0xFFD when cursorStyle is 2.
 * What the mode word's bits mean is not yet established.
 * Seen in play: draws the 「レンタルポケモン」 window holding the L50 rental grid (フシギバナ, リザードン, カメックス, …).
 */
void FreeBattle_DrawRentalTitleWindow(FreeBattleEntryScreen* entry, s32 cursor_style) {
    s32 sp44;
    TableArgs sp34;

    sp44 = 0xFED;
    FreeBattle_DrawRentalTitle(entry->window.frame.x + 6, entry->window.frame.y + 0x88, entry->window.frame.width - 0xC);
    gSPDisplayList(gDisplayListHead++, D_80060270);
    func_80029028(entry->window.frame.x + 6, entry->window.frame.y + 156, 2, 182, *(Color82001CCC*)&D_820090C0.dividerColor);
    sp34.table = D_8200AC58;
    sp34.unk_04 = 0x18;
    sp34.unk_08 = 0x10;
    sp34.unk_0C = 0;
    switch (cursor_style) {
        case 1:
            sp44 = 0xFEF;
            break;
        case 2:
            sp44 = 0xFFD;
            break;
    }
    ScrollGrid_DrawItems(&entry->rentalGrid, entry->window.frame.x + 8, entry->window.frame.y + 0xAC, sp44, (intptr_t)(&sp34));
}

/**
 * Draws the two textured panels behind the picked team's window (left the trainer column, right
 * the Pokemon).
 * Seen in play: runs while 「このデータでエントリーします。」 is shown over the picked team.
 * @unverified Which panel is which not checked.
 */
void FreeBattle_DrawTeamWindowPanels(FreeBattleEntryScreen* entry) {
    gSPDisplayList(gDisplayListHead++, D_80060270);
    func_80029028(entry->window.frame.x + 6, entry->window.frame.y + 106, 136, 232, D_82008F5C);
    func_80029028(entry->window.frame.x + 142, entry->window.frame.y + 106, 396, 232, D_82008F58);
}

/**
 * Draws the two short textured panels of the team being built above the rental grid (the 「なまえ 1P」
 * row).
 * Seen in play: runs while the L50～55 rental grid is open.
 */
void FreeBattle_DrawRentalTeamPanels(FreeBattleEntryScreen* entry) {
    gSPDisplayList(gDisplayListHead++, D_80060270);
    func_80029028(entry->window.frame.x + 6, entry->window.frame.y + 106, 136, 30, D_82008F5C);
    func_80029028(entry->window.frame.x + 142, entry->window.frame.y + 106, 396, 30, D_82008F58);
}

s32 func_8002AE6C(Window* window); // PORT: matches Window_UpdateAndDrawFrame
s32 func_800189C0(PokeIconRenderer* arg0, s32 arg1); // PORT: matches PokeIcon_ProcessAllSlots
void BgStage_DrawFrame(void);
void ScrollList_DrawItems(ScrollList* menu, s32 x, s32 y, s32 mask, intptr_t arg4); // PORT: matches the definition
void FreeBattle_DrawRentalListWindow(FreeBattleEntryScreen*, s32); // PORT: matches the definition
void FreeBattle_DrawDuplicateWarning(FreeBattleDuplicateWarning*);
void FreeBattle_DrawShortageWarning(FreeBattleShortageWarning*);
void FreeBattle_DrawLevelSumWarning(FreeBattleLevelSumWarning*);
void FreeBattle_DrawEntryConfirm(FreeBattleRentalConfirm*);
void FreeBattle_DrawMonStatusWindow(FreeBattleMonStatusWindow*);
void TextBox_Draw(TextBox*);
/**
 * Draws the rental selection screen for its current state (choosing label, list, entry confirm, status window).
 * Seen in play: runs every frame on 「エントリーポケモンのせんたく」 (Free Battle's team list).
 */
void FreeBattle_DrawRentalScreen(FreeBattleEntryScreen* entry) {
    if (func_8002AE6C(&entry->window) != 0) {
        return;
    }
    FreeBattle_DrawChoosingLabel(entry);
    switch (entry->state) {
        case 1:
            func_800189C0(D_8200AC50, 0);
            BgStage_DrawFrame();
            ScrollList_DrawItems(&entry->teamList, entry->window.frame.x + 6, entry->window.frame.y + 22, 0xFFF, (intptr_t)entry->playerTeams);
            break;
        case 2:
            func_800189C0(D_8200AC50, 0);
            BgStage_DrawFrame();
            FreeBattle_DrawRentalListWindow(entry, 1);
            FreeBattle_DrawTeamWindowPanels(entry);
            FreeBattle_DrawDuplicateWarning(&entry->duplicateWarning);
            break;
        case 6:
        case 8:
            func_800189C0(D_8200AC50, 0);
            BgStage_DrawFrame();
            FreeBattle_DrawRentalListWindow(entry, (entry->state == 6) ? 1 : 2);
            FreeBattle_DrawRentalTitleWindow(entry, (entry->state == 6) ? 0 : 1);
            FreeBattle_DrawRentalTeamPanels(entry);
            break;
        case 10:
            func_800189C0(D_8200AC50, 0);
            BgStage_DrawFrame();
            FreeBattle_DrawRentalListWindow(entry, 0);
            FreeBattle_DrawTeamWindowPanels(entry);
            FreeBattle_DrawEntryConfirm(&entry->entryConfirm);
            break;
        case 4:
            func_800189C0(D_8200AC50, 0);
            BgStage_DrawFrame();
            FreeBattle_DrawRentalListWindow(entry, 0);
            FreeBattle_DrawTeamWindowPanels(entry);
            FreeBattle_DrawShortageWarning(&entry->shortageWarning);
            break;
        case 5:
            func_800189C0(D_8200AC50, 0);
            BgStage_DrawFrame();
            FreeBattle_DrawRentalListWindow(entry, 0);
            FreeBattle_DrawTeamWindowPanels(entry);
            FreeBattle_DrawLevelSumWarning(&entry->levelSumWarning);
            break;
        case 7:
        case 9:
            FreeBattle_DrawRentalListWindow(entry, 2);
            FreeBattle_DrawRentalTitleWindow(entry, (entry->state == 7) ? 0 : 2);
            FreeBattle_DrawRentalTeamPanels(entry);
            FreeBattle_DrawMonStatusWindow(&entry->statusWindow);
            break;
        case 3:
            FreeBattle_DrawRentalListWindow(entry, 2);
            FreeBattle_DrawTeamWindowPanels(entry);
            FreeBattle_DrawMonStatusWindow(&entry->statusWindow);
            break;
        case 11:
            func_800189C0(D_8200AC50, 0);
            BgStage_DrawFrame();
            FreeBattle_DrawRentalListWindow(entry, 0);
            FreeBattle_DrawTeamWindowPanels(entry);
            break;
        case 12:
            func_800189C0(D_8200AC50, 0);
            BgStage_DrawFrame();
            ScrollList_DrawItems(&entry->teamList, entry->window.frame.x + 6, entry->window.frame.y + 22, 0xFFF, (intptr_t)entry->playerTeams);
            TextBox_Draw(&entry->textBox);
            break;
    }
}

/**
 * Moves the cursor over the picked team's six slots (GridMenu_UpdateCursor on the slot grid).
 * Seen in play (2026-11-14): ran while 「おなじポケモンが ２たいいます」 asked for the duplicate to
 * keep; D-pad moved the slot index at +0x300 (1, 2, 5, 3).
 */
s32 FreeBattle_UpdateTeamSlotCursor(FreeBattleEntryScreen* entry) {
    return GridMenu_UpdateCursor(&entry->slotMenu);
}

typedef struct { s32 w[20]; } Words80;


void FreeBattle_InitDuplicateWarning(FreeBattleDuplicateWarning*, s32, s32, s32); // PORT: matches the definition
void FreeBattle_InitShortageWarning(FreeBattleShortageWarning*, s32, s32, Controller*, s32); // PORT: matches the definition
s32 FreeBattle_CountDuplicateSpecies(void*, s32*);
s32 FreeBattle_AllTriosOverLevelLimit(TeamMonSlot*);

void FreeBattle_DisableNonDuplicateRows(void* /* SlotFlags* */ rows); // PORT: no prototype in the decomp
TeamMonSlot* FreeBattle_FindFlaggedSlot(TeamMonSlot* slots, s32* out_count); // PORT: no prototype in the decomp
/**
 * Decides the next state of the six-slot menu. Clears `picked`, then, if `mode`
 * is non-zero, inspects the slots in this order and returns the first result
 * that applies:
 *   1. FreeBattle_CountDuplicateSpecies reports a positive count: call FreeBattle_InitDuplicateWarning on the struct at +0x320,
 *      store the reported slot index in `cur`, call FreeBattle_DisableNonDuplicateRows on +0x2F8,
 *      return 2.
 *   2. FreeBattle_FindFlaggedSlot finds a flagged slot: set `picked`, store its index in
 *      `cur`, then return 8 if bit 0 of unk_784 is set, otherwise call
 *      FreeBattle_InitShortageWarning on the struct at +0x370 and return 4.
 *   3. `mode` is 1 and FreeBattle_AllTriosOverLevelLimit is non-zero: return 6 if bit 1 of unk_784
 *      is set, otherwise 5.
 * Otherwise (including when `mode` is 0) returns 0xA.
 * What the individual return values mean is not established from this function.
 * Seen in play: runs when A picks a team on the entry screen.
 */
s32 FreeBattle_CheckEntryRules(FreeBattleEntryScreen* entry) {
    TeamMonSlot* found;
    s32 sp38;
    s32 sp34;
    s32 ret;

    entry->replacing = 0;
    if (entry->ruleCategory == 0) {
        ret = 0xA;
    } else {

        sp38 = FreeBattle_CountDuplicateSpecies(entry->team.slots, &sp34);
        if (sp38 > 0) {
            FreeBattle_InitDuplicateWarning(&entry->duplicateWarning, 156, 261, sp38);
            entry->slotMenu.cursor = sp34;
            FreeBattle_DisableNonDuplicateRows(&entry->slotMenu);
            ret = 2;
        } else {
            found = FreeBattle_FindFlaggedSlot(entry->team.slots, &sp38);
            if (found != NULL) {
                entry->replacing = 1;
                entry->slotMenu.cursor = found - entry->team.slots;
                if (entry->teamFlags & 1) {
                    ret = 8;
                } else {
                    FreeBattle_InitShortageWarning(&entry->shortageWarning, 166, 261, entry->controller, sp38);
                    ret = 4;
                }
            } else if ((entry->ruleCategory == 1) && (FreeBattle_AllTriosOverLevelLimit(entry->team.slots) != 0)) {
                if (entry->teamFlags & 2) {
                    ret = 6;
                } else {
                    ret = 5;
                }
            } else {
                ret = 0xA;
            }
        }
    }
    return ret;
}


extern s32 D_8200AC58[];

/**
 * Puts the rental Pokemon under the cursor into the team's current slot and marks the team
 * changed.
 * Seen in play: はい on 「レンタルしますか？」 adds the Pokemon (フシギバナ L50) to the 「なまえ 1P」 row.
 */
void FreeBattle_RentSelectedMon(FreeBattleEntryScreen* entry) {
    entry->teamChanged = 1;
    entry->teamFlags |= 1;
    *(Words80*)&entry->team.slots[entry->slotMenu.cursor].mon = ((Words80*)entry->rentalGrid.items->rows)[entry->rentalGrid.cursor];
    entry->team.slots[entry->slotMenu.cursor].flags = 0;
    entry->slotMenu.itemFlags[entry->slotMenu.cursor] = 0;
    D_8200AC58[(entry->teamList.cursor * 6 + entry->slotMenu.cursor) % 24] = -1;
}

extern u8 D_82009D04[];
void FreeBattle_DrawSlotCell(s32 x, s32 y, s32* slot, s32 index, s32 flags, intptr_t icon_cache); // PORT: matches the definition

s32 Audio_PlaySystemSound(s32 sound_id); // PORT: no prototype in the decomp
void FreeBattle_EnableNonDuplicateRows(void* /* SlotFlags* */ rows); // PORT: no prototype in the decomp
void FreeBattle_InitNamedTeam(void* /* SlotList* */ team, s32 name_index); // PORT: no prototype in the decomp
void FreeBattle_KeepChosenDuplicate(TeamMonSlot* slots, s32 keep); // PORT: no prototype in the decomp
void FreeBattle_MarkIneligibleSlots(void* /* SlotFlags* */ rows); // PORT: no prototype in the decomp
void FreeBattle_MarkSlotsToReplace(void* /* SlotFlags* */ rows); // PORT: no prototype in the decomp
s32 FreeBattle_UpdateLevelSumWarning(FreeBattleLevelSumWarning* window); // PORT: no prototype in the decomp
s32 FreeBattle_UpdateRentalConfirm(FreeBattleRentalConfirm* window); // PORT: no prototype in the decomp
s32 FreeBattle_UpdateRentalConfirmChoice(FreeBattleMonStatusWindow* window); // PORT: no prototype in the decomp
s32 FreeBattle_UpdateShortageWarning(FreeBattleShortageWarning* window); // PORT: no prototype in the decomp
void GridMenu_Init(GridMenu* arg0, ByteMatrix* arg1, u16* arg2, MenuDrawItemFunc arg3, s32 arg4, s32 arg5, s32 arg6); // PORT: no prototype in the decomp
void TextBox_Init(TextBox* arg0, char* arg1, s32 arg2, s32 arg3, u32 arg4); // PORT: matches the definition
s32 Ui_GetSelectionDirection(Controller* arg0); // PORT: no prototype in the decomp
/**
 * Runs the entry Pokemon list: pick, rental, confirm and warnings.
 * Seen in play: runs while 1P picks a team on 「エントリーポケモンのせんたく」.
 */
s32 FreeBattle_UpdateEntrySelect(FreeBattleEntryScreen* entry)
{
  s32 ret = 0;
  s32 next;
  u8 *rec;
  if (entry->pendingState != 0)
  {
    next = entry->view->state;
    if (next == 4)
    {
      entry->state = entry->pendingState;
      if (entry->pendingState == 0xD)
      {
        ret = 1;
      }
      else
      {
        FreeBattle_EnterEntryScreenState(entry);
        entry->pendingState = 0;
      }
      return ret;
    }
    if ((next != 1) && (next != 2))
    {
      return 0;
    }
  }
  switch (entry->state)
  {
    case 1:
      if (ScrollList_UpdateCursor(&entry->teamList) == 0)
    {
      next = Ui_GetSelectionDirection(entry->controller);
      if (next == (-1))
      {
        Audio_PlaySystemSound(3);
        ret = -1;
      }
      else
        if (next == 1)
      {
        s32 sp40;
        rec = (u8*)entry->teams->items[entry->teamList.cursor];
        if ((entry->ruleCategory == 0) && ((FreeBattle_FindFlaggedSlot(((FreeBattleTeam*)rec)->slots, &sp40), sp40 == 6)))
        {
          Audio_PlaySystemSound(8);
          TextBox_Init(&entry->textBox, (char*)D_82009D04, 1, 2, 9);
          FreeBattle_SetEntryScreenState(entry, 0xC, 0);
        }
        else
        {
          Audio_PlaySystemSound(2);
          entry->teamFlags = 0;
          if (rec[0] & 2)
          {
            FreeBattle_InitNamedTeam(&entry->team, entry->player);
            entry->teamFlags |= 1;
          }
          else
          {
            entry->team = *(FreeBattleTeam*)rec;
          }
          func_80027410(&entry->slotItems, entry->team.slots, sizeof(TeamMonSlot), 6, 6);
          GridMenu_Init(&entry->slotMenu, &entry->slotItems, entry->slotItemFlags, (MenuDrawItemFunc)FreeBattle_DrawSlotCell, 3, 132, 50);
          entry->rentalGrid.cursor = 0;
          entry->slotMenu.options |= 0x10000;
          FreeBattle_MarkIneligibleSlots(&entry->slotMenu);
          FreeBattle_SetEntryScreenState(entry, FreeBattle_CheckEntryRules(entry), 0);
          entry->slotMenu.controller = entry->controller;
          Input_SetRepeatController(entry->controller);
        }
      }
    }
      break;

    case 2:
      if (FreeBattle_UpdateTeamSlotCursor(entry) == 0)
    {
      next = Ui_GetSelectionDirection(entry->controller);
      if (next == (-1))
      {
        Audio_PlaySystemSound(3);
        FreeBattle_SetEntryScreenState(entry, 1, 1);
      }
      else
        if (next == 1)
      {
        if (entry->team.slots[entry->slotMenu.cursor].flags & 0x40)
        {
          Audio_PlaySystemSound(2);
          FreeBattle_SetEntryScreenState(entry, 3, 0);
        }
        else
        {
          Audio_PlaySystemSound(8);
        }
      }
    }
      break;

    case 3:
      next = FreeBattle_UpdateRentalConfirmChoice(&entry->statusWindow);
      if (next == (-1))
    {
      FreeBattle_SetEntryScreenState(entry, 2, 3);
    }
    else
      if (next == 1)
    {
      Audio_PlaySystemSound(2);
      FreeBattle_EnableNonDuplicateRows(&entry->slotMenu);
      FreeBattle_KeepChosenDuplicate(entry->team.slots, entry->slotMenu.cursor);
      FreeBattle_MarkSlotsToReplace(&entry->slotMenu);
      FreeBattle_SetEntryScreenState(entry, FreeBattle_CheckEntryRules(entry), 1);
    }
      break;

    case 4:
      next = FreeBattle_UpdateShortageWarning(&entry->shortageWarning);
      if (next == (-1))
    {
      FreeBattle_SetEntryScreenState(entry, 1, 1);
    }
    else
      if (next == 1)
    {
      Audio_PlaySystemSound(2);
      FreeBattle_SetEntryScreenState(entry, 8, 1);
    }
      break;

    case 5:
      next = FreeBattle_UpdateLevelSumWarning(&entry->levelSumWarning);
      if (next == (-1))
    {
      FreeBattle_SetEntryScreenState(entry, 1, 1);
    }
    else
      if (next == 1)
    {
      entry->teamFlags |= 2;
      FreeBattle_SetEntryScreenState(entry, 6, 1);
    }
      break;

    case 6:
      if (FreeBattle_UpdateTeamSlotCursor(entry) == 0)
    {
      next = Ui_GetSelectionDirection(entry->controller);
      if (next == (-1))
      {
        Audio_PlaySystemSound(3);
        if (entry->teamFlags & 2)
        {
          FreeBattle_SetEntryScreenState(entry, 1, 0);
        }
        else
        {
          FreeBattle_SetEntryScreenState(entry, 0xA, 0);
        }
      }
      else
        if (next == 1)
      {
        Audio_PlaySystemSound(2);
        if (entry->team.slots[entry->slotMenu.cursor].flags & 1)
        {
          FreeBattle_SetEntryScreenState(entry, 8, 0);
        }
        else
        {
          FreeBattle_SetEntryScreenState(entry, 7, 0);
        }
      }
    }
      break;

    case 8:
      if (ScrollGrid_UpdateCursor(&entry->rentalGrid) == 0)
    {
      next = Ui_GetSelectionDirection(entry->controller);
      if (next == (-1))
      {
        Audio_PlaySystemSound(3);
        if (entry->replacing != 0)
        {
          FreeBattle_SetEntryScreenState(entry, 1, 0);
        }
        else
        {
          FreeBattle_SetEntryScreenState(entry, 6, 0);
        }
      }
      else
        if (next == 1)
      {
        if (!(entry->rentalGrid.itemFlags[entry->rentalGrid.cursor] & 1))
        {
          Audio_PlaySystemSound(2);
          FreeBattle_SetEntryScreenState(entry, 9, 0);
        }
        else
        {
          Audio_PlaySystemSound(8);
        }
      }
    }
      break;

    case 7:
      next = FreeBattle_UpdateRentalConfirmChoice(&entry->statusWindow);
      if (next == (-1))
    {
      FreeBattle_SetEntryScreenState(entry, 6, 3);
    }
    else
      if (next == 1)
    {
      Audio_PlaySystemSound(2);
      FreeBattle_SetEntryScreenState(entry, 8, 1);
    }
      break;

    case 9:
      next = FreeBattle_UpdateRentalConfirmChoice(&entry->statusWindow);
      if (next == (-1))
    {
      FreeBattle_SetEntryScreenState(entry, 8, 3);
    }
    else
      if (next == 1)
    {
      Audio_PlaySystemSound(0x22);
      FreeBattle_RentSelectedMon(entry);
      FreeBattle_SetEntryScreenState(entry, FreeBattle_CheckEntryRules(entry), 1);
    }
      break;

    case 10:
      next = FreeBattle_UpdateRentalConfirm(&entry->entryConfirm);
      if (next == (-1))
    {
      FreeBattle_SetEntryScreenState(entry, 1, 1);
    }
    else
      if (next == 2)
    {
      Audio_PlaySystemSound(2);
      entry->slotMenu.cursor = 0;
      entry->rentalGrid.cursor = 0;
      FreeBattle_SetEntryScreenState(entry, 6, 1);
    }
    else
      if (next == 1)
    {
      Audio_PlaySystemSound(0x20);
      FreeBattle_SetEntryScreenState(entry, 0xB, 1);
    }
      break;

    case 11:
      ret = 1;
      break;

    case 12:
      if (entry->controller->buttonPressed & 0xC000)
    {
      Audio_PlaySystemSound(2);
      FreeBattle_SetEntryScreenState(entry, 1, 1);
    }
      break;

  }

  return ret;
}

typedef struct Str82009D28 { char s[7]; } Str82009D28;
typedef struct Str82009D40 { char s[11]; } Str82009D40;
typedef struct Mon82004020 { s32 data[0x14]; } Mon82004020;
typedef struct Entry82004020 {
    /* 0x00 */ s32 unk_00;
    /* 0x04 */ Mon82004020 mon;
} Entry82004020; // size = 0x54
typedef struct Src82004020 {
    /* 0x00 */ u8 unk_00;
    /* 0x01 */ u8 unk_01;
    /* 0x02 */ u8 unk_02;
    /* 0x04 */ s32 unk_04;
    /* 0x08 */ char name[0x10];
    /* 0x18 */ s32 count;
    /* 0x1C */ Entry82004020 entries[1];
} Src82004020;
extern char D_82009D28[];
extern char D_82009D40[];
char* HAL_Strcpy(char*, const char*);
/**
 * Copies a picked team into a battle roster: its name (the preset or saved name), source buffer
 * and its non-empty Pokemon, then builds the display copy.
 * Seen in play: runs on 「エントリーけってい」.
 * Seen in play (2026-11-13) ran twice once both sides had confirmed 「このデータでエントリーします。」, once for
 * 1P's ガール team and once for the COM team.
 */
void FreeBattle_CopyTeamToRoster(TeamRoster* slot, ExtendedRosterInfo* info, s32 player, Src82004020* team) {
    s32 i;
    s32 n;
    char* name;

    if (player == 5) {
        slot->isHuman = 2;
        slot->controllerPort = 1;
        slot->controller = &gControllers[1];
        name = slot->trainerName;
        *(Str82009D28*)name = *(Str82009D28*)D_82009D28;
        slot->trainerId = 0;
    } else {
        slot->isHuman = 1;
        slot->controllerPort = player - 1;
        slot->controller = &gControllers[player - 1];
        name = slot->trainerName;
        if (team->unk_01 == 0x20 && player == team->unk_02 + 1) {
            HAL_Strcpy(name, team->name);
            slot->trainerId = team->unk_04;
        } else {
            slot->trainerId = 0;
            HAL_Strcpy(name, D_820090C0.playerNames[player - 1]); // PORT: was ((char**)&D_820090C0)[player]
        }
    }
    for (i = 0, n = 0; i < team->count; i++) {
        if (team->entries[i].unk_00 == 0) {
            ((Mon82004020*)slot->pokemonPicked)[n++] = team->entries[i].mon;
        }
    }
    slot->numberOfPokemonBrought = n;
    slot->rosterInfo = info;
    info->unk_000 = 0;
    info->numberOfPokemonCanBePicked = slot->numberOfPokemonBrought;
    bcopy(slot->pokemonPicked, info->battleMon, 0x1E0); // PORT: dropped an ignored fourth argument
    *(Str82009D40*)info->title = *(Str82009D40*)D_82009D40;
    HAL_Strcpy(info->fullName, name);
}

/**
 * Zeroes one side's roster header in gSessionContext.battleSides.
 * Seen in play: runs on 「エントリーけってい」.
 */
void FreeBattle_ClearSideRoster(BattleSide* side) {
    side->modelFlags = 0;
    side->tagTeamSize = 0;
    side->displayModel = 0;
    side->displayMon = NULL;
    side->loadedModel = NULL;
    side->teamMember[0] = NULL;
    side->teamMember[1] = NULL;
    side->backgroundLoadFlags = 0;
    side->backgroundFileList = 0;
}

extern SessionContext* D_8200ACF8;
void Session_LoadPortSnapshot(u8);

/**
 * Builds the battle rosters in gSessionContext (through D_8200ACF8) for each side's picked teams
 * (FreeBattle_CopyTeamToRoster), clears both side headers first and, in modes 1 and 2, loads the
 * picked port's session snapshot.
 * Seen in play: runs on 「エントリーけってい」.
 * Seen in play (2026-11-13) ran once after both Free Battle teams were confirmed, just before the
 * VS screen, and called FreeBattle_CopyTeamToRoster for each side.
 */
void FreeBattle_BuildBattleRosters(s32* side0_players, s32* side1_players, Src82004020** teams, s32 mode, s32 rule_category, s32 mode_selection) {
    Src82004020* src;

    D_8200ACF8->mode = mode;
    D_8200ACF8->ruleCategory = rule_category;
    if (D_8200ACF8->mode == 2) {
        D_8200ACF8->modeSelection = mode_selection;
    }
    if ((D_8200ACF8->mode == 1) || (D_8200ACF8->mode == 2)) {
        if (side0_players[0] > 0) {
            src = teams[side0_players[0] - 1];
            if (src->unk_01 == 0x20) {
                D_8200ACF8->teamGbPort = src->unk_02;
            } else {
                *(s8*)&D_8200ACF8->teamGbPort = -1; // FAKE: an s8 store is needed to match
            }
            Session_LoadPortSnapshot(D_8200ACF8->teamGbPort);
        }
    }
    // One word store over aiDifficulty..moveLimitSeconds and one over gbStarterChoice..continueCount.
    *(u32*)&D_8200ACF8->aiDifficulty = -1;
    *(u32*)&D_8200ACF8->gbStarterChoice = -1;
    FreeBattle_ClearSideRoster(&D_8200ACF8->battleSides[0]);
    FreeBattle_ClearSideRoster(&D_8200ACF8->battleSides[1]);
    D_8200ACF8->trainerSlots[0].isHuman = 0;
    D_8200ACF8->trainerSlots[1].isHuman = 0;
    D_8200ACF8->trainerSlots[2].isHuman = 0;
    D_8200ACF8->trainerSlots[3].isHuman = 0;
    if (side0_players[0] > 0) {
        FreeBattle_CopyTeamToRoster(&D_8200ACF8->trainerSlots[0], &D_8200ACF8->rosterInfo[0], side0_players[0], teams[side0_players[0] - 1]);
        D_8200ACF8->battleSides[0].teamMember[D_8200ACF8->battleSides[0].tagTeamSize] = &D_8200ACF8->trainerSlots[0];
        D_8200ACF8->battleSides[0].tagTeamSize++;
    }
    if (side0_players[1] > 0) {
        FreeBattle_CopyTeamToRoster(&D_8200ACF8->trainerSlots[1], &D_8200ACF8->rosterInfo[1], side0_players[1], teams[side0_players[1] - 1]);
        D_8200ACF8->battleSides[0].teamMember[D_8200ACF8->battleSides[0].tagTeamSize] = &D_8200ACF8->trainerSlots[1];
        D_8200ACF8->battleSides[0].tagTeamSize++;
    }
    if (side1_players[0] > 0) {
        FreeBattle_CopyTeamToRoster(&D_8200ACF8->trainerSlots[2], &D_8200ACF8->rosterInfo[2], side1_players[0], teams[side1_players[0] - 1]);
        D_8200ACF8->battleSides[1].teamMember[D_8200ACF8->battleSides[1].tagTeamSize] = &D_8200ACF8->trainerSlots[2];
        D_8200ACF8->battleSides[1].tagTeamSize++;
    }
    if (side1_players[1] > 0) {
        FreeBattle_CopyTeamToRoster(&D_8200ACF8->trainerSlots[3], &D_8200ACF8->rosterInfo[3], side1_players[1], teams[side1_players[1] - 1]);
        D_8200ACF8->battleSides[1].teamMember[D_8200ACF8->battleSides[1].tagTeamSize] = &D_8200ACF8->trainerSlots[3];
        D_8200ACF8->battleSides[1].tagTeamSize++;
    }
}

void FreeBattle_BuildBattleRosters(s32*, s32*, Src82004020**, s32, s32, s32);

/**
 * Commits both sides' picked teams to the battle (FreeBattle_BuildBattleRosters with the entry
 * screen's state).
 * Seen in play: runs when はい confirms the COM's team and 「エントリーけってい」 appears.
 */
void FreeBattle_CommitEntries(FreeBattleScreen* screen) {
    FreeBattle_BuildBattleRosters(screen->side0Players, screen->side1Players, (Src82004020**)screen->playerTeams, screen->unk_4370, screen->unk_4374, screen->unk_501C);
}
