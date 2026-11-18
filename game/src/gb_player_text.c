/* PORT: not part of the decomp. Translated text for the GB Player's own screens.
 *
 * The GB Player (fragment 000) does not use the Stadium font or EUC-JP strings. Its text is lists of glyph commands
 * (GbEmu_DrawTextCommands: flags, x, y, font height, palette, length, then glyph numbers into its own 10 and 12 pixel
 * fonts) drawn by the CPU into RGBA5551 frame buffers, plus three status strip words drawn glyph by glyph
 * (ロードちゅう, セーブちゅう, でメニュー). None of it reaches the text lookup, so it stayed in kana in every language.
 *
 * Here each command list is known by its address and has a GB_PLAYER_* row in translations/ai.csv. In a translated
 * language the list's text is laid out again: the latin languages draw with the GB Player's own font where it has the
 * letter (it holds A-Z, a-z, digits and some punctuation) and Unifont for the rest (accents); Korean and Japanese with
 * kanji draw with Unifont. Lines wrap inside the original box, `{...}` in a cell picks the list's second palette (the
 * red or yellow highlight some screens use), and a line over its box by two characters or less is squeezed. Kana mode
 * (and an empty cell) draws the original commands untouched.
 */
#include "global.h"
#include "text_lookup.h"
#include "unicode_font.h"
#include "gb_player_text.h"

#include <string.h>

#define GBT_MIN(a, b) ((a) < (b) ? (a) : (b))
#define GBT_MAX(a, b) ((a) > (b) ? (a) : (b))

// The GB Player's fonts (decompressed at boot): 16 bytes a row, one byte (a palette index in its low nibble) a texel.
extern u8* D_81156598; // 10 pixel font, 0xA0 bytes a glyph
extern u8* D_8115659C; // 12 pixel font, 0xC0 bytes a glyph

extern u16 D_81122260[];
extern u16 D_81122280[];
extern u16 D_811222A0[];
extern u16 D_811222C0[];
extern u16 D_811222E0[];
extern u16 D_81122300[];
extern u16 D_81122320[];
extern u16 D_81122340[];
extern u16 D_81122360[];
extern u16 D_81122380[];
extern u16 D_811223A0[];
extern u16 D_811223C0[];

// The command lists (gb_emu_screen.c, gb_emu_frame.c).
extern u8 D_8111E318[];
extern u8 D_8111E324[];
extern u8 D_8111E334[];
extern u8 D_8111E364[];
extern u8 D_8111E374[];
extern u8 D_8111E3A8[];
extern u8 D_8111E3CC[];
extern u8 D_8111E3E0[];
extern u8 D_8111E40C[];
extern u8 D_8111E454[];
extern u8 D_8111E47C[];
extern u8 D_8111E4A0[];
extern u8 D_8111E4AC[];
extern u8 D_8111E4BC[];
extern u8 D_8111E4D0[];
extern u8 D_8111E4E4[];
extern u8 D_8111E540[];
extern u8 D_8112A938[];

void GbEmu_BlendColorWithAlphaMap(u16* dst, u8* alpha_map, s32 color, s32 width, s32 height, u32 alpha_stride);

typedef enum GbTextAlign {
    GBTEXT_LEFT,   // at the first command's x
    GBTEXT_CENTER, // centred on the original text's centre
    GBTEXT_SCREEN  // each line centred on `right` / 2 (the full-screen Game Pak messages)
} GbTextAlign;

typedef struct GbTextList {
    u8* list;
    const char* id;     // a second id after '|' is for the list's second command (はい / いいえ: one each)
    s16 right;          // the box's right edge in the list's coordinates
    u8 maxLines;        // lines the box has room for (0 = one per original line)
    u8 align;
} GbTextList;

