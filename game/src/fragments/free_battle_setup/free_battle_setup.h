/* Declarations for the free_battle_setup fragment (src/fragments/free_battle_setup/). */
#ifndef FRAGMENT_FREE_BATTLE_SETUP_H
#define FRAGMENT_FREE_BATTLE_SETUP_H

#include "global.h"
#include "gb_save.h"
#include "menu.h"

/* A team in the entry list (FreeBattleScreen.teams): a Game Boy party, a saved deck, a preset
 * trainer's team, the rental row, or a team being built from rentals. Holds no pointers, so the
 * host layout is the N64 one. */
typedef struct FreeBattleTeam {
    /* 0x000 */ u8 flags; // 0 for GB and deck teams, 1 a named rental team being built (FreeBattle_InitNamedTeam), 2 the レンタルポケモン row, 4 a preset trainer; bit 0 is set once a built team is entered
    /* 0x001 */ u8 source; // 0x20 a Game Boy cartridge's party, 0x10 a saved deck
    /* 0x002 */ u8 port; // Transfer Pak port of a GB team
    /* 0x003 */ u8 index; // deck or preset index
    /* 0x004 */ s32 trainerId;
    /* 0x008 */ char name[0x10];
    /* 0x018 */ s32 slotCount; // always 6
    /* 0x01C */ TeamMonSlot slots[6];
} FreeBattleTeam; // size = 0x214

/* One battle mode card of the battle mode window (D_82009014 rows). */
typedef struct FreeBattleModeEntry {
    /* 0x0 */ char* label; // printed under the card's picture (FreeBattle_DrawBattleModeCard)
    /* 0x4 */ char* text; // description printed under the cards for the cursor card; swapping the two entries' texts showed the トーナメント (Tournament) text under フリーバトル (Free Battle)
    /* 0x8 */ u8* image; // the card's picture (FreeBattle_DrawBattleModeCard)
} FreeBattleModeEntry; // size = 0xC

/* One row of the cup select window (D_820091C4). */
typedef struct FreeBattleCupEntry {
    /* 0x0 */ char* text;
    /* 0x4 */ u8* icon; // drawn when the cup is cleared
} FreeBattleCupEntry; // size = 0x8

/* The strings of a popup menu (D_82009278: the rules of each mode, D_820093E0: the details of each rule). */
typedef struct FreeBattlePopupItems {
    /* 0x00 */ char** items;
    /* 0x04 */ s8 count;
    /* 0x08 */ char* title;
} FreeBattlePopupItems; // size = 0xC

/* One move row of the status window (D_82009040). */
typedef struct FreeBattleMoveRowPos {
    /* 0x00 */ s32 x;
    /* 0x04 */ s32 y;
    /* 0x08 */ s32 centered;
    /* 0x0C */ s32 iconX;
    /* 0x10 */ u8* icon;
} FreeBattleMoveRowPos; // size = 0x14

/* The battle mode window: a 2x1 grid of cards and the description of the cursor card
 * (FreeBattle_InitBattleModeWindow). */
typedef struct FreeBattleModeWindow {
    /* 0x00 */ Window window;
    /* 0x2C */ GridMenu menu;
    /* 0x54 */ ByteMatrix items; // rows in `modes`
    /* 0x64 */ u16 itemFlags[2];
    /* 0x68 */ FreeBattleModeEntry* modes;
} FreeBattleModeWindow; // size = 0x6C

/* The cup select window (FreeBattle_InitCupSelect): up to four cups. */
typedef struct FreeBattleCupWindow {
    /* 0x00 */ Window window;
    /* 0x2C */ Menu menu;
    /* 0x50 */ ByteMatrix items; // FreeBattleCupEntry rows from D_820091C4
    /* 0x60 */ u16 itemFlags[4]; // @unverified: 0x100 for the cups below arg3, 1 (disabled) for those past the next one
} FreeBattleCupWindow; // size = 0x68

/* A message window with one number (FreeBattle_InitDuplicateWarning). */
typedef struct FreeBattleDuplicateWarning {
    /* 0x00 */ Window window;
    /* 0x2C */ char unk2C[0x20];
    /* 0x4C */ s32 value; // @unverified: printed with the format D_8200A0B0
} FreeBattleDuplicateWarning; // size = 0x50

