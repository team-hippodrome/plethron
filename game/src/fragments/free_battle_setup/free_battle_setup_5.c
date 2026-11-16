/* Fixed-layout split (from FreeBattle_DrawScreen); original translation-unit boundaries remain unconfirmed. */

#include "global.h"
#include "m2c_macros.h"
#include "free_battle_setup.h"
#include "game_state.h"



void FreeBattle_SetScreenState(FreeBattleScreen*, s32, s32);

void FreeBattle_InitBattleModeWindow(FreeBattleModeWindow* window, s32 x, s32 y, FreeBattleModeEntry* modes, char* title); // PORT: matches the definition
void FreeBattle_InitMenuWindow(MenuWindow* window, s32 x, s32 y, s32 width, s32 height, char** items, s32 item_count, char** descriptions, s32 description_lines, char* title); // PORT: matches the definition
void FreeBattle_InitCupSelect(FreeBattleCupWindow* window, s32 x, s32 y, s32 cups_cleared); // PORT: matches the definition
void FreeBattle_InitOpponentSelect(FreeBattleOpponentWindow* window, s32 x, s32 y, s32 controller_mask, s32 controller_count, s32 gb_save_ready_mask); // PORT: matches the definition
void FreeBattle_InitEntryScreen(FreeBattleScreen* arg0);
void FreeBattle_InitPopupMenu(MenuWindow* window, s32 x, s32 y, FreeBattlePopupItems* items, u16* item_flags, Color_RGBA8 color); // PORT: matches the definition
void FreeBattle_InitUsableMonGrid(FreeBattleMonGridWindow* window, s32 x, s32 y, s32 rule_category, BattleMon* all_mons, BattleMon* l130_mons, PokeIconRenderer* icons); // PORT: matches the definition
void BlinkingTextBox_Init(BlinkingTextBox* arg0, const char* arg1, s32 arg2, s32 arg3, u32 arg4);
s32 Save_GetProgressFlags(void);
extern u8 D_82009004[];
extern u8 D_82009D4C[];
extern u8 D_82009D64[];
extern u8 D_82009D7C[];
extern u8 D_82009D9C[];
extern u8 D_82009DBC[];
extern u8 D_82009DCC[];
extern u8 D_82009DE4[];
extern u8 D_82008F54[];
extern u8 D_82009DFC[];

/**
 * Opens the window for screen state `state`: battle mode, rules, tournaments, cup, opponent, entry and popups.
 * Seen in play (2026-11-13) ran as each Free Battle window opened in turn: battle mode, rules
 * (フリーバトル), たいせんする, the opponent picker (1P VS COM) and the entry list.
 */