static const GbTextList sGbTextLists[] = {
    // C-Up menu (GbEmu_DrawPauseMenu), in its own coordinates: the separator under the items runs from x 3 to 159.
    { D_8111E318, "GB_PLAYER_MENU", 162, 1, GBTEXT_CENTER },
    { D_8111E324, "GB_PLAYER_CONTINUE_GAME", 162, 1, GBTEXT_LEFT },
    { D_8111E334, "GB_PLAYER_CONTINUE_GAME_HELP", 162, 3, GBTEXT_LEFT },
    { D_8111E364, "GB_PLAYER_QUIT_GAME", 162, 1, GBTEXT_LEFT },
    { D_8111E374, "GB_PLAYER_QUIT_GAME_HELP", 162, 3, GBTEXT_LEFT },
    { D_8111E3A8, "GB_PLAYER_QUIT_CONFIRM", 162, 2, GBTEXT_LEFT },
    { D_8111E3CC, "GB_PLAYER_HELP_CONFIG", 162, 1, GBTEXT_LEFT },
    { D_8111E3E0, "GB_PLAYER_HELP_CONFIG_HELP", 162, 3, GBTEXT_LEFT },
    { D_8111E4BC, "GB_PLAYER_YES|GB_PLAYER_NO", 162, 1, GBTEXT_LEFT },
    // せつめい・コンフィグ screen (GbEmu_DrawButtonConfig), same origin. コンフィグ sits in a 56 pixel gap of the frame.
    { D_8111E40C, "GB_PLAYER_SPEED_BUTTON", 162, 1, GBTEXT_LEFT },
    { D_8111E454, "GB_PLAYER_SPEED_HELP_DOUBLE", 162, 3, GBTEXT_LEFT },
    { D_8111E47C, "GB_PLAYER_SPEED_HELP_CYCLE", 162, 3, GBTEXT_LEFT },
    { D_8111E4A0, "GB_PLAYER_CONFIG", 72, 1, GBTEXT_CENTER },
    { D_8111E4AC, "GB_PLAYER_BUTTON_BACK", 162, 1, GBTEXT_LEFT },
    // Game Pak trouble while the GB Player runs (GbEmu_RunFrame states 21-25), on the whole screen: the GB Player's
    // video mode shows frame buffer x 0..283 (its lines are centred on x 142).
    { D_8111E4D0, "GB_PLAYER_COULD_NOT_SAVE", 284, 1, GBTEXT_SCREEN },
    { D_8111E4E4, "GB_PLAYER_CLEAN_CONNECTOR", 284, 5, GBTEXT_SCREEN },
    { D_8111E540, "GB_PLAYER_WRONG_GAME_PAK", 284, 3, GBTEXT_SCREEN },
    // The cartridge copy screen's scrolling line (GbEmu_DrawLoadingScreen): a 320x15 strip that wraps around.
    { D_8112A938, "GB_PLAYER_LOADING_TICKER", 318, 1, GBTEXT_LEFT },
};

static const char* const sStatusWordIds[] = { "GB_PLAYER_STATUS_LOADING", "GB_PLAYER_STATUS_SAVING",
                                              "GB_PLAYER_STATUS_MENU" };

#define GBTEXT_MAX_WIDTH 640
#define GBTEXT_MAX_HEIGHT 16
#define GBTEXT_ALT 0x10 // texel flag: the second palette

// One rendered line: palette indices (low nibble, 0 = clear) and GBTEXT_ALT.
static u8 sLine[GBTEXT_MAX_HEIGHT][GBTEXT_MAX_WIDTH];

static s32 GbText_UsesOwnFont(void) {
    s32 lang = Port_Text_GetLanguage();

    return (lang == PORT_LANG_EN) || (lang == PORT_LANG_ES) || (lang == PORT_LANG_DE) || (lang == PORT_LANG_PT) ||
           (lang == PORT_LANG_FR) || (lang == PORT_LANG_IT) || (lang == PORT_LANG_JA);
}

