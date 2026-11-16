/* Fixed-layout split; original translation-unit boundaries remain unconfirmed. */
#include "global.h"
#include "lab_pokemon_list.h"
#include "asset_archive.h"
#include "memory_main.h"
#include "m2c_macros.h"

// PORT: the decomp called these without a prototype (IDO allows it; clang does not).
s32 Font_Init(s32 load_size, s32 side);
void LabPCList_DrawListScreen(LabPCListScreen* screen);
void LabPCList_FormatLocationName(s8* buf, s32 source_kind, s32 box);
void LabPCList_InitListWindow(LabPCListScreen* screen, s32 x, s32 y, LabItemResult* msg_box); // PORT: matches the definition
void LabPCList_ReindexBoxSlots(LabPCBox* box);
s32 LabPCList_RunMoveFilter(LabPCListScreen* screen);
s32 LabPCList_RunTypeFilter(LabPCListScreen* screen);
s32 LabPCList_UpdateSourceSelect(LabPCListScreen* screen);

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
extern Gfx D_80060398[4]; // PORT: sized (open, the data after its G_ENDDL was lifted as commands)
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
s32 LabPCList_UpdateLevelInput(void*);
M2C_UNK LabPCList_ApplyFiltersAndSort(LabPCListScreen* screen); // PORT: matches the definition
extern char* D_8910A8DC[]; // PORT: a table of string pointers (was u8)
extern char D_8910DD34[]; // PORT: a string (was s32)
void TextBox_Init(TextBox* arg0, char* arg1, s32 arg2, s32 arg3, u32 arg4); // PORT: matches the definition
extern Info891033A0 D_80065D00[];
extern s8 D_8910DD40[];
extern s8 D_8910DD58[];

extern char D_8910DD94[];
extern char D_8910E398[];
extern char D_8910E3A8[];
void func_80027868(LabMonEntryList* arg0, s32 arg1); // PORT: matches PointerList_Remove
void func_80027814(PointerList* arg0, char* arg1, s32 arg2); // PORT: matches PointerList_Insert
/**
 * Moves the selected Pokemon to source `source_index`, box `box`, and shows the いどう (moved) message.
 * Seen in play (2026-11-12) in いちらん: ran with kind 1 (GB) or 2 (64 cartridge) and box 0, showed
 * the いどう message and left two Pokémon in the party.
 */
void LabPCList_MoveMonTo(LabPCListScreen* screen, LabPCEntry* entry, s32 source_index, s32 box, LabItemResult* msg) { // PORT: typed (were N64 views)
    Entry89100140* items = (Entry89100140*)screen->sourceMatrix.rows;
    LabPCBox* sub = &((LabPCBox*)items[source_index].boxes.rows)[box];
    LabPCBox* old = entry->box;
    msg->title = D_8910DD94;
    msg->kind = 1;
    LabPCList_FormatLocationName(D_8910E398, entry->box->source->unk_0C, entry->box->index);
    msg->left = D_8910E398;
    LabPCList_FormatLocationName(D_8910E3A8, source_index, box);
    msg->right = D_8910E3A8;
    func_80027868((LabMonEntryList*)&old->entries, entry->slot);
    LabPCList_ReindexBoxSlots(old);
    entry->box = sub;
    entry->slot = sub->entries.count;
    func_80027814(&sub->entries, (char*)entry, sub->entries.count);
    old->changed = 1;
    sub->changed = 1;
    screen->hasChanges = 1;
}

void func_800307D4(ScrollList* arg0, s32 arg1, s32 arg2); // PORT: matches WidgetTree_SetPagedGridSelectionWithOffset


extern char* D_8910A8E8[]; // PORT: a table of string pointers (was u8)
void LabPCList_DisableUnavailableBoxes(Menu* menu, LabPCEntry* entry, Entry89100140* source); // PORT: matches the definition
/**
 * Handles the どこへ？ (where to?) menu: party, GB box, Stadium box, or a box list.
 * Seen in play (2026-11-12) in いちらん (party list: フシギダネ, ヒトカゲ, ゼニガメ): いどうする opened 「どこへ？」 with
 * てもちのポケモン, ゲームボーイのボックス and 64カセットのボックス while this ran.
 */