/* A yes/no question asked of one controller (FreeBattle_InitLevelSumWarning). */
typedef struct FreeBattleLevelSumWarning {
    /* 0x00 */ Window window;
    /* 0x2C */ LabeledButton yesButton; // @unverified: label D_8200A188
    /* 0x68 */ LabeledButton noButton; // @unverified: label D_8200A190
    /* 0xA4 */ ChoiceCursor choice;
    /* 0xB0 */ Controller* controller; // @unverified: the player asked; read by Ui_GetSelectionDirection
} FreeBattleLevelSumWarning; // size = 0xB4

/* A yes/no question with a number, asked of one controller (FreeBattle_InitShortageWarning). */
typedef struct FreeBattleShortageWarning {
    /* 0x00 */ Window window;
    /* 0x2C */ LabeledButton yesButton; // @unverified: label D_8200A114
    /* 0x68 */ LabeledButton noButton; // @unverified: label D_8200A11C
    /* 0xA4 */ ChoiceCursor choice;
    /* 0xB0 */ s32 count; // @unverified: printed twice with the format D_8200A124
    /* 0xB4 */ Controller* controller; // @unverified: the player asked; read by Ui_GetSelectionDirection
} FreeBattleShortageWarning; // size = 0xB8

/* The confirm window of the rental and entry screens: two or three buttons (FreeBattle_InitRentalConfirm). */
typedef struct FreeBattleRentalConfirm {
    /* 0x00 */ Window window;
    /* 0x2C */ LabeledButton buttons[3]; // はい (Yes), いいえ (No), レンタル (Rental) under 「このデータでエントリーします。」 (Enter with this data) in play; the third only with hasThirdButton
    /* 0xE0 */ ChoiceCursor choice;
    /* 0xEC */ s32 hasThirdButton; // @unverified: arg3 of FreeBattle_InitRentalConfirm; choice.count is 3 with it, else 2 (1 and 3 with the レンタル button in play; poking 0 later did not hide it)
} FreeBattleRentalConfirm; // size = 0xF0

/* A Pokémon's status window with a はい/いいえ (yes/no) question (FreeBattle_InitConfirmWindow,
 * FreeBattle_DrawMonStatusWindow): 「レンタルしますか？」 (Rent this one?) and 「このポケモンにしますか？」. */
typedef struct FreeBattleMonStatusWindow {
    /* 0x00 */ Window window;
    /* 0x2C */ LabeledButton yesButton; // label D_82009B80
    /* 0x68 */ LabeledButton noButton; // label D_82009B88
    /* 0xA4 */ ChoiceCursor choice;
    /* 0xB0 */ BattleMon* mon;
    /* 0xB4 */ char* prompt;
} FreeBattleMonStatusWindow; // size = 0xB8

/* The opponent select window (FreeBattle_InitOpponentSelect): a menu of four opponents next to
 * the controller panel. */
typedef struct FreeBattleOpponentWindow {
    /* 0x00 */ Window window;
    /* 0x2C */ PointerList itemList;
    /* 0x38 */ Menu menu;
    /* 0x5C */ u16 itemFlags[4]; // 2P, 3P, 4P, COM; 1 1 1 0 (2P-4P grayed out) with one controller
    /* 0x64 */ s32 choice; // 2 + the row picked with A: 5 for COM
    /* 0x68 */ s32 controllerMask; // controllers plugged in, bit 1 << n for controller n + 1: 1 with only controller 1
    /* 0x6C */ char unk6C[0x4];
    /* 0x70 */ s32 unk_70; // passed to FreeBattle_DrawControllerPanel as `active`
} FreeBattleOpponentWindow; // size = 0x74

/* The opponent select window with partners, 「あいてをせんたく」 (Choose your opponent) with two
 * menus (FreeBattle_InitTagOpponentSelect): your side (1P, 1P&2P, 1P&3P, 1P&4P), then the other side (2P, 3P, 4P,
 * 2P&3P, 2P&4P, 3P&4P, COM). */
