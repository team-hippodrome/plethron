/* Stadium Zero fragment016_252E10. */

#include "unicode_font.h" // PORT: Port_Font_FitNext
#include "text_lookup.h" // PORT: the translated hint's width
#include "global.h"
#include "lab_pokemon_list.h"
#include "m2c_macros.h"

// PORT: the decomp called these without a prototype (IDO allows it; clang does not).
void Font_BeginTranslucentTextRendering(void);
void Font_EndTexturedTextRendering(void);
void Font_Printf(s32 x, s32 y, const char* fmt, ...);
void Font_SetActive(s32 mask, s32 arg1);
void Gfx_SetEnvColor(s32 r, s32 g, s32 b, s32 a);
void LabPCList_DrawLocation(s32 x, s32 y, LabPCEntry* entry); // PORT: matches the definition
void LabPCList_DrawMoveName(s32 x, s32 y, u8* mon, s32 slot); // PORT: matches the definition
void LabPCList_DrawTypeIcon(s32 x, s32 y, u8* mon, s32 type_slot);

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
extern u8 D_800734A8[];

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

/**
 * Draws one Pokemon row of the list with the columns of the current detail page.
 * Seen in play (2026-11-12): the rows 0001 フシギダネ ハナ 12345 サトシ てもち くさ どく, 0002 ヒトカゲ, 0003 ゼニガメ.
 */
void LabPCList_DrawMonRow(s32 x, s32 y, Entry891012A8 *entry, s32 row, s32 flags, LabPCListScreen *ctx)
{
  s32 col;
  s32 cx;
  Color_RGBA8 *colors;
  Color_RGBA8 *loop_colors;
  LabPCListScreen *c = ctx;
  gSPDisplayList(gDisplayListHead++, D_80060270);
  colors = D_8910A85C[row & 1];
  loop_colors = D_8910A86C[row & 1];
  func_80029028(x, y, 38, 20, colors[0]);
  func_80029028(x + 38, y, 80, 20, colors[1]);
  col = c->pageColumnStart[c->page];
  cx = x + 120;
  for (; cx < c->listFrame.width; col++)
  {
    func_80029028(cx, y, c->columnWidths[col], 20, loop_colors[col & 1]);
    cx += c->columnWidths[col];
  }

  if (entry != 0)
  {
    if (flags & 0x10)
    {
      Ui_FillRectHighlighted(x + 39, y + 1, 78, 18, D_8910A860);
    }
    Ui_DrawNumberZeroPadded(x + 1, y + 4, row + 1, 4);
    if ((entry->species <= 0) || (entry->species >= 152))
    {
      Font_BeginTranslucentTextRendering();
      Font_SetColorForFlags(flags);
      Font_SetActive(4, -1);
      Font_Printf(x + 40, y + 2, (M2C_UNK *) D_8910DBCC);
      Font_EndTexturedTextRendering();
    }
    else
    {
      Font_BeginTranslucentTextRendering();
      if (1 & D_80064C5C[entry->species - 1].cupEligibilityFlags)
      {
        Gfx_SetEnvColor(0xFF, 0xC8, 0xA0, 0xFF); // #FFC8A0FF
      }
      else
      {
        Font_SetColorForFlags(flags);
      }
      Font_SetActive(4, -1);
      Port_Font_FitNext(x + 40, x + 118, 0); // PORT: a translated species name fits its column ("Aerodactyl")
      Font_Printf(x + 40, y + 2, (M2C_UNK *) D_80064C5C[entry->species - 1].name);
      Font_EndTexturedTextRendering();
    }
    cx = x + 120;
    for (col = c->pageColumnStart[c->page]; cx < c->listFrame.width; col++)
    {
      switch (col)
      {
        case 0:
          Port_Font_FitNext(cx + 6, cx + c->columnWidths[col] - 2, 0); // PORT: a full Western nickname fits its column
          LabPCList_DrawPlainText(cx + 6, y + 2, entry->nickname, 0, 0);
          break;

        case 1:
          Ui_DrawNumberZeroPadded(cx + 4, y + 4, entry->level, 5);
          break;

        case 2:
          Port_Font_FitNext(cx + 4, cx + c->columnWidths[col] - 2, 0); // PORT: and a full Western OT name
          LabPCList_DrawPlainText(cx + 4, y + 2, entry->trainer, 0, 0);
          break;

        case 3:
          LabPCList_DrawLocation(cx + 4, y + 2, (LabPCEntry*) entry);
          break;

        case 4:

        case 5:
          LabPCList_DrawTypeIcon(cx + 4, y + 2, &entry->species, col - 4);
          break;

        case 6:

        case 7:

        case 8:

        case 9:
          Port_Font_FitNext(cx + 1, cx + c->columnWidths[col] - 1, 0); // PORT: a translated move name fits its column
          LabPCList_DrawMoveName(cx + 1, y + 2, &entry->species, col - 6);
          break;

        case 10:
          LabPCList_DrawPlainText(cx + 12, y + 2, D_8910DBD8, entry->unk2C, 0);
          break;

        case 11:
          LabPCList_DrawPlainText(cx + 12, y + 2, D_8910DBDC, entry->stats[0], 0);
          break;

        case 12:
          LabPCList_DrawPlainText(cx + 12, y + 2, D_8910DBE0, entry->stats[1], 0);
          break;

        case 13:
          LabPCList_DrawPlainText(cx + 12, y + 2, D_8910DBE4, entry->stats[2], 0);
          break;

        case 14:
          LabPCList_DrawPlainText(cx + 12, y + 2, D_8910DBE8, entry->stats[3], 0);
          break;

        case 15:
          LabPCList_DrawPlainText(cx + 12, y + 2, D_8910DBEC, entry->stats[4], 0);
          break;

      }

      cx += c->columnWidths[col];
    }

  }
}


