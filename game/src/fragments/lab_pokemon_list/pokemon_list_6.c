/* Fixed-layout split; original translation-unit boundaries remain unconfirmed. */
#include "global.h"
#include "lab_pokemon_list.h"
#include "m2c_macros.h"


// PORT: the decomp called these without a prototype (IDO allows it; clang does not).
s32 LabPCList_ToggleFilterFlag(void* /* unk_func_89101D80* */ arg0, s32 flag);

typedef struct Color82000BC4 {
    u8 r, g, b, a;
} Color82000BC4;
typedef struct Entry80066AD8 {
    const char* str;
    Color82000BC4 color;
} Entry80066AD8;

typedef struct Label89100FC4 {
    Color82000BC4 color;
    const char* str;
} Label89100FC4;

extern u8 D_80073520[];
M2C_UNK Ui_DrawRoundedFrame(s32, s32, s32, s32, Color_RGBA8);
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
s32 LabPCList_GetColumnCursorX(LabPCListScreen* screen); // PORT: matches the definition
extern Entry80066AD8 D_80066AD8[16]; // PORT: sized (the 16 type names and colors), so it stops at the move table
s32 Pokemon_GetTypeIndex(u8*, s32);
void func_8001A384(s32 mask, s32 arg1); // PORT: matches Font_SetActive
void Font_Printf(s32 x, s32 y, const char* fmt, ...);
void func_8001AA9C(s32 r, s32 g, s32 b, s32 a); // PORT: matches Gfx_SetEnvColor
void func_8001AB6C(void); // PORT: matches Font_BeginTranslucentTextRendering
void func_8001ABBC(void); // PORT: matches Font_EndTexturedTextRendering
// PORT: D_8006A9D4 is declared in global.h (MoveTableEntry)
extern M2C_UNK D_800734A8;
extern char D_8910DBAC[];
s32 BattleMon_GetMoveIndexChecked(u8* mon, s32 slot); // PORT: matches the definition
extern M2C_UNK D_800734A8;
extern M2C_UNK D_8910DBB4;
extern M2C_UNK D_8910DBBC;
extern M2C_UNK D_8910DBC0;
extern M2C_UNK D_8910DBC4;
extern u8 D_04006170[];
extern u8 D_04006CE0[];
extern Gfx D_80060290[];
extern Gfx D_80060398[];
extern u8 D_8910C370[];
extern u8 D_8910C978[];
extern char D_8910DBF0[];
extern char D_8910DC04[];
void Gfx_DrawTextureRgba16(s32, s32, s32, s32, u8*, s32, s32);
extern s32 (*D_8910D990[])(s32, s32); // PORT: the sort comparators (was s32)
void ScrollList_SetItems(s32*, void*, void*);
void func_80027D94(LabMonEntryList* arg0, LabMonEntryList* arg1, s32 (*arg2)(LabMonEntry*, s32*), s32* arg3); // PORT: matches PointerList_Filter
void func_80027D68(LabMonEntryList* arg0, s32 (*arg1)(s32, s32)); // PORT: matches PointerList_Sort
int LabPCList_FilterByLevelRange();
s32 LabPCList_FilterByAnyType();
s32 LabPCList_FilterByAnyMove();
s32 LabPCList_FilterBySpeciesId();
s32 LabPCList_FilterByExactTypePair();
s32 LabPCList_FilterByBattleEligible(void *entry, M2C_UNK unused); // PORT: matches the definition
char* func_8000A460(char* dest, char* src); // PORT: matches HAL_Strcpy
void LabPCList_InitPopupMenu(LabPopupMenu* popup, char** labels, u16* item_flags, s32 count, char* message); // PORT: matches the definition
void LabPCList_PlacePopup(LabPopupMenu*);
extern s8* D_8910A890[];
extern char* D_8910A898[]; // PORT: a table of string pointers (was u8)
extern u8 D_8910DC50[];
extern s8 D_8910E2D0[];
extern s8 D_8910E312[];
s32 Audio_PlaySystemSound();
s32 ScrollList_UpdateCursor(void*);
void ScrollList_SetCursor(void*, s32);
void LabPCList_FormatGatherPrompt(LabPCListScreen*);
char* func_8000A460(char* dest, char* src); // PORT: matches HAL_Strcpy
void LabPCList_InitPopupMenu(LabPopupMenu* popup, char** labels, u16* item_flags, s32 count, char* message); // PORT: matches the definition
void LabPCList_PlacePopup(LabPopupMenu*);
void Window_SetRect(s8 *, s32, s32, s32, s32);
extern Color_RGBA8 D_8910A7F4;
extern s8 *D_8910A8A8[];
extern s8 D_8910A8B0[];
extern char* D_8910A8C8[]; // PORT: a table of string pointers (was u8)
extern s8 D_8910E358[];
s32 LabPCList_UpdateHeaderCursor(void*);
M2C_UNK LabPCList_ApplyFiltersAndSort(LabPCListScreen* screen); // PORT: matches the definition
extern s32 D_8910E37C;
s32 LabPCList_UpdatePopupMenu(LabPopupMenu*);
void func_80027D68(LabMonEntryList* arg0, s32 (*arg1)(s32, s32)); // PORT: matches PointerList_Sort
M2C_UNK LabPCList_ApplyFiltersAndSort(LabPCListScreen* screen); // PORT: matches the definition
extern s32 (*D_8910D9D0)(s32, s32); // PORT: a sort comparator (was s32)
extern s32 (*D_8910D9D4)(s32, s32); // PORT: a sort comparator (was s32)

