/**
 * PORT: name order for the Pokédex index in the translated languages.
 *
 * The index's second sort (C-Right, アイウエオじゅん) walks the ROM's kana order table (Species_GetKanaOrderEntry),
 * which puts Ekans and Arbok first and makes no sense once the names are English, Spanish or Korean. In those
 * languages the order is built instead from the names as the port shows them, sorted with the Unicode collation in
 * src/port/collate.cpp (accents and case second, ñ after n in Spanish, Hangul in 가나다 order), and the sort bar
 * names it (PORT_POKEDEX_NAME_ORDER). The original language and Japanese keep the ROM's table: its names are the
 * original katakana. The port setting gPort.PokedexNameOrder 0 keeps the ROM's order everywhere.
 */
#include "pokedex_name_order.h"

#include <string.h>

#include "variables.h"
#include "text_lookup.h"
#include "collate.h"

#define DEX_COUNT 151
#define NAME_MAX 48

static u8 sOrder[DEX_COUNT];
static s32 sOrderLanguage = -1;

s32 PokedexNameOrder_Active(void) {
    s32 lang = Port_Text_GetLanguage();

    return (lang != PORT_LANG_ORIGINAL) && (lang != PORT_LANG_JA) && Port_PokedexNameOrder_Enabled();
}

static void PokedexNameOrder_Build(s32 lang) {
    static char names[DEX_COUNT][NAME_MAX];
    s32 i;
    s32 j;

    for (i = 0; i < DEX_COUNT; i++) {
        const char* name = Port_Text_Utf8Of(D_80064C5C[i].name);

        strncpy(names[i], (name != NULL) ? name : "", NAME_MAX - 1);
        names[i][NAME_MAX - 1] = '\0';
    }
    // insertion sort: stable, so names that compare equal stay in dex order
    for (i = 0; i < DEX_COUNT; i++) {
        u8 dex = i + 1;

        for (j = i; (j > 0) && (Port_Collate(names[sOrder[j - 1] - 1], names[i], lang) > 0); j--) {
            sOrder[j] = sOrder[j - 1];
        }
        sOrder[j] = dex;
    }
    sOrderLanguage = lang;
}

s32 PokedexNameOrder_Entry(s32 pos) {
    s32 lang;

    if (!PokedexNameOrder_Active() || (pos < 1) || (pos > DEX_COUNT)) {
        return 0;
    }
    lang = Port_Text_GetLanguage();
    if (sOrderLanguage != lang) {
        PokedexNameOrder_Build(lang);
    }
    return sOrder[pos - 1];
}