void FreeBattle_SetScreenState(FreeBattleScreen* screen, s32 state, s32 flags) {
    Window* view;

    switch (state) {
        case 1:
            if (!(flags & 2)) {
                FreeBattle_InitBattleModeWindow(&screen->modeWindow, 120, 99, D_82009014, (char*)D_82009004);
            }
            view = &screen->modeWindow.window;
            break;
        case 2:
            if (!(flags & 2)) {
                FreeBattle_InitMenuWindow(&screen->menuWindow, 0x40, 116, 408, 206, D_82008FCC, 3, D_82008FD8, 3, (char*)D_82009D4C);
            }
            view = &screen->menuWindow.window;
            break;
        case 3:
            if (!(flags & 2)) {
                FreeBattle_InitMenuWindow(&screen->menuWindow, 0x40, 116, 448, 286, D_82008F9C, 2, D_82008FA4, 7, (char*)D_82009D64);
            }
            view = &screen->menuWindow.window;
            break;
        case 7:
            if (!(flags & 2)) {
                screen->side0Players[0] = 1;
                screen->side0Players[1] = 0;
                screen->side1Players[0] = 0;
                screen->side1Players[1] = 0;
                screen->playerActive[0] = 1;
                screen->playerActive[4] = 0;
                screen->playerActive[3] = 0;
                screen->playerActive[2] = 0;
                screen->playerActive[1] = 0;
                screen->unk_4370 = 1;
                screen->unk_4374 = 1;
                FreeBattle_InitMenuWindow(&screen->ruleMenuWindow, 80, 140, 448, 174, D_82008FBC, 2, D_82008FC4, 3, (char*)D_82009D7C);
            }
            view = &screen->ruleMenuWindow.window;
            break;
        case 8:
            if (!(flags & 2)) {
                screen->side0Players[0] = 1;
                screen->side0Players[1] = 0;
                screen->side1Players[0] = 0;
                screen->side1Players[1] = 0;
                screen->playerActive[0] = 1;
                screen->playerActive[4] = 0;
                screen->playerActive[3] = 0;
                screen->playerActive[2] = 0;
                screen->playerActive[1] = 0;
                screen->unk_4370 = 2;
                screen->unk_4374 = 2;
                FreeBattle_InitMenuWindow(&screen->ruleMenuWindow, 80, 140, 448, 174, D_82008FAC, 2, D_82008FB4, 3, (char*)D_82009D9C);
            }
            view = &screen->ruleMenuWindow.window;
            break;
        case 9:
            if (!(flags & 2)) {
                FreeBattle_InitCupSelect(&screen->cupWindow, 0x60, 0xA4, Save_GetProgressFlags() & 0xFF);
            }
            view = &screen->cupWindow.window;
            break;
        case 4:
            if (!(flags & 2)) {
                screen->unk_4370 = 0;
                switch (screen->unk_4374) {
                    case 0:
                        FreeBattle_InitMenuWindow(&screen->ruleMenuWindow, 80, 140, 428, 146, D_82008FE4, 2, D_82008FEC, 2, (char*)D_82009DBC);
                        break;
                    case 1:
                        FreeBattle_InitMenuWindow(&screen->ruleMenuWindow, 80, 140, 428, 146, D_82008FE4, 2, D_82008FFC, 2, (char*)D_82009DCC);
                        break;
                    case 2:
                        FreeBattle_InitMenuWindow(&screen->ruleMenuWindow, 80, 140, 428, 146, D_82008FE4, 2, D_82008FF4, 2, (char*)D_82009DE4);
                        break;
                }
            }
            view = &screen->ruleMenuWindow.window;
            break;
        case 5:
            if (!(flags & 2)) {
                FreeBattle_InitOpponentSelect(&screen->opponentWindow, 176, 84, screen->connectedControllerMask, screen->connectedControllerCount, screen->gbSaveReadyMask);
            }
            view = &screen->opponentWindow.window;
            break;
        case 10:
            if (!(flags & 2)) {
                FreeBattle_InitEntryScreen(screen);
            }
            view = &screen->entry.window;
            break;
        case 12:
            if (!(flags & 2)) {
                FreeBattle_InitPopupMenu((MenuWindow*)&screen->popupMenu, 38, 90, &D_82009278[screen->unk_4374], screen->popupMenuFlags, *(Color_RGBA8*)D_82008F54);
            }
            view = &screen->popupMenu.window;
            break;
        case 13:
            if (!(flags & 2)) {
                FreeBattle_InitPopupMenu((MenuWindow*)&screen->popupMenu2, 0x36, 0x72, &D_820093E0[screen->unk_4374][screen->popupMenu.menu.cursor], screen->popupMenu2Flags, *(Color_RGBA8*)D_82008F54); // PORT: was popupMenu.menu.cursor (+0x4E84) + D_820093E0[...]
            }
            view = &screen->popupMenu2.window;
            break;
        case 14:
            if (!(flags & 2)) {
                FreeBattle_InitUsableMonGrid(&screen->monGridWindow, 37, 80, screen->unk_4374, D_8200ACFC, D_8200AD00, D_8200AC50);
            }
            view = &screen->monGridWindow.window;
            break;
        case 11:
            if (!(flags & 2)) {
                BlinkingTextBox_Init(&screen->textBox, (const char*)D_82009DFC, 0, 1, 0);
            }
            break;
    }
    if (flags & 1) {
        screen->view->animTimer = 7;
        screen->pendingState = state;
        screen->nextView = view;
    } else {
        screen->state = state;
        screen->view = view;
        screen->pendingState = 0;
    }
}

u8 GbSave_GetSaveState(s32 arg0);
u8 Cont_GetConnectedMask(void);

/**
 * FreeBattle_InitBattleModeScreen
 *
 * Recomputes two masks in the object at screen and then calls
 * FreeBattle_SetScreenState(screen, 1, 0). gbSaveReadyMask gets bit i set for each i in 0..3
 * where GbSave_GetSaveState(i) returns 0. connectedControllerMask is set to Cont_GetConnectedMask()
 * and connectedControllerCount to the number of bits among bits 0..3 of that value that
 * are set.
 * Seen in play (2026-11-13) ran once as the バトルモード window (フリーバトル / トーナメント) opened from the main
 * menu.
 */
void FreeBattle_InitBattleModeScreen(FreeBattleScreen* screen) {
    s32 i;

    screen->gbSaveReadyMask = 0;
    for (i = 0; i < 4; i++) {
        if (GbSave_GetSaveState(i) == 0) {
            screen->gbSaveReadyMask |= 1 << i;
        }
    }
    screen->connectedControllerMask = Cont_GetConnectedMask();
    screen->connectedControllerCount = 0;
    for (i = 0; i < 4; i++) {
        if ((screen->connectedControllerMask >> i) & 1) {
            screen->connectedControllerCount += 1;
        }
    }
    FreeBattle_SetScreenState(screen, 1, 0);
}

void FreeBattle_AddPresetTeams(void* /* List82000594* */ list);
void FreeBattle_LoadGbTeams(void* /* RecordList* */ list);
void func_80027804(PointerList* arg0, void** arg1, s32 arg2, s32 arg3); // PORT: matches PointerList_Initialize
void FreeBattle_AddRentalEntry(void* /* RecordList* */ list); // PORT: no prototype in the decomp
void FreeBattle_BuildTeamPointers(PointerList* dest, ByteMatrix* teams); // PORT: no prototype in the decomp
void FreeBattle_InitEntrySelect(FreeBattleEntryScreen* entry, s32 x, s32 y, PointerList* teams, FreeBattleTeam** picks, s32 rule_category); // PORT: no prototype in the decomp
void FreeBattle_MarkIneligibleMons(void* /* TeamList8200140C* */ list, s32 rule_category); // PORT: no prototype in the decomp
/**
 * Builds the team list from the GB cartridges and rentals and opens the entry list.
 * Seen in play: A on COM opens 「エントリーポケモンのせんたく」 and runs this.
 */