void LabPCList_InitLevelInputWindow(LabLevelInputWindow*, s32, s32, Color_RGBA8, s32, s32, s32);
s32 LabPCList_HasFilterFlag(LabPCListScreen*, s32);

/**
 * Handles the column sort popup: the first row sorts high to low (or by text), the second low to high, the third opens the level range input.
 * Seen in play (2026-11-14) in いちらん: sorted every stat column both ways and opened レベルのにゅうりょく.
 */
s32 LabPCList_UpdateColumnMenu(LabPCListScreen* screen) {
    UNUSED s32 pad; // FAKE: stack padding
    u32 sel;
    s32 a;
    s32 b;

    sel = screen->headerCursor.cursor + screen->pageColumnStart[screen->page] - 2;
    LabPCList_UpdatePopupMenu(&screen->popup);
    if (((u16*)D_800603C0)[4] & 0x4000) {
        Audio_PlaySystemSound(3);
        screen->state = 0;
    } else if (((u16*)D_800603C0)[4] & 0x8000) {
        Audio_PlaySystemSound(2);
        switch (screen->popup.menu.cursor) {
            case 0:
                switch (sel) {
                    case 0:
                        screen->sortKey = 15;
                        break;
                    case 1:
                        screen->sortKey = 13;
                        break;
                    case 2:
                        screen->sortKey = 14;
                        break;
                    case 3:
                        screen->sortKey = 0;
                        break;
                    case 10:
                        screen->sortKey = 2;
                        break;
                    case 11:
                        screen->sortKey = 4;
                        break;
                    case 12:
                        screen->sortKey = 6;
                        break;
                    case 13:
                        screen->sortKey = 8;
                        break;
                    case 14:
                        screen->sortKey = 10;
                        break;
                    case 15:
                        screen->sortKey = 12;
                        break;
                }
                func_80027D68((LabMonEntryList*)screen->list.items, /* PORT: was a raw read at N64 offset 0xB160 */ D_8910D990[screen->sortKey]);
                screen->state = 0;
                break;
            case 1:
                switch (sel) {
                    case 10:
                        screen->sortKey = 1;
                        break;
                    case 11:
                        screen->sortKey = 3;
                        break;
                    case 12:
                        screen->sortKey = 5;
                        break;
                    case 13:
                        screen->sortKey = 7;
                        break;
                    case 14:
                        screen->sortKey = 9;
                        break;
                    case 15:
                        screen->sortKey = 11;
                        break;
                }
                func_80027D68((LabMonEntryList*)screen->list.items, /* PORT: was a raw read at N64 offset 0xB160 */ D_8910D990[screen->sortKey]);
                screen->state = 0;
                break;
            case 2:
                if (screen->unk_982C != 0) {
                    a = screen->levelFilterMax;
                    b = screen->levelFilterMin;
                } else {
                    a = screen->levelFilterMin;
                    b = screen->levelFilterMax;
                }
                screen->state = 4;
                LabPCList_InitLevelInputWindow(&screen->levelInputWindow, 272, 245, D_8910A7F4, a, b, LabPCList_HasFilterFlag(screen, 1));
                break;
        }
    }
    return 0;
}