/**
 * Draws a fixed-size (0x50 x 0x14) cursor rectangle via Ui_DrawRoundedFrame for a scrolling list
 * menu: x = left + 0x26, y = cursorY + top - scrollY, minus (cellHeight - 1) when scrollFlags
 * 0x400 is set; color read from D_80073520 (list is the ScrollList). Compare ScrollGrid_DrawCursor and ScrollList_DrawCursor.
 * Seen in play (2026-11-12) in いちらん (party list: フシギダネ, ヒトカゲ, ゼニガメ): drew the cursor on the フシギダネ
 * row.
 */
void LabPCList_DrawRowCursor(ScrollList* list, s32 left, s32 top) { // PORT: typed (was a local struct with N64 offsets)
    s32 y;

    y = list->cursorY + top - list->scrollY;
    if (list->scrollFlags & 0x400) {
        y = y - list->cellHeight + 1;
    }
    Ui_DrawRoundedFrame(left + 38, y, 80, 20, *(Color_RGBA8*)D_80073520);
}

extern u8 D_04006170[];
extern u8 D_04006CE0[];
extern Gfx D_80060290[];
extern Gfx D_80060398[4]; // PORT: sized (open, the data after its G_ENDDL was lifted as commands)

extern u8 D_8910C370[];
extern u8 D_8910C978[];
extern char D_8910DBF0[];
extern char D_8910DC04[];
void Gfx_DrawTextureRgba16(s32, s32, s32, s32, u8*, s32, s32);

/**
 * Draws タテ／ヨコきりかえ (switch vertical/horizontal) and ページおくり%d/3 (page %d/3).
 * Seen in play (2026-11-12): Lタテ／ヨコきりかえR and ページおくり1／3.
 */
void LabPCList_DrawPageHint(LabPCListScreen* screen) {
    s32 r_x = 218;   // PORT: the R icon's x, after a translated hint too long to squeeze
    s32 page_dx = 0; // PORT: and how far the page hint and its left arrow move to make room for it

    Font_BeginTranslucentTextRendering();
    Gfx_SetEnvColor(D_800734A8[0], D_800734A8[1], D_800734A8[2], D_800734A8[3]);
    Font_SetActive(8, 0);
    if (Port_Text_GetLanguage() != PORT_LANG_ORIGINAL) {
        // PORT: Port_Font_FitNext squeezes a line over by up to two average characters into 32..216 and draws a
        // longer one from 32 as is ("Toggle vertical/horizontal"), which ran under the R icon: move the icon (and
        // the page hint, if needed) right of the text instead.
        const char* hint = Port_Text_Utf8Of(D_8910DBF0);
        s32 width = (hint != NULL) ? Port_Font_MeasureUtf8(hint) : 0;
        s32 count = 0;
        const char* q = hint;

        while (q != NULL && Port_Utf8Next(&q) != 0) {
            count++;
        }
        if (width > 184 && (width - 184) * count > 2 * width) {
            r_x = 32 + width + 2;
            page_dx = (r_x + 26 + 4 > 308) ? r_x + 26 + 4 - 308 : 0;
        }
    }
    Port_Font_FitNext(screen->headerFrame.x + 32, screen->headerFrame.x + 216, 1); // PORT: fits between the L and R icons
    Font_Printf(screen->headerFrame.x + 34, screen->headerFrame.y + 5, (M2C_UNK*)D_8910DBF0);
    Font_Printf(screen->headerFrame.x + 338 + page_dx, screen->headerFrame.y + 5, (M2C_UNK*)D_8910DC04,
                  screen->page + 1);
    Font_EndTexturedTextRendering();
    gSPDisplayList(gDisplayListHead++, D_80060290);
    Gfx_DrawTextureRgba16(screen->headerFrame.x + 4, screen->headerFrame.y + 3, 26, 24, D_8910C370, 0x20, 0x200000);
    Gfx_DrawTextureRgba16(screen->headerFrame.x + r_x, screen->headerFrame.y + 3, 26, 24, D_8910C978, 0x20, 0x200000);
    Gfx_DrawTextureRgba16(screen->headerFrame.x + 308 + page_dx, screen->headerFrame.y + 2, 26, 26, D_04006170, 0x1C, 0x200000);
    Gfx_DrawTextureRgba16(screen->headerFrame.x + 522, screen->headerFrame.y + 2, 26, 26, D_04006CE0, 0x1C, 0x200000);
    gSPDisplayList(gDisplayListHead++, D_80060398);
}

