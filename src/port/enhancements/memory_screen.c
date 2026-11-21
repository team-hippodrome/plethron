// The trainer memory screen (hippodrome-utils docs/specs/TRAINER_MEMORY.md): on the screen right before the player
// picks their Game Boy or rental Pokémon (Lv. 1-30: the cup select; Lv. 50-55: its トーナメントにでる menu), a C
// button opens a list of the cup's trainers with what the player has learned about each: its name once reached, its
// team, the moves seen and the speed estimates. Compiled with the game code (battle setup screen, 640x480 stage).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "global.h"
#include "controller.h"
#include "game_state.h"
#include "gb_data.h"
#include "variables.h"

#include "text_lookup.h"
#include "enhancements.h"
#include "battle_enhancements.h"
#include "enh_draw.h"
#include "unicode_font.h"
#include "zero_trainer_memory.h"

#include <hippodrome/summary_card.h>
#include <hippodrome/trainer_memory.h>

extern SpeciesInfo D_80064C5C[];
extern struct Controller* D_800603C0; // gPlayer1Controller

#define SETUP_STATE_L50_MENU 7
#define SETUP_STATE_L30_CUPS 9

#define MS_BTN_A 0x8000
#define MS_BTN_B 0x4000
#define MS_BTN_DUP 0x0800
#define MS_BTN_DDOWN 0x0400
#define MS_BTN_CUP 0x0008
#define MS_BTN_CDOWN 0x0004
#define MS_BTN_CANY 0x000F

static s32 sOpen;
static s32 sCursor;

// A port string: its translation, else the original language's kana, else English.
static const char* Str(const char* id, const char* kana, const char* en) {
    const char* t = Port_Text_LookupId(id);
    if (t != NULL && *t != '\0') return t;
    return (Port_Text_GetLanguage() == PORT_LANG_ORIGINAL) ? kana : en;
}

static s32 ScreenCup(s32 state, s32 cup_cursor, s32* division, s32* rows) {
    if (state == SETUP_STATE_L30_CUPS) {
        *division = (cup_cursor >= 0 && cup_cursor < 4) ? cup_cursor : 0;
        *rows = ZERO_TM_L30_ROUNDS;
        return ZERO_TM_CUP_L30;
    }
    if (state == SETUP_STATE_L50_MENU) {
        *division = 0;
        *rows = ZERO_TM_L50_FINALISTS;
        return ZERO_TM_CUP_L50;
    }
    return 0;
}

s32 PortEnh_MemoryScreenUpdate(s32 state, s32 cup_cursor) {
    s32 division, rows;
    s32 cup = ScreenCup(state, cup_cursor, &division, &rows);
    u16 pressed;

    if (!Port_Enh_TrainerMemoryEnabled() || cup == 0 || D_800603C0 == NULL) {
        sOpen = 0;
        return 0;
    }
    Port_Enh_SaveMemory(); // what the last battle learned (writes only when something changed)
    pressed = D_800603C0->buttonPressed;
    if (!sOpen) {
        if (pressed & MS_BTN_CANY) {
            sOpen = 1;
            sCursor = 0;
            return 1; // the press opened the screen; the menu does not see it
        }
        return 0;
    }
    if (pressed & (MS_BTN_B | (MS_BTN_CANY & ~(MS_BTN_CUP | MS_BTN_CDOWN)))) { // B, C-Left or C-Right closes it
        sOpen = 0;
    } else if (pressed & (MS_BTN_DUP | MS_BTN_CUP)) {
        sCursor = (sCursor + rows - 1) % rows;
    } else if (pressed & (MS_BTN_DDOWN | MS_BTN_CDOWN)) {
        sCursor = (sCursor + 1) % rows;
    }
    return 1;
}

static const char* SpeciesName(u8 species) {
    return (species >= 1 && species <= 151) ? Port_Text_Display(D_80064C5C[species - 1].name) : "";
}

static const char* MoveName(u16 move) {
    return (move >= 1 && move <= 165) ? Port_Text_Display(D_8006A9D4[move - 1].name) : "";
}

// A stored trainer name (the VS screen's two lines) on one line.
static void TrainerName(const hu_tm_trainer* t, char* out, size_t cap) {
    const char* s = Port_Text_Display((const char*)t->name);
    size_t n = 0;
    for (; *s != '\0' && n + 1 < cap; s++) {
        out[n++] = (*s == '\n') ? ' ' : *s;
    }
    out[n] = '\0';
}

