/* Fixed-layout split; original translation-unit boundaries remain unconfirmed. */

#include "global.h"
#include "lab_pokemon_list.h"
#include "text_lookup.h" // PORT: translated gather prompts
#include "m2c_macros.h"

extern u8 D_80073520[];
M2C_UNK Ui_DrawRoundedFrame(s32, s32, s32, s32, Color_RGBA8);

void LabPCList_InitDeckCategories(unk_func_8830867C_02C_154* decks, void* bag_buffer, void* pc_buffer, void* cart_buffer); // PORT: matches the definition

extern char* D_8910A7F8[3]; // PORT: the source names (was s32)
void func_80027410(ByteMatrix* arg0, void* arg1, s32 arg2, s32 arg3, s32 arg4); // PORT: matches TableView_Initialize

extern struct Controller* D_800603C0; // PORT: gPlayer1Controller, a pointer
void func_8000B2F4(Controller* arg0); // PORT: matches Input_SetRepeatController

void Window_SetRect(s8 *, s32, s32, s32, s32);
void func_80027804(PointerList* arg0, void** arg1, s32 arg2, s32 arg3); // PORT: matches PointerList_Initialize
void func_80027D68(LabMonEntryList* arg0, s32 (*arg1)(s32, s32)); // PORT: matches PointerList_Sort
void ScrollList_Init(s8*, s8*, s8*, s32, void*, s32, s32);
void LabPCList_DrawMonRow(s32 x, s32 y, Entry891012A8 *entry, s32 row, s32 flags, LabPCListScreen *ctx); // PORT: matches the definition
extern s32 (*D_8910D990[])(s32, s32); // PORT: the sort comparators (was s32)
extern Color_RGBA8 D_8910A7F4;
extern u8 D_8910A844[];
extern u8 D_8910A848[];
extern s32 D_8910A804[]; // PORT: s32 tables (was u8), so the lifter swaps them as words
extern s32 D_8910A84C[]; // PORT: s32 tables (was u8), so the lifter swaps them as words

void LabPCList_ResetHeaderCursor(LabPCListScreen* screen);
s32 LabPCList_GetColumnCursorX(LabPCListScreen* screen);
extern M2C_UNK D_800734A8;

extern Gfx D_80060270[];
extern Color_RGBA8 D_8910A85C[2][2];
extern Color_RGBA8 D_8910A860;
extern Color_RGBA8 D_8910A86C[2][2];
extern SpeciesInfo D_80064C5C[];
extern char D_8910DBCC[];
extern char D_8910DBD8[];
extern char D_8910DBDC[];
extern char D_8910DBE0[];
extern char D_8910DBE4[];
extern char D_8910DBE8[];
extern char D_8910DBEC[];
void func_80029028(s32, s32, s32, s32, Color_RGBA8);
void Ui_FillRectHighlighted(s32, s32, s32, s32, Color_RGBA8);
void Ui_DrawNumberZeroPadded(s32, s32, s32, s32);
void Font_SetColorForFlags(s32);
void LabPCList_DrawPlainText(s32, s32, s8*, s32, s32);

s32 Window_DrawAnimatedFrame(Window* arg0); // PORT: the definition's prototype (was (void))
void Window_DrawHeaderStripe(s32, s32, s32);
void Window_DrawFrame(s8*);
void Ui_DrawTexturedDivider(s32, s32, s32, u8*);
void ScrollList_DrawScrollArrows(s8*, s32, s32, s32, s32);
void ScrollList_DrawItems(ScrollList* menu, s32 x, s32 y, s32 mask, intptr_t arg4); // PORT: matches the definition
void LabPCList_DrawDetailColumns(s8*, s32, s32, s32);
void LabPCList_DrawPopupMenu(LabPopupMenu*, s32);
void LabPCList_DrawLevelInputWindow(LabLevelInputWindow*);
void TextBox_Draw(s8*);
extern struct FrameTextures D_8007356C; // PORT: typed so the lifter widens its texture pointers (was raw bytes)
extern Gfx D_80060270[];
extern Color_RGBA8 D_8910A88C;
void LabPCList_DrawRowCursor(ScrollList*, s32, s32); // PORT: matches the definition
void LabPCList_DrawPageHint(LabPCListScreen*);
void LabPCList_DrawColumnCursor(s8*);
void LabPCList_DrawHeaderCursor(LabPCListScreen*);
/**
 * Draws the lab PC list screen: page hint, column headers and rows.
 * Seen in play (2026-11-12): the いちらん list with the page hint, column headers and rows.
 */