void FreeBattle_InitEntryScreen(FreeBattleScreen* screen)
{
  s32 i;
  screen->rentalsAllowed = (!((screen->unk_4370) == 0)) || (!((screen->unk_4374) == 0));
  screen->presetsAllowed = ((screen->unk_4370) == 0) && ((screen->unk_4374) == 0);
  TableView_Initialize(&screen->teamTable, screen->teams, sizeof(FreeBattleTeam), 0x20, 0);
  FreeBattle_LoadGbTeams(&screen->teamTable);
  if (screen->rentalsAllowed)
  {
    FreeBattle_AddRentalEntry(&screen->teamTable);
  }
  if (screen->presetsAllowed)
  {
    FreeBattle_AddPresetTeams(&screen->teamTable);
  }
  func_80027804(&screen->teamList, (void**)screen->teamPtrs, 0x20, 0);
  FreeBattle_BuildTeamPointers(&screen->teamList, &screen->teamTable);
  FreeBattle_MarkIneligibleMons(&screen->teamTable, screen->unk_4374);
  screen->currentPlayer = 0;
    for (i = 0; i < 5; i++) { screen->playerTeams[i] = NULL;
  }

  FreeBattle_InitEntrySelect(&screen->entry, 0x30, 0x5C, &screen->teamList, screen->playerTeams, screen->unk_4374);
}

void BgStage_DrawFrame(void);
M2C_UNK Ui_DrawTintedScreenBackground(M2C_UNK, M2C_UNK, u8, u8, s32);
extern u8 D_82008F50[];
void FreeBattle_DrawBattleModeWindow(FreeBattleModeWindow*);
void FreeBattle_DrawMenuWindow(MenuWindow*, s32);
void FreeBattle_DrawPopupMenu(MenuWindow*, s32);
void FreeBattle_DrawUsableMonGrid(FreeBattleMonGridWindow*, s32);
void FreeBattle_DrawOpponentSelect(FreeBattleOpponentWindow*);
void FreeBattle_DrawTagOpponentSelect(FreeBattleTeamOpponentWindow*);
void FreeBattle_DrawCupSelect(FreeBattleCupWindow*, s32);
void FreeBattle_DrawRentalScreen(FreeBattleEntryScreen*);
void BlinkingTextBox_Draw(BlinkingTextBox*);
/**
 * Draws the battle setup screen for its current state.
 * Seen in play: runs every frame on the battle menu screens.
 */
M2C_UNK FreeBattle_DrawScreen(FreeBattleScreen* screen) {
    FreeBattleScreen* base = screen; // PORT: was (s8*)screen with raw N64 offsets

    BgStage_DrawFrame();
    Ui_DrawTintedScreenBackground(56, 480, D_82008F50[0], D_82008F50[1], D_82008F50[2]);
    switch (*(s32*)base) {
        case 1:
            FreeBattle_DrawBattleModeWindow(&base->modeWindow);
            break;
        case 2:
        case 3:
            FreeBattle_DrawMenuWindow(&base->menuWindow, 2);
            break;
        case 4:
            FreeBattle_DrawMenuWindow(&base->menuWindow, 0x10);
            FreeBattle_DrawMenuWindow(&base->ruleMenuWindow, 2);
            break;
        case 12:
            FreeBattle_DrawPopupMenu((MenuWindow*)&base->popupMenu, 2);
            break;
        case 13:
            FreeBattle_DrawPopupMenu((MenuWindow*)&base->popupMenu, 0x10);
            FreeBattle_DrawPopupMenu((MenuWindow*)&base->popupMenu2, 0);
            break;
        case 14:
            FreeBattle_DrawUsableMonGrid(&base->monGridWindow, 0);
            break;
        case 5:
            FreeBattle_DrawMenuWindow(&base->menuWindow, 0x10);
            FreeBattle_DrawMenuWindow(&base->ruleMenuWindow, 0x10);
            FreeBattle_DrawOpponentSelect(&base->opponentWindow);
            break;
        case 6:
            FreeBattle_DrawMenuWindow(&base->menuWindow, 0x10);
            FreeBattle_DrawMenuWindow(&base->ruleMenuWindow, 0x10);
            FreeBattle_DrawTagOpponentSelect(&base->tagOpponentWindow);
            break;
        case 7:
        case 8:
            FreeBattle_DrawMenuWindow(&base->menuWindow, 0x10);
            FreeBattle_DrawMenuWindow(&base->ruleMenuWindow, 2);
            break;
        case 9:
            FreeBattle_DrawMenuWindow(&base->menuWindow, 0x10);
            FreeBattle_DrawMenuWindow(&base->ruleMenuWindow, 0x10);
            FreeBattle_DrawCupSelect(&base->cupWindow, 2);
            break;
        case 10:
            FreeBattle_DrawRentalScreen(&base->entry);
            break;
        case 11:
            FreeBattle_DrawRentalScreen(&base->entry);
            BlinkingTextBox_Draw(&base->textBox);
            break;
    }
}


