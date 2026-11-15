/* Fixed-layout split; original translation-unit boundaries remain unconfirmed. */
#include "global.h"
#include "lab_pokemon_list.h"
#include "m2c_macros.h"

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
extern s32 D_8910A7F8[3];
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
extern Entry80066AD8 D_80066AD8[];
s32 Pokemon_GetTypeIndex(u8*, s32);
void func_8001A384(s32 mask, s32 arg1); // PORT: matches Font_SetActive
void Font_Printf(s32 x, s32 y, const char* fmt, ...);
void func_8001AA9C(s32 r, s32 g, s32 b, s32 a); // PORT: matches Gfx_SetEnvColor
void func_8001AB6C(void); // PORT: matches Font_BeginTranslucentTextRendering
void func_8001ABBC(void); // PORT: matches Font_EndTexturedTextRendering
extern Entry89100FC4 D_8006A9D4[];
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
s32 LabPCList_UpdateLevelInput(void*);
M2C_UNK LabPCList_ApplyFiltersAndSort(LabPCListScreen* screen); // PORT: matches the definition
extern char* D_8910A8DC[]; // PORT: a table of string pointers (was u8)
extern char D_8910DD34[]; // PORT: a string (was s32)
void TextBox_Init(TextBox* arg0, char* arg1, s32 arg2, s32 arg3, u32 arg4); // PORT: matches the definition
extern Info891033A0 D_80065D00[];
extern s8 D_8910DD40[];
extern s8 D_8910DD58[];

typedef struct LocationText7 { char s[7]; } LocationText7;
typedef struct LocationText9 { char s[9]; } LocationText9;
/**
 * Writes the name of a location: てもち (party), ＧＢ N (GB box N) and so on.
 * Seen in play (2026-11-12) in いちらん: kind 0 gave てもち, kind 1 box 0 gave GB 1, and kind 2 box 0
 * gave 64 1, all in the いどう message.
 */
void LabPCList_FormatLocationName(s8* buf, s32 source_kind, s32 box) {
    switch (source_kind) {
        case 0:
            *(LocationText7*)buf = *(LocationText7*)"てもち"; // "Party"
            break;
        case 1:
            *(LocationText9*)buf = *(LocationText9*)"ＧＢ　０"; // "GB 0"
            buf[strlen(buf) - 1] += box + 1;
            break;
        case 2:
            *(LocationText9*)buf = *(LocationText9*)"６４　０"; // "64 0"
            buf[strlen(buf) - 1] += box + 1;
            break;
    }
}