void LabPCList_DrawListScreen(LabPCListScreen* screen) {
    s32 sel;
    s32 y;
    s32 flags[2]; // FAKE (only [0] is used; has to live on the stack)

    if (Window_DrawAnimatedFrame(&screen->window) != 0) { // PORT: the decomp relied on a0 still holding screen
        return;
    }
    Window_DrawHeaderStripe(screen->window.frame.x, screen->window.frame.y - 10, 500);
    Window_DrawFrame((s8*)&screen->headerFrame);
    Ui_DrawTexturedDivider(screen->window.frame.x, screen->window.frame.y + 35, screen->window.frame.width, &D_8007356C);
    LabPCList_DrawPageHint(screen);
    ScrollList_DrawScrollArrows((s8*)&screen->list, screen->listFrame.x, screen->listFrame.y - 0x10,
                  screen->listFrame.y + screen->listFrame.height, screen->listFrame.width);
    y = screen->listFrame.y + 22;
    if (screen->focus == 0) {
        flags[0] = ((screen->state != 0) ? 0x10 : 0) | 2;
    } else {
        flags[0] = 0;
    }
    ScrollList_DrawItems((s8*)&screen->list, screen->listFrame.x, y, flags[0] | 0xFFED, (intptr_t)(screen));
    sel = (screen->focus == 1) ? (((screen->state != 0) ? 0x10 : 0) | 2) : 0;
    LabPCList_DrawDetailColumns(screen, screen->listFrame.x, screen->listFrame.y, sel);
    gSPDisplayList(gDisplayListHead++, D_80060270);
    func_80029028(screen->listFrame.x, screen->listFrame.y + 20, screen->listFrame.width, 2, D_8910A88C);
    func_80029028(screen->listFrame.x + 118, screen->listFrame.y, 2, screen->listFrame.height, D_8910A88C);
    if (screen->state == 0) {
        switch (screen->focus) {
            case 0:
                LabPCList_DrawRowCursor(&screen->list, screen->listFrame.x, y);
                break;
            case 1:
                LabPCList_DrawColumnCursor(screen);
                break;
            case 2:
                LabPCList_DrawHeaderCursor(screen);
                break;
        }
    }
    switch (screen->state) {
        case 0:
            break;
        case 1:
            LabSourceSelect_Draw(&screen->sourceSelectWindow, 2);
            break;
        case 2:
        case 3:
            LabPCList_DrawPopupMenu(&screen->popup, 2);
            break;
        case 4:
            LabPCList_DrawLevelInputWindow(&screen->levelInputWindow);
            break;
        case 5:
            LabPCList_DrawTypeFilter(&screen->typeFilterWindow);
            break;
        case 6:
            LabPCList_DrawMoveFilter(&screen->moveFilterWindow);
            break;
        case 7:
            LabPCList_DrawPopupMenu(&screen->popup, 2);
            break;
        case 8:
            LabPCList_DrawPopupMenu(&screen->popup, 0x10);
            LabPCList_DrawPopupMenu(&screen->subPopup, 2);
            break;
        case 9:
            LabPCList_DrawPopupMenu(&screen->popup, 0x10);
            LabPCList_DrawPopupMenu(&screen->subPopup, 0x10);
            LabPCList_DrawPopupMenu(&screen->subPopup2, 2);
            break;
        case 10:
            LabPCList_DrawPopupMenu(&screen->popup, 0x10);
            LabPCList_DrawPopupMenu(&screen->subPopup, 0x10);
            LabPCList_DrawPopupMenu(&screen->subPopup2, 2);
            break;
        case 11:
        case 12:
            TextBox_Draw((s8*)screen->textBox);
            break;
    }
}