static void DrawDetail(const ZeroTmView* v, f32 x, f32 y, f32 w) {
    const f32 row_h = 56.0f;
    char buf[64];
    s32 i, k;

    if (v->teamCount == 0) {
        Enh_Text(x, y, 14.0f, w, Str("PORT_MEMORY_TEAM_UNKNOWN", "てもちは まだ わからない", "Team not seen yet"), 0xA0,
                 0xA0, 0xA0, 0xFF);
        return;
    }
    for (i = 0; i < v->teamCount; i++) {
        const hu_tm_mon* team_mon = &v->team->team[i];
        const hu_tm_mon* own = (v->own != NULL) ? hu_tm_find_mon(v->own, team_mon->species) : NULL;
        f32 ry = y + i * row_h;
        f32 mx = x + 150.0f;
        Enh_Rect(x - 4.0f, ry - 2.0f, w + 8.0f, row_h - 4.0f, 0x20, 0x28, 0x40, 0xC0);
        Enh_Text(x, ry, 14.0f, 140.0f, SpeciesName(team_mon->species), 0xFF, 0xFF, 0xFF, 0xFF);
        if (own != NULL || v->team == v->own) {
            snprintf(buf, sizeof buf, "L%u", team_mon->level);
            Enh_Text(x, ry + 17.0f, 12.0f, 140.0f, buf, 0xE0, 0xE0, 0xE0, 0xFF);
        }
        if (own != NULL && (own->flags & HU_TM_MON_HAS_SPEED)) {
            snprintf(buf, sizeof buf, "%s %u-%u", Str("PORT_MEMORY_SPEED", "すばやさ", "Spe"), own->speed.min,
                     own->speed.max);
            if (own->speed.min == own->speed.max) {
                snprintf(buf, sizeof buf, "%s %u", Str("PORT_MEMORY_SPEED", "すばやさ", "Spe"), own->speed.min);
            }
            Enh_Text(x, ry + 33.0f, 12.0f, 140.0f, buf, 0x90, 0xD8, 0xFF, 0xFF);
        }
        // the moves seen, two by two; the others as dashes (moves are per cup, so only this trainer's own record)
        for (k = 0; k < 4; k++) {
            f32 kx = mx + (k & 1) * ((w - 150.0f) / 2);
            f32 ky = ry + (k >> 1) * 18.0f + 4.0f;
            const char* name = "-";
            u8 a = 0x80;
            hu_rgb col = { 0xFF, 0xFF, 0xFF };
            if (own != NULL && own->moves[k] != 0 && (own->seen & (1 << k))) {
                name = MoveName(own->moves[k]);
                col = hu_type_text_color((hu_type)PortEnh_MoveHuType(own->moves[k]));
                a = 0xFF;
            } else if (team_mon->moves[k] == 0 && v->team == v->own) {
                continue; // an empty slot
            }
            Enh_Text(kx, ky, 14.0f, (w - 150.0f) / 2 - 6.0f, name, col.r, col.g, col.b, a);
        }
    }
}

void PortEnh_MemoryScreenDraw(s32 state, s32 cup_cursor) {
    s32 division, rows;
    s32 cup = ScreenCup(state, cup_cursor, &division, &rows);
    const hu_tm* tm;
    char buf[64];
    s32 i;

    if (!Port_Enh_TrainerMemoryEnabled() || cup == 0) {
        return;
    }
    Enh_Begin();
    if (!sOpen) {
        // the hint, under the menus
        const char* hint = Str("PORT_MEMORY_HINT", "Ｃ：トレーナーの きおく", "C: Trainer memory");
        f32 w = Enh_TextWidth(14.0f, hint);
        Enh_Rect(620.0f - w - 12.0f, 426.0f, w + 12.0f, 22.0f, 0, 0, 0, 0xA0);
        Enh_Text(620.0f - w - 6.0f, 430.0f, 14.0f, 0, hint, 0xFF, 0xFF, 0xFF, 0xFF);
        Enh_End();
        return;
    }
    tm = Port_Enh_Memory();
    Enh_Rect(24.0f, 40.0f, 592.0f, 412.0f, 0x08, 0x0C, 0x18, 0xF0);
    Enh_Rect(24.0f, 40.0f, 592.0f, 2.0f, 0xF8, 0xD8, 0x30, 0xFF);
    Enh_Text(40.0f, 48.0f, 18.0f, 400.0f, Str("PORT_MEMORY_TITLE", "トレーナーの きおく", "Trainer memory"), 0xFF, 0xFF,
             0xFF, 0xFF);
    {
        const char* back = Str("PORT_MEMORY_BACK", "Ｂ：もどる", "B: Back");
        f32 w = Enh_TextWidth(14.0f, back);
        Enh_Text(600.0f - w, 52.0f, 14.0f, 0, back, 0xC0, 0xC0, 0xC0, 0xFF);
    }
    // the trainers
    for (i = 0; i < rows; i++) {
        ZeroTmView v = ZeroTm_View(tm, cup, division, i);
        f32 ry = 78.0f + i * (rows > 8 ? 24.0f : 44.0f);
        u8 a = v.reached ? 0xFF : 0x90;
        if (i == sCursor) {
            Enh_Rect(32.0f, ry - 3.0f, 196.0f, (rows > 8 ? 22.0f : 40.0f), 0x30, 0x50, 0x90, 0xFF);
        }
        snprintf(buf, sizeof buf, "%d", i + 1);
        Enh_Text(38.0f, ry, 14.0f, 0, buf, 0xC0, 0xC0, 0xC0, a);
        if (v.reached) {
            TrainerName(v.own, buf, sizeof buf);
        } else {
            snprintf(buf, sizeof buf, "%s", Str("PORT_MEMORY_NOT_MET", "まだ あっていない", "Not met yet"));
        }
        Enh_Text(62.0f, ry, 14.0f, 160.0f, buf, 0xFF, 0xFF, 0xFF, a);
    }
    // the highlighted trainer's team
    {
        ZeroTmView v = ZeroTm_View(tm, cup, division, sCursor);
        Enh_Rect(236.0f, 74.0f, 2.0f, 370.0f, 0x40, 0x48, 0x60, 0xFF);
        DrawDetail(&v, 252.0f, 80.0f, 352.0f);
    }
    Enh_End();
}

