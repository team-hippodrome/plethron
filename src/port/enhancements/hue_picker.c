// Hue choice (hippodrome-utils docs/specs/HUE_CHANGE.md): every Pokémon gets a hue the player can pick, kept in
// <save>.hue beside the N64 save (hippodrome/hue_choice.h). Zero normally derives a nicknamed Pokémon's hue from the
// bytes of its nickname (Model_ComputeNicknameColorVariant); with the enhancement on, a stored choice replaces that
// wherever the game builds the Pokémon's model or icon (party screens, menus, battle, the summary bars' cards). The
// picker is on the entry screen (エントリーポケモンのせんたく), where Zero's player picks a party or rentals: on any
// Pokémon card there (a rental's, or the card Z opens for a party row), L / R step the hue round the colour wheel (the
// original colours included) and Z goes back to the game's own hue. Compiled with the game code.
// Restricted (the setting's default choice once it is on) keeps the picker inside the species' own hue range, the
// min/max of Zero's species table D_80064C5C that the nickname hue is picked from (the same ranges as Stadium 1/2).
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

// The fields that make a Pokémon itself, in this order: the game, its species, its original trainer's ID and its DVs.
// Its names stay out: a tournament's continue record (Save_WriteContinueData) keeps a Pokémon in the Game Boy's
// format, so a rental comes back with a blank OT name, and the bytes after a name's end are not kept either; a renamed
// Pokémon keeps its colour too. Experience, level, moves and HP change as it grows, so they stay out as well.
static hu_hue_id HueId(const BattleMon* mon) {
    u8 head[6];
    head[0] = HU_GAME_STADIUM_ZERO;
    head[1] = mon->species.id;
    head[2] = (u8)(mon->otId >> 8);
    head[3] = (u8)mon->otId;
    head[4] = (u8)(mon->dvs >> 8);
    head[5] = (u8)mon->dvs;
    return hu_hue_id_of(head, sizeof head);
}

// Restricted: the species' range in degrees (out_min/out_max) and 1; unrestricted: 0.
static s32 HueRange(const BattleMon* mon, s16* out_min, s16* out_max) {
    const SpeciesInfo* info;
    if (Port_Enh_HueChoiceMode() != PORT_HUE_RESTRICTED || mon->species.id == 0 || mon->species.id > 151) {
        return 0;
    }
    info = &D_80064C5C[mon->species.id - 1];
    *out_min = info->nicknameHueMin;
    *out_max = info->nicknameHueMax;
    return 1;
}

// The stored choice or the game's hue, restricted to the species' range when the setting says so (a choice picked
// while unrestricted stays stored and comes back when switching back).
static s16 Resolve(const BattleMon* mon, hu_hue_id id, s16 game_shift) {
    s16 lo, hi;
    s16 v = hu_hue_resolve(Port_Enh_Hues(), id, game_shift);
    return HueRange(mon, &lo, &hi) ? hu_hue_clamp(v, lo, hi) : v;
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
        return Resolve(mon, id, game_shift);
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
    s16 lo, hi;

    if (mon == NULL || mon->species.id == 0 || !Port_Enh_HueChoiceEnabled()) {
        return 0;
    }
    if (!(pressed & (back | forward | reset))) {
        return 0;
    }
    store = Port_Enh_Hues();
    id = HueId(mon);
    now = Resolve(mon, id, GameShift(mon));
    if (pressed & reset) {
        if (!hu_hue_get(store, id, NULL)) {
            return 0; // already the game's own
        }
        hu_hue_unset(store, id);
    } else {
        next = HueRange(mon, &lo, &hi) ? hu_hue_step_in_range(now, (pressed & forward) ? 1 : -1, lo, hi)
                                       : hu_hue_step(now, (pressed & forward) ? 1 : -1);
        if (hu_hue_signed(next) == hu_hue_signed(now)) {
            return 0; // at an end of the species' range
        }
        hu_hue_set(store, id, next);
    }
    Audio_PlaySystemSound(2);
    Port_Enh_SaveHues();
    return 1;
}

// The stick turns the hue smoothly, for the hues between L / R's 15° steps: its tilt left / right sets the speed,
// from 1/64° a game update at the edge of the dead zone to 90° a second at full tilt (squared, so a light touch is
// fine enough to land on single degrees). The entry screen updates 30 times a second. Restricted stops at the ends of
// the species' range. The file is written once the stick is let go.
#define HUE_STICK_FULL 64.0f
#define HUE_STICK_MAX_UNITS 192.0f // per update at full tilt: 3°, 90° a second