void func_80027814(PointerList* arg0, char* arg1, s32 arg2); // PORT: matches PointerList_Insert
s32 Ui_GetSelectionDirection(Controller* arg0); // PORT: matches the definition
void Audio_PlaySystemSound(s32);
s32 Audio_PlayCommand(u32 arg0, s32 arg1, s32 arg2);
void FreeBattle_ClearIndices(void*, s32, s32);
void FreeBattle_InitEntryListCursor(FreeBattleEntryScreen*);
s32 FreeBattle_UpdateEntrySelect(FreeBattleEntryScreen*);
void FreeBattle_CommitEntries(FreeBattleScreen*);
s32 FreeBattle_UpdateMenuWindow(MenuWindow*);
s32 FreeBattle_UpdatePopupMenu(MenuWindow*);
s32 FreeBattle_UpdateOpponentSelect(FreeBattleOpponentWindow*);
s32 FreeBattle_UpdateBattleModeWindow(FreeBattleModeWindow*);
s32 FreeBattle_UpdateCupSelect(FreeBattleCupWindow*);
s32 FreeBattle_UpdateUsableMonGrid(FreeBattleMonGridWindow*);
extern struct Controller* D_800603C0; // PORT: gPlayer1Controller, a pointer

/**
 * Runs the battle setup screen's state machine for one frame.
 * Seen in play (2026-11-13) ran every frame from the battle mode window through the entry list.
 */
