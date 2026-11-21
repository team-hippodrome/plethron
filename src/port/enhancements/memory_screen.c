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
#include "asset_archive.h"
#include "input.h"
#include "memory_main.h"
#include "poke_icon.h"

#include <hippodrome/summary_card.h>
#include <hippodrome/trainer_memory.h>

extern SpeciesInfo D_80064C5C[];
extern struct Controller* D_800603C0; // gPlayer1Controller
extern PokeIconRenderer* D_8200AC50; // the setup screen's icon renderer (40 slots of 40x40, free_battle_setup)
extern Gfx D_80073408[];             // render state around Gfx_DrawTextureRgba16 (as the setup screen's icons)
extern Gfx D_80073438[];
void PokeIcon_ResetSlots(PokeIconRenderer* renderer); // poke_icon.c
extern u8 D_002F5940[]; // the trainer picture archive the VS screen opens (VsScene_LoadGeoLayout)
extern u8 D_003139C0[];

#define SETUP_STATE_L50_MENU 7
#define SETUP_STATE_L30_CUPS 9

#define MS_BTN_A 0x8000
#define MS_BTN_B 0x4000
#define MS_BTN_DUP 0x0800
#define MS_BTN_DDOWN 0x0400
#define MS_BTN_CUP 0x0008
#define MS_BTN_CDOWN 0x0004
#define MS_BTN_CANY 0x000F

static s32 sOpen;    // shown (also while it opens and closes)
static s32 sClosing; // closing: the window empties, then contracts
static s32 sAnim;    // 0 closed .. MS_EXPAND + MS_FILL fully open
static s32 sCursor;

// The window opens as the game's windows do: it expands from its middle, then fills with its contents; it closes the
// other way round (hippodrome-utils AGENTS.md, design principle 2).
#define MS_EXPAND 8
#define MS_FILL 6
// The window on the 640x480 stage: from under the menu's header (it ends at y 55) to the hint's line.
#define MS_WIN_Y 60.0f
#define MS_WIN_H 392.0f
static f32 sFade = 1.0f; // the contents' opacity while drawing

static void MsRect(f32 x, f32 y, f32 w, f32 h, u8 r, u8 g, u8 b, u8 a) {
    Enh_Rect(x, y, w, h, r, g, b, (u8)(a * sFade));
}

static void MsText(f32 x, f32 y, f32 height, f32 max_width, const char* utf8, u8 r, u8 g, u8 b, u8 a) {
    Enh_Text(x, y, height, max_width, utf8, r, g, b, (u8)(a * sFade));
}

// A port string: its translation, else the original language's kana, else English.
static const char* Str(const char* id, const char* kana, const char* en) {
    const char* t = Port_Text_LookupId(id);
    if (t != NULL && *t != '\0') return t;
    return (Port_Text_GetLanguage() == PORT_LANG_ORIGINAL) ? kana : en;
}

// ---- the highlighted trainer's picture and its team's icons ----

// The VS screen's trainer picture (VsScene_PrepareTeams: file trainerIndex of archive D_002F5940): 64x64 RGBA16,
// loaded once per trainer into a copy of its own. TournamentOpponent_SetUpLv50Round gives a finalist its own
// picture (its index); TournamentOpponent_SetUpLv1To30Round the round's trainer class picture (round + 15).
#define MS_PORTRAIT_SIZE 64
#define MS_PORTRAITS 24
static u8* sPortrait[MS_PORTRAITS];
static u8 sPortraitTried[MS_PORTRAITS];

static s32 PortraitIndex(s32 cup, s32 trainer) {
    return (cup == ZERO_TM_CUP_L50) ? trainer : trainer + 15;
}