typedef struct FreeBattleTeamOpponentWindow {
    /* 0x00 */ u32 focus; // @unverified: 1 while the side menu has the cursor, 0 for the opponent menu (FreeBattle_UpdateTagOpponentSelect)
    /* 0x04 */ Window window;
    /* 0x30 */ PointerList sideItemList; // D_82009190
    /* 0x3C */ PointerList opponentItemList; // D_820091A0
    /* 0x48 */ Menu sideMenu;
    /* 0x6C */ Menu opponentMenu;
    /* 0x90 */ u16 sideItemFlags[4];
    /* 0x98 */ u16 opponentItemFlags[7];
    /* 0xA6 */ char padA6[0x2];
    /* 0xA8 */ u32 players[4]; // @unverified: [0] [1] your side, [2] [3] the other side: 1-4 = 1P-4P, 5 = COM, 0 = nobody
    /* 0xB8 */ u32 playerCount; // @unverified: players in use, 2 to 4
    /* 0xBC */ s32 controllerMask; // @unverified: as FreeBattleOpponentWindow.controllerMask; clear bits disable the matching options
    /* 0xC0 */ char unkC0[0x4];
    /* 0xC4 */ s32 unk_C4; // passed to FreeBattle_DrawControllerPanel as `active`
} FreeBattleTeamOpponentWindow; // size = 0xC8

/* One of the two configurations of the Pokémon grid window (D_820091EC). */
typedef struct FreeBattleMonGridConfig {
    /* 0x0 */ char* title; // @unverified: copied to window.title
    /* 0x4 */ BattleMon* mons; // @unverified: row buffer for FreeBattleMonGridWindow.mons; arg4/arg5 of FreeBattle_InitUsableMonGrid
    /* 0x8 */ s32 count; // @unverified: rows in mons
} FreeBattleMonGridConfig; // size = 0xC

/* A 7x6 grid of Pokémon with their names and icons (FreeBattle_InitUsableMonGrid, FreeBattle_DrawUsableMonGrid). */
typedef struct FreeBattleMonGridWindow {
    /* 0x00 */ Window window;
    /* 0x2C */ char unk2C[0xA0];
    /* 0xCC */ ByteMatrix mons; // rows of BattleMon; the cell shows nickname and icon
    /* 0xDC */ PokeIconRenderer* icons; // @unverified: icon set built by PokeIcon_ResetSlots/PokeIcon_SetSpeciesSlot, drawn with PokeIcon_GetImage
} FreeBattleMonGridWindow; // size = 0xE0 (FreeBattleScreen.monGridWindow is followed by connectedControllerMask)

/* A popup menu (FreeBattle_InitPopupMenu): the head of a MenuWindow, which its functions take. */
typedef struct FreeBattlePopupMenu {
    /* 0x00 */ Window window;
    /* 0x2C */ Menu menu;
    /* 0x50 */ PointerList itemList;
} FreeBattlePopupMenu; // size = 0x5C

/* The entry screen, 「エントリーポケモンのせんたく」 (Choose entry Pokémon): the team list, the team
 * being entered and its slot grid, the rental grid and the windows opened over them
 * (FreeBattle_InitEntrySelect, FreeBattle_UpdateEntrySelect, FreeBattle_DrawRentalScreen).
 * PORT: the decomp reached all of it through raw N64 offsets; typed here so the host layout
 * (wider Window, menus and pointers) is used. */