// ---- prewarm ----

typedef struct {
    char* text;
    size_t len, cap;
} PrewarmText;

static void PrewarmAdd(PrewarmText* t, const char* s) {
    size_t n = strlen(s);
    if (t->len + n + 2 > t->cap) {
        size_t cap = (t->cap == 0) ? 8192 : t->cap;
        char* grown;
        while (t->len + n + 2 > cap) cap *= 2;
        grown = (char*)realloc(t->text, cap);
        if (grown == NULL) return;
        t->text = grown;
        t->cap = cap;
    }
    memcpy(t->text + t->len, s, n);
    t->len += n;
    t->text[t->len++] = ' ';
    t->text[t->len] = '\0';
}

void PortEnh_MemoryScreenPrewarm(void) {
    static const f32 kHeights[] = { 12.0f, 14.0f, 18.0f }; // the sizes the screen draws text at (above)
    PrewarmText t = { NULL, 0, 0 };
    const hu_tm* tm;
    char buf[64];
    s32 i, j, px[3], n = 0;
    size_t k;

    if (!Port_Enh_TrainerMemoryEnabled()) {
        return;
    }
    tm = Port_Enh_Memory(); // the file, read now rather than on the first open
    // everything the screen can show, in the language chosen before the logo
    PrewarmAdd(&t, Str("PORT_MEMORY_HINT", "Ｃ：トレーナーの きおく", "C: Trainer memory"));
    PrewarmAdd(&t, Str("PORT_MEMORY_TITLE", "トレーナーの きおく", "Trainer memory"));
    PrewarmAdd(&t, Str("PORT_MEMORY_BACK", "Ｂ：もどる", "B: Back"));
    PrewarmAdd(&t, Str("PORT_MEMORY_NOT_MET", "まだ あっていない", "Not met yet"));
    PrewarmAdd(&t, Str("PORT_MEMORY_TEAM_UNKNOWN", "てもちは まだ わからない", "Team not seen yet"));
    PrewarmAdd(&t, Str("PORT_MEMORY_SPEED", "すばやさ", "Spe"));
    PrewarmAdd(&t, "0123456789 L - ?");
    for (i = 1; i <= 151; i++) {
        PrewarmAdd(&t, SpeciesName((u8)i));
    }
    for (i = 1; i <= 165; i++) {
        PrewarmAdd(&t, MoveName((u16)i));
    }
    for (k = 0; k < hu_tm_count(tm); k++) {
        TrainerName(hu_tm_at(tm, k), buf, sizeof buf);
        PrewarmAdd(&t, buf);
    }
    if (t.text == NULL) {
        return;
    }
    for (i = 0; i < 3; i++) { // one worker per glyph size (all three are the largest size today)
        s32 p = Enh_GlyphPx(kHeights[i]);
        for (j = 0; j < n && px[j] != p; j++) {}
        if (j == n) {
            px[n++] = p;
            Port_Font_PrewarmAsync(t.text, p);
        }
    }
    free(t.text);
}