static s32 sStickDirty;
static f32 sStickCarry; // the part of a unit not applied yet

s32 PortEnh_HuePickerStick(BattleMon* mon, f32 stick_x) {
    hu_hue_store* store;
    hu_hue_id id;
    s16 now, next, lo, hi;
    f32 t, d;
    s32 units;

    if (mon == NULL || mon->species.id == 0 || !Port_Enh_HueChoiceEnabled()) {
        return 0;
    }
    if (stick_x == 0.0f) {
        sStickCarry = 0.0f;
        if (sStickDirty) {
            sStickDirty = 0;
            Port_Enh_SaveHues();
        }
        return 0;
    }
    t = stick_x / HUE_STICK_FULL;
    if (t > 1.0f) t = 1.0f;
    if (t < -1.0f) t = -1.0f;
    d = t * (t < 0.0f ? -t : t) * HUE_STICK_MAX_UNITS;
    if (d > -1.0f && d < 1.0f) d = (d < 0.0f) ? -1.0f : 1.0f; // never stalls inside the live range
    sStickCarry += d;
    units = (s32)sStickCarry;
    sStickCarry -= (f32)units;
    if (units == 0) {
        return 0;
    }
    store = Port_Enh_Hues();
    id = HueId(mon);
    now = Resolve(mon, id, GameShift(mon));
    next = (s16)(hu_hue_signed(now) + units);
    if (HueRange(mon, &lo, &hi)) {
        next = hu_hue_clamp(next, lo, hi);
    }
    if (hu_hue_signed(next) == hu_hue_signed(now)) {
        return 0; // at an end of the species' range
    }
    // a tick each time it crosses one of L / R's 15° steps
    if (hu_hue_degrees(now) / 15 != hu_hue_degrees(next) / 15) {
        Audio_PlaySystemSound(1);
    }
    hu_hue_set(store, id, next);
    sStickDirty = 1;
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
    s32 restricted;
    s16 lo = 0, hi = 0;
    u8 keyl = 0x40, keyr = 0x40;
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
    restricted = HueRange(mon, &lo, &hi);
    if (restricted) {
        // signed degrees inside the species' range; a key at an end of it is dimmed
        shift = hu_hue_clamp(shift, lo, hi);
        if (hu_hue_step_in_range(shift, -1, lo, hi) == shift) keyl = 0x18;
        if (hu_hue_step_in_range(shift, 1, lo, hi) == shift) keyr = 0x18;
    }
    if (hu_hue_degrees(shift) == 0) {
        value = Str("PORT_HUE_ORIGINAL", "もとの いろ", "Original");
        snprintf(buf, sizeof buf, "%s", value);
    } else if (restricted && hi - lo < 360) {
        snprintf(buf, sizeof buf, "%+d°", hu_hue_signed(shift) / HU_HUE_UNITS_PER_DEGREE);
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
    Enh_Rect(bx + 2.0f, by + 2.0f, keyw, bh - 4.0f, 0xFF, 0xFF, 0xFF, keyl);
    Enh_Rect(bx + bw - keyw - 2.0f, by + 2.0f, keyw, bh - 4.0f, 0xFF, 0xFF, 0xFF, keyr);
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
    const char* hint = party ? Str("PORT_HUE_CARD_PARTY", "じゅうじキー：ポケモン　L・R・スティック：いろ　B：もどる",
                                   "D-Pad: Pokémon  L/R/Stick: Color  B: Back")
                             : Str("PORT_HUE_CARD", "L・R・スティック：いろ　B：もどる", "L/R/Stick: Color  B: Back");
    f32 size = 16.0f;
    f32 tw = Enh_TextWidth(size, hint);
    if (tw > (f32)w - 16.0f) tw = (f32)w - 16.0f;
    Enh_Begin();
    Enh_Rect((f32)x, (f32)y, (f32)w, (f32)h, 0x10, 0x18, 0x40, 0xFF);
    Enh_Text((f32)x + 8.0f, (f32)y + ((f32)h - size) * 0.5f, size, (f32)w - 16.0f, hint, 0xFF, 0xFF, 0xFF, 0xFF);
    Enh_End();
}