static const u8* Portrait(s32 index) {
    if (index < 0 || index >= MS_PORTRAITS) return NULL;
    if (!sPortraitTried[index]) {
        BinArchive* archive;
        void* file = NULL;
        sPortraitTried[index] = 1;
        // the archive and the file only for the copy: the pool goes back to where the setup screen left it
        MainPool_PushState('TMEM');
        archive = BinArchive_Open(D_002F5940, D_003139C0, 1, 1);
        if (archive != NULL) {
            file = BinArchive_GetFile(archive, index); // (NULL past the archive's files)
        }
        if (file != NULL) {
            sPortrait[index] = (u8*)malloc(MS_PORTRAIT_SIZE * MS_PORTRAIT_SIZE * 2);
            if (sPortrait[index] != NULL) {
                memcpy(sPortrait[index], file, MS_PORTRAIT_SIZE * MS_PORTRAIT_SIZE * 2);
            }
        }
        MainPool_PopState('TMEM');
    }
    return sPortrait[index];
}

// The icons: the setup screen's own renderer, slots 0-5 for the team on show. The entry screen and the usable
// Pokémon grid reset its slots when they open, so borrowing them here leaves nothing behind.
static s32 sIconKey = -1;
#define MS_ICON 40

static void SetTeamIcons(s32 key, const ZeroTmView* v) {
    s32 i;
    if (D_8200AC50 == NULL || key == sIconKey) return;
    sIconKey = key;
    PokeIcon_ResetSlots(D_8200AC50);
    for (i = 0; i < v->teamCount && i < 6; i++) {
        // the row's colour behind the model (RGBA5551, as FreeBattle_InitUsableMonGrid passes it)
        PokeIcon_SetSpeciesSlot(D_8200AC50, i, v->team->team[i].species,
                                ((0x20 >> 3) << 11) | ((0x28 >> 3) << 6) | ((0x40 >> 3) << 1) | 1);
    }
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
        sOpen = sClosing = sAnim = 0;
        return 0;
    }
    Port_Enh_SaveMemory(); // what the last battle learned (writes only when something changed)
    pressed = D_800603C0->buttonPressed;
    if (!sOpen) {
        if (pressed & MS_BTN_CANY) {
            sOpen = 1;
            sClosing = 0;
            sAnim = 0;
            sCursor = 0;
            sIconKey = -1;
            return 1; // the press opened the screen; the menu does not see it
        }
        return 0;
    }
    if (sClosing) {
        if (--sAnim <= 0) {
            sOpen = sClosing = sAnim = 0;
        }
        return 1; // (the menu waits until it has closed)
    }
    if (sAnim < MS_EXPAND + MS_FILL) {
        sAnim++;
    }
    if (pressed & (MS_BTN_B | (MS_BTN_CANY & ~(MS_BTN_CUP | MS_BTN_CDOWN)))) { // B, C-Left or C-Right closes it
        sClosing = 1;
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

// The detail pane: the trainer's picture and name, then its team, one row each: the icon, name, level and speed,
// and the moves seen. The picture and the icons are textures, drawn after the text (DrawDetailImages).
#define MS_HEADER_H 72.0f
#define MS_ROW_H 46.0f
#define MS_ROW_TEXT_X 46.0f
#define MS_ROW_MOVES_X 170.0f

static void DrawDetail(const ZeroTmView* v, f32 x, f32 y, f32 w) {
    char buf[64];
    s32 i, k;
    f32 ty = y + MS_HEADER_H;
    f32 mw = (w - MS_ROW_MOVES_X) / 2;

    MsRect(x, y, MS_PORTRAIT_SIZE, MS_PORTRAIT_SIZE, 0x20, 0x28, 0x40, 0xC0); // the picture's frame
    if (v->reached) {
        TrainerName(v->own, buf, sizeof buf);
        MsText(x + MS_PORTRAIT_SIZE + 10.0f, y + 22.0f, 18.0f, w - MS_PORTRAIT_SIZE - 10.0f, buf, 0xFF, 0xFF, 0xFF,
               0xFF);
    }
    if (v->teamCount == 0) {
        MsText(x, ty, 14.0f, w, Str("PORT_MEMORY_TEAM_UNKNOWN", "てもちは まだ わからない", "Team not seen yet"), 0xA0,
               0xA0, 0xA0, 0xFF);
        return;
    }
    for (i = 0; i < v->teamCount; i++) {
        const hu_tm_mon* team_mon = &v->team->team[i];
        const hu_tm_mon* own = (v->own != NULL) ? hu_tm_find_mon(v->own, team_mon->species) : NULL;
        f32 ry = ty + i * MS_ROW_H;
        f32 tx = x + MS_ROW_TEXT_X;
        f32 mx = x + MS_ROW_MOVES_X;
        MsRect(x - 4.0f, ry - 2.0f, w + 8.0f, MS_ROW_H - 3.0f, 0x20, 0x28, 0x40, 0xC0);
        MsText(tx, ry, 14.0f, MS_ROW_MOVES_X - MS_ROW_TEXT_X - 6.0f, SpeciesName(team_mon->species), 0xFF, 0xFF, 0xFF,
               0xFF);
        if (own != NULL || v->team == v->own) {
            snprintf(buf, sizeof buf, "L%u", team_mon->level);
            MsText(tx, ry + 15.0f, 12.0f, 0, buf, 0xE0, 0xE0, 0xE0, 0xFF);
        }
        if (own != NULL && (own->flags & HU_TM_MON_HAS_SPEED)) {
            snprintf(buf, sizeof buf, "%s %u-%u", Str("PORT_MEMORY_SPEED", "すばやさ", "Spe"), own->speed.min,
                     own->speed.max);
            if (own->speed.min == own->speed.max) {
                snprintf(buf, sizeof buf, "%s %u", Str("PORT_MEMORY_SPEED", "すばやさ", "Spe"), own->speed.min);
            }
            MsText(tx, ry + 28.0f, 12.0f, MS_ROW_MOVES_X - MS_ROW_TEXT_X - 6.0f, buf, 0x90, 0xD8, 0xFF, 0xFF);
        }
        // the moves seen, two by two; the others as dashes (moves are per cup, so only this trainer's own record)
        for (k = 0; k < 4; k++) {
            f32 kx = mx + (k & 1) * mw;
            f32 ky = ry + (k >> 1) * 19.0f + 3.0f;
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
            MsText(kx, ky, 14.0f, mw - 6.0f, name, col.r, col.g, col.b, a);
        }
    }
}

// The picture (met trainers only) and the team's icons, once the window is fully open (textures have no fade).
static void DrawDetailImages(const ZeroTmView* v, s32 cup, s32 trainer, f32 x, f32 y) {
    const u8* portrait = v->reached ? Portrait(PortraitIndex(cup, trainer)) : NULL;
    s32 i;

    if (portrait == NULL && v->teamCount == 0) return;
    gSPDisplayList(gDisplayListHead++, D_80073408);
    if (portrait != NULL) { // two 64x32 halves, as sVsPanelPortraitDL loads it (4 KB of TMEM each)
        Gfx_DrawTextureRgba16((s32)x, (s32)y, MS_PORTRAIT_SIZE, MS_PORTRAIT_SIZE / 2, (u8*)portrait, MS_PORTRAIT_SIZE,
                              G_CYC_COPY);
        Gfx_DrawTextureRgba16((s32)x, (s32)y + MS_PORTRAIT_SIZE / 2, MS_PORTRAIT_SIZE, MS_PORTRAIT_SIZE / 2,
                              (u8*)portrait + MS_PORTRAIT_SIZE * MS_PORTRAIT_SIZE, MS_PORTRAIT_SIZE, G_CYC_COPY);
    }
    if (D_8200AC50 != NULL) {
        for (i = 0; i < v->teamCount && i < 6; i++) {
            u8* image = PokeIcon_GetImage((s8*)D_8200AC50, i);
            if (image != NULL) {
                Gfx_DrawTextureRgba16((s32)x, (s32)(y + MS_HEADER_H + i * MS_ROW_H), MS_ICON, MS_ICON, image, MS_ICON,
                                      G_CYC_COPY);
            }
        }
    }
    gSPDisplayList(gDisplayListHead++, D_80073438);
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
        const char* hint = Str("PORT_MEMORY_HINT", "C：トレーナーの きおく", "C: Trainer memory");
        f32 w = Enh_TextWidth(14.0f, hint);
        Enh_Rect(620.0f - w - 12.0f, 426.0f, w + 12.0f, 22.0f, 0, 0, 0, 0xA0);
        Enh_Text(620.0f - w - 6.0f, 430.0f, 14.0f, 0, hint, 0xFF, 0xFF, 0xFF, 0xFF);
        Enh_End();
        return;
    }
    tm = Port_Enh_Memory();
    {
        // the window: it expands from its middle, then its contents fade in. It stays under the menu's header
        // (1P, name, IDNo.), which the game draws once and never again: anything drawn there stays after closing.
        f32 e = (sAnim >= MS_EXPAND) ? 1.0f : (f32)sAnim / MS_EXPAND;
        f32 w = 592.0f * e, h = MS_WIN_H * e;
        f32 x = 24.0f + (592.0f - w) / 2, y = MS_WIN_Y + (MS_WIN_H - h) / 2;
        Enh_Rect(x, y, w, h, 0x08, 0x0C, 0x18, 0xF0);
        Enh_Rect(x, y, w, 2.0f, 0xF8, 0xD8, 0x30, 0xFF);
        sFade = (sAnim <= MS_EXPAND) ? 0.0f : (f32)(sAnim - MS_EXPAND) / MS_FILL;
        if (sFade <= 0.0f) {
            Enh_End();
            return;
        }
    }
    MsText(40.0f, MS_WIN_Y + 8.0f, 18.0f, 400.0f, Str("PORT_MEMORY_TITLE", "トレーナーの きおく", "Trainer memory"), 0xFF, 0xFF,
             0xFF, 0xFF);
    {
        const char* back = Str("PORT_MEMORY_BACK", "Ｂ：もどる", "B: Back");
        f32 w = Enh_TextWidth(14.0f, back);
        MsText(600.0f - w, MS_WIN_Y + 12.0f, 14.0f, 0, back, 0xC0, 0xC0, 0xC0, 0xFF);
    }
    // the trainers
    for (i = 0; i < rows; i++) {
        ZeroTmView v = ZeroTm_View(tm, cup, division, i);
        f32 ry = MS_WIN_Y + 36.0f + i * (rows > 8 ? 22.0f : 42.0f);
        u8 a = v.reached ? 0xFF : 0x90;
        if (i == sCursor) {
            MsRect(32.0f, ry - 3.0f, 196.0f, (rows > 8 ? 20.0f : 38.0f), 0x30, 0x50, 0x90, 0xFF);
        }
        snprintf(buf, sizeof buf, "%d", i + 1);
        MsText(38.0f, ry, 14.0f, 0, buf, 0xC0, 0xC0, 0xC0, a);
        if (v.reached) {
            TrainerName(v.own, buf, sizeof buf);
        } else {
            snprintf(buf, sizeof buf, "%s", Str("PORT_MEMORY_NOT_MET", "まだ あっていない", "Not met yet"));
        }
        MsText(62.0f, ry, 14.0f, 160.0f, buf, 0xFF, 0xFF, 0xFF, a);
    }
    // the highlighted trainer: its picture, name and team
    {
        ZeroTmView v = ZeroTm_View(tm, cup, division, sCursor);
        MsRect(236.0f, MS_WIN_Y + 32.0f, 2.0f, MS_WIN_H - 40.0f, 0x40, 0x48, 0x60, 0xFF);
        DrawDetail(&v, 252.0f, MS_WIN_Y + 38.0f, 352.0f);
        sFade = 1.0f;
        Enh_End();
        if (sAnim >= MS_EXPAND + MS_FILL && !sClosing) {
            SetTeamIcons((cup << 8) | (division << 5) | sCursor, &v);
            if (D_8200AC50 != NULL) {
                PokeIcon_ProcessAllSlots(D_8200AC50, 0); // renders the icons still waiting (the setup screen's call)
            }
            DrawDetailImages(&v, cup, sCursor, 252.0f, MS_WIN_Y + 38.0f);
        }
    }
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
    PrewarmAdd(&t, Str("PORT_MEMORY_HINT", "C：トレーナーの きおく", "C: Trainer memory"));
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
