// The Esc menu: libultraship's Gui menu window (Gui::SetMenu), which Esc (or the controller's menu button with
// controller navigation on) shows and hides while the game runs. For now it holds the settings that the
// enhancements' specs put in the Esc menu as well as the launcher: DV and Stat Exp difficulty
// (DV_STATEXP_DIFFICULTY). It is set once the launcher has closed, so Esc keeps closing the launcher's own pop-ups.
#include "enhancements.h"

#include <libultraship/libultraship.h>
#include <ship/window/gui/Gui.h>
#include <ship/window/gui/GuiWindow.h>
#include <imgui.h>

#include <algorithm>
#include <cstdlib>
#include <memory>

namespace {

const ImVec4 kError(0.90f, 0.28f, 0.30f, 1.0f);
const ImVec4 kDim(0.55f, 0.58f, 0.64f, 1.0f);

class EscMenuWindow : public Ship::GuiWindow {
  public:
    // no visibility CVar: the menu starts hidden on every run
    EscMenuWindow() : GuiWindow("", "Menu") {
    }

    void Draw() override {
        if (!IsVisible()) return;
        const ImGuiViewport* vp = ImGui::GetMainViewport();
        const float scale = std::max(1.0f, vp->Size.y / 720.0f);
        ImGui::SetNextWindowPos(ImVec2(vp->Pos.x + vp->Size.x / 2, vp->Pos.y + vp->Size.y / 2), ImGuiCond_Always,
                                ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(560 * scale, 0), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.94f);
        bool open = true;
        if (ImGui::Begin("Menu##port-esc", &open,
                         ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
                             ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::SetWindowFontScale(scale);
            ImGui::TextColored(kDim, "Esc closes this menu. The game keeps running.");
            Port_Enh_StatDifficultyControls();
            ImGui::Spacing();
            if (ImGui::Button("Close")) open = false;
        }
        ImGui::End();
        if (!open) Hide();
    }

  protected:
    void DrawElement() override {
    }
    void UpdateElement() override {
    }
};

} // namespace

extern "C" void Port_Enh_StatDifficultyControls(void) {
    static const char* const kNames[2] = { "Rental Pokémon", "Opponent Pokémon" };
    static const char* const kEnv[2] = { "PORT_STAT_DIFFICULTY_RENTAL", "PORT_STAT_DIFFICULTY_OPPONENT" };
    ImGui::SeparatorText("DV and Stat Exp difficulty");
    ImGui::TextColored(kDim, "Vanilla keeps the game's own values. Rentals change the next time the entry\n"
                             "screen opens, opponents as each battle is set up.");
    for (int g = 0; g < 2; g++) {
        ImGui::PushID(g);
        hu_sd_range r;
        Port_Enh_StatDifficulty(g, &r);
        bool changed = false;
        ImGui::Spacing();
        ImGui::TextUnformatted(kNames[g]);
        ImGui::SameLine(ImGui::GetFontSize() * 12);
        int mode = r.custom ? 1 : 0;
        changed |= ImGui::RadioButton("Vanilla", &mode, 0);
        ImGui::SameLine();
        changed |= ImGui::RadioButton("Custom", &mode, 1);
        r.custom = mode == 1;
        if (r.custom) {
            int v[4] = { r.dv_min, r.dv_max, r.stat_exp_min, r.stat_exp_max };
            ImGui::PushItemWidth(-ImGui::GetFontSize() * 10);
            changed |= ImGui::SliderInt("Lowest DV", &v[0], 0, 15, "%d", ImGuiSliderFlags_AlwaysClamp);
            changed |= ImGui::SliderInt("Highest DV", &v[1], 0, 15, "%d", ImGuiSliderFlags_AlwaysClamp);
            changed |= ImGui::SliderInt("Lowest Stat Exp", &v[2], 0, 65535, "%d", ImGuiSliderFlags_AlwaysClamp);
            changed |= ImGui::SliderInt("Highest Stat Exp", &v[3], 0, 65535, "%d", ImGuiSliderFlags_AlwaysClamp);
            ImGui::PopItemWidth();
            r.dv_min = (uint8_t)v[0];
            r.dv_max = (uint8_t)v[1];
            r.stat_exp_min = (uint16_t)v[2];
            r.stat_exp_max = (uint16_t)v[3];
            const unsigned errors = hu_sd_errors(&r);
            for (unsigned bit = 1; bit <= HU_SD_DV_TOO_HIGH; bit <<= 1) {
                if (errors & bit) ImGui::TextColored(kError, "%s", hu_sd_error_text(bit));
            }
            if (errors) ImGui::TextColored(kError, "Vanilla values are used until this is fixed.");
        }
        const char* env = getenv(kEnv[g]);
        if (env != nullptr && *env != '\0') {
            ImGui::PushStyleColor(ImGuiCol_Text, kDim);
            ImGui::TextWrapped("(%s=%s is set and wins over this setting)", kEnv[g], env);
            ImGui::PopStyleColor();
        }
        if (changed) Port_Enh_SetStatDifficulty(g, &r);
        ImGui::PopID();
    }
}

extern "C" void Port_Enh_InstallEscMenu(void* gui) {
    static bool added = false;
    if (added || gui == nullptr) return;
    added = true;
    auto menu = std::make_shared<EscMenuWindow>();
    static_cast<Ship::Gui*>(gui)->SetMenu(menu);
    if (getenv("PORT_ESC_MENU_OPEN") != nullptr) menu->Show(); // test hook: start with the menu shown
}