s32 LabPCList_UpdateLevelInput(void*);
M2C_UNK LabPCList_ApplyFiltersAndSort(LabPCListScreen* screen); // PORT: matches the definition
void LabPCList_ClearFilterFlag(LabPCListScreen* screen, M2C_UNK flag); // PORT: no prototype in the decomp
void LabPCList_SetFilterFlag(LabPCListScreen* screen, s32 flag); // PORT: no prototype in the decomp
/**
 * Runs the レベルのにゅうりょく (level input) window and applies the level range filter.
 * Seen in play (2026-11-12) in いちらん: あつめる with 006から100 stored 6 and 100 at +0x9828/+0x9829, set
 * flag 1, and the Lv 5 party disappeared; かいじょ reset the range to 1-100 and cleared flag 1.
 */
s32 LabPCList_RunLevelFilter(s8 *screen_arg)
{
  LabPCListScreen *screen = (LabPCListScreen *) screen_arg;
  s32 ret;
  ret = LabPCList_UpdateLevelInput(&screen->levelInputWindow);
  if (ret == (-1))
  {
    screen->state = 0;
  }
  else
    if (ret == 1)
  {
    if (screen->levelInputWindow.maxLevel.value >= screen->levelInputWindow.minLevel.value)
    {
      screen->unk_982C = 0;
      screen->levelFilterMin = screen->levelInputWindow.minLevel.value;
      screen->levelFilterMax = screen->levelInputWindow.maxLevel.value;
      screen->sortKey = 1;
    }
    else
    {
      screen->unk_982C = 1;
      screen->levelFilterMax = screen->levelInputWindow.minLevel.value;
      screen->levelFilterMin = screen->levelInputWindow.maxLevel.value;
      screen->sortKey = 2;
    }
    LabPCList_SetFilterFlag((s8 *) screen, 1);
    LabPCList_ApplyFiltersAndSort((s8 *) screen);
    screen->state = 0;
  }
  else
  {
    if (ret == 2)
    {
      screen->levelFilterMin = 1; screen->levelFilterMax = 100; // IDO: both stores must share one source line
      screen->unk_982C = 0;
      LabPCList_ClearFilterFlag((s8 *) screen, 1);
      LabPCList_ApplyFiltersAndSort((s8 *) screen);
      screen->state = 0;
    }
  }
  return 0;
}

/**
 * Runs the type filter window and turns filter flag 2 on or off.
 * Seen in play (2026-11-12) in いちらん: ran while the タイプであつめる window was open.
 */
s32 LabPCList_RunTypeFilter(LabPCListScreen* screen) {
    LabTypeFilterWindow* sp24 = &screen->typeFilterWindow;
    s32 ret = LabPCList_UpdateTypeFilter(sp24);

    if (ret == -1) {
        screen->state = 0;
    } else if (ret == 1) {
        LabPCList_CommitTypeFilter(sp24, &screen->typeFilterCount);
        if (screen->typeFilterCount == 0) {
            LabPCList_ClearFilterFlag(screen, 2);
        } else {
            LabPCList_SetFilterFlag(screen, 2);
        }
        LabPCList_ApplyFiltersAndSort(screen);
        screen->state = 0;
    } else if (ret == 2) {
        screen->typeFilterCount = 0;
        LabPCList_ClearFilterFlag(screen, 2);
        LabPCList_ApplyFiltersAndSort(screen);
        screen->state = 0;
    }
    return 0;
}

/**
 * Runs the move filter window and turns filter flag 4 on or off.
 * Seen in play (2026-11-12) in いちらん: あつめる with ひっかく set filter flag 4 and left only ヒトカゲ (ひっさつまえば
 * left the list empty); かいじょ cleared flag 4 and all three party Pokémon returned.
 */