// The GB Player font's glyph for `cp`, or -1. The fonts hold kana, digits, A-Z, a-z and a few symbols.
// The fonts' kana, in glyph order from 0x45, 0x50 and 0xAF (Japanese with kanji draws them in the GB fonts too).
static const char kKana45[] = "ァィゥェォッャュョヲン";
static const char kKana50[] = "アイウエオカキクケコサシスセソタチツテトナニヌネノハヒフヘホマミムメモヤユヨラリルレロワガギグゲ"
                              "ゴザジズゼゾダヂヅデドバビブベボパピプペポ";
static const char kKanaAF[] = "ぁぃぅぇぉっゃゅょをんあいうえおかきくけこさしすせそたちつてとなにぬねのはひふへほまみむめもやゆよ"
                              "らりるれろわがぎぐげござじずぜぞだぢづでどばびぶべぼぱぴぷぺぽヴ";

static s32 GbText_KanaGlyph(u32 cp) {
    static const char* const kRuns[] = { kKana45, kKana50, kKanaAF };
    static const s32 kStarts[] = { 0x45, 0x50, 0xAF };

    for (s32 r = 0; r < 3; r++) {
        const char* p = kRuns[r];
        for (s32 i = 0; *p != 0; i++) {
            if (Port_Utf8Next(&p) == cp) {
                return kStarts[r] + i;
            }
        }
    }
    return -1;
}

static s32 GbText_OwnGlyph(u32 cp) {
    if (cp >= 0x3041 && cp <= 0x30FA) {
        return GbText_KanaGlyph(cp);
    }
    if (cp >= '0' && cp <= '9') {
        return 0x10 + (cp - '0');
    }
    if (cp >= 'A' && cp <= 'Z') {
        return 0x1A + (cp - 'A');
    }
    if (cp >= 'a' && cp <= 'z') {
        return 0x95 + (cp - 'a');
    }
    switch (cp) {
        case ' ':
            return 0;
        case '(':
            return 0x03;
        case ')':
            return 0x04;
        case '%':
            return 0x06;
        case '&':
            return 0x0D;
        case '~':
            return 0x0E;
        case '!':
            return 0x34;
        case '\'':
        case 0x2019:
            return 0x37;
        case '*':
            return 0x38;
        case '+':
            return 0x39;
        case ',':
            return 0x3A;
        case '-':
        case 0x2013: // –
            return 0x3B;
        case '.':
            return 0x3C;
        case '/':
            return 0x3D;
        case ':':
            return 0x3E;
        case '=':
            return 0x3F;
        case '?':
            return 0x40;
        case 0x2642: // ♂
            return 0x02;
        case 0x2640: // ♀
            return 0x41;
        case 0x300C: // 「
            return 0x07;
        case 0x300D: // 」
            return 0x08;
        case 0x30FB: // ・
            return 0x05;
        case 0x30FC: // ー
            return 0x3B;
        case 0x3001: // 、
            return 0x01;
        case 0x3002: // 。
            return 0x42;
        case 0x3000: // full-width space
            return 0;
    }
    return -1;
}

// Accented Latin letters the GB fonts lack, built from the plain letter and a mark drawn above it (below for the
// cedilla), and ¡ ¿ as ! ? turned over. 0xC0-0xFF: the base letter and the mark, '\0' where Latin-1 has none to
// build (ß, Æ, Ø, Þ ... come from Unifont).
static const char kLatin1Base[] = "AAAAAA\0CEEEEIIII\0NOOOOO\0\0UUUUY\0\0aaaaaa\0ceeeeiiii\0nooooo\0\0uuuuy\0y";
static const char kLatin1Mark[] = "`'^~:o\0,`'^:`'^:\0~`'^~:\0\0`'^:'\0\0`'^~:o\0,`'^:`'^:\0~`'^~:\0\0`'^:'\0:";

typedef struct GbComposed {
    u32 cp;
    s32 height;
    u8 tile[12 * 16];
} GbComposed;

static GbComposed sComposed[48];
static s32 sComposedCount;

