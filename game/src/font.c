/* Stadium Zero resident code. */
/* C ported from the Stadium 1 decomp: src/graphics_textures.c. */

#include "global.h"
#include "font.h"
#include "asset_archive.h"
#include "gfx_buffer.h"
#include "graphics_textures.h"
#include "memory_main.h"
#include "ui_graphics.h"
#include "string.h"
#include "stdarg.h"
// PORT: _Printf is declared in port_compat.h (xstdio.h is libultra-internal)
extern FontContext* font_context;

#include "m2c_macros.h"
#include "text_lookup.h" // PORT: translations
#include "unicode_font.h" // PORT: Unicode glyphs for translated text
extern Gfx D_80060950[];
extern Gfx D_800609A8[];

void Font_ResetContext(UNUSED s32 arg0, UNUSED s32 arg1);
s32 Font_GetSetIndexFromMask(s32 mask);
void Font_Free(void);
char* String_CopyBytesAndAdvance(char* buffer, const char* data, u32 size);
void Gfx_SetEnvColor(s32 r, s32 g, s32 b, s32 a);
void Gfx_SetPrimColor(s32 r, s32 g, s32 b, s32 a);
void Font_EndTexturedTextRendering(void);

/** Clears the font context pointer. */
void Font_ResetContext(UNUSED s32 arg0, UNUSED s32 arg1) {
    font_context = NULL;
}

/** Returns the index of the font set selected by a bit mask. */
s32 Font_GetSetIndexFromMask(s32 mask) {
    s32 index = 0;

    if (mask != 0x1) {
        if (mask == 0x2) {
            index = 1;
        } else if (mask == 0x4) {
            index = 2;
        } else if (mask == 0x8) {
            index = 3;
        } else if (mask == 0x10) {
            index = 4;
        } else if (mask == 0x20) {
            index = 5;
        }
    }

    return index;
}

typedef struct FontEntry8001A118 {
    /* 0x0 */ s8 size_a;
    /* 0x1 */ s8 size_b;
    /* 0x2 */ s8 size_c;
    /* 0x4 */ void* file;
} FontEntry8001A118; // size = 0x8
s32 func_8001A0A4(void);
void Font_RegisterSet(s32 mask, s32 arg1, s32 arg2, s32 arg3, void* file);
/**
 * Registers one font set in the next free slot of the font context: glyph advance, height and width,
 * and the font file. The slot becomes the active set. `mask` is unused; the empty if only keeps IDO from
 * spilling it.
 * Seen in play (2026-11-14): Font_Init calls it on every screen that loads fonts, e.g. mask 2 with
 * 12x12 glyphs and mask 1 with 10x10.
 */
void Font_RegisterSet(s32 mask, s32 advance, s32 height, s32 width, void* file) {
    s32 index;

    if (mask) {}  // karodo
    index = Font_GetSetIndexFromMask(mask); // PORT: was func_8001A0A4(), an alias of this function called with no
                                            // argument: on the N64 `mask` was still in a0, on the host it is lost

    ((FontEntry8001A118*)font_context)[index].file = file;
    ((FontEntry8001A118*)font_context)[index].size_a = advance;
    ((FontEntry8001A118*)font_context)[index].size_b = height;
    ((FontEntry8001A118*)font_context)[index].size_c = width;
    font_context->unk_50 = index;
}

/*
 * Font_Init (Font_Init in symbol_addrs.txt)
 *
 * Creates the font context.  Allocates a 0x3C-byte block from the main pool
 * on heap `side` (the second argument is passed straight through to
 * MainPool_AllocWithFunc), tagged 'PRNT', with Font_ResetContext as the
 * free callback; opens the font archive (ROM range D_002B2C80..D_002E0BE0)
 * into font_context->unk_48 and clears the two bytes at +0x38 / +0x39.
 * Then, for every bit set in `load_size`, loads one font set with
 * Font_RegisterSet (mask, three size values, file number in the archive):
 *
 *   bit 0x20: file 5   bit 0x10: file 4   bit 0x08: file 3
 *   bit 0x04: file 2   bit 0x02: file 1   bit 0x01: file 0
 *
 * Always returns 1.
 *
 * The Stadium 1 version (Font_Init + Font_LoadSizes) is structured the same
 * way but zeroes the block with bzero, returns the context and calls a
 * three-argument Font_LoadSet; this Zero version is the older shape.  The
 * meaning of the three size values passed to Font_RegisterSet is not
 * established here (Font_RegisterSet is still assembly); the values are copied
 * from the retail code.
 */
extern u8 D_002B2C80[];
extern u8 D_002E0BE0[];
void Font_RegisterSet(s32 mask, s32 arg1, s32 arg2, s32 arg3, void* file);

/** Allocates the font context. */
s32 Font_Init(s32 load_size, s32 side) {
    font_context = MainPool_AllocWithFunc(sizeof(*font_context), side, 'PRNT', (AllocateFunc)Font_ResetContext);
    font_context->unk_48 = BinArchive_Open(D_002B2C80, D_002E0BE0, 1, 1);
    font_context->unk_50 = 0;
    font_context->unk_51 = 0;

    if (load_size & 0x20) {
        Font_RegisterSet(0x20, 0x20, 0x20, 0x20, BinArchive_GetFile(font_context->unk_48, 5));
    }
    if (load_size & 0x10) {
        Font_RegisterSet(0x10, 0x18, 0x18, 0x18, BinArchive_GetFile(font_context->unk_48, 4));
    }
    if (load_size & 0x8) {
        Font_RegisterSet(0x8, 0x14, 0x14, 0x18, BinArchive_GetFile(font_context->unk_48, 3));
    }
    if (load_size & 0x4) {
        Font_RegisterSet(0x4, 0x10, 0x10, 0x10, BinArchive_GetFile(font_context->unk_48, 2));
    }
    if (load_size & 0x2) {
        Font_RegisterSet(0x2, 0xC, 0xC, 0x10, BinArchive_GetFile(font_context->unk_48, 1));
    }
    if (load_size & 0x1) {
        Font_RegisterSet(0x1, 0xA, 0xA, 0x10, BinArchive_GetFile(font_context->unk_48, 0));
    }
    return 1;
}