typedef struct FreeBattleEntryScreen {
    /* 0x000 */ s32 unk_00;
    /* 0x004 */ s32 state; // FreeBattle_EnterEntryScreenState: 1 team list, 2 duplicate warning, 3/7/9 status window, 4 shortage, 5 level sum, 6/8 rental grid, 10 entry confirm, 11 entered, 12 text box
    /* 0x008 */ s32 pendingState;
    /* 0x00C */ Window* view; // the panel of `state`
    /* 0x010 */ char unk010[0x4];
    /* 0x014 */ Window window; // the screen's own window (FreeBattleScreen.view in state 10)
    /* 0x040 */ ScrollList teamList; // rows of `teams`, drawn by FreeBattle_DrawTeamRow
    /* 0x084 */ u16 teamListFlags[32];
    /* 0x0C4 */ PointerList* teams; // FreeBattleScreen.teamList
    /* 0x0C8 */ FreeBattleTeam team; // the team being entered
    /* 0x2DC */ ByteMatrix slotItems; // over team.slots
    /* 0x2EC */ u16 slotItemFlags[6];
    /* 0x2F8 */ GridMenu slotMenu; // the six slots; cursor = the slot (D-pad moved it 1, 2, 5, 3 in play)
    /* 0x320 */ FreeBattleDuplicateWarning duplicateWarning;
    /* 0x370 */ FreeBattleShortageWarning shortageWarning;
    /* 0x428 */ FreeBattleLevelSumWarning levelSumWarning;
    /* 0x4DC */ FreeBattleRentalConfirm entryConfirm;
    /* 0x5CC */ FreeBattleMonStatusWindow statusWindow;
    /* 0x684 */ ByteMatrix rentalItems; // D_8200ACFC (L50-55) or D_8200AD00 (L1-30)
    /* 0x694 */ u16 rentalItemFlags[40];
    /* 0x6E4 */ ScrollGrid rentalGrid;
    /* 0x734 */ s32 teamChanged; // a rental was put into the team
    /* 0x738 */ s32 player; // 1-4 = 1P-4P, 5 = COM: who is choosing (「1Pせんたくちゅう」)
    /* 0x73C */ FreeBattleTeam** playerTeams; // FreeBattleScreen.playerTeams
    /* 0x740 */ s32 ruleCategory; // 0 Free Battle, 1 L50-55, 2 L1-30
    /* 0x744 */ Controller* controller; // the controller of `player`
    /* 0x748 */ s32 replacing; // a slot is flagged and must be replaced from the rentals
    /* 0x74C */ TextBox textBox;
    /* 0x784 */ s32 teamFlags; // bit 0: rentals were added; bit 1: the level sum warning was accepted
    /* 0x788 */ s32 stateFlags; // flags of the last FreeBattle_SetEntryScreenState
} FreeBattleEntryScreen; // size = 0x78C

/* The Free Battle setup screen: its state machine and every window it switches between
   (FreeBattle_SetScreenState). PORT: fully typed (the decomp reached most of it through raw
   N64 offsets, which are wrong on a 64-bit host). */
typedef struct FreeBattleScreen {
    /* 0x0000 */ s32 state; // @unverified: FreeBattle_SetScreenState's arg1 when it opens a window
    /* 0x0004 */ s32 pendingState; // @unverified: arg1 when the window is opened on top (arg2 & 1), else 0
    /* 0x0008 */ Window* view; // @unverified: window that is shown; set to state 7 (+0x24) when another opens on top
    /* 0x000C */ Window* nextView; // @unverified: window opened on top of view (arg2 & 1)
    /* 0x0010 */ FreeBattleTeam teams[32];
    /* 0x4290 */ FreeBattleTeam* teamPtrs[32]; // storage of teamList
    /* 0x4310 */ ByteMatrix teamTable; // over teams
    /* 0x4320 */ PointerList teamList; // the entry list's rows
    /* 0x432C */ s32 playerActive[5]; // 1P-4P, COM take part
    /* 0x4340 */ FreeBattleTeam* playerTeams[5]; // the team each player entered
    /* 0x4354 */ s32 currentPlayer; // index into playerActive of the player choosing
    /* 0x4358 */ s32 side0Players[2]; // 1-4 = 1P-4P, 5 = COM, 0 = nobody
    /* 0x4360 */ s32 side1Players[2];
    /* 0x4368 */ s32 rentalsAllowed; // L50-55 or L1-30: the entry list ends with the レンタルポケモン row
    /* 0x436C */ s32 presetsAllowed; // Free Battle: the preset trainers' teams are listed
    /* 0x4370 */ s32 unk_4370; // 0 Free Battle, 1/2 tournament (gSessionContext.mode)
    /* 0x4374 */ s32 unk_4374; // rule category: 0 Free Battle, 1 L50-55, 2 L1-30
    /* 0x4378 */ FreeBattleModeWindow modeWindow; // @unverified: state 1, FreeBattle_InitBattleModeWindow
    /* 0x43E4 */ MenuWindow menuWindow; // @unverified: states 2 and 3, FreeBattle_InitMenuWindow
    /* 0x4468 */ MenuWindow ruleMenuWindow; // @unverified: states 4, 7 and 8, FreeBattle_InitMenuWindow
    /* 0x44EC */ FreeBattleCupWindow cupWindow; // @unverified: state 9, FreeBattle_InitCupSelect
    /* 0x4554 */ FreeBattleOpponentWindow opponentWindow; // @unverified: state 5, FreeBattle_InitOpponentSelect
    /* 0x45C8 */ FreeBattleTeamOpponentWindow tagOpponentWindow; // drawn in state 6, which is never set
    /* 0x4690 */ FreeBattleEntryScreen entry; // state 10, FreeBattle_InitEntryScreen
    /* 0x4E1C */ BlinkingTextBox textBox; // @unverified: state 11, BlinkingTextBox_Init
    /* 0x4E50 */ FreeBattlePopupMenu popupMenu; // @unverified: state 12, FreeBattle_InitPopupMenu
    /* 0x4EAC */ FreeBattlePopupMenu popupMenu2; // @unverified: state 13, FreeBattle_InitPopupMenu
    /* 0x4F08 */ u16 popupMenuFlags[10]; // @unverified: item flags passed with popupMenu
    /* 0x4F1C */ u16 popupMenu2Flags[10]; // @unverified: item flags passed with popupMenu2
    /* 0x4F30 */ FreeBattleMonGridWindow monGridWindow; // @unverified: state 14, FreeBattle_InitUsableMonGrid
    /* 0x5010 */ s32 connectedControllerMask; // @unverified: Cont_GetConnectedMask (the controller-present mask controller.c builds gControllers from)
    /* 0x5014 */ s32 connectedControllerCount; // @unverified: bits set in connectedControllerMask
    /* 0x5018 */ s32 gbSaveReadyMask; // @unverified: bit i set when GbSave_GetSaveState(i) is 0
    /* 0x501C */ s32 unk_501C;
} FreeBattleScreen; // size = 0x5020