s32 LabPCList_UpdateDestinationMenu(LabPCListScreen* screen) {
    s32 ret = 0;
    s32 cursor;
    s32 top;

    LabPCList_UpdatePopupMenu(&screen->subPopup);
    if (D_800603C0->buttonPressed & 0x4000) {
        Audio_PlaySystemSound(3);
        screen->state = 7;
    } else if (D_800603C0->buttonPressed & 0x8000) {
        if (!(screen->subPopup.menu.itemFlags[screen->subPopup.menu.cursor] & 1)) {
            switch (screen->subPopup.menu.cursor) {
                case 0:
                    Audio_PlaySystemSound(24);
                    cursor = screen->list.cursor;
                    top = screen->list.cursor - screen->list.scrollY / screen->list.cellHeight;
                    LabPCList_MoveMonTo((LabPCListScreen*)screen, ((LabPCEntry**)((PointerList*)screen->list.items)->items)[screen->list.cursor], 0, 0, screen->msg);
                    LabPCList_FlattenEntries(&screen->rows, &screen->sourceMatrix);
                    LabPCList_ApplyFiltersAndSort(screen);
                    func_800307D4(&screen->list.items, cursor, top);
                    screen->state = 0;
                    ret = 3;
                    break;
                case 3:
                    Audio_PlaySystemSound(24);
                    cursor = screen->list.cursor;
                    top = screen->list.cursor - screen->list.scrollY / screen->list.cellHeight;
                    LabPCList_MoveMonTo((LabPCListScreen*)screen, ((LabPCEntry**)((PointerList*)screen->list.items)->items)[screen->list.cursor], 3, 0, screen->msg);
                    LabPCList_FlattenEntries(&screen->rows, &screen->sourceMatrix);
                    LabPCList_ApplyFiltersAndSort(screen);
                    func_800307D4(&screen->list.items, cursor, top);
                    screen->state = 0;
                    ret = 3;
                    break;
                case 1:
                    Audio_PlaySystemSound(2);
                    LabPCList_InitPopupMenu(&screen->subPopup2, D_8910A8E8, (u16*)&screen->popupItemFlags[2], 8, 0);
                    Window_SetRect((s8*)&screen->subPopup2.anchorX, screen->subPopup.window.frame.x + 14, screen->subPopup.window.frame.y + 28, 228, 28);
                    screen->subPopup2.window.frame.style |= 0x01000000;
                    screen->subPopup2.limitX = screen->listFrame.x + screen->listFrame.width;
                    screen->subPopup2.limitY = screen->listFrame.y + screen->listFrame.height;
                    LabPCList_PlacePopup(&screen->subPopup2);
                    screen->subPopup2.window.frame.color = D_8910A7F4;
                    LabPCList_DisableUnavailableBoxes(&screen->subPopup2.menu, ((LabPCEntry**)((PointerList*)screen->list.items)->items)[screen->list.cursor], (Entry89100140*)screen->sourceMatrix.rows + 1) /* PORT: was rows + 0x24 */;
                    screen->state = 9;
                    break;
                case 2:
                    Audio_PlaySystemSound(2);
                    LabPCList_InitPopupMenu(&screen->subPopup2, D_8910A8E8, (u16*)&screen->popupItemFlags[2], 4, 0);
                    Window_SetRect((s8*)&screen->subPopup2.anchorX, screen->subPopup.window.frame.x + 14, screen->subPopup.window.frame.y + 56, 228, 28);
                    screen->subPopup2.window.frame.style |= 0x01000000;
                    screen->subPopup2.limitX = screen->listFrame.x + screen->listFrame.width;
                    screen->subPopup2.limitY = screen->listFrame.y + screen->listFrame.height;
                    LabPCList_PlacePopup(&screen->subPopup2);
                    screen->subPopup2.window.frame.color = D_8910A7F4;
                    LabPCList_DisableUnavailableBoxes(&screen->subPopup2.menu, ((LabPCEntry**)((PointerList*)screen->list.items)->items)[screen->list.cursor], (Entry89100140*)screen->sourceMatrix.rows + 2) /* PORT: was rows + 0x48 */;
                    screen->state = 10;
                    break;
            }
        } else {
            Audio_PlaySystemSound(8);
        }
    }
    return ret;
}


/**
 * Moves the Pokemon to the chosen Game Boy box.
 * Seen in play (2026-11-12) in いちらん: いどうする > ゲームボーイのボックス showed ボックス1-8; ボックス1 moved フシギダネ out of
 * the party (「いどう てもち -> GB 1」).
 */