/** Frees the font context. */
void Font_Free(void) {
    MainPool_TryFree(font_context);
}

/** Selects the active font set. */
void Font_SetActive(s32 mask, s32 arg1) {
    if (font_context != NULL) {
        font_context->unk_50 = Font_GetSetIndexFromMask(mask);
        font_context->unk_51 = arg1;
        font_context->line_break_flag &= ~2;
    }
}

typedef struct FontGlyphSet8001A3E4 {
    /* 0x0 */ u8 advance;
    /* 0x1 */ u8 height;
    /* 0x2 */ u8 width;
    /* 0x4 */ u8* tex;
} FontGlyphSet8001A3E4; // size = 0x8
s32 Font_GetGlyphIndex(u16);

/**
 * Draws character `chr` of the active font set at (x, y) as a texture rectangle, loading the glyph
 * into TMEM first when it changed. Returns the x position of the next character.
 * Verified in play (2026-11-14): stubbed on the main menu, every item label (せいとん (Organize), バトル (Battle), ...) vanished.
 * Matching: the gDP* macros are expanded by hand and several share one line; IDO schedules by source line, so only this layout matches.
 */
s32 Font_DrawChar(s16 x, s16 y, s16 chr)
{
  FontGlyphSet8001A3E4 *set = &((FontGlyphSet8001A3E4 *) font_context)[font_context->unk_50];
  s32 offset = (Font_GetGlyphIndex(chr) * set->width) * set->height;
  s16 ulx = x * 4;
  s16 uly = y * 4;
  s16 lrx = (x + set->advance) * 4;
  s16 lry = (y + set->height) * 4;
  if ((font_context->line_break_flag & 1) && (!(font_context->line_break_flag & 2)))
  {
    font_context->line_break_flag |= 2;
    {
      {
        Gfx *_g = (Gfx *) (gDisplayListHead++);
        _g->words.w0 = ((((unsigned int) ((((unsigned int) 0xfd) & ((0x01 << 8) - 1)) << 24)) | ((unsigned int) ((((unsigned int) 4) & ((0x01 << 3) - 1)) << 21))) | ((unsigned int) ((((unsigned int) 2) & ((0x01 << 2) - 1)) << 19))) | ((unsigned int) ((((unsigned int) (1 - 1)) & ((0x01 << 12) - 1)) << 0)); // FAKE: hand-expanded gbi macro
        _g->words.w1 = (uintptr_t) (set->tex + ((set->width << 8) * set->height)); // FAKE: hand-expanded gbi macro
      }
      ;
      { Gfx *_g = (Gfx *) (gDisplayListHead++); _g->words.w0 = (((((unsigned int) ((((unsigned int) 0xf5) & ((0x01 << 8) - 1)) << 24)) | ((unsigned int) ((((unsigned int) 4) & ((0x01 << 3) - 1)) << 21))) | ((unsigned int) ((((unsigned int) 2) & ((0x01 << 2) - 1)) << 19))) | ((unsigned int) ((((unsigned int) 0) & ((0x01 << 9) - 1)) << 9))) | ((unsigned int) ((((unsigned int) 0x0100) & ((0x01 << 9) - 1)) << 0)); _g->words.w1 = ((((((((unsigned int) ((((unsigned int) 7) & ((0x01 << 3) - 1)) << 24)) | ((unsigned int) ((((unsigned int) 0) & ((0x01 << 4) - 1)) << 20))) | ((unsigned int) ((((unsigned int) (0 | 0x2)) & ((0x01 << 2) - 1)) << 18))) | ((unsigned int) ((((unsigned int) 0) & ((0x01 << 4) - 1)) << 14))) | ((unsigned int) ((((unsigned int) 0) & ((0x01 << 4) - 1)) << 10))) | ((unsigned int) ((((unsigned int) (0 | 0x2)) & ((0x01 << 2) - 1)) << 8))) | ((unsigned int) ((((unsigned int) 0) & ((0x01 << 4) - 1)) << 4))) | ((unsigned int) ((((unsigned int) 0) & ((0x01 << 4) - 1)) << 0)); } // FAKE: hand-expanded gbi macro
      ;
 { Gfx *_g = (Gfx *) (gDisplayListHead++); _g->words.w0 = (unsigned int) ((((unsigned int) 0xe6) & ((0x01 << 8) - 1)) << 24); _g->words.w1 = 0; } ; { Gfx *_g = (Gfx *) (gDisplayListHead++); _g->words.w0 = (((unsigned int) ((((unsigned int) 0xf3) & ((0x01 << 8) - 1)) << 24)) | ((unsigned int) ((((unsigned int) 0) & ((0x01 << 12) - 1)) << 12))) | ((unsigned int) ((((unsigned int) 0) & ((0x01 << 12) - 1)) << 0)); _g->words.w1 = (((unsigned int) ((((unsigned int) 7) & ((0x01 << 3) - 1)) << 24)) | ((unsigned int) ((((unsigned int) ((((((set->width * set->height) + 1) >> 1) - 1) < 2047) ? ((((set->width * set->height) + 1) >> 1) - 1) : (2047))) & ((0x01 << 12) - 1)) << 12))) | ((unsigned int) ((((unsigned int) ((((1 << 11) + ((1 > ((set->width * 1) / 8)) ? (1) : ((set->width * 1) / 8))) - 1) / ((1 > ((set->width * 1) / 8)) ? (1) : ((set->width * 1) / 8)))) & ((0x01 << 12) - 1)) << 0)); } ; { Gfx *_g = (Gfx *) (gDisplayListHead++); _g->words.w0 = (unsigned int) ((((unsigned int) 0xe7) & ((0x01 << 8) - 1)) << 24); _g->words.w1 = 0; } ; { Gfx *_g = (Gfx *) (gDisplayListHead++); _g->words.w0 = (((((unsigned int) ((((unsigned int) 0xf5) & ((0x01 << 8) - 1)) << 24)) | ((unsigned int) ((((unsigned int) 4) & ((0x01 << 3) - 1)) << 21))) | ((unsigned int) ((((unsigned int) 1) & ((0x01 << 2) - 1)) << 19))) | ((unsigned int) ((((unsigned int) (((set->width * 1) + 7) >> 3)) & ((0x01 << 9) - 1)) << 9))) | ((unsigned int) ((((unsigned int) 0x0100) & ((0x01 << 9) - 1)) << 0)); _g->words.w1 = ((((((((unsigned int) ((((unsigned int) 1) & ((0x01 << 3) - 1)) << 24)) | ((unsigned int) ((((unsigned int) 0) & ((0x01 << 4) - 1)) << 20))) | ((unsigned int) ((((unsigned int) (0 | 0x2)) & ((0x01 << 2) - 1)) << 18))) | ((unsigned int) ((((unsigned int) 0) & ((0x01 << 4) - 1)) << 14))) | ((unsigned int) ((((unsigned int) 0) & ((0x01 << 4) - 1)) << 10))) | ((unsigned int) ((((unsigned int) (0 | 0x2)) & ((0x01 << 2) - 1)) << 8))) | ((unsigned int) ((((unsigned int) 0) & ((0x01 << 4) - 1)) << 4))) | ((unsigned int) ((((unsigned int) 0) & ((0x01 << 4) - 1)) << 0)); } ; { Gfx *_g = (Gfx *) (gDisplayListHead++); _g->words.w0 = (((unsigned int) ((((unsigned int) 0xf2) & ((0x01 << 8) - 1)) << 24)) | ((unsigned int) ((((unsigned int) 0) & ((0x01 << 12) - 1)) << 12))) | ((unsigned int) ((((unsigned int) 0) & ((0x01 << 12) - 1)) << 0)); _g->words.w1 = (((unsigned int) ((((unsigned int) 1) & ((0x01 << 3) - 1)) << 24)) | ((unsigned int) ((((unsigned int) ((set->width - 1) << 2)) & ((0x01 << 12) - 1)) << 12))) | ((unsigned int) ((((unsigned int) ((set->height - 1) << 2)) & ((0x01 << 12) - 1)) << 0)); } ; } ; } if (((uintptr_t) (set->tex + offset)) != font_context->unk_4C) { font_context->unk_4C = (uintptr_t) (set->tex + offset); { { Gfx *_g = (Gfx *) (gDisplayListHead++); _g->words.w0 = ((((unsigned int) ((((unsigned int) 0xfd) & ((0x01 << 8) - 1)) << 24)) | ((unsigned int) ((((unsigned int) 3) & ((0x01 << 3) - 1)) << 21))) | ((unsigned int) ((((unsigned int) 2) & ((0x01 << 2) - 1)) << 19))) | ((unsigned int) ((((unsigned int) (1 - 1)) & ((0x01 << 12) - 1)) << 0)); _g->words.w1 = (uintptr_t) (set->tex + offset); } ; { Gfx *_g = (Gfx *) (gDisplayListHead++); _g->words.w0 = (((((unsigned int) ((((unsigned int) 0xf5) & ((0x01 << 8) - 1)) << 24)) | ((unsigned int) ((((unsigned int) 3) & ((0x01 << 3) - 1)) << 21))) | ((unsigned int) ((((unsigned int) 2) & ((0x01 << 2) - 1)) << 19))) | ((unsigned int) ((((unsigned int) 0) & ((0x01 << 9) - 1)) << 9))) | ((unsigned int) ((((unsigned int) 0) & ((0x01 << 9) - 1)) << 0)); _g->words.w1 = ((((((((unsigned int) ((((unsigned int) 7) & ((0x01 << 3) - 1)) << 24)) | ((unsigned int) ((((unsigned int) 0) & ((0x01 << 4) - 1)) << 20))) | ((unsigned int) ((((unsigned int) (0 | 0x2)) & ((0x01 << 2) - 1)) << 18))) | ((unsigned int) ((((unsigned int) 0) & ((0x01 << 4) - 1)) << 14))) | ((unsigned int) ((((unsigned int) 0) & ((0x01 << 4) - 1)) << 10))) | ((unsigned int) ((((unsigned int) (0 | 0x2)) & ((0x01 << 2) - 1)) << 8))) | ((unsigned int) ((((unsigned int) 0) & ((0x01 << 4) - 1)) << 4))) | ((unsigned int) ((((unsigned int) 0) & ((0x01 << 4) - 1)) << 0)); } ; { Gfx *_g = (Gfx *) (gDisplayListHead++); _g->words.w0 = (unsigned int) ((((unsigned int) 0xe6) & ((0x01 << 8) - 1)) << 24); _g->words.w1 = 0; } ; { Gfx *_g = (Gfx *) (gDisplayListHead++); _g->words.w0 = (((unsigned int) ((((unsigned int) 0xf3) & ((0x01 << 8) - 1)) << 24)) | ((unsigned int) ((((unsigned int) 0) & ((0x01 << 12) - 1)) << 12))) | ((unsigned int) ((((unsigned int) 0) & ((0x01 << 12) - 1)) << 0)); _g->words.w1 = (((unsigned int) ((((unsigned int) 7) & ((0x01 << 3) - 1)) << 24)) | ((unsigned int) ((((unsigned int) ((((((set->width * set->height) + 1) >> 1) - 1) < 2047) ? ((((set->width * set->height) + 1) >> 1) - 1) : (2047))) & ((0x01 << 12) - 1)) << 12))) | ((unsigned int) ((((unsigned int) ((((1 << 11) + ((1 > ((set->width * 1) / 8)) ? (1) : ((set->width * 1) / 8))) - 1) / ((1 > ((set->width * 1) / 8)) ? (1) : ((set->width * 1) / 8)))) & ((0x01 << 12) - 1)) << 0)); } // FAKE: hand-expanded gbi macro
      ;
      { Gfx *_g = (Gfx *) (gDisplayListHead++); _g->words.w0 = (unsigned int) ((((unsigned int) 0xe7) & ((0x01 << 8) - 1)) << 24); _g->words.w1 = 0; } // FAKE: hand-expanded gbi macro
      ;
      { Gfx *_g = (Gfx *) (gDisplayListHead++); _g->words.w0 = (((((unsigned int) ((((unsigned int) 0xf5) & ((0x01 << 8) - 1)) << 24)) | ((unsigned int) ((((unsigned int) 3) & ((0x01 << 3) - 1)) << 21))) | ((unsigned int) ((((unsigned int) 1) & ((0x01 << 2) - 1)) << 19))) | ((unsigned int) ((((unsigned int) (((set->width * 1) + 7) >> 3)) & ((0x01 << 9) - 1)) << 9))) | ((unsigned int) ((((unsigned int) 0) & ((0x01 << 9) - 1)) << 0)); _g->words.w1 = ((((((((unsigned int) ((((unsigned int) 0) & ((0x01 << 3) - 1)) << 24)) | ((unsigned int) ((((unsigned int) 0) & ((0x01 << 4) - 1)) << 20))) | ((unsigned int) ((((unsigned int) (0 | 0x2)) & ((0x01 << 2) - 1)) << 18))) | ((unsigned int) ((((unsigned int) 0) & ((0x01 << 4) - 1)) << 14))) | ((unsigned int) ((((unsigned int) 0) & ((0x01 << 4) - 1)) << 10))) | ((unsigned int) ((((unsigned int) (0 | 0x2)) & ((0x01 << 2) - 1)) << 8))) | ((unsigned int) ((((unsigned int) 0) & ((0x01 << 4) - 1)) << 4))) | ((unsigned int) ((((unsigned int) 0) & ((0x01 << 4) - 1)) << 0)); } // FAKE: hand-expanded gbi macro
      ;
      {
        Gfx *_g = (Gfx *) (gDisplayListHead++);
        _g->words.w0 = (((unsigned int) ((((unsigned int) 0xf2) & ((0x01 << 8) - 1)) << 24)) | ((unsigned int) ((((unsigned int) 0) & ((0x01 << 12) - 1)) << 12))) | ((unsigned int) ((((unsigned int) 0) & ((0x01 << 12) - 1)) << 0)); // FAKE: hand-expanded gbi macro
        _g->words.w1 = (((unsigned int) ((((unsigned int) 0) & ((0x01 << 3) - 1)) << 24)) | ((unsigned int) ((((unsigned int) ((set->width - 1) << 2)) & ((0x01 << 12) - 1)) << 12))) | ((unsigned int) ((((unsigned int) ((set->height - 1) << 2)) & ((0x01 << 12) - 1)) << 0)); // FAKE: hand-expanded gbi macro
      }
      ;
    }
    ;
  }
  {
    Gfx *_g = (Gfx *) (gDisplayListHead++);
    _g->words.w0 = (((unsigned int) ((((unsigned int) 0xe4) & ((0x01 << 8) - 1)) << 24)) | ((unsigned int) ((((unsigned int) lrx) & ((0x01 << 12) - 1)) << 12))) | ((unsigned int) ((((unsigned int) lry) & ((0x01 << 12) - 1)) << 0)); // FAKE: hand-expanded gDPFillRectangle
    _g->words.w1 = (((unsigned int) ((((unsigned int) 0) & ((0x01 << 3) - 1)) << 24)) | ((unsigned int) ((((unsigned int) ulx) & ((0x01 << 12) - 1)) << 12))) | ((unsigned int) ((((unsigned int) uly) & ((0x01 << 12) - 1)) << 0)); // FAKE: hand-expanded gDPFillRectangle
    { Gfx *_g = (Gfx *) (gDisplayListHead++); _g->words.w0 = (unsigned int) ((((unsigned int) 0xe1) & ((0x01 << 8) - 1)) << 24); _g->words.w1 = (unsigned int) (((unsigned int) ((((unsigned int) 0) & ((0x01 << 16) - 1)) << 16)) | ((unsigned int) ((((unsigned int) 0) & ((0x01 << 16) - 1)) << 0))); } // FAKE: hand-expanded gDPPipeSync
    ;
    {
      Gfx *_g = (Gfx *) (gDisplayListHead++);
      _g->words.w0 = (unsigned int) ((((unsigned int) 0xf1) & ((0x01 << 8) - 1)) << 24); // FAKE: hand-expanded gbi macro
      _g->words.w1 = (unsigned int) (((unsigned int) ((((unsigned int) 0x400) & ((0x01 << 16) - 1)) << 16)) | ((unsigned int) ((((unsigned int) 0x400) & ((0x01 << 16) - 1)) << 0))); // FAKE: hand-expanded gbi macro
    }
    ;
  }
  ;
  return (x + set->advance) + font_context->unk_51;
}