// The mark's texels (2 rows of 5, '#' = ink), centred over the letter.
static const char* GbText_MarkShape(char mark) {
    switch (mark) {
        case '\'':
            return "...##" "..##.";
        case '`':
            return "##..." ".##..";
        case '^':
            return ".###." "##.##";
        case '~':
            return ".##.#" "#.##.";
        case ':':
            return "##.##" "##.##";
        case 'o':
            return ".###." ".#.#.";
        default:
            return NULL;
    }
}

static s32 GbText_RowEmpty(const u8* t, s32 y, s32 tile) {
    for (s32 x = 0; x < tile; x++) {
        if (t[y * 16 + x] & 0xF) {
            return 0;
        }
    }
    return 1;
}

static const u8* GbText_ComposedGlyph(u32 cp, s32 height, u8* font, s32 tile) {
    GbComposed* c;
    s32 base;
    char mark = 0;
    s32 turn = 0;

    for (s32 i = 0; i < sComposedCount; i++) {
        if (sComposed[i].cp == cp && sComposed[i].height == height) {
            return sComposed[i].tile;
        }
    }
    if (cp == 0xA1 || cp == 0xBF) { // ¡ ¿
        base = GbText_OwnGlyph(cp == 0xA1 ? '!' : '?');
        turn = 1;
    } else if (cp >= 0xC0 && cp <= 0xFF && kLatin1Base[cp - 0xC0] != 0) {
        base = GbText_OwnGlyph((u8)kLatin1Base[cp - 0xC0]);
        mark = kLatin1Mark[cp - 0xC0];
    } else {
        return NULL;
    }
    if (base < 0 || sComposedCount >= ARRAY_COUNT(sComposed)) {
        return NULL;
    }
    c = &sComposed[sComposedCount++];
    c->cp = cp;
    c->height = height;
    memset(c->tile, 0, sizeof(c->tile));
    {
        const u8* t = font + base * tile * 16;
        s32 top = tile;
        s32 bottom = -1;
        s32 x0 = tile;
        s32 x1 = -1;
        const char* shape = GbText_MarkShape(mark);

        for (s32 y = 0; y < tile; y++) {
            for (s32 x = 0; x < tile; x++) {
                c->tile[y * 16 + x] = turn ? t[(tile - 1 - y) * 16 + (tile - 1 - x)] : t[y * 16 + x];
            }
        }
        if (cp >= 0xEC && cp <= 0xEF) { // í ì î ï: drop the i's dot (the rows above the first gap)
            s32 y = 0;
            while (y < tile && GbText_RowEmpty(c->tile, y, tile)) {
                y++;
            }
            while (y < tile && !GbText_RowEmpty(c->tile, y, tile)) {
                memset(&c->tile[y * 16], 0, tile);
                y++;
            }
        }
        for (s32 y = 0; y < tile; y++) {
            for (s32 x = 0; x < tile; x++) {
                if (c->tile[y * 16 + x] & 0xF) {
                    top = GBT_MIN(top, y);
                    bottom = GBT_MAX(bottom, y);
                    x0 = GBT_MIN(x0, x);
                    x1 = GBT_MAX(x1, x);
                }
            }
        }
        if (mark == ',') { // cedilla: under the letter
            s32 cx = (x0 + x1) / 2;
            if (bottom + 1 < tile) {
                c->tile[(bottom + 1) * 16 + cx] = 0xFF;
                c->tile[(bottom + 1) * 16 + cx + 1] = 0xFF;
            }
        } else if (shape != NULL && x1 >= 0) {
            s32 cx = (x0 + x1 + 1) / 2 - 2;
            s32 rows = (top >= 3) ? 2 : 1;
            s32 y0 = GBT_MAX(0, top - 1 - rows);

            for (s32 r = 0; r < rows; r++) {
                for (s32 i = 0; i < 5; i++) {
                    s32 x = cx + i;
                    if (shape[(rows == 1 ? 1 : r) * 5 + i] == '#' && x >= 0 && x < tile) {
                        c->tile[(y0 + r) * 16 + x] = 0xFF;
                    }
                }
            }
        }
    }
    return c->tile;
}