s32 LabPCList_UpdateGbBoxMenu(LabPCListScreen* screen) {
    s32 ret = 0;
    s32 cursor;
    s32 top;

    LabPCList_UpdatePopupMenu(&screen->subPopup2);
    if (D_800603C0->buttonPressed & 0x4000) {
        Audio_PlaySystemSound(3);
        screen->state = 8;
    } else if (D_800603C0->buttonPressed & 0x8000) {
        if (!(screen->subPopup2.menu.itemFlags[screen->subPopup2.menu.cursor] & 1)) {
            cursor = screen->list.cursor;
            top = screen->list.cursor - screen->list.scrollY / screen->list.cellHeight;
            Audio_PlaySystemSound(24);
            LabPCList_MoveMonTo((LabPCListScreen*)screen, ((LabPCEntry**)((PointerList*)screen->list.items)->items)[screen->list.cursor], 1, screen->subPopup2.menu.cursor, screen->msg);
            LabPCList_FlattenEntries(&screen->rows, &screen->sourceMatrix);
            LabPCList_ApplyFiltersAndSort(screen);
            func_800307D4(&screen->list.items, cursor, top);
            screen->state = 0;
            ret = 3;
        } else {
            Audio_PlaySystemSound(8);
        }
    }
    return ret;
}

/**
 * Moves the Pokemon to the chosen Stadium (64 cartridge) box.
 * Seen in play (2026-11-12) in いちらん: いどうする > 64カセットのボックス showed ボックス1-4; ボックス1 moved フシギダネ out of
 * the party (「いどう てもち -> 64 1」).
 */
s32 LabPCList_UpdateStadiumBoxMenu(LabPCListScreen* screen) {
    s32 ret = 0;
    s32 cursor;
    s32 top;

    LabPCList_UpdatePopupMenu(&screen->subPopup2);
    if (D_800603C0->buttonPressed & 0x4000) {
        Audio_PlaySystemSound(3);
        screen->state = 8;
    } else if (D_800603C0->buttonPressed & 0x8000) {
        if (!(screen->subPopup2.menu.itemFlags[screen->subPopup2.menu.cursor] & 1)) {
            cursor = screen->list.cursor;
            top = screen->list.cursor - screen->list.scrollY / screen->list.cellHeight;
            Audio_PlaySystemSound(24);
            LabPCList_MoveMonTo((LabPCListScreen*)screen, ((LabPCEntry**)((PointerList*)screen->list.items)->items)[screen->list.cursor], 2, screen->subPopup2.menu.cursor, screen->msg);
            LabPCList_FlattenEntries(&screen->rows, &screen->sourceMatrix);
            LabPCList_ApplyFiltersAndSort(screen);
            func_800307D4(&screen->list.items, cursor, top);
            screen->state = 0;
            ret = 3;
        } else {
            Audio_PlaySystemSound(8);
        }
    }
    return ret;
}

extern char D_8910DDFC[];
extern char D_8910DE04[];
extern char D_8910DE10[];
extern char D_8910DE1C[];
extern char D_8910DE28[];
extern char D_8910DE34[];
u32 func_80020234(s32 stat_id, s32 species_id, u16 stat_modifier, u16 level, u16 dv_bits); // PORT: matches Pokemon_CalcStat
u32 func_8002057C(s32 arg0, u16 arg1); // PORT: matches Pokemon_ExpForLevel
void func_800203FC(BattleMon* arg0); // PORT: matches Pokemon_RecalcStats
void LabPCList_ConsumeUsedItem(TmEntry8910A21C* item); // PORT: matches the definition
/**
 * Uses a vitamin (HP Up, Protein, Iron, Carbos, Calcium; 35-39) or a Rare Candy (40) on the
 * Pokemon and fills in the old and new values for the result message.
 * Seen in play (2026-11-12) in いちらん: マックスアップ (HP Up) on フシギダネ showed HP 19 -> 20, and
 * ふしぎなアメ showed レベル 5 -> 6 with the stats recomputed. After レポートしてやめる the Game Boy save
 * had HP stat exp +2560, max HP 20 and one HP Up fewer.
 */