/** Copies bytes into a buffer and returns the pointer past the copy. */
char* String_CopyBytesAndAdvance(char* buffer, const char* data, u32 size) {
    return (char*)memcpy(buffer, data, size) + size;
}

// PORT: the glyph height translated text is drawn at instead of the active set's (Port_Font_SetHeight), 0 for none.
static s32 sPortFontHeight;

void Port_Font_SetHeight(s32 height) {
    sPortFontHeight = height;
}

static s32 Port_Font_Height(FontGlyphSet8001A3E4* set) {
    return (sPortFontHeight != 0) ? sPortFontHeight : set->height;
}

// PORT: draws translated text with the Unicode glyphs (src/port/unicode_font.cpp) in place of the game font, which
// has kana and fixed-width ASCII only. Each glyph is sized to the active set's height and advances by its own width.
// Follows Font_DrawChar: the two-cycle shading texture (glyph 256 of the set) still goes to tile 1 first.
// `x4` is in quarter pixels and `scale` squeezes the glyph horizontally (1.0 = as designed); returns the next x4.
static s32 Port_Font_DrawCodepointScaled(s32 x4, s32 y, u32 cp, f32 scale) {
    FontGlyphSet8001A3E4* set = &((FontGlyphSet8001A3E4*) font_context)[font_context->unk_50];
    s32 height = Port_Font_Height(set);
    int tex_width, draw_width, advance;
    const unsigned char* tex = Port_Font_Glyph(cp, height, &tex_width, &draw_width, &advance);

    if (tex == NULL) {
        tex = Port_Font_Glyph('?', height, &tex_width, &draw_width, &advance);
        if (tex == NULL) {
            return x4 + (s32)(set->advance * 4 * scale);
        }
    }
    if ((font_context->line_break_flag & 1) && !(font_context->line_break_flag & 2)) {
        font_context->line_break_flag |= 2;
        gDPLoadMultiBlock(gDisplayListHead++, set->tex + ((set->width << 8) * set->height), 0x100, 1, G_IM_FMT_I,
                          G_IM_SIZ_8b, set->width, set->height, 0, G_TX_CLAMP, G_TX_CLAMP, 0, 0, 0, 0);
    }
    if (cp != ' ') {
        font_context->unk_4C = (uintptr_t) tex;
        gDPLoadTextureBlock(gDisplayListHead++, tex, G_IM_FMT_IA, G_IM_SIZ_8b, tex_width, height, 0, G_TX_CLAMP,
                            G_TX_CLAMP, 0, 0, 0, 0);
        gSPTextureRectangle(gDisplayListHead++, x4, y << 2, x4 + (s32)(draw_width * 4 * scale), (y + height) << 2,
                            G_TX_RENDERTILE, 0, 0, (s32)((1 << 10) / scale), 1 << 10);
    }
    return x4 + (s32)(advance * 4 * scale);
}