s32 LabPCList_RunMoveFilter(LabPCListScreen* screen) {
    LabMoveFilterWindow* sp24 = &screen->moveFilterWindow;
    s32 ret = LabPCList_UpdateMoveFilter(sp24);

    if (ret == -1) {
        screen->state = 0;
    } else if (ret == 1) {
        LabPCList_CommitMoveFilter(sp24, screen->moveFilterMoves);
        if (screen->moveFilterCount == 0) {
            LabPCList_ClearFilterFlag(screen, 4);
        } else {
            LabPCList_SetFilterFlag(screen, 4);
        }
        LabPCList_ApplyFiltersAndSort(screen);
        screen->state = 0;
    } else if (ret == 2) {
        screen->moveFilterCount = 0;
        LabPCList_ClearFilterFlag(screen, 4);
        LabPCList_ApplyFiltersAndSort(screen);
        screen->state = 0;
    }
    return 0;
}

/**
 * Grays out the boxes of a source that are not listed, full, or the Pokémon's own box.
 * Seen in play (2026-11-14) in いちらん: ran when いどうする > ゲームボーイのボックス / 64カセットのボックス opened the box list.
 */
void LabPCList_DisableUnavailableBoxes(Menu* menu, LabPCEntry* entry, Entry89100140* source) { // PORT: typed (were N64 views)
    s32 i;
    LabPCBox* item;

    item = (LabPCBox*)source->boxes.rows;
    i = 0;
    if (source->boxes.count > 0) {
        do {
            if ((item->unk_08 == 0) || (item->entries.count >= source->unk_20) || (item == entry->box)) {
                menu->itemFlags[i] |= 1;
            }
            i++;
            item++;
        } while (i < source->boxes.count);
    }
}

/**
 * Grays out each destination source with no box that could take the Pokémon.
 * Seen in play (2026-11-14) in いちらん: ran when いどうする opened 「どこへ？」.
 */
void LabPCList_DisableUnavailableSources(Menu* menu, LabPCEntry* moving_entry, ByteMatrix* sources) { // PORT: typed (were s8* and N64 offsets)
    s32 i;
    s32 j;
    Entry89100140* box;
    LabPCBox* entry;

    for (box = (Entry89100140*)sources->rows, i = 0; i < sources->count; i++, box++) {
        for (entry = (LabPCBox*)box->boxes.rows, j = 0; j < box->boxes.count; j++, entry++) {
            if ((entry->unk_08 != 0) && (entry->entries.count < box->unk_20) &&
                (entry != moving_entry->box)) {
                break;
            }
        }
        if (j >= box->boxes.count) {
            menu->itemFlags[i] |= 1;
        }
    }
}

extern char* D_8910A8DC[]; // PORT: a table of string pointers (was u8)
extern char D_8910DD34[]; // PORT: a string (was s32)
/**
 * Opens the 「どこへ？」 (where to?) menu: てもちのポケモン, ゲームボーイのボックス, 64カセットのボックス.
 * Seen in play (2026-11-14) in いちらん: いどうする on フシギダネ opened it.
 */
void LabPCList_OpenDestinationMenu(LabPCListScreen* screen, LabPCEntry* entry) { // PORT: entry typed (was s8**)
    LabPCList_InitPopupMenu(&screen->subPopup, D_8910A8DC, (u16*)&screen->popupItemFlags[1], 3, D_8910DD34);
    Window_SetRect((s8*)&screen->subPopup.anchorX, screen->popup.window.frame.x + 14, screen->popup.window.frame.y + 56, 108, 28);
    screen->subPopup.window.frame.style |= 0x1000000;
    screen->subPopup.limitX = screen->listFrame.x + screen->listFrame.width;
    screen->subPopup.limitY = screen->listFrame.y + screen->listFrame.height;
    LabPCList_PlacePopup(&screen->subPopup);
    screen->subPopup.window.frame.color = D_8910A7F4;
    LabPCList_DisableUnavailableSources(&screen->subPopup.menu, entry, &screen->sourceMatrix);
}

