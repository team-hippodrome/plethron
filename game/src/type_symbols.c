/* PORT: not part of the decomp. Type symbols for the battle move panel in the translated languages.
 *
 * Holding R in battle shows each move with its type as a 32x9 texture of kana (でんき, ノーマル, ...; the pointers
 * D_8218AF70[type]), which only the Japanese modes can read. In the other languages each label is drawn as a
 * language-neutral symbol instead: a pill in the type's colour with a pictogram (tools/font/make_type_symbols.py,
 * src/port/generated/type_symbols.h). The symbols are 64x18 texels, drawn over the original 32x9 rectangle, so they
 * stay sharp at the port's higher resolutions. にほんご and Japanese with kanji keep the original textures.
 *
 * The switch menu's status stamps (ひんし, ねむり, どく, やけど, こおり, まひ; 24x11 textures D_8218AFDC[1..6]) get the same
 * treatment: a stamp in the status colour with a pictogram (tools/font/make_status_symbols.py,
 * src/port/generated/status_symbols.h), 48x22 texels drawn over the original rectangle.
 */
#include "global.h"
#include "input.h" // Gfx_DrawTextureRgba16
#include "text_lookup.h"
#include "type_symbols.h"

#include "generated/status_symbols.h"
#include "generated/type_symbols.h"

#define TYPE_LABEL_WIDTH 32
#define TYPE_LABEL_HEIGHT 9
#define STATUS_LABEL_WIDTH 24
#define STATUS_LABEL_HEIGHT 11

static s32 TypeSymbols_UseOriginal(void) {
    s32 lang = Port_Text_GetLanguage();

    return (lang == PORT_LANG_ORIGINAL) || (lang == PORT_LANG_JA);
}

// Draws a symbol of tw x th texels over the w x h screen rectangle at (x, y).
static void TypeSymbols_DrawScaled(s32 x, s32 y, s32 w, s32 h, const u8* symbol, s32 tw, s32 th) {
    gDPLoadTextureBlock(gDisplayListHead++, symbol, G_IM_FMT_RGBA, G_IM_SIZ_16b, tw, th, 0, G_TX_NOMIRROR | G_TX_CLAMP,
                        G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    // tw / w texels per screen pixel across, th / h down (5.10 fixed point)
    gSPTextureRectangle(gDisplayListHead++, x << 2, y << 2, (x + w) << 2, (y + h) << 2, G_TX_RENDERTILE, 0, 0,
                        (tw << 10) / w, (th << 10) / h);
}

void TypeSymbols_DrawMoveTypeLabel(s32 x, s32 y, s32 type, u8* original) {
    const u8* symbol = ((type >= 0) && (type < TYPE_SYMBOL_COUNT)) ? sTypeSymbols[type] : NULL;

    if (TypeSymbols_UseOriginal() || (symbol == NULL)) {
        Gfx_DrawTextureRgba16(x, y, TYPE_LABEL_WIDTH, TYPE_LABEL_HEIGHT, original, TYPE_LABEL_WIDTH, 0);
        return;
    }
    TypeSymbols_DrawScaled(x, y, TYPE_LABEL_WIDTH, TYPE_LABEL_HEIGHT, symbol, TYPE_SYMBOL_WIDTH, TYPE_SYMBOL_HEIGHT);
}

void TypeSymbols_DrawStatusLabel(s32 x, s32 y, s32 status, u8* original) {
    const u8* symbol = ((status >= 0) && (status < STATUS_SYMBOL_COUNT)) ? sStatusSymbols[status] : NULL;

    if (TypeSymbols_UseOriginal() || (symbol == NULL)) {
        Gfx_DrawTextureRgba16(x, y, STATUS_LABEL_WIDTH, STATUS_LABEL_HEIGHT, original, STATUS_LABEL_WIDTH, 0);
        return;
    }
    TypeSymbols_DrawScaled(x, y, STATUS_LABEL_WIDTH, STATUS_LABEL_HEIGHT, symbol, STATUS_SYMBOL_WIDTH,
                           STATUS_SYMBOL_HEIGHT);
}
