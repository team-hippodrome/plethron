// Hue choice (hippodrome-utils docs/specs/HUE_CHANGE.md): every Pokémon gets a hue the player can pick, kept in
// <save>.hue beside the N64 save (hippodrome/hue_choice.h). Zero normally derives a nicknamed Pokémon's hue from the
// bytes of its nickname (Model_ComputeNicknameColorVariant); with the enhancement on, a stored choice replaces that
// wherever the game builds the Pokémon's model or icon (party screens, menus, battle, the summary bars' cards). The
// picker is on the entry screen (エントリーポケモンのせんたく), where Zero's player picks a party or rentals: on any
// Pokémon card there (a rental's, or the card Z opens for a party row), L / R step the hue round the colour wheel (the
// original colours included) and Z goes back to the game's own hue. Compiled with the game code.
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

void Audio_PlaySystemSound(s32 id);
arg1_func_80010CA8* Model_ComputeNicknameColorVariant(arg1_func_80010CA8* arg0, BattleMon* arg1); // model_renderer.c

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

s32 PortEnh_HuePickerInput(BattleMon* mon, u16 pressed, u16 back, u16 forward, u16 reset) {
    hu_hue_store* store;
    hu_hue_id id;
    s16 now;
    s16 next;

    if (mon == NULL || mon->species.id == 0 || !Port_Enh_HueChoiceEnabled()) {
        return 0;
    }
    if (!(pressed & (back | forward | reset))) {
        return 0;
    }
    store = Port_Enh_Hues();
    id = HueId(mon);
    now = hu_hue_resolve(store, id, GameShift(mon));
    if (pressed & reset) {
        if (!hu_hue_get(store, id, NULL)) {
            return 0; // already the game's own
        }
        hu_hue_unset(store, id);
    } else {
        next = hu_hue_step(now, (pressed & forward) ? 1 : -1);
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

void PortEnh_HuePickerDraw(s32 x, s32 y, s32 w, BattleMon* mon) {
    char buf[96];
    char line[160];
    const char* value;
    hu_hue_store* store;
    hu_hue_id id;
    s16 shift;
    s32 chosen;
    f32 bx, by, bw, bh, tw, size, keyw;

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

    // the bar sits on the bottom edge of the model's panel: L and R at its ends step the hue
    size = (w < 200) ? 11.0f : 16.0f;
    bh = size + 8.0f;
    bw = (f32)w - 4.0f;
    bx = (f32)x + 2.0f;
    by = (f32)y - bh - 2.0f;
    keyw = size + 4.0f;
    Enh_Begin();
    Enh_Rect(bx, by, bw, bh, 0x00, 0x00, 0x00, 0xA0);
    Enh_Rect(bx + 2.0f, by + 2.0f, keyw, bh - 4.0f, 0xFF, 0xFF, 0xFF, 0x40);
    Enh_Rect(bx + bw - keyw - 2.0f, by + 2.0f, keyw, bh - 4.0f, 0xFF, 0xFF, 0xFF, 0x40);
    Enh_Text(bx + 2.0f + (keyw - Enh_TextWidth(size, "L")) * 0.5f, by + 4.0f, size, 0.0f, "L", 0xFF, 0xFF, 0xFF, 0xFF);
    Enh_Text(bx + bw - keyw - 2.0f + (keyw - Enh_TextWidth(size, "R")) * 0.5f, by + 4.0f, size, 0.0f, "R", 0xFF, 0xFF,
             0xFF, 0xFF);
    snprintf(line, sizeof line, "%s: %s", Str("PORT_HUE_LABEL", "いろ", "Color"), buf);
    tw = Enh_TextWidth(size, line);
    if (tw > bw - 2.0f * keyw - 8.0f) tw = bw - 2.0f * keyw - 8.0f;
    Enh_Text(bx + (bw - tw) * 0.5f, by + 4.0f, size, bw - 2.0f * keyw - 8.0f, line, 0xFF, 0xFF, 0xFF, 0xFF);
    if (chosen) {
        const char* reset = Str("PORT_HUE_RESET", "Z：もとにもどす", "Z: Reset");
        f32 rs = size - 1.0f;
        tw = Enh_TextWidth(rs, reset);
        if (tw > bw - 8.0f) tw = bw - 8.0f;
        Enh_Rect(bx + bw - tw - 8.0f, by - rs - 6.0f, tw + 8.0f, rs + 5.0f, 0x00, 0x00, 0x00, 0xA0);
        Enh_Text(bx + bw - tw - 4.0f, by - rs - 4.0f, rs, bw - 8.0f, reset, 0xFF, 0xFF, 0xFF, 0xFF);
    }
    Enh_End();
}

// The bottom strip of the colour card the entry screen opens with Z: what the buttons do there.
void PortEnh_HueCardHint(s32 x, s32 y, s32 w, s32 h, s32 party) {
    const char* hint = party ? Str("PORT_HUE_CARD_PARTY", "じゅうじキー：ポケモン　L・R：いろ　B：もどる",
                                   "D-Pad: Pokémon  L/R: Color  B: Back")
                             : Str("PORT_HUE_CARD", "L・R：いろ　B：もどる", "L/R: Color  B: Back");
    f32 size = 16.0f;
    f32 tw = Enh_TextWidth(size, hint);
    if (tw > (f32)w - 16.0f) tw = (f32)w - 16.0f;
    Enh_Begin();
    Enh_Rect((f32)x, (f32)y, (f32)w, (f32)h, 0x10, 0x18, 0x40, 0xFF);
    Enh_Text((f32)x + 8.0f, (f32)y + ((f32)h - size) * 0.5f, size, (f32)w - 16.0f, hint, 0xFF, 0xFF, 0xFF, 0xFF);
    Enh_End();
}