/**
 * Returns whether the Pokémon is the only one left in the party, which cannot be moved.
 * Seen in play (2026-11-14) in いちらん: with the party count poked to 1 at this check, いどうする showed 「さいごのポケモンです」.
 */
s32 LabPCList_IsLastPartyMon(LabPCEntry* entry) { // PORT: typed (was s8** and N64 offsets)
    s32 var_v1;
    LabPCBox* temp_v0;

    temp_v0 = entry->box;
    var_v1 = temp_v0->source->unk_04 == 0x20;
    if (var_v1 != 0) {
        var_v1 = temp_v0->entries.count == 1;
    }
    return var_v1;
}

void TextBox_Init(TextBox* arg0, char* arg1, s32 arg2, s32 arg3, u32 arg4); // PORT: matches the definition
extern Info891033A0 D_80065D00[];
extern s8 D_8910DD40[];
extern s8 D_8910DD58[];

/**
 * Handles the menu opened on a selected Pokemon.
 * Seen in play (2026-11-12) in いちらん (party list: フシギダネ, ヒトカゲ, ゼニガメ): was handling the popup
 * opened with A on フシギダネ.
 */
s32 LabPCList_UpdateMonMenu(LabPCListScreen* screen) {
    s32 ret = 0;
    LabPCEntry* entry; // PORT: typed (was s8*)

    LabPCList_UpdatePopupMenu(&screen->popup);
    if (((u16*)D_800603C0)[4] & 0x4000) {
        Audio_PlaySystemSound(3);
        screen->state = 0;
    } else if (((u16*)D_800603C0)[4] & 0x8000) {
        entry = ((PointerList*)screen->list.items)->items[screen->list.cursor];
        switch (screen->popup.menu.cursor) {
            case 0:
                Audio_PlaySystemSound(2);
                if (LabPCList_ToggleFilterFlag(screen, 8) != 0) {
                    screen->unk_9E58 = entry->mon.species.id;
                }
                LabPCList_ApplyFiltersAndSort(screen);
                screen->state = 0;
                break;
            case 1:
                Audio_PlaySystemSound(2);
                if (LabPCList_ToggleFilterFlag(screen, 0x10) != 0) {
                    s32 id;

                    id = entry->mon.species.id - 1;
                    screen->sameTypeFilterCount = 1;
                    screen->sameTypeFilterTypes[0] = D_80065D00[id].unk06;
                    if (D_80065D00[id].unk06 != D_80065D00[id].unk07) {
                        screen->sameTypeFilterTypes[1] = D_80065D00[id].unk07;
                        screen->sameTypeFilterCount++;
                    }
                }
                LabPCList_ApplyFiltersAndSort(screen);
                screen->state = 0;
                break;
            case 2:
                if (LabPCList_IsLastPartyMon(entry) != 0) {
                    Audio_PlaySystemSound(8);
                    TextBox_Init((s8*)screen->textBox, D_8910DD40, 0, 1, 0);
                    screen->state = 11;
                } else {
                    Audio_PlaySystemSound(2);
                    LabPCList_OpenDestinationMenu(screen, entry);
                    screen->state = 8;
                }
                break;
            case 3:
                if (!(screen->popup.menu.itemFlags[screen->popup.menu.cursor] & 1)) {
                    Audio_PlaySystemSound(2);
                    screen->state = 0;
                    ret = 2;
                } else {
                    Audio_PlaySystemSound(8);
                    TextBox_Init((s8*)screen->textBox, D_8910DD58, 0, 1, 0);
                    screen->state = 12;
                }
                break;
        }
    }
    return ret;
}

/**
 * Renumbers the slot index (entry[1]) of every Pokemon left in a box after one is taken out. Seen
 * in play (2026-11-12) in いちらん: after フシギダネ left the party, the two remaining entries went from
 * slots 1, 2 to 0, 1.
 */
void LabPCList_ReindexBoxSlots(LabPCBox* box) { // PORT: typed (was u8* and N64 offsets)
    s32 i;
    LabPCEntry** ptr = (LabPCEntry**)box->entries.items;

    for (i = 0; i < box->entries.count; i++) {
        (*ptr++)->slot = i;
    }
}