/**
 * Returns whether any of filter flags `flag` is active.
 * Seen in play (2026-11-12) in いちらん: opening レベルをきめてあつめる called this with 1; it returned 0 with
 * no level filter, and 1 once 006から100 had been applied (かいじょ is then usable).
 */
s32 LabPCList_HasFilterFlag(LabPCListScreen* screen, s32 flag) {
    return (screen->activeFilters & flag) != 0;
}

/**
 * Turns on filter flags `flag`.
 * Seen in play (2026-11-12) in いちらん: choosing どく in タイプであつめる and pressing あつめる called this with
 * 2, the flag word at +0x7C went 0 -> 2, and the list then showed only フシギダネ.
 */
void LabPCList_SetFilterFlag(LabPCListScreen* screen, s32 flag) {
    screen->activeFilters = (s32) (screen->activeFilters | flag);
}

/**
 * Turns off filter flags `flag`.
 * Seen in play (2026-11-12) in いちらん: pressing かいじょ in タイプであつめる called this with 2, the flag word
 * at +0x7C went 2 -> 0, and all three party Pokémon came back.
 */
void LabPCList_ClearFilterFlag(LabPCListScreen* screen, M2C_UNK flag) {
    screen->activeFilters = (s32) (screen->activeFilters & ~flag);
}

/**
 * Turns off every filter flag.
 * Seen in play (2026-11-12) in いちらん: すべてのせんたくをやめる took the flag word from 0x20 to 0.
 */
void LabPCList_ClearFilterFlags(LabPCListScreen* screen) {
    screen->activeFilters = 0;
}

typedef struct unk_func_89101D80 {
    /* 0x00 */ char unk00[0x7C];
    /* 0x7C */ s32 flags;
} unk_func_89101D80;

/**
 * Toggles filter flags `flag` and returns whether they are now on.
 * Seen in play (2026-11-12) in いちらん: バトルようのポケモンをあつめる called this with 0x20; the flag word went 0
 * -> 0x20 and it returned 0x20 (on). The menu entry then read バトルようのポケモンをかいじょする.
 */
s32 LabPCList_ToggleFilterFlag(unk_func_89101D80* arg0, s32 flag) {
    arg0->flags ^= flag;
    return arg0->flags & flag;
}

extern s32 (*D_8910D990[])(s32, s32); // PORT: the sort comparators (was s32)
void ScrollList_SetItems(ScrollList*, PointerList*, u16*);
void func_80027D94(LabMonEntryList* arg0, LabMonEntryList* arg1, s32 (*arg2)(LabMonEntry*, s32*), s32* arg3); // PORT: matches PointerList_Filter
void func_80027D68(LabMonEntryList* arg0, s32 (*arg1)(s32, s32)); // PORT: matches PointerList_Sort
int LabPCList_FilterByLevelRange();
s32 LabPCList_FilterByAnyType();
s32 LabPCList_FilterByAnyMove();
s32 LabPCList_FilterBySpeciesId();
s32 LabPCList_FilterByExactTypePair();
s32 LabPCList_FilterByBattleEligible(void *entry, M2C_UNK unused); // PORT: matches the definition
/**
 * Rebuilds the visible list from the source boxes using the active filter flags.
 * Seen in play (2026-11-12) in いちらん (party list: フシギダネ, ヒトカゲ, ゼニガメ): ran when OK was pressed on
 * 「どこのポケモンを いちらん するか」 with てもち chosen, and the list then showed the three party Pokémon.
 */