static s32 Port_Font_DrawCodepoint(s32 x, s32 y, u32 cp) {
    return Port_Font_DrawCodepointScaled(x << 2, y, cp, 1.0f) >> 2;
}

// PORT: the width in pixels of one line of `utf8` in the active set, as Port_Font_PrintUtf8 draws it.
s32 Port_Font_MeasureUtf8(const char* utf8) {
    FontGlyphSet8001A3E4* set = &((FontGlyphSet8001A3E4*) font_context)[font_context->unk_50];
    int tex_width, draw_width, advance;
    s32 width = 0;
    u32 cp;

    while ((cp = Port_Utf8Next(&utf8)) != 0 && cp != '\n') {
        if (Port_Font_Glyph(cp, Port_Font_Height(set), &tex_width, &draw_width, &advance) != NULL ||
            Port_Font_Glyph('?', Port_Font_Height(set), &tex_width, &draw_width, &advance) != NULL) {
            width += advance;
        }
    }
    return width;
}

// PORT: the box the next translated print must fit in (Port_Font_FitNext), or sFitArmed 0.
static s32 sFitArmed;
static s32 sFitLeft;
static s32 sFitRight;
static s32 sFitAlign;

// PORT: makes the next translated Font_Printf / Port_Font_PrintUtf8 fit each line between x `left` and `right`.
// A line that overflows by at most two average characters is squeezed horizontally to fit; a longer one is drawn
// as is and logged to text_overflows.csv, to be shortened in the translation. `align` 1 centres a line that fits
// between left and right (for labels the game centres by byte count); 0 keeps the x it is printed at.
void Port_Font_FitNext(s32 left, s32 right, s32 align) {
    if (Port_Text_GetLanguage() == PORT_LANG_ORIGINAL) {
        return;
    }
    sFitArmed = 1;
    sFitLeft = left;
    sFitRight = right;
    sFitAlign = align;
}