/* The exit code and the names of a new rental team (D_82008F84). */
typedef struct FreeBattleExitCode {
    /* 0x00 */ s32 code; // 0x8040 back to the main menu, 0x8050 start the battle
    /* 0x04 */ char* teamNames[5]; // 「なまえ 1P」 ... for player 1-5 (FreeBattle_InitNamedTeam)
} FreeBattleExitCode; // size = 0x18

/* A colour and the player names that follow it (D_820090C0). */
typedef struct FreeBattleDividerAndNames {
    /* 0x00 */ Color_RGBA8 dividerColor; // FreeBattle_DrawRentalTitleWindow
    /* 0x04 */ char* playerNames[4]; // 1P-4P: trainer names of players without a GB team
} FreeBattleDividerAndNames; // size = 0x14

/* PORT: shared declarations of the fragment's data and BSS. The data lifter sizes and relocates
 * each symbol from its declared type, so these must be the true types and the same in every file. */
extern FreeBattleExitCode D_82008F84;
extern char* D_82008F9C[2];
extern char* D_82008FA4[2];
extern char* D_82008FAC[2];
extern char* D_82008FB4[2];
extern char* D_82008FBC[2];
extern char* D_82008FC4[2];
extern char* D_82008FCC[3];
extern char* D_82008FD8[3];
extern char* D_82008FE4[2];
extern char* D_82008FEC[2];
extern char* D_82008FF4[2];
extern char* D_82008FFC[2];
extern FreeBattleModeEntry D_82009014[2];
extern FreeBattleMoveRowPos D_82009040[4];
extern char* D_820090A8[6];
extern FreeBattleDividerAndNames D_820090C0;
extern char* D_8200917C[5];
extern char* D_82009190[4];
extern char* D_820091A0[7];
extern FreeBattleCupEntry D_820091C4[4];
extern FreeBattleMonGridConfig D_820091EC[2];
extern FreeBattlePopupItems D_82009278[3]; // the rules list of each mode; the record and row tables after it are reached through D_820093E0 and the lifter lifts them on their own
extern FreeBattlePopupItems* D_820093E0[3];
extern void* D_8200AC40; // the archive of FreeBattle_LoadArchiveFiles
extern ModelLoadContext* D_8200AC44;
extern PokeModelPreview* D_8200AC48;
extern s32 D_8200AC4C;
extern PokeIconRenderer* D_8200AC50;
extern s32 D_8200AC58[40];
extern SessionContext* D_8200ACF8;
extern BattleMon* D_8200ACFC; // the 40 Free Battle / L50-55 rentals (segment 0x0A)
extern BattleMon* D_8200AD00; // the 33 L1-30 rentals
extern u8* D_8200AD04; // the preset trainers' teams (segment 0x0A)
extern u8* D_8200AD08[8]; // pictures of the preset trainers
extern FreeBattleScreen* D_8200AD28;

#endif // FRAGMENT_FREE_BATTLE_SETUP_H