M2C_UNK LabPCList_ApplyFiltersAndSort(LabPCListScreen* screen) {
    ScrollList_SetItems(&screen->list, &screen->rows, screen->rowFlags);
    if (screen->activeFilters != 0) {
        if (screen->activeFilters & 1) {
            func_80027D94(screen->list.items, &screen->filteredRows, LabPCList_FilterByLevelRange, &screen->levelFilterMin);
            ScrollList_SetItems(&screen->list, &screen->filteredRows, screen->rowFlags);
        }
        if (screen->activeFilters & 2) {
            func_80027D94(screen->list.items, &screen->filteredRows, LabPCList_FilterByAnyType, &screen->typeFilterCount);
            ScrollList_SetItems(&screen->list, &screen->filteredRows, screen->rowFlags);
        }
        if (screen->activeFilters & 4) {
            func_80027D94(screen->list.items, &screen->filteredRows, LabPCList_FilterByAnyMove, &screen->moveFilterMoves);
            ScrollList_SetItems(&screen->list, &screen->filteredRows, screen->rowFlags);
        }
        if (screen->activeFilters & 8) {
            func_80027D94(screen->list.items, &screen->filteredRows, LabPCList_FilterBySpeciesId, &screen->unk_9E58);
            ScrollList_SetItems(&screen->list, &screen->filteredRows, screen->rowFlags);
        }
        if (screen->activeFilters & 0x10) {
            func_80027D94(screen->list.items, &screen->filteredRows, LabPCList_FilterByExactTypePair, &screen->sameTypeFilterCount);
            ScrollList_SetItems(&screen->list, &screen->filteredRows, screen->rowFlags);
        }
        if (screen->activeFilters & 0x20) {
            func_80027D94(screen->list.items, &screen->filteredRows, LabPCList_FilterByBattleEligible, NULL);
            ScrollList_SetItems(&screen->list, &screen->filteredRows, screen->rowFlags);
        }
    }
    func_80027D68(screen->list.items, D_8910D990[screen->sortKey]);
}

typedef struct Pair8910D910 { char** a; s32 b; } Pair8910D910;
extern Pair8910D910 D_8910D910[];
void LabPCList_InitPopupMenu(LabPopupMenu* popup, char** labels, u16* item_flags, s32 count, char* message); // PORT: matches the definition
void LabPCList_PlacePopup(LabPopupMenu*);

/**
 * Opens what A does on the selected column header: a sort popup for the text and stat columns, the type filter for タイプ1/タイプ2, the move filter for わざ1-4.
 * Seen in play (2026-11-14) in いちらん: A on ニックネーム opened アイウエオじゅんにならべる, on タイプ1 タイプであつめる, on わざ わざであつめる, on レベル たかいじゅん/ひくいじゅん/レベルをきめてあつめる.
 */
void LabPCList_OpenColumnMenu(LabPCListScreen* screen) {
    s32 sel;

    sel = screen->pageColumnStart[screen->page] + screen->headerCursor.cursor - 2;
    switch (sel) {
        case 0:
        case 1:
        case 2:
        case 3:
        case 10:
        case 11:
        case 12:
        case 13:
        case 14:
        case 15:
            screen->state = 3;
            LabPCList_InitPopupMenu(&screen->popup, D_8910D910[sel].a, (u16*)&screen->popupItemFlags, D_8910D910[sel].b, 0);
            Window_SetRect((s8*)&screen->popup.anchorX, LabPCList_GetColumnCursorX(screen), screen->listFrame.y, 0, 22);
            screen->popup.limitX = screen->listFrame.x + screen->listFrame.width;
            screen->popup.limitY = screen->listFrame.y + screen->listFrame.height;
            LabPCList_PlacePopup(&screen->popup);
            screen->popup.window.frame.color = D_8910A7F4;
            break;
        case 4:
        case 5:
            LabPCList_InitTypeFilter(&screen->typeFilterWindow, 129, 148, &screen->typeFilterCount);
            screen->state = 5;
            break;
        case 6:
        case 7:
        case 8:
        case 9:
            LabPCList_InitMoveFilter(&screen->moveFilterWindow, 129, 148, screen->moveFilterMoves);
            screen->state = 6;
            break;
    }
}