// PORT: Font_Printf for a translated language: `utf8` drawn glyph by glyph, '\n' handled as the original does.
void Port_Font_PrintUtf8(s32 x, s32 y, const char* utf8) {
    s32 fit = sFitArmed;
    s32 start_x = x;
    s32 x4;
    f32 scale = 1.0f;
    s32 line_start = 1;
    u32 cp;

    sFitArmed = 0;
    x4 = x << 2;
    while (1) {
        if (line_start && fit) {
            // PORT: measure this line and place or squeeze it inside the box
            const char* q = utf8;
            s32 width = Port_Font_MeasureUtf8(utf8);
            s32 count = 0;
            s32 left = (sFitAlign != 0) ? sFitLeft : start_x;
            s32 avail = sFitRight - left;

            while ((cp = Port_Utf8Next(&q)) != 0 && cp != '\n') {
                count++;
            }
            scale = 1.0f;
            if (width <= avail) {
                x4 = ((sFitAlign != 0) ? left + (avail - width) / 2 : start_x) << 2;
            } else {
                x4 = left << 2;
                if (count > 0 && (width - avail) * count <= 2 * width) {
                    scale = (f32) avail / (f32) width;
                } else {
                    Port_Text_LogOverflow(utf8, width, avail);
                }
            }
        }
        line_start = 0;
        cp = Port_Utf8Next(&utf8);
        if (cp == 0) {
            break;
        }
        if (cp == '\n' && !(font_context->line_break_flag & 4)) {
            x4 = start_x << 2;
            y += font_context->line_height;
            line_start = 1;
            continue;
        }
        x4 = Port_Font_DrawCodepointScaled(x4, y, cp, scale);
    }
}

