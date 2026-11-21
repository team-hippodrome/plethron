// Hue choice (hippodrome-utils docs/specs/HUE_CHANGE.md): every Pokémon gets a hue the player can pick, kept in
// <save>.hue beside the N64 save (hippodrome/hue_choice.h). Zero normally derives a nicknamed Pokémon's hue from the
// bytes of its nickname (Model_ComputeNicknameColorVariant); with the enhancement on, a stored choice replaces that
// wherever the game builds the Pokémon's model or icon (party screens, menus, battle, the summary bars' cards). The
// picker is on the てもち summary pages, under the turning model: the D-pad's Left / Right step the hue round the
// colour wheel (the original colours included), Z goes back to the game's own hue. Compiled with the game code.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "global.h"
#include "controller.h"
#include "game_state.h"

#include "text_lookup.h"
#include "enhancements.h"
#include "battle_enhancements.h"
#include "enh_draw.h"

#include <hippodrome/hue_choice.h>
#include <hippodrome/trainer_memory.h> // enum hu_game

extern struct Controller* D_800603C0; // gPlayer1Controller
void Audio_PlaySystemSound(s32 id);
arg1_func_80010CA8* Model_ComputeNicknameColorVariant(arg1_func_80010CA8* arg0, BattleMon* arg1); // model_renderer.c

#define HP_BTN_Z 0x2000
#define HP_BTN_DLEFT 0x0200
#define HP_BTN_DRIGHT 0x0100

// The fields that make a Pokémon itself, in this order: the game, its species, its original trainer (id and name),
// its nickname and its DVs. Experience, level, moves and HP change as it grows, so they stay out.
static hu_hue_id HueId(const BattleMon* mon) {
    u8 head[6];
    hu_hue_id id;
    head[0] = HU_GAME_STADIUM_ZERO;
    head[1] = mon->species.id;
    head[2] = (u8)(mon->otId >> 8);
    head[3] = (u8)mon->otId;
    head[4] = (u8)(mon->dvs >> 8);
    head[5] = (u8)mon->dvs;
    id = hu_hue_id_of(head, sizeof head);
    id = hu_hue_id_add(id, mon->otName, sizeof mon->otName);
    return hu_hue_id_add(id, mon->nickname, sizeof mon->nickname);
}

s16 PortEnh_HueShift(BattleMon* mon, s16 game_shift) {
    if (mon == NULL || !Port_Enh_HueChoiceEnabled()) {
        return game_shift;
    }
    {
        hu_hue_id id = HueId(mon);
        if (getenv("PORT_ENH_LOG") != NULL) {
            static hu_hue_id logged[64];
            static s32 n;
            s32 i;
            for (i = 0; i < n && logged[i] != id; i++) {
            }
            if (i == n && n < 64) {
                logged[n++] = id;
                fprintf(stderr, "[hue] species %d id %016llx game %d\n", mon->species.id, (unsigned long long)id,
                        game_shift);
            }
        }
        return hu_hue_resolve(Port_Enh_Hues(), id, game_shift);
    }
}

static s32 sBypass; // set while GameShift asks the game for its own hue

s32 PortEnh_HueBypassed(void) {
    return sBypass;
}

// The hue the game itself gives `mon` (Model_ComputeNicknameColorVariant without the player's choice).
static s16 GameShift(BattleMon* mon) {
    arg1_func_80010CA8 v;
    sBypass = 1;
    Model_ComputeNicknameColorVariant(&v, mon);
    sBypass = 0;
    return v.hueShift;
}

s32 PortEnh_HuePickerUpdate(BattleMon* mon) {
    u16 pressed;
    hu_hue_store* store;
    hu_hue_id id;
    s16 now;
    s16 next;

    if (mon == NULL || mon->species.id == 0 || !Port_Enh_HueChoiceEnabled() || D_800603C0 == NULL) {
        return 0;
    }
    pressed = D_800603C0->buttonPressed;
    if (!(pressed & (HP_BTN_DLEFT | HP_BTN_DRIGHT | HP_BTN_Z))) {
        return 0;
    }
    store = Port_Enh_Hues();
    id = HueId(mon);
    now = hu_hue_resolve(store, id, GameShift(mon));
    if (pressed & HP_BTN_Z) {
        if (!hu_hue_get(store, id, NULL)) {
            return 0; // already the game's own
        }
        hu_hue_unset(store, id);
    } else {
        next = hu_hue_step(now, (pressed & HP_BTN_DRIGHT) ? 1 : -1);
        hu_hue_set(store, id, next);
    }
    Audio_PlaySystemSound(2);
    Port_Enh_SaveHues();
    return 1;
}