typedef struct Str891020F8 { char s[13]; } Str891020F8;
char* func_8000A460(char* dest, char* src); // PORT: matches HAL_Strcpy
void LabPCList_InitPopupMenu(LabPopupMenu* popup, char** labels, u16* item_flags, s32 count, char* message); // PORT: matches the definition
void LabPCList_PlacePopup(LabPopupMenu*);
extern s8* D_8910A890[];
extern char* D_8910A898[]; // PORT: a table of string pointers (was u8)
extern u8 D_8910DC50[];
extern s8 D_8910E2D0[];
extern s8 D_8910E312[];

/**
 * Builds the "<species>をあつめる / をかいじょする" (gather / ungather) prompt for the selected Pokemon.
 * Seen in play (2026-11-12) in いちらん (party list: フシギダネ, ヒトカゲ, ゼニガメ): A on フシギダネ built the popup's
 * first row, 「フシギダネをあつめる」.
 */
void LabPCList_FormatGatherPrompt(LabPCListScreen* screen) {
    func_8000A460(D_8910E2D0, (s8*)D_80064C5C[((LabPCEntry*)((PointerList*)screen->list.items)->items[screen->list.cursor])->mon.species.id - 1].name);
    func_8000A460(D_8910E2D0 + strlen(D_8910E2D0), D_8910A890[(screen->activeFilters & 8) ? 1 : 0]);
    *(Str891020F8*)D_8910E312 = *(Str891020F8*)D_8910DC50;
    func_8000A460(D_8910E2D0 + (strlen(D_8910E312) + 0x42), D_8910A890[(screen->activeFilters & 0x10) ? 1 : 0]);
    // PORT: the two rows are built from a name and 「をあつめる」/「をかいじょする」: translate them as those pieces
    Port_Text_NoteJoined((const char*)D_8910E2D0,
                         (const char*)D_80064C5C[((LabPCEntry*)((PointerList*)screen->list.items)->items[screen->list.cursor])->mon.species.id - 1].name,
                         (const char*)D_8910A890[(screen->activeFilters & 8) ? 1 : 0]);
    Port_Text_NoteJoined((const char*)D_8910E312, (const char*)D_8910DC50,
                         (const char*)D_8910A890[(screen->activeFilters & 0x10) ? 1 : 0]);
    LabPCList_InitPopupMenu(&screen->popup, D_8910A898, (u16*)screen->popupItemFlags, 4, 0);
    Window_SetRect((s8*)&screen->popup.anchorX, screen->listFrame.x + 38,
                  screen->listFrame.y + screen->list.cursorY - screen->list.scrollY + 22, 0, 20);
    screen->popup.limitX = screen->listFrame.x + screen->listFrame.width;
    screen->popup.limitY = screen->listFrame.y + screen->listFrame.height;
    LabPCList_PlacePopup(&screen->popup);
    screen->popup.window.frame.color = D_8910A7F4;
    screen->state = 7;
    func_80027804(&screen->usableItemList, (void**)screen->usableItemPtrs, 0xAA, 0); // PORT: typed (were N64 offsets)
    LabPCList_BuildUsableItemList(&screen->usableItemList, screen->usableItems, screen->itemDecks, 3);
    if (screen->usableItemList.count == 0) {
        screen->popup.menu.itemFlags[3] |= 1;
    }
}

typedef struct List89102344 {
    /* 0x00 */ u8** items;
    /* 0x04 */ char unk04[4];
    /* 0x08 */ s32 count;
} List89102344;


s32 Audio_PlaySystemSound();
s32 ScrollList_UpdateCursor(void*);
void ScrollList_SetCursor(void*, s32);
void LabPCList_FormatGatherPrompt(LabPCListScreen*);