s32 FreeBattle_UpdateScreen(FreeBattleScreen* screen) {
    s32 ret;
    s32 res;
    s32 pad; // FAKE: unused stack word
    s32 idx;
    s32 sel;

    ret = 0;
    res = screen->pendingState;
    if (res != 0) {
        if (screen->view->state == 4) {
            screen->state = res;
            if (res == 0xF) {
                ret = 1;
            } else {
                screen->pendingState = 0;
                screen->view = screen->nextView;
            }
            return ret;
        }
        if ((screen->view->state != 1) && (screen->view->state != 2)) {
            return 0;
        }
    }
    switch (screen->state) {
        case 1:
            res = FreeBattle_UpdateBattleModeWindow(&screen->modeWindow);
            if (res == -1) {
                FreeBattle_SetScreenState(screen, 0xF, 1);
                D_82008F84.code = 0x8040;
            } else if (res == 1) {
                FreeBattle_SetScreenState(screen, 2, 1);
            } else if (res == 2) {
                FreeBattle_SetScreenState(screen, 3, 1);
            }
            break;
        case 3:
            res = FreeBattle_UpdateMenuWindow(&screen->menuWindow);
            if (res == -1) {
                FreeBattle_SetScreenState(screen, 1, 1);
            } else if (res == 1) {
                FreeBattle_SetScreenState(screen, 8, 0);
            } else if (res == 2) {
                FreeBattle_SetScreenState(screen, 7, 0);
            }
            break;
        case 2:
            res = FreeBattle_UpdateMenuWindow(&screen->menuWindow);
            if (res == -1) {
                FreeBattle_SetScreenState(screen, 1, 1);
            } else if (res == 1) {
                screen->unk_4374 = 0;
                FreeBattle_SetScreenState(screen, 4, 0);
            } else if (res == 2) {
                screen->unk_4374 = 2;
                FreeBattle_SetScreenState(screen, 4, 0);
            } else if (res == 3) {
                screen->unk_4374 = 1;
                FreeBattle_SetScreenState(screen, 4, 0);
            }
            break;
        case 4:
            res = FreeBattle_UpdateMenuWindow(&screen->ruleMenuWindow);
            if (res == -1) {
                FreeBattle_SetScreenState(screen, 2, 3);
            } else if (res == 1) {
                FreeBattle_SetScreenState(screen, 5, 0);
            } else if (res == 2) {
                screen->menuWindow.window.animTimer = 7;
                FreeBattle_SetScreenState(screen, 0xC, 1);
            }
            break;
        case 12:
            res = FreeBattle_UpdatePopupMenu((MenuWindow*)&screen->popupMenu);
            if (res == -1) {
                sel = (screen->unk_4370 == 0) ? 2 : 3;
                FreeBattle_SetScreenState(screen, sel, 1);
            } else if (res > 0) {
                Audio_PlaySystemSound(2);
                if (res == D_82009278[screen->unk_4374].count) {
                    FreeBattle_SetScreenState(screen, 0xE, 1);
                } else {
                    FreeBattle_SetScreenState(screen, 0xD, 0);
                }
            }
            break;
        case 13:
            res = Ui_GetSelectionDirection(D_800603C0);
            if ((res == -1) || (res == 1)) {
                Audio_PlaySystemSound(3);
                FreeBattle_SetScreenState(screen, 0xC, 3);
            }
            break;
        case 14:
            if (FreeBattle_UpdateUsableMonGrid(&screen->monGridWindow) == -1) {
                FreeBattle_SetScreenState(screen, 0xC, 1);
            }
            break;
        case 5:
            res = FreeBattle_UpdateOpponentSelect(&screen->opponentWindow);
            if (res == -1) {
                FreeBattle_SetScreenState(screen, 4, 3);
            } else if (res == 1) {
                screen->side0Players[0] = 1;
                screen->side0Players[1] = 0;
                screen->side1Players[1] = 0;
                screen->playerActive[0] = 1;
                screen->playerActive[4] = 0;
                screen->playerActive[3] = 0;
                screen->playerActive[2] = 0;
                screen->playerActive[1] = 0;
                screen->side1Players[0] = screen->opponentWindow.choice;
                screen->playerActive[screen->opponentWindow.choice - 1] = 1;
                screen->ruleMenuWindow.window.animTimer = 7;
                screen->menuWindow.window.animTimer = 7;
                FreeBattle_SetScreenState(screen, 0xA, 1);
            }
            break;
        case 7:
            res = FreeBattle_UpdateMenuWindow(&screen->ruleMenuWindow);
            if (res == -1) {
                FreeBattle_SetScreenState(screen, 3, 3);
            } else if (res == 1) {
                screen->unk_501C = 0;
                screen->menuWindow.window.animTimer = 7;
                FreeBattle_SetScreenState(screen, 0xA, 1);
            } else if (res == 2) {
                screen->menuWindow.window.animTimer = 7;
                FreeBattle_SetScreenState(screen, 0xC, 1);
            }
            break;
        case 8:
            res = FreeBattle_UpdateMenuWindow(&screen->ruleMenuWindow);
            if (res == -1) {
                FreeBattle_SetScreenState(screen, 3, 3);
            } else if (res == 1) {
                FreeBattle_SetScreenState(screen, 9, 0);
            } else if (res == 2) {
                screen->menuWindow.window.animTimer = 7;
                FreeBattle_SetScreenState(screen, 0xC, 1);
            }
            break;
        case 9:
            res = FreeBattle_UpdateCupSelect(&screen->cupWindow);
            if (res == -1) {
                FreeBattle_SetScreenState(screen, 8, 3);
            } else if (res > 0) {
                screen->menuWindow.window.animTimer = 7;
                screen->ruleMenuWindow.window.animTimer = 7;
                screen->unk_501C = screen->cupWindow.menu.cursor; // PORT: was +0x4520
                FreeBattle_SetScreenState(screen, 0xA, 1);
            }
            break;
        case 10:
            res = FreeBattle_UpdateEntrySelect(&screen->entry);
            if (res == -1) {
                if (screen->modeWindow.menu.cursor == 0) { // PORT: was +0x43AC
                    FreeBattle_SetScreenState(screen, 2, 1);
                } else {
                    FreeBattle_SetScreenState(screen, 3, 1);
                }
            } else if (res == 1) {
                if (screen->entry.teamChanged != 0) {
                    screen->teamTable.count++;
                    screen->entry.team.flags |= 1;
                    screen->teams[screen->teamTable.count - 1] = screen->entry.team;
                    idx = screen->teamList.count + ((screen->rentalsAllowed != 0) ? -1 : 0);
                    func_80027814(&screen->teamList, (char*)&screen->teams[screen->teamTable.count - 1], idx);
                } else {
                    idx = screen->entry.teamList.cursor;
                }
                screen->playerTeams[screen->currentPlayer] = screen->teamPtrs[idx];
                screen->currentPlayer++;
                if (screen->currentPlayer < 5) {
                loop:
                    if (screen->playerActive[screen->currentPlayer] == 0) {
                        screen->currentPlayer++;
                        if (screen->currentPlayer < 5) {
                            goto loop; // FAKE: goto loop kept from m2c output
                        }
                    }
                }
                if (screen->currentPlayer >= 5) {
                    FreeBattle_CommitEntries(screen);
                    Audio_PlayCommand(0x53, 0, 0);
                    FreeBattle_SetScreenState(screen, 0xB, 0);
                } else {
                    screen->entry.player = screen->currentPlayer + 1;
                    if (screen->entry.teamChanged != 0) {
                        FreeBattle_ClearIndices(D_8200AC58, (screen->entry.teamList.cursor * 6) % 24, 6);
                    }
                    FreeBattle_InitEntryListCursor(&screen->entry);
                }
            }
            break;
        case 11:
            if (screen->textBox.blinkTimer >= 0x19) {
                screen->textBox.noBlink = 1;
                ret = 1;
                D_82008F84.code = 0x8050;
            }
            break;
    }
    return ret;
}

/*
 * FreeBattle_RestoreMenuState
 *
 * Writes two words in the structure `arg0` points to, derived from the
 * gSessionContext.mode and gSessionContext.ruleCategory, and then
 * calls FreeBattle_SetScreenState(arg0, 2, 0) or FreeBattle_SetScreenState(arg0, 3, 0).
 *
 *   gSessionContext.mode == 0:
 *     arg0->0x43AC = 0
 *     arg0->0x4418 = 0 if gSessionContext.ruleCategory == 0, else 1 if it is 2, else 2
 *     FreeBattle_SetScreenState(arg0, 2, 0)
 *   otherwise:
 *     arg0->0x43AC = 1
 *     arg0->0x4418 = 0 if gSessionContext.mode == 2, else 1
 *     FreeBattle_SetScreenState(arg0, 3, 0)
 *
 * FreeBattle_SetScreenState is still assembly, so what the 2 / 3 select is not
 * established.  The first argument of every call is `arg0` itself; the ROM
 * never writes $a0 before the call, so it is the caller's argument passed
 * through.
 */