typedef struct GbGlyph {
    const u8* own;     // own font: the tile (16 bytes a row), from column `x0`
    const u8* ia8;     // Unifont: IA8 texels, `stride` a row
    s32 stride;
    s32 x0;
    s32 width;         // texels to draw
    s32 advance;
} GbGlyph;

// The first and last columns with ink in a glyph tile, cached per font glyph (layout runs every frame).
static s8 sInk[2][256][2];
static u8 sInkKnown[2][256];

static void GbText_InkColumns(const u8* t, s32 tile, s8* cache, s32* x0, s32* x1) {
    if (cache != NULL && sInkKnown[tile == 12][(cache - sInk[tile == 12][0]) / 2]) {
        *x0 = cache[0];
        *x1 = cache[1];
        return;
    }
    *x0 = tile;
    *x1 = -1;
    for (s32 y = 0; y < tile; y++) {
        for (s32 x = 0; x < tile; x++) {
            if (t[y * 16 + x] & 0xF) {
                *x0 = GBT_MIN(*x0, x);
                *x1 = GBT_MAX(*x1, x);
            }
        }
    }
    if (cache != NULL) {
        cache[0] = (s8)*x0;
        cache[1] = (s8)*x1;
        sInkKnown[tile == 12][(cache - sInk[tile == 12][0]) / 2] = 1;
    }
}

static s32 GbText_GetGlyph(u32 cp, s32 height, GbGlyph* g) {
    s32 idx = GbText_UsesOwnFont() ? GbText_OwnGlyph(cp) : -1;
    u8* font = (height >= 12) ? D_8115659C : D_81156598;
    s32 tile = (height >= 12) ? 12 : 10;

    const u8* composed = NULL;

    memset(g, 0, sizeof(*g));
    if ((idx < 0) && GbText_UsesOwnFont() && (font != NULL)) {
        composed = GbText_ComposedGlyph(cp, height, font, tile);
    }
    if (((idx >= 0) || (composed != NULL)) && (font != NULL)) {
        const u8* t = (composed != NULL) ? composed : font + idx * tile * 16;
        s32 x0;
        s32 x1;

        GbText_InkColumns(t, tile, (composed != NULL) ? NULL : sInk[tile == 12][idx], &x0, &x1);
        if (cp >= 0x3000) { // kana keep the fonts' fixed pitch, like the original lines
            g->own = t;
            g->width = tile;
            g->advance = tile - 1;
            return 1;
        }
        if (x1 < 0) { // the space (a full glyph in Japanese, as in the original lines)
            g->advance = (Port_Text_GetLanguage() == PORT_LANG_JA) ? tile - 1 : tile / 3 + 1;
            return 1;
        }
        g->own = t;
        g->x0 = x0;
        g->width = x1 - x0 + 1;
        g->advance = g->width + 1;
        return 1;
    }
    if (cp == ' ' || cp == 0x3000) {
        g->advance = (cp == ' ') ? height / 3 + 1 : height;
        return 1;
    }
    g->ia8 = Port_Font_Glyph(cp, height, &g->stride, &g->width, &g->advance);
    if (g->ia8 == NULL) {
        g->ia8 = Port_Font_Glyph('?', height, &g->stride, &g->width, &g->advance);
    }
    return g->ia8 != NULL;
}

// Width of `s` up to `end` (markup braces take no room).
static s32 GbText_Measure(const char* s, const char* end, s32 height) {
    s32 w = 0;
    GbGlyph g;

    while (s < end) {
        u32 cp = Port_Utf8Next(&s);
        if (cp == 0) {
            break;
        }
        if (cp == '{' || cp == '}') {
            continue;
        }
        if (GbText_GetGlyph(cp, height, &g)) {
            w += g.advance;
        }
    }
    return w;
}