s32 Font_DrawChar(s16, s16, s16);
/**
 * Formats `fmt` and draws it at (x, y) with the active font set. '\n' starts a new line unless
 * Font_DisableNewlines was called; bytes >= 0x80 start a two-byte (JIS) character.
 * Seen in play (2026-11-14): draws the main menu item labels and the battle message box (through
 * Font_DrawChar, whose stub test removed them).
 */
void Font_Printf(s32 x, s32 y, const char* fmt, ...) {
    u8 buf[0x100];
    UNUSED s32 pad; // FAKE: stack padding
    s32 start_x;
    s32 len;
    u8* p;
    s32 chr;
    s32 draw;
    va_list args;

    va_start(args, fmt);
    start_x = x;
    if (Port_Text_GetLanguage() != PORT_LANG_ORIGINAL) {
        // PORT: the translated text (format and %s arguments looked up by id, then md5) in Unicode glyphs
        char utf8[0x200];
        Port_Text_Vformat(utf8, sizeof(utf8), fmt, args);
        Port_Font_PrintUtf8(x, y, utf8);
        va_end(args);
        return;
    }
    len = _Printf(String_CopyBytesAndAdvance, (char*)buf, fmt, args);
    if (len > 0) {
        buf[len] = 0;
        chr = buf[0];
        p = &buf[1];
        while (chr != 0) {
            draw = 1;
            if (!(font_context->line_break_flag & 4) && (chr == 0xA)) {
                x = start_x;
                draw = 0;
                y += font_context->line_height;
            }
            if (draw) {
                if (chr >= 0x80) {
                    if (*p != 0) {
                        chr = *p | (chr << 8);
                        p++;
                    }
                }
                x = Font_DrawChar(x, y, chr & 0x7F7F);
            }
            chr = *p;
            p++;
        }
    }
    va_end(args);
}

s32 Font_DrawChar(s16, s16, s16);
/**
 * Draws the single character `chr` at (x, y).
 * Seen in play (2026-11-14): GbSave_DrawSummaryBar draws the '1' and 'P' of the 1P label in the top bar
 * with it.
 */
void Font_DrawCharAt(s32 x, s32 y, s32 chr) {
    Font_DrawChar(x, y, (s16) (chr & 0x7F7F));
}

/** Emits a command that sets the RDP environment color. */
void Gfx_SetEnvColor(s32 r, s32 g, s32 b, s32 a) {
    gDPSetEnvColor(gDisplayListHead++, r, g, b, a);
}

/** Emits a command that sets the RDP primitive color. */
void Gfx_SetPrimColor(s32 r, s32 g, s32 b, s32 a) {
    gDPSetPrimColor(gDisplayListHead++, 0, 0, r, g, b, a);
}

extern s8 *D_800AD200;
/** Sets the line height used for text. */
void Font_SetLineHeight(s32 arg0) {
    font_context->line_height = arg0; // PORT: was M2C_FIELD(D_800AD200, s8*, 0x3A), the N64 offset
}

extern s8 *D_800AD200;

/**
 * Makes Font_Printf draw '\n' as a character instead of breaking the line.
 * @unverified Name read from the code.
 */
void Font_DisableNewlines(void) {
    font_context->line_break_flag |= 4; // PORT: was M2C_FIELD(D_800AD200, u8*, 0x3B), the N64 offset
}

extern s8 *D_800AD200;

/**
 * Undoes Font_DisableNewlines.
 * @unverified Name read from the code.
 */
void Font_EnableNewlines(void) {
    font_context->line_break_flag &= ~4; // PORT: was M2C_FIELD(D_800AD200, u8*, 0x3B), the N64 offset
}

/** Begins rendering text with translucency. */
void Font_BeginTranslucentTextRendering(void) {
    font_context->line_break_flag = 0;
    font_context->line_height = 40;
    font_context->unk_4C = 0;

    gSPDisplayList(gDisplayListHead++, D_80060950);
}

/** Ends textured text rendering by restoring the default state. */
void Font_EndTexturedTextRendering(void) {
    gSPDisplayList(gDisplayListHead++, D_80060990);
}

/** Enables two-cycle texturing for text. */
void Font_EnableTwoCycleTexturing(void) {
    font_context->line_break_flag |= 1;
    font_context->line_break_flag &= ~2;

    gSPDisplayList(gDisplayListHead++, D_800609A8);
}

extern Gfx D_800609D8[];

/** Disables two-cycle texturing for text. */
void Font_DisableTwoCycleTexturing(void) {
    font_context->line_break_flag &= ~3;
    gSPDisplayList(gDisplayListHead++, D_800609D8);
}

extern Gfx D_80060A00[6]; // PORT: sized: the Pokédex text follows it
/**
 * Draws a textured rectangle tinted with the given RGBA primitive color.
 * Seen in play (2026-11-13) (tour of menus and a COM battle): ran in the battle opening, a switch
 * in battle, running from battle and a Pound turn. Stubbed on the battle command menu, the COM
 * info box lost its colored background fill.
 * @unverified Name and comment guessed from the code.
 */
void Ui_DrawPrimColorRect(s32 x, s32 y, s32 w, s32 h, s32 r, s32 g, s32 b, s32 a) {
    gSPDisplayList(gDisplayListHead++, D_80060A00);
    gDPSetPrimColor(gDisplayListHead++, 0, 0, r, g, b, a);
    gSPTextureRectangle(gDisplayListHead++, x << 2, y << 2, (x + w) << 2, (y + h) << 2, G_TX_RENDERTILE, 0, 0, 0x1000, 0x0400);
    gDPSetTexturePersp(gDisplayListHead++, G_TP_PERSP);
}

/**
 * Allocates the four vertices of a glyph quad at (x, y), scaled by `scaleX` / `scaleY` percent from
 * the glyph size at `glyphSet` (width byte, height byte).
 * @unverified Name read from the code.
 */
