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
#include <fast/Fast3dWindow.h>
#include <fast/interpreter.h>
#include <ship/window/gui/Gui.h>
#include <ship/window/gui/GuiWindow.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <memory>

namespace Port {
int AspectSetting(); // Engine.cpp: 0 = 4:3, 1 = 16:9
std::shared_ptr<Fast::Fast3dWindow> GetWindow(); // Engine.cpp
}
ImFont* Port_Enh_CardFont(); // enhancements.cpp

namespace {

// the cards need at least this much room beside the picture (a fraction of the window's height): a 4:3 picture in
// a 16:9 window leaves 0.22, a 16:9 picture in a 21:9 window 0.29, a 4:3 picture in a 16:10 window only 0.13 (no bars)
constexpr float kMinSide = 0.18f;

// the cards' model picture (card_models.c draws into it): created on the render thread, read by the game thread
// One picture per side (a row of cells each): a single two-row picture came out with its rows swapped on one
// renderer and not the other (Linux GL vs Windows DirectX 11), as the cells' heights are placed per renderer.
std::atomic<int> gCardFb[2] = { 0, 0 };
ImTextureID gCardTexture[2] = { 0, 0 };

void EnsureCardPicture() {
    for (int s = 0; s < 2; s++) {
        if (gCardFb[s].load() > 0) {
            auto window = Port::GetWindow();
            auto interp = window ? window->GetInterpreterWeak().lock() : nullptr;
            if (interp) {
                gCardTexture[s] = (ImTextureID)interp->GetCurrentRenderingAPI()->GetFramebufferTextureId(gCardFb[s].load());
            }
            continue;
        }
        // fixed size, no widescreen adjustment: the game's coordinates are the picture's pixels
        int fb = gfx_create_framebuffer(PORT_ENH_CARD_ATLAS_W, PORT_ENH_CARD_CELL, PORT_ENH_CARD_ATLAS_W,
                                        PORT_ENH_CARD_CELL, 0, false);
        if (fb > 0) gCardFb[s].store(fb);
    }
}

ImU32 Rgba(int r, int g, int b, int a) {
    return IM_COL32(std::clamp(r, 0, 255), std::clamp(g, 0, 255), std::clamp(b, 0, 255), std::clamp(a, 0, 255));
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

// Where the face is in a large Pokémon's cell (as a fraction of the cell) and how far to zoom in on it. Zero's
// model framing (its preview scene) shows them whole, their faces small.
void FaceFrame(int species, float* zoom, float* cx, float* cy) {
    struct Frame {
        int species;
        float zoom, cx, cy;
    };
    static const Frame kFrames[] = {
        { 95, 1.9f, 0.62f, 0.36f },  // Onix
        { 103, 1.6f, 0.50f, 0.36f }, // Exeggutor
        { 131, 1.5f, 0.48f, 0.34f }, // Lapras
    };
    for (const Frame& f : kFrames) {
        if (f.species == species) {
            *zoom = f.zoom;
            *cx = f.cx;
            *cy = f.cy;
            return;
        }
    }
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

    // the model: its cell of the card picture (card_models.c), on the card's outer side (the left trainer's on the
    // left, the right trainer's on the right), the text column beside it
    const bool right = (s == 1);
    const float m = std::min(h - 2 * pad, w * 0.34f);
    const ImVec2 m0(right ? p1.x - pad - m : p0.x + pad, p0.y + (h - m) / 2), m1(m0.x + m, m0.y + m);
    dl->AddRectFilled(m0, m1, Rgba(0x30, 0x3C, 0x60, 0xFF), m * 0.18f);
    if (v.modelReady[s][i] && gCardTexture[s]) {
        // large Pokémon: the part of the cell with the face, while it has not fainted (the spec's "Model")
        float zoom = 1.0f, cx = 0.5f, cy = 0.5f;
        if (state != HU_CARD_FAINTED) FaceFrame(v.modelSpecies[s][i], &zoom, &cx, &cy);
        const float half = 0.5f / zoom;
        const float x0 = std::clamp(cx - half, 0.0f, 1.0f - 2 * half), y0 = std::clamp(cy - half, 0.0f, 1.0f - 2 * half);
        const float cu = 1.0f / HU_SIDE_MAX;
        // the models face a little to the left; the right trainer's cards show theirs mirrored, so they face right (a
        // mirror image of the same pose, as the card itself is a mirror image of the left trainer's)
        const float u0 = (i + x0) * cu, u1 = (i + x0 + 2 * half) * cu;
        const ImVec2 uv0(right ? u1 : u0, y0), uv1(right ? u0 : u1, y0 + 2 * half);
        // (its tint is the model's own: card_models.c blends it in as the game blends a model toward a colour)
        dl->AddImageRounded(gCardTexture[s], m0, m1, uv0, uv1, IM_COL32_WHITE, m * 0.18f);
    } else {
        char dex[8];
        snprintf(dex, sizeof dex, "%u", c.model_species);
        const float ts = m * 0.32f;
        const ImVec2 d = f->CalcTextSizeA(ts, FLT_MAX, 0, dex);
        dl->AddText(f, ts, ImVec2((m0.x + m1.x - d.x) / 2, (m0.y + m1.y - d.y) / 2), Rgba(0xFF, 0xFF, 0xFF, 0xB0), dex);
    }
    if (c.flags & HU_CARD_F_SUBSTITUTE) {
        dl->AddRect(m0, m1, Rgba(0xFF, 0xFF, 0xFF, 0xC0), m * 0.18f, 0, std::max(1.0f, m * 0.05f));
    }

    // the text column: name, HP, the moves two by two under it, speed under them
    const float x = right ? p0.x + pad : m1.x + pad, tw = (right ? m0.x : p1.x) - pad - x;
    const float row = (h - 2 * pad) / 5.0f, ts = row * 0.86f;
    float y = p0.y + pad;
    const ImU32 white = Rgba(0xFF, 0xFF, 0xFF, alpha);
    {
        char lv[8];
        snprintf(lv, sizeof lv, "L%u", c.level);
        const float lw = f->CalcTextSizeA(ts * 0.8f, FLT_MAX, 0, lv).x;
        Text(dl, f, x, y, ts, tw - lw - pad, v.names[s][i], white);
        dl->AddText(f, ts * 0.8f, ImVec2(x + tw - lw, y + ts * 0.15f), Rgba(0xD0, 0xD0, 0xD0, alpha), lv);
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
            Text(dl, f, x + tw - hw, y + (row - hs) / 2, hs, hw + 1, hp, white);
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
        EnsureCardPicture();

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

extern "C" int Port_Enh_CardModelFramebuffer(int side) {
    return (side == 0 || side == 1) ? gCardFb[side].load() : 0;
}

void Port_Enh_InstallOverlay(void* gui) {
    static bool added = false;
    if (added || gui == nullptr) return;
    added = true;
    Port_Enh_LoadFonts();
    auto w = std::make_shared<SummaryBarsWindow>();
    static_cast<Ship::Gui*>(gui)->AddGuiWindow(w);
    w->Show();
}