// Renders `s` up to `end` into sLine (cleared first). `alt` is the markup state at the start; returns the state at
// the end, so a highlight can run over a line break.
static s32 GbText_Render(const char* s, const char* end, s32 height, s32 alt) {
    s32 x = 0;
    GbGlyph g;

    memset(sLine, 0, sizeof(sLine));
    while (s < end) {
        u32 cp = Port_Utf8Next(&s);
        if (cp == 0) {
            break;
        }
        if (cp == '{' || cp == '}') {
            alt = (cp == '{');
            continue;
        }
        if (!GbText_GetGlyph(cp, height, &g)) {
            continue;
        }
        for (s32 y = 0; y < height && y < GBTEXT_MAX_HEIGHT; y++) {
            for (s32 i = 0; i < g.width && x + i < GBTEXT_MAX_WIDTH; i++) {
                s32 v = 0;
                if (g.own != NULL) {
                    v = g.own[y * 16 + g.x0 + i] & 0xF;
                } else if (g.ia8 != NULL) {
                    // Unifont's outlined IA8 as the GB fonts' 16 shades: fill at 15, the outline at the darkest
                    u8 t = g.ia8[y * g.stride + i];
                    s32 a = t & 0xF;
                    s32 in = t >> 4;
                    if (a != 0) {
                        v = (a * (15 + 14 * in) + 112) / 225;
                        v = GBT_MAX(1, GBT_MIN(v, 15));
                    }
                }
                if (v != 0) {
                    sLine[y][x + i] = (u8)(v | (alt ? GBTEXT_ALT : 0));
                }
            }
        }
        x += g.advance;
    }
    return alt;
}

static u16* GbText_Palette(s32 n) {
    switch (n) {
        case 1:
            return D_81122280;
        case 2:
            return D_811222A0;
        case 3:
            return D_811222C0;
        case 4:
            return D_811222E0;
        case 5:
            return D_81122300;
        case 6:
            return D_81122320;
        case 7:
            return D_81122340;
        case 8:
            return D_81122360;
        case 9:
            return D_81122380;
        case 10:
            return D_811223A0;
        case 11:
            return D_811223C0;
        default:
            return D_81122260;
    }
}

// Copies sLine (`width` x `height`) to the frame buffer at (x, y), squeezed to `drawWidth` when that is narrower.
static void GbText_Blit(u16* fb, s32 x, s32 y, s32 width, s32 drawWidth, s32 height, s32 pal, s32 altPal) {
    u16* p0 = GbText_Palette(pal);
    u16* p1 = GbText_Palette(altPal);

    for (s32 row = 0; row < height && row < GBTEXT_MAX_HEIGHT; row++) {
        for (s32 col = 0; col < drawWidth; col++) {
            s32 sx = (drawWidth < width) ? (col * width) / drawWidth : col;
            u8 v;
            if (sx >= GBTEXT_MAX_WIDTH || x + col < 0 || x + col >= 320) {
                continue;
            }
            v = sLine[row][sx];
            if ((v & 0xF) == 0) {
                continue;
            }
            u16 c = ((v & GBTEXT_ALT) ? p1 : p0)[v & 0xF];
            // the palettes are lifted u16 (host order), the frame buffer holds N64 byte order
            fb[(y + row) * 320 + x + col] = (u16)((c >> 8) | (c << 8));
        }
    }
}

// Two average characters: how far over its box a line may run and still be squeezed in.
static s32 GbText_SqueezeLimit(s32 height) {
    return (height * 6 / 10 + 1) * 2;
}

// A line wider than `avail`: squeezed when over by two characters or less, else logged and drawn as is.
static s32 GbText_FitWidth(const char* s, const char* end, s32 width, s32 avail, s32 height) {
    if (width <= avail) {
        return width;
    }
    if (width - avail <= GbText_SqueezeLimit(height)) {
        return avail;
    }
    {
        char buf[256];
        s32 n = GBT_MIN((s32)(end - s), (s32)sizeof(buf) - 1);
        memcpy(buf, s, n);
        buf[n] = 0;
        Port_Text_LogOverflow(buf, width, avail);
    }
    return width;
}