void LabPCList_ApplyVitaminItem(LabPCEntry* entry, TmEntry8910A21C* item, LabItemResult* msg) { // PORT: typed (were N64 views)
    BattleMon* mon;
    u16 old_max_hp;

    msg->kind = 0;
    mon = &entry->mon;
    switch (item->unk_00[0]) {
        case 35:
            msg->title = D_8910DDFC;
            msg->left = (char*)(intptr_t)mon->maxHp; // PORT: a number when kind is 0
            mon->statExp[0] += 2560;
            if (mon->statExp[0] > 25600) {
                mon->statExp[0] = 25600;
            }
            mon->maxHp = func_80020234(1, mon->species.id, mon->statExp[0], mon->level, mon->dvs);
            msg->right = (char*)(intptr_t)mon->maxHp;
            break;
        case 36:
            msg->title = D_8910DE04;
            msg->left = (char*)(intptr_t)mon->attack; // PORT: a number when kind is 0
            mon->statExp[1] += 2560;
            if (mon->statExp[1] > 25600) {
                mon->statExp[1] = 25600;
            }
            mon->attack = func_80020234(2, mon->species.id, mon->statExp[1], mon->level, mon->dvs);
            msg->right = (char*)(intptr_t)mon->attack;
            break;
        case 37:
            msg->title = D_8910DE10;
            msg->left = (char*)(intptr_t)mon->defense; // PORT: a number when kind is 0
            mon->statExp[2] += 2560;
            if (mon->statExp[2] > 25600) {
                mon->statExp[2] = 25600;
            }
            mon->defense = func_80020234(3, mon->species.id, mon->statExp[2], mon->level, mon->dvs);
            msg->right = (char*)(intptr_t)mon->defense;
            break;
        case 38:
            msg->title = D_8910DE1C;
            msg->left = (char*)(intptr_t)mon->speed; // PORT: a number when kind is 0
            mon->statExp[3] += 2560;
            if (mon->statExp[3] > 25600) {
                mon->statExp[3] = 25600;
            }
            mon->speed = func_80020234(4, mon->species.id, mon->statExp[3], mon->level, mon->dvs);
            msg->right = (char*)(intptr_t)mon->speed;
            break;
        case 39:
            msg->title = D_8910DE28;
            msg->left = (char*)(intptr_t)mon->special; // PORT: a number when kind is 0
            mon->statExp[4] += 2560;
            if (mon->statExp[4] > 25600) {
                mon->statExp[4] = 25600;
            }
            mon->special = func_80020234(5, mon->species.id, mon->statExp[4], mon->level, mon->dvs);
            msg->right = (char*)(intptr_t)mon->special;
            break;
        case 40:
            msg->title = D_8910DE34;
            msg->left = (char*)(intptr_t)mon->level; // PORT: a number when kind is 0
            old_max_hp = mon->maxHp;
            mon->exp = func_8002057C(mon->species.id, mon->level + 1);
            func_800203FC(mon);
            mon->hp += mon->maxHp - old_max_hp;
            msg->right = (char*)(intptr_t)mon->level;
            break;
    }
    LabPCList_ConsumeUsedItem(item);
    entry->box->changed = 1;
}

typedef struct Entry891041D8 {
    /* 0x0 */ char* name;
    /* 0x4 */ char unk04[0x8];
} Entry891041D8; // size = 0xC
// PORT: D_8006A9CC (the move table seen 8 bytes early) is read as D_8006A9D4[id - 1]
typedef struct Str891041D8 {
    char c[7];
} Str891041D8;
extern char D_8910DE3C[];
extern s8 D_8910E3B8[];

/**
 * Uses a PP Up on move slot `slot` of the Pokemon and writes the result message.
 * Seen in play (2026-11-12) in いちらん: ポイントアップ on フシギダネ, move slot 0, showed 「たいあたりのPP」 35 -> 42.
 */
void LabPCList_ApplyPpUpItem(LabPCEntry* entry, TmEntry8910A21C* item, s32 slot, LabItemResult* msg) { // PORT: typed (were N64 views)
    u8* ptr;
    s32 base_pp;
    s32 step;
    s8* end;

    func_8000A460(D_8910E3B8, D_8006A9D4[(entry->mon.moves[slot]) - 1].name);
    ptr = (u8*)&entry->mon + slot; // PORT: was entry + slot + 8; ptr[9] is mon.moves[slot], ptr[0x20] is mon.pp[slot]
    end = D_8910E3B8 + strlen(D_8910E3B8);
    *(Str891041D8*)end = *(Str891041D8*)D_8910DE3C;
    if (1) {} // karodo
    msg->title = D_8910E3B8;
    msg->kind = 0;
    base_pp = gMoveData[ptr[9] - 1].pp;
    step = base_pp / 5;
    if (step >= 7) {
        step = 7;
    }
    msg->left = (char*)(intptr_t)((ptr[0x20] >> 6) * step + base_pp); // PORT: a number when kind is 0
    msg->right = (char*)(intptr_t)((s32)(intptr_t)msg->left + step);
    ptr[0x20] += step + 0x40;
    LabPCList_ConsumeUsedItem(item); // PORT: the definition takes one argument (m2c passed leftover registers)
    entry->box->changed = 1;
}

