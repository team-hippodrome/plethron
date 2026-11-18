/* PORT: not part of the decomp. Type symbols for the battle move panel in the translated languages.
 *
 * Holding R in battle shows each move with its type as a 32x9 texture of kana (でんき, ノーマル, ...; the pointers
 * D_8218AF70[type]), which only the Japanese modes can read. In the other languages each label is drawn as a
 * language-neutral symbol instead: a pill in the type's colour with a pictogram (tools/font/make_type_symbols.py,
 * src/port/generated/type_symbols.h). The symbols are 64x18 texels, drawn over the original 32x9 rectangle, so they
 * stay sharp at the port's higher resolutions. にほんご and Japanese with kanji keep the original textures.
 */
#include "global.h"
#include "input.h" // Gfx_DrawTextureRgba16
#include "text_lookup.h"
#include "type_symbols.h"

#include "generated/type_symbols.h"

#define TYPE_LABEL_WIDTH 32
#define TYPE_LABEL_HEIGHT 9

void TypeSymbols_DrawMoveTypeLabel(s32 x, s32 y, s32 type, u8* original) {
    s32 lang = Port_Text_GetLanguage();
    const u8* symbol = ((type >= 0) && (type < TYPE_SYMBOL_COUNT)) ? sTypeSymbols[type] : NULL;

    if ((lang == PORT_LANG_ORIGINAL) || (lang == PORT_LANG_JA) || (symbol == NULL)) {
        Gfx_DrawTextureRgba16(x, y, TYPE_LABEL_WIDTH, TYPE_LABEL_HEIGHT, original, TYPE_LABEL_WIDTH, 0);
        return;
    }
    gDPLoadTextureBlock(gDisplayListHead++, symbol, G_IM_FMT_RGBA, G_IM_SIZ_16b, TYPE_SYMBOL_WIDTH, TYPE_SYMBOL_HEIGHT,
                        0, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
                        G_TX_NOLOD);
    // two texels per screen pixel each way (s and t steps of 2.0 in 5.10 fixed point)
    gSPTextureRectangle(gDisplayListHead++, x << 2, y << 2, (x + TYPE_LABEL_WIDTH) << 2, (y + TYPE_LABEL_HEIGHT) << 2,
                        G_TX_RENDERTILE, 0, 0, (TYPE_SYMBOL_WIDTH << 10) / TYPE_LABEL_WIDTH,
                        (TYPE_SYMBOL_HEIGHT << 10) / TYPE_LABEL_HEIGHT);
}
