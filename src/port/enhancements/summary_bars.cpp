// The side summary bars (hippodrome-utils docs/specs/PARTY_SUMMARY_BARS.md): a column of cards on each side of the
// game's picture during a battle, 1P's party on the left and the opponent's on the right. They are drawn by ImGui in
// the window's room beside the picture, never over it: a 4:3 picture in a wider window, or a 16:9 picture on an
// ultrawide screen. When the window has no such room (16:9 on a 16:9 screen) there are no bars; the game's own
// picture and HUD are never moved or covered (preservation).
//
// The battle code (battle_enhancements.c) publishes what the cards show each frame; this file only draws it.
// The cards' 3D models are a placeholder for now (a tile tinted the way the model will be, hu_card_model_look).
#include "enhancements.h"
#include "summary_bars.h"
#include "port/text_lookup.h"

#include <imgui.h>
#include <libultraship/libultraship.h>
#include <ship/window/gui/Gui.h>
#include <ship/window/gui/GuiWindow.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>

namespace Port {
int AspectSetting(); // Engine.cpp: 0 = 4:3, 1 = 16:9
}
ImFont* Port_Enh_CardFont(); // enhancements.cpp

namespace {

// the cards need at least this much room beside the picture (a fraction of the window's height): a 4:3 picture in
// a 16:9 window leaves 0.22, a 16:9 picture in a 21:9 window 0.29, a 4:3 picture in a 16:10 window only 0.13 (no bars)
constexpr float kMinSide = 0.18f;
constexpr uint16_t kFaintFrames = 40; // the placeholder's faint fade (the models' own faint length later)

ImU32 Rgba(int r, int g, int b, int a) {
    return IM_COL32(std::clamp(r, 0, 255), std::clamp(g, 0, 255), std::clamp(b, 0, 255), std::clamp(a, 0, 255));
}

ImU32 Mix(hu_rgb base, hu_rgb tint, float amount, int alpha) {
    return Rgba((int)(base.r * (1 - amount) + tint.r * amount), (int)(base.g * (1 - amount) + tint.g * amount),
                (int)(base.b * (1 - amount) + tint.b * amount), alpha);
}

const char* Str(const char* id, const char* kana, const char* en) {
    const char* t = Port_Text_LookupId(id);
    if (t != nullptr && *t != '\0') return t;
    return Port_Text_GetLanguage() == PORT_LANG_ORIGINAL ? kana : en;
}

// text at `size` pixels, squeezed down (to 70%) when it is wider than `maxw`, then clipped
void Text(ImDrawList* dl, ImFont* f, float x, float y, float size, float maxw, const char* s, ImU32 col) {
    if (s == nullptr || *s == '\0') return;
    float w = f->CalcTextSizeA(size, FLT_MAX, 0, s).x;
    if (w > maxw && maxw > 0) size = std::max(size * 0.7f, size * maxw / w);
    dl->PushClipRect(ImVec2(x, y - size), ImVec2(x + maxw, y + size * 2), true);
    dl->AddText(f, size, ImVec2(x, y), col, s);
    dl->PopClipRect();
}

void DrawCard(ImDrawList* dl, ImFont* f, const PortEnhBattleView& v, int s, int i, ImVec2 p0, ImVec2 p1) {
    const hu_card& c = v.sides[s].cards[i];
    const hu_card_state state = hu_card_get_state(&c);
    const bool own = (s == v.viewer);
    const float w = p1.x - p0.x, h = p1.y - p0.y;
    const float pad = std::max(2.0f, h * 0.06f);
    const bool active = (c.flags & HU_CARD_F_ACTIVE) != 0;
    const int alpha = (state == HU_CARD_NOT_PICKED || state == HU_CARD_FAINTED) ? 150 : 255;

    dl->AddRectFilled(p0, p1, active ? Rgba(0x28, 0x34, 0x58, 0xF0) : Rgba(0x14, 0x18, 0x28, 0xE8), h * 0.12f);
    if (active) dl->AddRect(p0, p1, Rgba(0xF8, 0xD8, 0x30, 0xFF), h * 0.12f, 0, std::max(1.0f, h * 0.025f));

    // the model (a placeholder tile for now, tinted as the model will be)
    const float m = std::min(h - 2 * pad, w * 0.34f);
    const ImVec2 m0(p0.x + pad, p0.y + (h - m) / 2), m1(m0.x + m, m0.y + m);
    const hu_model_look look = hu_card_model_look(&c, kFaintFrames);
    dl->AddRectFilled(m0, m1, Mix(hu_rgb{ 0x70, 0x88, 0xB8 }, look.tint, look.tint_amount, 0xFF), m * 0.18f);
    if (c.flags & HU_CARD_F_SUBSTITUTE) {
        dl->AddRect(m0, m1, Rgba(0xFF, 0xFF, 0xFF, 0xC0), m * 0.18f, 0, std::max(1.0f, m * 0.05f));
    }
    {
        char dex[8];
        snprintf(dex, sizeof dex, "%u", c.model_species);
        const float ts = m * 0.32f;
        const ImVec2 d = f->CalcTextSizeA(ts, FLT_MAX, 0, dex);
        dl->AddText(f, ts, ImVec2((m0.x + m1.x - d.x) / 2, (m0.y + m1.y - d.y) / 2), Rgba(0xFF, 0xFF, 0xFF, 0xB0), dex);
    }

    // the text column: name, HP, the moves two by two, speed
    const float x = m1.x + pad, tw = p1.x - pad - x;
    const float row = (h - 2 * pad) / 5.0f, ts = row * 0.86f;
    float y = p0.y + pad;
    const ImU32 white = Rgba(0xFF, 0xFF, 0xFF, alpha);
    {
        char lv[8];
        snprintf(lv, sizeof lv, "L%u", c.level);
        const float lw = f->CalcTextSizeA(ts * 0.8f, FLT_MAX, 0, lv).x;
        Text(dl, f, x, y, ts, tw - lw - pad, v.names[s][i], white);
        dl->AddText(f, ts * 0.8f, ImVec2(p1.x - pad - lw, y + ts * 0.15f), Rgba(0xD0, 0xD0, 0xD0, alpha), lv);
    }
    y += row;
    // HP: the Game Boy's bar colours; the numbers only for the side whose HP the player may know
    {
        const float bh = row * 0.42f;
        char hp[16];
        snprintf(hp, sizeof hp, "%u/%u", c.hp, c.max_hp);
        const float hs = ts * 0.75f;
        const float hw = f->CalcTextSizeA(hs, FLT_MAX, 0, hp).x;
        // the numbers keep their room; the bar takes the rest (less the status dot)
        const float bw = own ? std::max(tw * 0.3f, tw - hw - bh * 2.2f) : tw - bh * 2.2f;
        const ImVec2 b0(x, y + (row - bh) / 2), b1(x + bw, b0.y + bh);
        dl->AddRectFilled(b0, b1, Rgba(0x30, 0x30, 0x30, alpha), bh / 2);
        const int fill = hu_hp_fill(c.hp, c.max_hp, 480);
        if (fill > 0) {
            static const ImU32 bands[3] = { IM_COL32(0x48, 0xD8, 0x60, 0xFF), IM_COL32(0xF8, 0xD0, 0x30, 0xFF),
                                            IM_COL32(0xF0, 0x48, 0x38, 0xFF) };
            dl->AddRectFilled(b0, ImVec2(b0.x + bw * fill / 480.0f, b1.y), bands[hu_hp_band_of(c.hp, c.max_hp)],
                              bh / 2);
        }
        if (c.status != HU_STATUS_NONE) { // the status colour as a dot after the bar
            const hu_rgb sc = hu_status_color((hu_status)c.status);
            dl->AddCircleFilled(ImVec2(b1.x + bh * 0.9f, (b0.y + b1.y) / 2), bh * 0.6f, Rgba(sc.r, sc.g, sc.b, 0xFF));
        }
        if (own) {
            Text(dl, f, p1.x - pad - hw, y + (row - hs) / 2, hs, hw + 1, hp, white);
        }
    }
    y += row;
    for (int k = 0; k < HU_CARD_MOVES; k++) {
        const float kx = x + (k & 1) * (tw / 2), ky = y + (k >> 1) * row;
        if (hu_card_move_visible(&c, k)) {
            if (c.moves[k] == 0) continue; // an empty slot
            const hu_rgb tc = hu_type_text_color((hu_type)v.moveTypes[s][i][k]);
            Text(dl, f, kx, ky, ts * 0.9f, tw / 2 - pad, v.moves[s][i][k], Rgba(tc.r, tc.g, tc.b, alpha));
        } else if (c.moves[k] != 0 || !own) {
            Text(dl, f, kx, ky, ts * 0.9f, tw / 2 - pad, "-", Rgba(0xFF, 0xFF, 0xFF, 0x70));
        }
    }
    y += 2 * row;
    {
        const hu_speed_range r = v.speed[s][i];
        char sp[48];
        const char* label = Str("PORT_MEMORY_SPEED", "すばやさ", "Spe");
        if (r.min == r.max) snprintf(sp, sizeof sp, "%s %u", label, r.min);
        else snprintf(sp, sizeof sp, "%s %u-%u", label, r.min, r.max);
        Text(dl, f, x, y, ts * 0.85f, tw, sp, Rgba(0x90, 0xD8, 0xFF, alpha));
    }
}

class SummaryBarsWindow : public Ship::GuiWindow {
  public:
    SummaryBarsWindow() : GuiWindow("gEnhancements.SummaryBarsOverlay", "Summary bars") {
    }
    void Draw() override {
        if (!Port_Enh_SummaryBarsEnabled()) return;
        PortEnhBattleView v;
        if (!Port_Enh_LatestBattle(&v)) return;
        const ImGuiViewport* vp = ImGui::GetMainViewport();
        const float W = vp->Size.x, H = vp->Size.y;
        if (W <= 0 || H <= 0) return;
        const float aspect = Port::AspectSetting() == 1 ? 16.0f / 9.0f : 4.0f / 3.0f;
        const float picW = std::min(W, H * aspect);
        const float side = (W - picW) / 2;
        if (side < kMinSide * H) return; // no room beside the picture: no bars

        ImDrawList* dl = ImGui::GetBackgroundDrawList(); // under ImGui's own windows (menus), over the black bars
        ImFont* f = Port_Enh_CardFont();
        const float margin = H * 0.012f, gap = H * 0.008f;
        const float cw = std::min(side - 2 * margin, H * 0.34f);
        const float ch = (H - 2 * margin - (HU_SIDE_MAX - 1) * gap) / HU_SIDE_MAX;
        for (int s = 0; s < 2; s++) {
            const hu_side& sd = v.sides[s];
            // each column sits in the middle of its band beside the picture
            const float x0 = vp->Pos.x + (s == 0 ? (side - cw) / 2 : W - side + (side - cw) / 2);
            for (int i = 0; i < sd.count && i < HU_SIDE_MAX; i++) {
                const float y0 = vp->Pos.y + margin + i * (ch + gap);
                DrawCard(dl, f, v, s, i, ImVec2(x0, y0), ImVec2(x0 + cw, y0 + ch));
            }
        }
    }

  protected:
    void DrawElement() override {
    }
    void UpdateElement() override {
    }
};

} // namespace

void Port_Enh_InstallOverlay(void* gui) {
    static bool added = false;
    if (added || gui == nullptr) return;
    added = true;
    Port_Enh_LoadFonts();
    auto w = std::make_shared<SummaryBarsWindow>();
    static_cast<Ship::Gui*>(gui)->AddGuiWindow(w);
    w->Show();
}