/**
 * Reopens the rule menu with the choice saved from the previous visit.
 * Seen in play (2026-11-14): a write watchpoint on +0x4418 stopped in it when a surrendered COM
 * Free Battle returned to the battle setup screen, which reopened on 「たいせんルールせんたく」 (state 2)
 * instead of the バトルモード window.
 */
void FreeBattle_RestoreMenuState(FreeBattleScreen* screen) {
    s32 var_v0;

    if (gSessionContext.mode == 0) {
        screen->modeWindow.menu.cursor = 0;
        if (gSessionContext.ruleCategory == 0) {
            screen->menuWindow.menu.cursor = 0;
        } else {
            if (gSessionContext.ruleCategory == 2) {
                var_v0 = 1;
            } else {
                var_v0 = 2;
            }
            screen->menuWindow.menu.cursor = var_v0;
        }
        FreeBattle_SetScreenState(screen, 2, 0);
        return;
    }
    screen->modeWindow.menu.cursor = 1;
    if (gSessionContext.mode == 2) {
        screen->menuWindow.menu.cursor = 0;
    } else {
        screen->menuWindow.menu.cursor = 1;
    }
    FreeBattle_SetScreenState(screen, 3, 0);
}

extern s32 D_8200AD2C;
extern M2C_UNK D_8200AD30;
s32 GbSave_GetActivePort(void);
void func_80027FF8(GbSavePortSummary* arg0, s32 arg1); // PORT: matches GbSave_BuildPortSummary
void* Util_Malloc(s32 size); // PORT: matches the definition
/**
 * Reads the GB save summary of the active port (if any), allocates the 0x5020-byte
 * FreeBattleScreen (D_8200AD28) and opens it on the battle mode window.
 * Seen in play (2026-11-14): ran once when バトル was entered from the main menu, before the
 * バトルモード window appeared.
 */
void FreeBattle_CreateScreen(void) {
    D_8200AD2C = GbSave_GetActivePort();
    if (D_8200AD2C != -1) {
        func_80027FF8(&D_8200AD30, D_8200AD2C);
    }
    D_8200AD28 = Util_Malloc(sizeof(FreeBattleScreen)); // PORT: was 0x5020, the N64 size
    FreeBattle_InitBattleModeScreen(D_8200AD28);
}

typedef struct Col8200579C { u8 r, g, b, a; } Col8200579C;
extern s32 D_820090D4;
extern s32 D_820090D8;
/*
 * Second name for D_820090D4. The fade-out store in this function family
 * (also FreeBattle_UpdateFade, LabTerminal_UpdateFade, LabPCList_UpdateFade, Pokedex_DrawFadeFrame) does not
 * share the counter's address with the load before it, which only happens
 * when the two accesses name different symbols.
 */
extern s32 D_820090D4_2;
extern char D_82009E10[]; // PORT: text (was M2C_UNK, lifted as swapped words)
M2C_UNK GbSave_DrawSummaryBar(M2C_UNK, M2C_UNK *, M2C_UNK *, s32);
void BgStage_DrawFrame(void);
M2C_UNK Ui_DrawTintedScreenBackground(M2C_UNK, M2C_UNK, u8, u8, s32);
Color_RGBA8 func_8002C8D8(Color_RGBA8 arg0, f32 arg1); // PORT: matches Color_InterpolateFromGray
/**
 * Fade callback: draws the stage, the GB summary bar and the screen background tinted by an
 * 8-step counter that counts up to 8 and then back down.
 * Seen in play (2026-11-14): ran as the battle setup screen faded in from the main menu, and
 * again as it faded out when B left the バトルモード window.
 */
s32 FreeBattle_UpdateFade(FreeBattleScreen* screen) {
    Col8200579C sp2C;
    M2C_UNK *var_a1;
    s32 n;

    n = D_820090D4;
    if (n >= 8) {
        D_820090D8 = 1;
    }
    if (D_820090D8 != 0) {
        n--;
        D_820090D4_2 = n;
    }
    BgStage_DrawFrame();
    if (D_8200AD2C == -1) {
        var_a1 = NULL;
    } else {
        var_a1 = &D_8200AD30;
    }
    GbSave_DrawSummaryBar(0, var_a1, D_82009E10, D_8200AD2C);
    n = D_820090D4;
    *(Color_RGBA8*)&sp2C = func_8002C8D8(*(Color_RGBA8*)&D_82008F50, n / (f64)8.0f); // PORT: the color is the return value (the N64 ABI passed &sp2C)
    Ui_DrawTintedScreenBackground(56, 480, sp2C.r, sp2C.g, sp2C.b);
    if (D_820090D8 == 0) {
        n = D_820090D4;
        n++;
        D_820090D4 = n;
    }
    return 0;
}