/**
 * Handles list input while the cursor is on the Pokemon rows.
 * Seen in play (2026-11-12): moved the cursor over the rows.
 */
s32 LabPCList_UpdateListCursor(LabPCListScreen* screen) {
    s32 ret;

    if ((screen->list.cursorY == 0) && (D_800603C0->buttonDown & 0x800)) {
        Audio_PlaySystemSound(1);
        screen->focus = 2;
        screen->headerCursor.cursor = 1;
        ScrollList_SetCursor(&screen->list, screen->list.scrollY / screen->list.cellHeight);
        return 1;
    }
    ret = ScrollList_UpdateCursor(&screen->list.items) != 0;
    if ((ret == 0) && (D_800603C0->buttonPressed & 0x8000)) {
        // PORT: the species through LabPCEntry (was byte 8 of the entry, the N64 offset of mon.species.id: on a
        // 64-bit host that is the low byte of `slot`, so A did nothing on each box's first row)
        if ((screen->list.items->count > 0) && (((LabPCEntry**)((List89102344*)screen->list.items)->items)[screen->list.cursor]->mon.species.id > 0) && (((LabPCEntry**)((List89102344*)screen->list.items)->items)[screen->list.cursor]->mon.species.id < 0x98)) {
            Audio_PlaySystemSound(2);
            LabPCList_FormatGatherPrompt((s8*)screen);
            ret = 1;
        } else {
            Audio_PlaySystemSound(8);
        }
    }
    return ret;
}

char* func_8000A460(char* dest, char* src); // PORT: matches HAL_Strcpy
void LabPCList_InitPopupMenu(LabPopupMenu* popup, char** labels, u16* item_flags, s32 count, char* message); // PORT: matches the definition
void LabPCList_PlacePopup(LabPopupMenu*);
void Window_SetRect(s8 *, s32, s32, s32, s32);
extern Color_RGBA8 D_8910A7F4;
extern s8 *D_8910A8A8[];
extern s8 D_8910A8B0[];
extern char* D_8910A8C8[]; // PORT: a table of string pointers (was u8)
extern s8 D_8910E358[];
/**
 * Sets up the sort menu: by name, by number, gather battle Pokemon, clear selections, choose what to list.
 * Seen in play (2026-11-12) in いちらん: A on the ポケモン header opened the menu アイウエオじゅんにならべる,
 * ポケモンばんごうでならべる, バトルようのポケモンをあつめる, すべてのせんたくをやめる, いちらんするポケモンをえらぶ.
 */
void LabPCList_InitSortMenu(LabPCListScreen *screen) {
    func_8000A460(D_8910E358, D_8910A8B0);
    func_8000A460(D_8910E358 + strlen(D_8910E358), D_8910A8A8[(screen->activeFilters & 0x20) ? 1 : 0]);
    Port_Text_NoteJoined((const char*)D_8910E358, (const char*)D_8910A8B0,
                         (const char*)D_8910A8A8[(screen->activeFilters & 0x20) ? 1 : 0]); // PORT: translated as its pieces
    LabPCList_InitPopupMenu(&screen->popup, D_8910A8C8, (u16*)&screen->popupItemFlags, 5, 0);
    Window_SetRect((s8*)&screen->popup.anchorX, screen->listFrame.x + 38, screen->listFrame.y, 0, 22);
    screen->popup.limitX = screen->listFrame.x + screen->listFrame.width;
    screen->popup.limitY = screen->listFrame.y + screen->listFrame.height;
    LabPCList_PlacePopup(&screen->popup);
    screen->popup.window.frame.color = D_8910A7F4;
    if (screen->activeFilters == 0) {
        screen->popup.menu.itemFlags[3] |= 1;
    }
}

s32 func_8002B390(UiSelection* arg0); // PORT: matches Ui_HandleHorizontalSelectionInput

