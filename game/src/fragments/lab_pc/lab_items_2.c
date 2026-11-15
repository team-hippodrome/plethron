/* Fixed-layout split; original translation-unit boundaries remain unconfirmed. */
#include "lab_pc.h"
#include "input.h"
extern s32 pad_D_8831A4DC[8];
#include "m2c_macros.h"

// PORT: the decomp called these without a prototype (IDO allows it; clang does not).
void LabItem_DrawBackground(void);
void ScrollList_SetItems(ScrollList* arg0, ByteMatrix* arg1, u16* arg2); // PORT: matches the definition
void LabItem_Unused(void);
s32 LabItem_ComputeRemainingCapacity(s32 arg0, ByteMatrix* arg1);
s32 LabItem_ComputeMaxTransferQuantity(s32 arg0, ByteMatrix* arg1);
s32 LabItem_FindOrAllocateGamePakBoxSlot(LabItemSaveSlot* arg0, s32 arg1, char* arg2);
void LabItem_LoadGamePakBoxSlots(LabItemSaveSlot* arg0);
void LabItem_ReadDeckEntries(s32 arg0, s32 arg1, s32 arg2, ByteMatrix* arg3);
s32 func_800275E4(ByteMatrix* arg0, s32 arg1); // PORT: matches ByteMatrix_RemoveRow
void ScrollList_SetCursorKeepOffset(s32*, s32);
s32 func_800275E4(ByteMatrix* arg0, s32 arg1); // PORT: matches ByteMatrix_RemoveRow
s32 func_800274E0(ByteMatrix* arg0, s8* arg1, s32 arg2); // PORT: matches ByteMatrix_InsertRow
void ScrollList_SetCursorKeepOffset(s32*, s32);
extern M2C_UNK D_8900A5B8;
void Window_SetRect(s8*, s32, s32, s32, s32);
void Window_InitLabeled(s8*, s32, s32, s32, Color_RGBA8, char*);
void func_8000B2F4(Controller* arg0); // PORT: matches Input_SetRepeatController
extern Color_RGBA8 D_8900A5C0;
extern char D_89018460[];
extern char D_89018468[];
extern struct Controller* D_800603C0; // PORT: gPlayer1Controller, a pointer
void Font_Printf(s32 x, s32 y, const char* fmt, ...);
void Ui_DrawTexturedDivider(s32, s32, s32, void*);
void Ui_FillRectPrimEnvColor(s32, s32, s32, s32, Color_RGBA8, Color_RGBA8);
void func_80029028(s32, s32, s32, s32, Color_RGBA8);
void Window_DrawLabeled(s8*, s32);
extern u8 D_8007356C[];
extern M2C_UNK D_800734A8;
extern Gfx D_80060270[];
extern Color_RGBA8 D_8900A5C4;
extern Color_RGBA8 D_8900A5C8;
extern Color_RGBA8 D_8900A5CC;
s32 func_8002B390(UiSelection* arg0); // PORT: matches Ui_HandleHorizontalSelectionInput
s32 func_8002BC14(Controller* arg0); // PORT: matches Ui_GetSelectionDirection
extern struct Controller* D_800603C0; // PORT: gPlayer1Controller, a pointer
extern Color_RGBA8 D_8900A5BC;
void func_8001B884(s32 x1, s32 y1, s32 draw_width, s32 height, u8* texture, s32 load_width, s32 texture_spread_x_mode); // PORT: matches Gfx_DrawTextureRgba16
void Font_DrawCharAt(s32 x, s32 y, s32 chr);
void Font_SetColorForFlags(s32);
void Ui_DrawRoundedFrame(s32, s32, s32, s32, Color_RGBA8);
void TextBox_Draw(TextBox*);
extern Gfx D_80073408[];
extern Gfx D_80073438[];
extern Color_RGBA8 D_80073520;
extern u8 D_04002130[];
extern u8 D_04002238[];
extern u8 D_04002340[];
extern u8 D_04002448[];
extern char D_89018470[];
extern struct Controller* D_800603C0; // PORT: gPlayer1Controller, a pointer
extern M2C_UNK D_89018474;
void Ui_FillRectHighlighted(s32, s32, s32, s32, Color_RGBA8);
void func_8001AA9C(s32 r, s32 g, s32 b, s32 a); // PORT: matches Gfx_SetEnvColor
extern Color_RGBA8 D_8900A5D4[];
extern M2C_UNK D_800734A8;
void Window_SetRect(s8*, s32, s32, s32, s32);
void Menu_Init(Menu* arg0, ByteMatrix* arg1, u16* arg2, MenuDrawItemFunc arg3, s32 arg4, s32 arg5); // PORT: matches the definition
void func_80027804(PointerList* arg0, void** arg1, s32 arg2, s32 arg3); // PORT: matches PointerList_Initialize
s32 Menu_UpdateCursorSkipDisabled(void*);
extern Gfx D_80060270[];
extern Color_RGBA8 D_8900A5E0;
extern Color_RGBA8 D_8900A5E8;
extern Color_RGBA8 D_8900A5EC[];
extern char D_8901848C[];
char* func_8001FDC8(s32 arg0, char* arg1); // PORT: matches Text_GetTmHmName
s32 func_80031C24(s32 arg0); // PORT: matches LabItem_IsQuantitylessItem
s32 func_80031CC8(s32 arg0); // PORT: matches LabItem_IsQuantitylessItemForOperation
void Font_SetColorForFlags(s32);
void ScrollList_Init(ScrollList* arg0, ByteMatrix* arg1, u16* arg2, s32 arg3, MenuDrawItemFunc arg4, s32 arg5, s32 arg6); // PORT: matches the definition
void LabItem_DrawItemCountRow(s32 x, s32 y, u8* row, s32 index, s32 flags); // PORT: matches the definition
void ScrollList_DrawSmoothScroll(ScrollList* menu, s32 x, s32 y, s32 mask, intptr_t arg4); // PORT: matches the definition
void ScrollList_DrawInlineItems(ScrollList* menu, s32 x, s32 y, s32 mask, intptr_t arg4); // PORT: matches the definition
void Ui_DrawNumberZeroPadded(s32, s32, s32, s32);
void func_8001BCB4(s32 x1, s32 y1, s32 draw_width, s32 height, u8* texture, s32 load_width, s32 texture_spread_x_mode); // PORT: matches Gfx_DrawTextureIa8
extern Gfx D_80073450[];
extern u8 D_04011AB8[];
s32 ScrollList_UpdateCursor(void*);
s32 ScrollList_UpdateSmoothScroll(void*);
extern PrimEnvColor D_8900A608;
extern PrimEnvColor D_8900A610;
extern u8* D_89017A80[];
extern u8 D_89017A98[];
extern s32 D_89019114;
extern Gfx D_80073408[];
extern Gfx D_80073438[];
void Gfx_DrawTextureRgba16(s32, s32, s32, s32, u8*, s32, s32);
void Font_SetColorForFlags(s32);
s32 func_8002BC44(s32 arg0); // PORT: matches GbSave_GameToCategoryIndex
void Font_Printf(s32 x, s32 y, const char* fmt, ...);
extern M2C_UNK D_800734A8;
void Font_Printf(s32 x, s32 y, const char* fmt, ...);
extern M2C_UNK D_800734A8;
extern char* D_8900A650[]; // PORT: a table of string pointers (was s32)
extern char* D_8900A660[];
extern char* D_8900A66C[];
extern s8* D_8900A678[];
char* func_80031BC0(s32 item_id, char* dest); // PORT: matches func_8002E160_jp
typedef struct LabItemGridZ {
    /* 0x00 */ u8* data;
    /* 0x04 */ s32 pad04[2];
    /* 0x0C */ s32 stride;
} LabItemGridZ;
typedef struct LabItemCursorZ {
    /* 0x00 */ LabItemGridZ* grid;
    /* 0x04 */ s32 pad04;
    /* 0x08 */ s32 selected;
} LabItemCursorZ;
#define CURSOR(p) ((LabItemCursorZ*)((p) + 0x1E4))
s32 func_80027654(ByteMatrix* arg0, s8* a1, s32 arg2); // PORT: matches ByteMatrix_CopyRow
s32 func_80027700(ByteMatrix* arg0, s8* a1, s32 arg2); // PORT: matches ByteMatrix_CopyRowAt
s32 func_80027654(ByteMatrix* arg0, s8* a1, s32 arg2); // PORT: matches ByteMatrix_CopyRow
s32 func_80027700(ByteMatrix* arg0, s8* a1, s32 arg2); // PORT: matches ByteMatrix_CopyRowAt
extern Color_RGBA8 D_8900A698;
extern LabPcImage D_8900A69C; // PORT: an image header (was u8[])
extern char D_89018660[];
extern char D_8901866C[];
void Ui_DrawImageInStrips(void*, s32, s32);
void Font_SetColorForFlags(s32);
extern Color_RGBA8 D_8900A6AC;
extern Color_RGBA8 D_8900A6B0;
extern char D_89018678[];
void LabItem_DrawTrainerInfo(s32 x, s32 y, LabItemSaveSlot* slot, s32 index, s32 flags); // PORT: matches the definition
void Window_SetRect(s8*, s32, s32, s32, s32);
void Menu_Init(Menu* arg0, ByteMatrix* arg1, u16* arg2, MenuDrawItemFunc arg3, s32 arg4, s32 arg5); // PORT: matches the definition
void func_80027410(ByteMatrix* arg0, void* arg1, s32 arg2, s32 arg3, s32 arg4); // PORT: matches TableView_Initialize
void Ui_DrawTexturedDivider(s32, s32, s32, void*);
void Window_DrawFrame(void*);
void Menu_DrawInlineItems(Menu* arg0, s32 arg1, s32 arg2, s32 arg3, intptr_t arg4); // PORT: matches the definition
extern u8 D_8007356C[];
extern u8 D_89018688[];
s32 Menu_UpdateCursor(void*);
extern u8 D_8900A580[];
extern u8 D_8900A584[];
extern u8 D_8900A5A0[];
extern u8 D_8900A5A4[];
extern u8 D_8900A5A8[];
extern u8 D_8900A5AC[];
extern Color_RGBA8 D_8900A6B4;
extern Color_RGBA8 D_8900A6B8;
extern u8 D_8900BFC0[];
extern u8 D_89010368[];
extern u8 D_890110F0[];
extern PrimEnvColor D_8900A6BC;
extern PrimEnvColor D_8900A6C4;
extern Gfx D_80073408[];
extern Gfx D_80073438[];
extern u8 D_89017A98[];
extern char D_890186C0[];
extern char D_890186CC[];
void Ui_FillRectPrimEnvColor(s32, s32, s32, s32, Color_RGBA8, Color_RGBA8);
void Font_SetColorForFlags(s32);
extern char* D_8900A6CC[]; // PORT: a table of string pointers (was s32)
extern s8 *D_8900A6D8[];
void Ui_DrawTexturedFrameDecal(s32, s32, s32, s32, u8*);
extern u8 D_800735C4[];
void Window_DrawFrame(void*);
s32 func_8002B2B0(void* arg0); // PORT: matches Ui_HandleVerticalSelectionInput (a cursor pointer)
s32 LabItem_UpdateItemList(void *, u32);