// A Port string: its translation, else the original language's kana, else English (as the memory screen's).
static const char* Str(const char* id, const char* kana, const char* en) {
    const char* t = Port_Text_LookupId(id);
    if (t != NULL && *t != '\0') return t;
    return (Port_Text_GetLanguage() == PORT_LANG_ORIGINAL) ? kana : en;
}

// A small solid triangle pointing left (dir -1) or right (+1), `h` pixels tall, from rectangles.
static void Arrow(f32 x, f32 y, f32 h, s32 dir, u8 a) {
    s32 rows = (s32)(h * 2.0f);
    s32 i;
    for (i = 0; i < rows; i++) {
        f32 t = (f32)i / (f32)(rows - 1);       // 0 top .. 1 bottom
        f32 w = (h * 0.5f) * (1.0f - (t > 0.5f ? (t - 0.5f) * 2.0f : (0.5f - t) * 2.0f));
        f32 rx = (dir < 0) ? x + (h * 0.5f) - w : x;
        Enh_Rect(rx, y + i * 0.5f, w, 0.5f, 0xFF, 0xFF, 0xFF, a);
    }
}

void PortEnh_HuePickerDraw(s32 x, s32 y, s32 w, BattleMon* mon) {
    char buf[96];
    const char* value;
    hu_hue_store* store;
    hu_hue_id id;
    s16 shift;
    s32 chosen;
    f32 bx, by, bw, bh, tw;

    if (mon == NULL || mon->species.id == 0 || !Port_Enh_HueChoiceEnabled()) {
        return;
    }
    store = Port_Enh_Hues();
    id = HueId(mon);
    chosen = hu_hue_get(store, id, &shift);
    if (!chosen) {
        shift = GameShift(mon);
    }
    if (shift == 0) {
        value = Str("PORT_HUE_ORIGINAL", "もとの いろ", "Original");
        snprintf(buf, sizeof buf, "%s", value);
    } else {
        snprintf(buf, sizeof buf, "%d°", hu_hue_degrees(shift));
    }
    if (!chosen && shift != 0) {
        // a nickname's own hue: say so, the player has not picked it
        snprintf(buf + strlen(buf), sizeof buf - strlen(buf), " (%s)",
                 Str("PORT_HUE_NICKNAME", "ニックネーム", "Nickname"));
    }

    bh = 26.0f;
    bw = (f32)w - 16.0f;
    bx = (f32)x + 8.0f;
    by = (f32)y - bh - 6.0f;
    Enh_Begin();
    Enh_Rect(bx, by, bw, bh, 0x00, 0x00, 0x00, 0x90);
    Arrow(bx + 6.0f, by + 6.0f, 14.0f, -1, 0xFF);
    Arrow(bx + bw - 13.0f, by + 6.0f, 14.0f, 1, 0xFF);
    {
        const char* label = Str("PORT_HUE_LABEL", "いろ", "Color");
        char line[160];
        snprintf(line, sizeof line, "%s: %s", label, buf);
        tw = Enh_TextWidth(16.0f, line);
        if (tw > bw - 40.0f) tw = bw - 40.0f;
        Enh_Text(bx + (bw - tw) * 0.5f, by + 5.0f, 16.0f, bw - 40.0f, line, 0xFF, 0xFF, 0xFF, 0xFF);
    }
    if (chosen) {
        const char* reset = Str("PORT_HUE_RESET", "Z：もとにもどす", "Z: Reset");
        tw = Enh_TextWidth(14.0f, reset);
        Enh_Rect(bx + bw - tw - 10.0f, by - 21.0f, tw + 10.0f, 19.0f, 0x00, 0x00, 0x00, 0x90);
        Enh_Text(bx + bw - tw - 5.0f, by - 19.0f, 14.0f, 0.0f, reset, 0xFF, 0xFF, 0xFF, 0xFF);
    }
    Enh_End();
}