u8 Move_GetIdFromTableIndex(s32 arg0);

/**
 * Teaches the TM or HM move of `item` in move slot `slot` (item ids from 201 are TMs, below are HMs).
 * Seen in play (2026-11-12) in いちらん: わざマシン06 on フシギダネ ran this with slot 2, and どくどく (Toxic) then
 * appeared as わざ3.
 */
void LabPCList_TeachTmHmMove(LabPCEntry* entry, TmEntry8910A21C* item, s32 slot) { // PORT: typed (were N64 views)
    u8* ptr;
    s32 var_a0;

    if (item->unk_00[0] >= 201) {
        var_a0 = item->unk_00[0] - 201;
    } else {
        var_a0 = item->unk_00[0] - 146;
    }

    ptr = (u8*)&entry->mon + slot; // PORT: was entry + slot + 8; ptr[9] is mon.moves[slot], ptr[0x20] is mon.pp[slot]
    ptr[9] = Move_GetIdFromTableIndex(var_a0);
    ptr[0x20] = gMoveData[ptr[9] - 1].pp;

    if (item->unk_00[0] >= 201) {
        LabPCList_ConsumeUsedItem(item);
    }
    entry->box->changed = 1;
}

s32 LabPCList_UpdateBrowse(LabPCListScreen* screen); // PORT: matches the definition
s32 LabPCList_UpdateSortMenu(LabPCListScreen*);
s32 LabPCList_UpdateColumnMenu(LabPCListScreen*);
s32 LabPCList_RunLevelFilter(s8*);
s32 LabPCList_UpdateMonMenu(LabPCListScreen*);
s32 LabPCList_UpdateGbBoxMenu();
s32 LabPCList_UpdateStadiumBoxMenu();
s32 Ui_CheckConfirmPressed(void* arg0); // PORT: matches the definition
/**
 * Runs the list window's state machine (browse, sort menu, filters, moves).
 * Seen in play (2026-11-12): ran the list window.
 */
s32 LabPCList_UpdateList(LabPCListScreen* screen) {
    s32 ret = 0;

    if (screen->window.state == 0) {
        return 0;
    }
    switch (screen->state) {
        case 0:
            ret = LabPCList_UpdateBrowse(screen);
            break;
        case 1:
            ret = LabPCList_UpdateSourceSelect(screen);
            break;
        case 2:
            ret = LabPCList_UpdateSortMenu(screen);
            break;
        case 3:
            ret = LabPCList_UpdateColumnMenu(screen);
            break;
        case 4:
            ret = LabPCList_RunLevelFilter(screen);
            break;
        case 5:
            ret = LabPCList_RunTypeFilter(screen);
            break;
        case 6:
            ret = LabPCList_RunMoveFilter(screen);
            break;
        case 7:
            ret = LabPCList_UpdateMonMenu(screen);
            break;
        case 8:
            ret = LabPCList_UpdateDestinationMenu(screen);
            break;
        case 9:
            ret = LabPCList_UpdateGbBoxMenu(screen);
            break;
        case 10:
            ret = LabPCList_UpdateStadiumBoxMenu(screen);
            break;
        case 11:
        case 12:
            if (Ui_CheckConfirmPressed((s8*)screen->textBox) == 1) {
                screen->state = 0;
            }
            break;
    }
    return ret;
}

/**
 * Sets the state to the list and sets up the list screen, which gets the item result message.
 * Seen in play (2026-11-12) in いちらん (party list: フシギダネ, ヒトカゲ, ゼニガメ): ran on entry, right before
 * LabPCList_InitListWindow.
 */
void LabPCList_InitScreen(LabPCList* lab) {
    lab->state = 0;
    LabPCList_InitListWindow(&lab->listScreen, 38, 72, &lab->message);
}