/**
 * Handles input while the cursor is on the header buttons.
 * Seen in play (2026-11-12) in いちらん: R moved the focus to the column headers, and D-Left/D-Right
 * moved the highlight between ニックネーム, ID, おや and ばしょ while this ran.
 */
s32 LabPCList_UpdateHeaderCursor(LabPCListScreen* screen) {
    s32 ret = 0;
    s32 button;

    button = func_8002B390(&screen->headerCursor.controller);
    if (button != 0) {
        if ((button == 0x400) && (screen->headerCursor.cursor == 1)) {
            Audio_PlaySystemSound(1);
            screen->focus = 0;
            screen->list.repeatTimer = 8; screen->list.inputDirection = 0x400; // IDO: both stores must share one source line
            ret = 1;
        } else {
            screen->focus = (screen->headerCursor.cursor < 2) ? 2 : 1;
            ret = 1;
        }
    } else if (D_800603C0->buttonPressed & 0x8000) {
        if (screen->headerCursor.cursor == 0) {
            Audio_PlaySystemSound(3);
            ret = -1;
        } else if (screen->headerCursor.cursor == 1) {
            Audio_PlaySystemSound(2);
            LabPCList_InitSortMenu((LabPCListScreen*)screen);
            ret = 1;
            screen->state = 2;
        } else {
            Audio_PlaySystemSound(2);
            LabPCList_OpenColumnMenu((LabPCListScreen*)screen);
            ret = 1;
        }
    }
    return ret;
}

/**
 * Browse state: list or header input, C-Left/C-Right change the detail page, L/R move the focus.
 * Seen in play (2026-11-12) in いちらん (party list: フシギダネ, ヒトカゲ, ゼニガメ): was running while browsing;
 * C-Right turned the detail page from ページおくり1/3 to 2/3 (the move columns).
 */
s32 LabPCList_UpdateBrowse(LabPCListScreen* screen) {
    s32 done = FALSE;
    s32 result;
    u16 buttons;

    switch (screen->focus) {
        case 0:
            result = LabPCList_UpdateListCursor((LabPCListScreen*)screen);
            break;
        case 1:
        case 2:
            result = LabPCList_UpdateHeaderCursor((LabPCListScreen*)screen);
            break;
    }
    if (result == -1) {
        done = TRUE;
    } else if (result == 0) {
        buttons = D_800603C0->buttonPressed;
        if (buttons & 0x4000) {
            Audio_PlaySystemSound(3);
            done = TRUE;
        } else if (buttons & 2) {
            Audio_PlaySystemSound(2);
            if (screen->page == 0) {
                screen->page += 3;
            }
            screen->page--;
            LabPCList_ResetHeaderCursor(screen);
        } else if (buttons & 1) {
            Audio_PlaySystemSound(2);
            if (screen->page >= 2) {
                screen->page -= 3;
            }
            screen->page++;
            LabPCList_ResetHeaderCursor(screen);
        } else if (buttons & 0x20) {
            if (screen->focus != 0) {
                Audio_PlaySystemSound(2);
                screen->focus = 0;
            }
        } else if ((buttons & 0x10) && (screen->focus != 1)) {
            Audio_PlaySystemSound(2);
            screen->focus = 1;
            screen->headerCursor.cursor = 2;
        }
    }
    return done;
}

M2C_UNK LabPCList_ApplyFiltersAndSort();
/**
 * Handles the window that chooses which boxes to list.
 * Seen in play (2026-11-12): ran the 「どこのポケモンを いちらん するか えらんでください。」 window (てもち / ゲームボーイのボックス /
 * 64カセットのボックス, OK).
 */