static const GbTextList* GbText_Find(const u8* cmd) {
    for (s32 i = 0; i < ARRAY_COUNT(sGbTextLists); i++) {
        if (sGbTextLists[i].list == cmd) {
            return &sGbTextLists[i];
        }
    }
    return NULL;
}

typedef struct GbTextCmd {
    s32 x, y, height, pal, count, flags;
} GbTextCmd;

// Breaks `text` into lines that fit `avail`: at '\n', and before a word that would not fit (spaces, or between
// characters of CJK text, which has none). Returns the line count; starts/ends hold the byte ranges.
static s32 GbText_Wrap(const char* text, s32 avail, s32 height, const char** starts, const char** ends, s32 max) {
    s32 n = 0;
    const char* line = text;

    while (*line != 0 && n < max) {
        const char* nl = strchr(line, '\n');
        const char* stop = nl ? nl : line + strlen(line);
        const char* brk = NULL; // the last place this line may end
        const char* p = line;

        while (p < stop) {
            const char* q = p;
            u32 cp = Port_Utf8Next(&q);
            if (cp == ' ') {
                brk = p;
            } else if (cp >= 0x2E80 && !(cp >= 0xAC00 && cp <= 0xD7AF) && !(cp >= 0x3130 && cp <= 0x318F) && p != line) {
                brk = p; // before a Chinese or Japanese character (Korean puts spaces between words)
            }
            if (GbText_Measure(line, q, height) > avail && brk != NULL && brk != line) {
                break;
            }
            p = q;
        }
        if (p < stop) {
            starts[n] = line;
            ends[n++] = brk;
            line = brk;
            while (*line == ' ') {
                line++;
            }
            continue;
        }
        starts[n] = line;
        ends[n++] = stop;
        if (nl == NULL) {
            break;
        }
        line = nl + 1;
    }
    return n;
}