void SavePrompt_Init(SavePrompt*, s32, s32);
void Save_DrawReportingWindow(SavePrompt*);
s32 SavePrompt_Update(SavePrompt*);
void LabPCList_SaveChanges(LabPCListScreen*);
void LabPCList_InitItemWindow(LabItemWindow* window, s32 x, s32 y, PointerList* items, u16* item_flags, LabPCEntry* mon); // PORT: matches the definition
s32 LabPCList_UpdateItemWindow(LabItemWindow*);
void LabPCList_InitMessageWindow(LabMessageWindow*, s32, s32, LabItemResult*);
s32 LabPCList_UpdateMessageWindow(LabMessageWindow*);
/**
 * Per-frame update of the いちらん screen: runs the list, the save prompt, the item window or the
 * message window by state. Returns 1 when the caller should leave this screen.
 * Seen in play (2026-11-12): ran the いちらん screen.
 */
s32 LabPCList_UpdateScreen(LabPCList* lab) {
    s32 ret = 0;
    s32 r;
    s8* sub;

    switch (lab->state) {
        case 0:
            sub = (s8*)&lab->listScreen;
            r = LabPCList_UpdateList(sub);
            if (r == 1) {
                if (lab->listScreen.hasChanges != 0) {
                    SavePrompt_Init(&lab->savePrompt, 202, 173);
                    lab->state = 1;
                } else {
                    ret = 1;
                }
            } else if (r == 2) {
                // PORT: N64 offsets 0xAFFC/0xB008 and the entry + 8 mon replaced with fields
                LabPCList_UpdateItemUsability(&lab->listScreen.usableItemList, (u8*)&((LabPCEntry**)lab->listScreen.list.items->rows)[lab->listScreen.list.cursor]->mon);
                LabPCList_InitItemWindow(&lab->itemWindow, ((LabPCListScreen*)sub)->window.frame.x + 171, ((LabPCListScreen*)sub)->window.frame.y + 13, &((LabPCListScreen*)sub)->usableItemList,
                              ((LabPCListScreen*)sub)->usableItemFlags, ((LabPCEntry**)((LabPCListScreen*)sub)->list.items->rows)[((LabPCListScreen*)sub)->list.cursor]);
                lab->state = 2;
            } else if (r == 3) {
                LabPCList_InitMessageWindow(&lab->messageWindow, 182, 234, &lab->message);
                lab->state = 3;
            }
            break;
        case 1:
            sub = (s8*)&lab->savePrompt;
            r = SavePrompt_Update((SavePrompt*)sub);
            if (r == -1) {
                lab->state = 0;
            } else if (r > 0) {
                switch (r) {
                    case 1:
                        Save_DrawReportingWindow((SavePrompt*)sub);
                        LabPCList_SaveChanges(&lab->listScreen);
                        ret = 1;
                        break;
                    case 2:
                        ret = 1;
                        break;
                }
            }
            break;
        case 2:
            r = LabPCList_UpdateItemWindow(&lab->itemWindow);
            if (r == -1) {
                lab->state = 0;
            } else {
                // PORT: the ROM stores through lab + 0xB55C (lab->listScreen.hasChanges) held in a variable, a FAKE that
                // stops IDO reusing lab + 0x8000; the pointers below were s32

                if (r > 0) {
                    TmEntry8910A21C* a1 = ((TmEntry8910A21C**)lab->itemWindow.list.items->rows)[lab->itemWindow.list.cursor];
                    LabPCEntry* a0 = ((LabPCEntry**)lab->listScreen.list.items->rows)[lab->listScreen.list.cursor];

                    switch (r) {
                        case 1:
                            LabPCList_ApplyVitaminItem(a0, a1, &lab->message);
                            break;
                        case 2:
                            LabPCList_ApplyPpUpItem(a0, a1, lab->itemWindow.moveSelect.menu.cursor, &lab->message);
                            break;
                        case 3:
                            LabPCList_TeachTmHmMove(a0, a1, lab->itemWindow.moveSelect.menu.cursor);
                            break;
                    }
                    lab->listScreen.hasChanges = 1;
                    if (r == 3) {
                        lab->state = 0;
                    } else {
                        LabPCList_InitMessageWindow(&lab->messageWindow, 182, 234, &lab->message);
                        lab->state = 3;
                    }
                }
            }
            break;
        case 3:
            if (LabPCList_UpdateMessageWindow(&lab->messageWindow) == 1) {
                lab->state = 0;
            }
            break;
    }
    return ret;
}

M2C_UNK Ui_DrawTintedScreenBackground(M2C_UNK, M2C_UNK, u8, u8, s32); /* extern */
extern M2C_UNK D_8910A7F0;