s32 LabPCList_UpdateSourceSelect(LabPCListScreen* screen)
{
  s8 *new_var2; // FAKE: permuter temp
  s32 temp_v0;
  s8 *new_var; // FAKE: permuter temp
  temp_v0 = LabSourceSelect_Update(&screen->sourceSelectWindow);
  new_var2 = (s8*)&screen->rows; // FAKE: permuter temp
  if (temp_v0 == (-1))
  {
    goto block_4; // FAKE: decomp goto
  }
  if (temp_v0 == 1)
  {
    new_var = (s8*)&screen->sourceMatrix; // FAKE: permuter temp
    LabPCList_FlattenEntries((PointerList*)new_var2, (ByteMatrix*)new_var); // FAKE: permuter temp
    LabPCList_ApplyFiltersAndSort(screen);
    screen->state = 0; // FAKE: stored twice as in the ROM (was a raw N64 offset)
    block_4:
    screen->state = 0; // FAKE: stored twice as in the ROM (was a raw N64 offset)

  }
  return 0;
}

/**
 * Returns which saves need writing: 1 a Game Boy box or the party changed, 2 a Game Boy item deck, 0x10 a 64 cartridge box, 0x20 the 64 item deck.
 * Seen in play (2026-11-14) in いちらん: ran on レポートしてやめる after a TM and a move; the Game Boy save was written.
 */
s32 LabPCList_GetChangedSaveFlags(LabPCListScreen* screen) { // PORT: typed (was K&R s8* with N64 offsets and views)
    s32 flags = 0;
    s32 i;
    s32 j;
    Entry89100140* item = (Entry89100140*)screen->sourceMatrix.rows;

    for (i = 0; i < screen->sourceMatrix.count; i++, item++) {
        LabPCBox* sub = (LabPCBox*)item->boxes.rows;
        for (j = 0; j < item->boxes.count; j++, sub++) {
            if (sub->changed != 0) {
                if (item->unk_04 == 0x20 || item->unk_04 == 0x21) {
                    flags |= 1;
                } else {
                    flags |= 0x10;
                }
            }
        }
    }
    for (i = 0; i < 3; i++) {
        if (screen->itemDecks[i].unk_04 != 0 && screen->itemDecks[i].unk_08 != 0) {
            if (i == 0 || i == 1) {
                flags |= 2;
            } else {
                flags |= 0x20;
            }
        }
    }
    return flags;
}

s32 func_8002207C(s32 arg0); // PORT: matches GbSave_RebuildAndWriteSave
void func_80022118(s32 arg0); // PORT: matches GbSave_ValidateBeforeWrite
void func_80022188(s32 arg0); // PORT: matches GbSave_RequirePakPresent
void func_800230C4(s32 arg0); // PORT: matches GbSave_RebuildHeldSpecies
M2C_UNK Save_CommitPokemonRecords();                            /* extern */
M2C_UNK GbSave_RunReportErrorLoop(s32);                         /* extern */
void LabPCList_DrawListScreen(LabPCListScreen*);
s32 LabPCList_GetChangedSaveFlags(LabPCListScreen* screen);
extern s32 D_8910E37C;

/**
 * Writes the changed boxes back to the save.
 * Seen in play (2026-11-12) in いちらん: ran on レポートしてやめる (B after an item was used, 「ないようが
 * へんこうされています」), and the Game Boy save on the Transfer Pak then held the changed party and bag.
 */
void LabPCList_SaveChanges(LabPCListScreen* screen) { // PORT: typed (was s32)
    s32 sp1C;
    s32 temp_v0;

    temp_v0 = LabPCList_GetChangedSaveFlags(screen); // PORT: was implicit a0
    sp1C = temp_v0;
    if (temp_v0 & 3) {
        func_80022118(D_8910E37C);
    }
    LabPCList_SaveDirtyBoxes(&screen->sourceMatrix);
    LabPCList_SaveDirtyItemDecks(screen);
    if (sp1C & 3) {
        if (sp1C & 1) {
            func_800230C4(D_8910E37C);
        }
        if (func_8002207C(D_8910E37C) == 0) {
            GbSave_RunReportErrorLoop(D_8910E37C);
            LabPCList_DrawListScreen(screen);
        }
        func_80022188(D_8910E37C);
    }
    if (sp1C & 0x30) {
        Save_CommitPokemonRecords();
    }
}