void BgStage_DrawFrame(void);
void LabItem_DrawOrganizeWindow(LabItemOrganizeWindow*);
void SavePrompt_Draw(SavePrompt*);
void LabItem_DrawConfirmWindow(LabItemConfirmWindow*);
void LabItem_DrawDeleteDataWindow(LabItemDeleteDataWindow*, s32);
void LabItem_DrawSaveDataWindow(LabItemSaveDataWindow*);
/**
 * Draws the item organizer for its current state.
 * Seen in play (2026-11-12): drew the どうぐのせいり screen.
 */
void LabItem_DrawScreen(LabItemScreen* screen) {
    BgStage_DrawFrame();
    LabItem_DrawBackground();
    switch (screen->state) {
        case 1:
            LabItem_DrawOrganizeWindow(&screen->organizeWindow);
            break;
        case 2:
            LabItem_DrawOrganizeWindow(&screen->organizeWindow);
            SavePrompt_Draw(&screen->savePrompt);
            break;
        case 3:
            LabItem_DrawOrganizeWindow(&screen->organizeWindow);
            LabItem_DrawConfirmWindow(&screen->confirmWindow);
            break;
        case 4:
            LabItem_DrawDeleteDataWindow(&screen->deleteDataWindow, 2);
            break;
        case 5:
            LabItem_DrawDeleteDataWindow(&screen->deleteDataWindow, 0x10);
            LabItem_DrawSaveDataWindow(&screen->saveDataWindow);
            break;
        case 6:
            LabItem_DrawDeleteDataWindow(&screen->deleteDataWindow, 0x10);
            LabItem_DrawSaveDataWindow(&screen->saveDataWindow);
            LabItem_DrawConfirmWindow(&screen->confirmWindow);
            break;
    }
}