Vtx* Font_AllocGlyphQuad(s16 x, s16 y, s16 scale_x, s16 scale_y, u8* glyph_set) {
    s16 w = (glyph_set[0] * scale_x) / 100;
    s16 h = (glyph_set[1] * scale_y) / 100;
    Vtx* vtx = Gfx_AllocDisplayList(sizeof(Vtx) * 4);

    Gfx_SetVertexAttributes(&vtx[0], x, y, 0, 0, 0, 0, 0, 0x7F, 0xFF);
    Gfx_SetVertexAttributes(&vtx[1], x, y - h, 0, 0, (glyph_set[1] << 5) - 1, 0, 0, 0x7F, 0xFF);
    Gfx_SetVertexAttributes(&vtx[2], x + w, y - h, 0, (glyph_set[0] << 5) - 1, (glyph_set[1] << 5) - 1, 0, 0, 0x7F, 0xFF);
    Gfx_SetVertexAttributes(&vtx[3], x + w, y, 0, (glyph_set[0] << 5) - 1, 0, 0, 0, 0x7F, 0xFF);
    return vtx;
}

typedef struct Glyph8001AFB0 {
    /* 0x0 */ u8 advance;
    /* 0x1 */ u8 height;
    /* 0x2 */ u8 width;
    /* 0x4 */ u8* file;
} Glyph8001AFB0;
s32 Font_GetGlyphIndex(u16);
/**
 * Builds a display list that draws character `chr` as a textured 3D quad and chains it in front of
 * *`list`. Returns the scaled advance.
 * @unverified Name read from the code.
 */