void Cont_StartReadInputs(void);                            /* extern */
void Cont_ReadInputs(void);                            /* extern */
M2C_UNK FreeBattle_DrawScreen(FreeBattleScreen*);
s32 FreeBattle_UpdateScreen(FreeBattleScreen*);

/**
 * One frame of the battle setup screen.
 * Seen in play: runs every frame on the battle menu screens (バトルモード, たいせんルールせんたく).
 */
void FreeBattle_RunFrame(FreeBattleScreen* screen) {
    Cont_StartReadInputs();
    Cont_ReadInputs();
    FreeBattle_DrawScreen(D_8200AD28);
    FreeBattle_UpdateScreen(D_8200AD28);
}

M2C_UNK GbSave_DrawSummaryBar(M2C_UNK, M2C_UNK *, M2C_UNK *, s32); /* extern */
M2C_UNK FreeBattle_DrawScreen(FreeBattleScreen*);
extern char D_82009E18[]; // PORT: text (was M2C_UNK, lifted as swapped words)
extern s32 D_8200AD2C;
extern M2C_UNK D_8200AD30;

/**
 * Draws the setup screen and the GB save summary bar.
 * Seen in play: runs as 「エントリーけってい」 fades out after both teams are entered.
 */
s32 FreeBattle_DrawScreenWithSummary(FreeBattleScreen* screen) {
    M2C_UNK *var_a1;

    FreeBattle_DrawScreen(D_8200AD28);
    if (D_8200AD2C == -1) {
        var_a1 = NULL;
    } else {
        var_a1 = &D_8200AD30;
    }
    GbSave_DrawSummaryBar(0, var_a1, D_82009E18, D_8200AD2C);
    return 0;
}

void Util_Free(void* ptr); // PORT: matches the definition

/**
 * Frees the FreeBattleScreen allocated by FreeBattle_CreateScreen.
 * Seen in play (2026-11-14): ran after B on the バトルモード window, as the screen closed.
 */
void FreeBattle_FreeScreen(void) {
    Util_Free(D_8200AD28);
}

extern s32 D_820090DC[8];
void* BinArchive_GetFile(void* archive, s32 file_number); // PORT: matches the definition

/**
 * Loads the eight files listed in D_820090DC from the archive D_8200AC40 into D_8200AD08.
 * Seen in play (2026-11-14): ran once while the battle setup screen was set up; what the eight
 * files hold was not checked.
 */
void FreeBattle_LoadArchiveFiles(void) {
    s32 i;

    for (i = 0; i < 8; i++) {
        D_8200AD08[i] = BinArchive_GetFile(D_8200AC40, D_820090DC[i]);
    }
}

s32 Font_Init(s32 load_size, s32 side);
ModelLoadContext* PokeIcon_AllocFramebuffers(s32 arg0);
PokeModelPreview* func_80018F2C(ModelLoadContext* arg0, s16 arg1, s16 arg2, s16 arg3, s16 arg4, s16 arg5, s16 arg6, u8* arg7); // PORT: matches PokeIcon_CreateModelPreview
PokeIconRenderer* func_80017FDC(ModelLoadContext* arg0, s16 arg1, s16 arg2, s16 arg3); // PORT: matches PokeIcon_CreateRenderList
void* BinArchive_Open(u8* rom_start, u8* rom_end, s32 arg2, s32 arg3); // PORT: matches the definition
void PokeIcon_OpenModelArchives(void);
void* Fragment_Load(s32 arg0, u8* rom_start, u8* rom_end); // PORT: matches the definition
u8* Asset_LoadToSegment(s32 id, u8* rom_start, u8* rom_end, s32 arg3); // PORT: matches the definition
uintptr_t Memmap_GetSegmentVaddr(uintptr_t mask); // PORT: matches the definition
void FreeBattle_RestoreMenuState(FreeBattleScreen*);
s32 StageFade_Start(s32);
s32 func_80006828(u32 timer, s32 (*f)(u8)); // PORT: matches BgStage_RunUntilCondition
void Audio_PlayMusicIfChanged(s32);
void func_80006998(u16 color); // PORT: matches StageContext_SetClearColor
void Audio_StopMusic(s32);
void StageLoader_WaitForRetrace(void);
void StageLoader_UpdateSegments(void);
s32 FreeBattle_UpdateFade(FreeBattleScreen*);
extern u8 D_81000000[];
extern u8 D_0015B1A0[];
extern u8 D_0015E220[];
extern u8 D_01000000[];
extern u8 D_002E0BE0[];
extern u8 D_002E2B90[];
extern u8 D_0A000000[];
extern u8 D_0A000C80[];
extern u8 D_0A0016D0[];
extern u8 D_003FFD60[];
extern u8 D_00400F70[];
extern u8 D_04000000[];
extern u8 D_004076E0[];
extern u8 D_05000000[];
extern u8 D_004199E0[];
extern u8 D_0042C4A0[];
extern u8 D_002F5940[];
extern u8 D_003139C0[];
/**
 * Sets up the battle setup screen (font, Pokémon icons, model archives, segments), opens it and
 * runs it until it closes: a fade in, FreeBattle_RunFrame every frame, then a fade out, and frees
 * the screen. After a battle (transitionFlags & 0x100) it reopens the menu the player came from
 * (FreeBattle_RestoreMenuState) instead of the battle mode window.
 * Seen in play (2026-11-14): ran each time バトル was entered from the main menu, and when a
 * surrendered COM battle returned to 「たいせんルールせんたく」.
 */