s32 GbPlayerText_DrawCommands(u16* fb, u8* cmd) {
    const GbTextList* entry;
    GbTextCmd cmds[8];
    s32 count = 0;
    u8* p = cmd;

    if (Port_Text_GetLanguage() == PORT_LANG_ORIGINAL || (entry = GbText_Find(cmd)) == NULL) {
        return 0;
    }
    while (*p != 0 && count < (s32)ARRAY_COUNT(cmds)) {
        GbTextCmd* c = &cmds[count++];
        c->flags = p[0];
        c->x = p[1] + ((p[0] & 1) << 8);
        c->y = p[2];
        c->height = (p[3] == 0xC) ? 12 : 10;
        c->pal = p[4];
        c->count = p[5];
        p += 6 + p[5];
    }
    if (count == 0) {
        return 0;
    }

    // はい / いいえ: one row per command, each drawn where its command was.
    if (strchr(entry->id, '|') != NULL) {
        char first[64];
        const char* bar = strchr(entry->id, '|');
        const char* text[2];
        s32 n = GBT_MIN((s32)(bar - entry->id), (s32)sizeof(first) - 1);

        memcpy(first, entry->id, n);
        first[n] = 0;
        text[0] = Port_Text_LookupId(first);
        text[1] = Port_Text_LookupId(bar + 1);
        if (text[0] == NULL || text[1] == NULL || count < 2) {
            return 0;
        }
        for (s32 i = 0; i < 2; i++) {
            const char* end = text[i] + strlen(text[i]);
            s32 h = cmds[i].height;
            s32 avail = ((i == 0) ? cmds[1].x - 8 : entry->right) - cmds[i].x;
            s32 w = GbText_Measure(text[i], end, h);
            GbText_Render(text[i], end, h, 0);
            GbText_Blit(fb, cmds[i].x, cmds[i].y, w, GbText_FitWidth(text[i], end, w, avail, h), h, cmds[i].pal,
                        cmds[i].pal);
        }
        return 1;
    }

    {
        const char* text = Port_Text_LookupId(entry->id);
        const char* starts[8];
        const char* ends[8];
        s32 h = cmds[0].height;
        s32 pal = cmds[0].pal;
        s32 altPal = pal;
        s32 x = cmds[0].x;
        s32 pitch = (count >= 2 && cmds[1].y > cmds[0].y) ? cmds[1].y - cmds[0].y : h + 2;
        s32 avail;
        s32 lines;
        s32 alt = 0;
        s32 maxLines = entry->maxLines ? entry->maxLines : count;

        if (text == NULL) {
            return 0;
        }
        for (s32 i = 1; i < count; i++) {
            x = GBT_MIN(x, cmds[i].x);
            if (cmds[i].pal != pal) {
                altPal = cmds[i].pal;
                break;
            }
        }
        avail = (entry->align == GBTEXT_SCREEN) ? entry->right - 8 : entry->right - x;
        lines = GbText_Wrap(text, avail, h, starts, ends, ARRAY_COUNT(starts));
        if (lines > maxLines) {
            // too many lines: wrap a little wider and squeeze the lines that run over (two characters at most)
            lines = GbText_Wrap(text, avail + GbText_SqueezeLimit(h), h, starts, ends, ARRAY_COUNT(starts));
        }
        if (lines > maxLines) {
            Port_Text_LogOverflow(text, lines, maxLines); // more lines than the box holds (width = lines here)
        }
        for (s32 i = 0; i < lines; i++) {
            s32 w = GbText_Measure(starts[i], ends[i], h);
            s32 dw = GbText_FitWidth(starts[i], ends[i], w, avail, h);
            s32 lx = x;
            s32 ly = cmds[0].y + i * pitch;

            if (entry->align == GBTEXT_CENTER) {
                s32 adv = (cmds[0].flags & 0x40) ? cmds[0].height - 1 : cmds[0].height;
                lx = cmds[0].x + (cmds[0].count * adv) / 2 - dw / 2;
                lx = GBT_MAX(lx, 0);
            } else if (entry->align == GBTEXT_SCREEN) {
                lx = entry->right / 2 - dw / 2;
            }
            if (i < count && entry->align != GBTEXT_SCREEN && cmds[i].y == ly) {
                pal = cmds[i].pal; // per-line palettes (the menu items' highlight is on their only line)
            }
            alt = GbText_Render(starts[i], ends[i], h, alt);
            GbText_Blit(fb, lx, ly, w, dw, h, pal, altPal);
        }
    }
    return 1;
}

s32 GbPlayerText_BlendStatusWord(u16* dst, s32 word, s32 avail, s32 height, s32 color) {
    static u8 sAlpha[GBTEXT_MAX_HEIGHT * GBTEXT_MAX_WIDTH];
    const char* text;
    const char* end;
    s32 w;
    s32 dw;

    if (Port_Text_GetLanguage() == PORT_LANG_ORIGINAL || word < 0 || word >= (s32)ARRAY_COUNT(sStatusWordIds) ||
        (text = Port_Text_LookupId(sStatusWordIds[word])) == NULL) {
        return 0;
    }
    end = text + strlen(text);
    w = GbText_Measure(text, end, height);
    dw = GBT_MIN(GbText_FitWidth(text, end, w, avail, height), GBTEXT_MAX_WIDTH);
    w = GBT_MIN(w, GBTEXT_MAX_WIDTH);
    GbText_Render(text, end, height, 0);
    for (s32 y = 0; y < height; y++) {
        for (s32 x = 0; x < dw; x++) {
            s32 sx = (dw < w) ? (x * w) / dw : x;
            sAlpha[y * dw + x] = sLine[y][sx] & 0xF;
        }
    }
    GbEmu_BlendColorWithAlphaMap(dst, sAlpha, color, dw, height, dw);
    return 1;
}