s32 Font_AddGlyphDisplayList(Gfx** list, s16 x, s16 y, s16 scale_x, s16 scale_y, u16 chr) {
    Glyph8001AFB0* font = &((Glyph8001AFB0*)font_context)[font_context->unk_50];
    s32 offset = Font_GetGlyphIndex(chr) * font->width * font->height;
    Vtx* vtx = Font_AllocGlyphQuad(x, y, scale_x, scale_y, (u8*)font);
    Gfx* dl = Gfx_AllocDisplayList(sizeof(Gfx) * 20); // PORT: was 0xA0 (20 N64 commands)
    Gfx* gfx = dl;

    gDPLoadTextureBlock(gfx++, font->file + offset, G_IM_FMT_IA, G_IM_SIZ_8b, font->width, font->height, 0,
                        G_TX_CLAMP, G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gSPVertex(gfx++, vtx, 4, 0);
    gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
    if (*list == NULL) {
        gSPEndDisplayList(gfx++);
    } else {
        gSPBranchList(gfx++, *list);
    }
    *list = dl;
    return ((font->advance + font_context->unk_51) * scale_x) / 100;
}

/**
 * Builds a display list that calls `inner` with lighting off and its texels tinted by the environment
 * color `color`.
 * @unverified Name read from the code.
 */
Gfx* Gfx_WrapWithEnvColor(Gfx* inner, Color_RGBA8_u32 color) {
    Gfx* dl = Gfx_AllocDisplayList(sizeof(Gfx) * 16); // PORT: was 0x80 (16 N64 commands)
    Gfx* gfx = dl;

    gDPPipeSync(gfx++);
    gSPClearGeometryMode(gfx++, G_LIGHTING);
    gSPTexture(gfx++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON);
    gDPSetCombineLERP(gfx++, TEXEL0, 0, ENVIRONMENT, 0, TEXEL0, 0, ENVIRONMENT, 0, 0, 0, 0, COMBINED, 0, 0, 0,
                      COMBINED);
    gDPSetEnvColor(gfx++, color.r, color.g, color.b, color.a);
    gSPDisplayList(gfx++, inner);
    gSPSetGeometryMode(gfx++, G_LIGHTING);
    gSPEndDisplayList(gfx++);
    return dl;
}

/**
 * Returns the x position at which `str` starts when centered on x, using the
 * current font's glyph width, spacing and the scale (percent).
 * Seen in play (2026-11-13) ran during the VS intro (トレーナー (Trainer) 1P vs トレーナー COM) (Free Battle, L1-30
 * rules, six L30 rentals vs COM); also in an ordinary Free Battle.
 * @unverified Name and comment guessed from the code.
 */
s32 Font_GetCenteredStartX(s32 x, s32 scale, const char* str) {
    s32 width;
    // PORT: the decomp read these through an N64-layout struct (8-byte sets, current set at 0x38, spacing at 0x39)
    FontEntry8001A118* set = &((FontEntry8001A118*)font_context)[font_context->unk_50];

    width = (set->size_b + font_context->unk_51) * scale / 100;

    return x - (((strlen(str) >> 1) * width) >> 1);
}

// PORT: Font_PrintfToDisplayList for a translated language. Each segment between the control characters ('\n', '\t')
// is translated on its own (a VS panel nickname is "\t" + the name), or the whole format with its arguments when it
// has conversions. Each character is a quad with its Unicode glyph, sized to the active set's height.
static s32 Port_Font_AddGlyphQuad(Gfx** list, s16 x, s16 y, s16 scale_x, s16 scale_y, u32 cp) {
    Glyph8001AFB0* font = &((Glyph8001AFB0*)font_context)[font_context->unk_50];
    int tex_width, draw_width, advance;
    const unsigned char* tex = Port_Font_Glyph(cp, font->height, &tex_width, &draw_width, &advance);
    Vtx* vtx;
    Gfx* dl;
    Gfx* gfx;
    s16 w;
    s16 h;

    if (tex == NULL) {
        tex = Port_Font_Glyph('?', font->height, &tex_width, &draw_width, &advance);
        if (tex == NULL) {
            return ((font->advance + font_context->unk_51) * scale_x) / 100;
        }
    }
    if (cp != ' ') {
        w = (draw_width * scale_x) / 100;
        h = (font->height * scale_y) / 100;
        vtx = Gfx_AllocDisplayList(sizeof(Vtx) * 4);
        Gfx_SetVertexAttributes(&vtx[0], x, y, 0, 0, 0, 0, 0, 0x7F, 0xFF);
        Gfx_SetVertexAttributes(&vtx[1], x, y - h, 0, 0, (font->height << 5) - 1, 0, 0, 0x7F, 0xFF);
        Gfx_SetVertexAttributes(&vtx[2], x + w, y - h, 0, (draw_width << 5) - 1, (font->height << 5) - 1, 0, 0, 0x7F, 0xFF);
        Gfx_SetVertexAttributes(&vtx[3], x + w, y, 0, (draw_width << 5) - 1, 0, 0, 0, 0x7F, 0xFF);
        dl = Gfx_AllocDisplayList(sizeof(Gfx) * 20);
        gfx = dl;
        gDPLoadTextureBlock(gfx++, tex, G_IM_FMT_IA, G_IM_SIZ_8b, tex_width, font->height, 0, G_TX_CLAMP, G_TX_CLAMP,
                            G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
        gSPVertex(gfx++, vtx, 4, 0);
        gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
        if (*list == NULL) {
            gSPEndDisplayList(gfx++);
        } else {
            gSPBranchList(gfx++, *list);
        }
        *list = dl;
    }
    return ((advance + font_context->unk_51) * scale_x) / 100;
}

static s32 Port_Font_MeasureLineScaled(const char* utf8, s16 scale_x) {
    Glyph8001AFB0* font = &((Glyph8001AFB0*)font_context)[font_context->unk_50];
    int tex_width, draw_width, advance;
    s32 width = 0;
    u32 cp;

    while ((cp = Port_Utf8Next(&utf8)) != 0 && cp != '\n' && cp != '\t') {
        if (Port_Font_Glyph(cp, font->height, &tex_width, &draw_width, &advance) == NULL) {
            advance = font->advance;
        }
        width += ((advance + font_context->unk_51) * scale_x) / 100;
    }
    return width;
}

static Gfx* Port_Font_PrintfToDisplayListV(s16 x, s16 y, s16 scale_x, s16 scale_y, const char* fmt, va_list args) {
    Glyph8001AFB0* font = &((Glyph8001AFB0*)font_context)[font_context->unk_50];
    char utf8[0x200];
    char seg[0x100];
    const char* src;
    const char* q;
    Gfx* dl = NULL;
    s32 start_x = x;
    s32 n = 0;
    u32 cp;

    if (strchr(fmt, '%') != NULL) {
        Port_Text_Vformat(utf8, sizeof(utf8), fmt, args);
    } else {
        for (src = fmt; *src != 0;) {
            if (*src == '\n' || *src == '\t') {
                if (n < (s32)sizeof(utf8) - 1) utf8[n++] = *src;
                src++;
                continue;
            }
            {
                s32 k = 0;
                const char* t;
                while (*src != 0 && *src != '\n' && *src != '\t' && k < (s32)sizeof(seg) - 1) {
                    seg[k++] = *src++;
                }
                seg[k] = 0;
                t = Port_Text_Utf8Of(seg);
                while (t != NULL && *t != 0 && n < (s32)sizeof(utf8) - 1) {
                    utf8[n++] = *t++;
                }
            }
        }
        utf8[n] = 0;
    }
    for (q = utf8; (cp = Port_Utf8Next(&q)) != 0;) {
        if (cp == '\n') {
            x = start_x;
            y -= ((u8)((FontEntry8001A118*)font_context)[font_context->unk_50].size_b * ((scale_y * 0xB) / 10)) / 100;
        } else if (cp == '\t') {
            x -= Port_Font_MeasureLineScaled(q, scale_x) / 2;
        } else {
            x += Port_Font_AddGlyphQuad(&dl, x, y, scale_x, scale_y, cp);
        }
    }
    (void)font;
    return dl;
}

/**
 * Formats `fmt` into a chain of textured glyph quads (Font_AddGlyphDisplayList) scaled by
 * `scaleX` / `scaleY` percent and tinted with `color`, for drawing as part of a model. '\t' centers the
 * rest of the line on x.
 * @unverified Name read from the code; called by the geo text nodes.
 */
Gfx* Font_PrintfToDisplayList(s16 x, s16 y, s16 scale_x, s16 scale_y, Color_RGBA8_u32 color, const char* fmt, ...) {
    u8 buf[0x100];
    UNUSED s32 pad[2]; // FAKE: stack padding
    s32 start_x;
    Gfx* dl;
    s32 len;
    u8* p;
    s32 chr;
    va_list args;

    va_start(args, fmt);
    start_x = x;
    dl = NULL;
    if (Port_Text_GetLanguage() != PORT_LANG_ORIGINAL) { // PORT: translated text as Unicode glyph quads
        dl = Port_Font_PrintfToDisplayListV(x, y, scale_x, scale_y, fmt, args);
        if (dl != NULL) {
            dl = Gfx_WrapWithEnvColor(dl, color);
        }
        va_end(args);
        return dl;
    }
    len = _Printf(String_CopyBytesAndAdvance, (char*)buf, fmt, args);
    if (len > 0) {
        buf[len] = 0;
        chr = buf[0];
        p = &buf[1];
        while (chr != 0) {
            if (chr == 0xA) {
                x = start_x;
                y -= ((u8)((FontEntry8001A118*)font_context)[font_context->unk_50].size_b * ((scale_y * 0xB) / 10)) / 100; // PORT: was the N64 offset 0x38
            } else if (chr == 9) {
                x = Font_GetCenteredStartX(x, scale_x, (const char*)p);
            } else {
                if (chr >= 0x80) {
                    if (*p != 0) {
                        chr = *p | (chr << 8);
                        p++;
                    }
                }
                x += Font_AddGlyphDisplayList(&dl, x, y, scale_x, scale_y, chr & 0x7F7F);
            }
            chr = *p;
            p++;
        }
    }
    if (dl != NULL) {
        dl = Gfx_WrapWithEnvColor(dl, color);
    }
    va_end(args);
    return dl;
}