M2C_UNK FreeBattle_RunSetupScreen(void) {
    Font_Init(0x3C, 0);
    D_8200AC44 = PokeIcon_AllocFramebuffers(1);
    D_8200AC48 = func_80018F2C(D_8200AC44, 0x80, 0x78, 0, 0, 0x78, 0x78, 0);
    D_8200AC48->flags &= ~2;
    D_8200AC48->flags &= ~4;
    D_8200AC4C = 0;
    D_8200AC50 = func_80017FDC(D_8200AC44, 0x28, 0x28, 0x28);
    D_8200AC40 = BinArchive_Open(D_002F5940, D_003139C0, 1, 1);
    PokeIcon_OpenModelArchives();

    Fragment_Load(((((uintptr_t)D_81000000) & 0x0FF00000) >> 0x14) - 0x10, D_0015B1A0, D_0015E220);
    Asset_LoadToSegment(((uintptr_t)D_01000000 & 0x0F000000) >> 0x18, D_002E0BE0, D_002E2B90, 0);
    Asset_LoadToSegment(((uintptr_t)D_0A000000 & 0x0F000000) >> 0x18, D_003FFD60, D_00400F70, 0);
    Asset_LoadToSegment(((uintptr_t)D_04000000 & 0x0F000000) >> 0x18, D_00400F70, D_004076E0, 0);
    Asset_LoadToSegment(((uintptr_t)D_05000000 & 0x0F000000) >> 0x18, D_004199E0, D_0042C4A0, 0);

    D_8200ACFC = (BattleMon*)Memmap_GetSegmentVaddr((uintptr_t)D_0A000000);
    D_8200AD00 = (BattleMon*)Memmap_GetSegmentVaddr((uintptr_t)D_0A000C80);
    D_8200AD04 = (u8*)Memmap_GetSegmentVaddr((uintptr_t)D_0A0016D0);
    FreeBattle_LoadArchiveFiles();
    FreeBattle_CreateScreen();
    StageLoader_UpdateSegments();

    if (gSessionContext.transitionFlags & 0x100) {
        gSessionContext.transitionFlags &= ~0x100;
        FreeBattle_RestoreMenuState(D_8200AD28);
        StageFade_Start(0xF);
        func_80006828(17, FreeBattle_DrawScreenWithSummary);
        Audio_PlayMusicIfChanged(9);
    } else {
        func_80006828(8, FreeBattle_UpdateFade);
    }
    func_80006828(0, FreeBattle_RunFrame);

    if (D_82008F84.code == 0x8050) {
        func_80006998(0xFFFF);
        StageFade_Start(0xF);
        func_80006828(17, FreeBattle_DrawScreenWithSummary);
        Audio_StopMusic(0xF);
    } else {
        func_80006828(8, FreeBattle_UpdateFade);
    }
    StageLoader_WaitForRetrace();
    FreeBattle_FreeScreen();
}

M2C_UNK MainPool_PushState(M2C_UNK);                     /* extern */
M2C_UNK MainPool_PopState(M2C_UNK);                     /* extern */
void func_80025BA0(s32 arg0, u8* arg1, u8* arg2, s32 arg3, uintptr_t arg4); // PORT: matches Fragment_QueueCall
M2C_UNK FreeBattle_RunSetupScreen();                            /* extern */
M2C_UNK FreeBattle_SetupGbCartVersus();                            /* extern */
extern s8 D_00235D70;
extern s8 D_00238BD0;
extern s8 D_88000000;
/**
 * Entry point of the battle setup fragment: pushes the 'MENT' pool state, runs the setup screen
 * (or, when `mode` is 1, FreeBattle_SetupGbCartVersus), pops the state and, when the screen was
 * left with B (0x8040), queues the main menu. Returns 0x8040 (back to the main menu) or 0x8050
 * (start the battle).
 * Seen in play (2026-11-14): ran with `mode` 0 when はい was picked on the main menu's バトル
 * prompt. Both callers (main_menu.c, main_menu_entry.c) pass 0, so the `mode` 1 branch is not
 * reachable in retail.
 */
s32 FreeBattle_Main(s32 mode, M2C_UNK unused)
{
  s32 temp_v1;
  MainPool_PushState(0x4D454E54);
  D_8200ACF8 = &gSessionContext;
  if (mode == 1)
  {
    FreeBattle_SetupGbCartVersus();
  }
  else
  {
    FreeBattle_RunSetupScreen();
  }
  MainPool_PopState(0x4D454E54);
  temp_v1 = D_82008F84.code;
  if (temp_v1 == 0x8040)
  {
    func_80025BA0((((u32) (((uintptr_t) (&D_88000000)) & 0x0FF00000)) >> 0x14) - 0x10, &D_00235D70, &D_00238BD0, 0, 0);
  }
  return D_82008F84.code;
}