/**
 * Draws the tinted terminal screen background.
 * Seen in play (2026-11-12): the いちらん background.
 */
void LabPCList_DrawBackground(void) {
    Ui_DrawTintedScreenBackground(56, 480, M2C_FIELD(&D_8910A7F0, u8 *, 0), M2C_FIELD(&D_8910A7F0, u8 *, 1), (s32) M2C_FIELD(&D_8910A7F0, u8 *, 2));
}

void BgStage_DrawFrame(void);
s32 Window_UpdateAnimation(Window* arg0);
void SavePrompt_Draw(SavePrompt*);
void LabPCList_DrawItemWindow(LabItemWindow*);
void LabPCList_DrawMessageWindow(LabMessageWindow*);

/**
 * Draws the lab PC list for its current state.
 * Seen in play (2026-11-12): drew the いちらん screen.
 */
M2C_UNK LabPCList_DrawScreen(LabPCList* lab) {
    BgStage_DrawFrame();
    switch (lab->state) {
        case 0:
            if (Window_UpdateAnimation(&lab->listScreen) != 0) {
                LabPCList_DrawBackground();
            }
            LabPCList_DrawListScreen(&lab->listScreen);
            break;
        case 1:
            if (Window_UpdateAnimation(&lab->listScreen) != 0) {
                LabPCList_DrawBackground();
            }
            LabPCList_DrawListScreen(&lab->listScreen);
            SavePrompt_Draw(&lab->savePrompt);
            break;
        case 2:
            if (Window_UpdateAnimation(&lab->listScreen) != 0) {
                LabPCList_DrawBackground();
            }
            if (Window_UpdateAnimation(&lab->itemWindow) != 0) {
                LabPCList_DrawListScreen(&lab->listScreen);
            }
            LabPCList_DrawItemWindow(&lab->itemWindow);
            break;
        case 3:
            if (Window_UpdateAnimation(&lab->listScreen) != 0) {
                LabPCList_DrawBackground();
            }
            LabPCList_DrawListScreen(&lab->listScreen);
            LabPCList_DrawMessageWindow(&lab->messageWindow);
            break;
    }
}

void func_80027FF8(GbSavePortSummary* arg0, s32 arg1); // PORT: matches GbSave_BuildPortSummary
extern M2C_UNK D_8910E380;
extern LabPCList D_8910E3D0;

/**
 * Seen in play (2026-11-12): runs once when いちらん opens; it reads the active controller port and
 * calls LabPCList_InitScreen. The old name BattlePrep_UpdateSingleBattleIntroFrame was a Stadium
 * 1 name for the same address.
 */
void LabPCList_Setup(void) {
    D_8910E37C = GbSave_GetActivePort();
    if (D_8910E37C != -1) {
        func_80027FF8(&D_8910E380, D_8910E37C);
    }
    LabPCList_InitScreen(&D_8910E3D0);
}

typedef struct Col891049E8 { u8 r, g, b, a; } Col891049E8;
extern s32 D_8910A908;
extern s32 D_8910A90C;
/*
 * Second name for D_8910A908. The fade-out store in this function family
 * (also FreeBattle_UpdateFade, LabTerminal_UpdateFade, LabPCList_UpdateFade, Pokedex_DrawFadeFrame) does not
 * share the counter's address with the load before it, which only happens
 * when the two accesses name different symbols.
 */
extern s32 D_8910A908_2;
extern char D_8910DE44[]; // PORT: text (was M2C_UNK, lifted as swapped words)
M2C_UNK GbSave_DrawSummaryBar(M2C_UNK, M2C_UNK *, M2C_UNK *, s32);
Color_RGBA8 func_8002C8D8(Color_RGBA8 arg0, f32 arg1); // PORT: matches Color_InterpolateFromGray
/**
 * Steps the screen fade; returns once eight frames have passed.
 * Seen in play (2026-11-13) ran on the frames the いちらん (list) screen faded in from the main menu.
 */
s32 LabPCList_UpdateFade(UNUSED u8 unused) { // PORT: u8 to match BgStage_RunUntilCondition's callback type
    Col891049E8 sp2C;
    M2C_UNK *var_a1;
    s32 n;

    n = D_8910A908;
    if (n >= 8) {
        D_8910A90C = 1;
    }
    if (D_8910A90C != 0) {
        n--;
        D_8910A908_2 = n;
    }
    BgStage_DrawFrame();
    if (D_8910E37C == -1) {
        var_a1 = NULL;
    } else {
        var_a1 = &D_8910E380;
    }
    GbSave_DrawSummaryBar(0, var_a1, D_8910DE44, D_8910E37C);
    n = D_8910A908;
    *(Color_RGBA8*)&sp2C = func_8002C8D8(*(Color_RGBA8*)&D_8910A7F0, n / (f64)8.0f); // PORT: the color is the return value (the N64 ABI passed &sp2C)
    Ui_DrawTintedScreenBackground(56, 480, sp2C.r, sp2C.g, sp2C.b);
    if (D_8910A90C == 0) {
        n = D_8910A908;
        n++;
        D_8910A908 = n;
    }
    return 0;
}

void Cont_StartReadInputs(void);                            /* extern */
void Cont_ReadInputs(void);                            /* extern */
extern LabPCList D_8910E3D0;

/**
 * One frame of the Pokemon list screen.
 * Seen in play (2026-11-12): ran every frame of いちらん.
 */
s32 LabPCList_RunFrame(UNUSED u8 unused) { // PORT: was void; on the N64 UpdateScreen's result stayed in v0 and ended the loop
    Cont_StartReadInputs();
    Cont_ReadInputs();
    LabPCList_DrawScreen(&D_8910E3D0);
    return LabPCList_UpdateScreen(&D_8910E3D0);
}

extern u8 D_01000000[];
extern u8 D_002E0BE0[];
extern u8 D_002E2B90[];
extern u8 D_04000000[];
extern u8 D_00400F70[];
extern u8 D_004076E0[];
extern u8 D_88000000[];
extern u8 D_00235D70[];
extern u8 D_00238BD0[];
s32 LabPCList_UpdateFade(u8);

/**
 * LabPCList_Init
 *
 * Fragment entry point.  It has the same steps as LabTerminal_Init (and the Pokedex
 * entry function Pokedex_Main), differing only in the functions it calls in
 * steps 4 and 5.  Neither argument is used.  The steps are:
 *
 *   1. MainPool_PushState('TERM') saves the main pool state under that tag;
 *      the matching MainPool_PopState('TERM') is near the end.
 *   2. Font_Init(0x3C, 0) is called (Font_Init in symbol_addrs.txt).
 *   3. Two assets are loaded with Asset_LoadToSegment: the ROM range
 *      D_002E0BE0..D_002E2B90 into segment 1 and D_00400F70..D_004076E0 into
 *      segment 4.  The segment number is bits 24-27 of the address of
 *      D_01000000 / D_04000000.
 *   4. LabPCList_Setup() and StageLoader_UpdateSegments() are called.
 *   5. BgStage_RunUntilCondition is run three times, with timer values 8, 0
 *      and 8: the first and third with callback LabPCList_UpdateFade, the second
 *      with LabPCList_RunFrame: fade in, run the list until it exits, fade out.
 *   6. StageLoader_WaitForRetrace() and MainPool_PopState('TERM').
 *   7. Fragment_QueueCall queues a call to the fragment whose id comes from
 *      the address D_88000000 (bits 20-27, minus 0x10), with the ROM range
 *      D_00235D70..D_00238BD0.
 *
 * Returns 0x8040.
 * Seen in play (2026-11-12) in いちらん (party list: フシギダネ, ヒトカゲ, ゼニガメ): ran when いちらん was entered
 * from the main menu, before LabPCList_InitScreen.
 */
s32 LabPCList_Init(UNUSED s32 unused0, UNUSED s32 unused1) {
    MainPool_PushState('TERM');
    Font_Init(0x3C, 0);
    Asset_LoadToSegment(((uintptr_t)D_01000000 & 0x0F000000) >> 0x18, D_002E0BE0, D_002E2B90, 0);
    Asset_LoadToSegment(((uintptr_t)D_04000000 & 0x0F000000) >> 0x18, D_00400F70, D_004076E0, 0);
    LabPCList_Setup();
    StageLoader_UpdateSegments();
    BgStage_RunUntilCondition(8, LabPCList_UpdateFade);
    BgStage_RunUntilCondition(0, LabPCList_RunFrame);
    BgStage_RunUntilCondition(8, LabPCList_UpdateFade);
    StageLoader_WaitForRetrace();
    MainPool_PopState('TERM');
    Fragment_QueueCall(((((uintptr_t)D_88000000) & 0x0FF00000) >> 0x14) - 0x10, D_00235D70, D_00238BD0, 0, 0);
    return 0x8040;
}
