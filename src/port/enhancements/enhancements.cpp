// Settings and the trainer memory file of the hippodrome-utils enhancements (enhancements.h).
#include "enhancements.h"

#include <hippodrome/hue_choice.h>
#include <hippodrome/trainer_memory.h>
#include <libultraship/libultraship.h>
#include <imgui.h>

#include "port/app_file.h"

#include <chrono>
#include <cstdio>
#include <mutex>
#include <filesystem>
#include <cstdlib>
#include <string>

extern "C" const char* Port_SramFilePath(void); // os_devices.cpp

namespace {

int EnvOverride(const char* name) {
    const char* v = getenv(name);
    if (v == nullptr || *v == '\0') return -1;
    return atoi(v) != 0 ? 1 : 0;
}

hu_tm* gMemory = nullptr;
std::string gMemoryPath;
hu_hue_store* gHues = nullptr;
std::string gHuePath;

// keeps a damaged side file for inspection; the next save starts a fresh one (paths are UTF-8:
// std::filesystem::u8path keeps a non-ASCII folder working on Windows)
void KeepDamaged(const char* path, int code) {
    std::error_code ec;
    const auto from = std::filesystem::u8path(path);
    const auto to = std::filesystem::u8path(std::string(path) + ".bad");
    std::filesystem::remove(to, ec);
    std::filesystem::rename(from, to, ec);
    fprintf(stderr, "[enhancements] %s could not be read (%d), kept as %s.bad\n", path, code, path);
}

int Setting(const char* env_name, const char* cvar) {
    const int env = EnvOverride(env_name);
    return env >= 0 ? env : CVarGetInteger(cvar, 0) != 0;
}

void SetSetting(const char* cvar, int on) {
    CVarSetInteger(cvar, on ? 1 : 0);
    CVarSave();
}

} // namespace

extern "C" int Port_Enh_SummaryBarsEnabled(void) {
    static const int env = EnvOverride("PORT_SUMMARY_BARS");
    return env >= 0 ? env : CVarGetInteger("gEnhancements.SummaryBars", 0) != 0;
}

extern "C" void Port_Enh_SetSummaryBarsEnabled(int on) {
    CVarSetInteger("gEnhancements.SummaryBars", on ? 1 : 0);
    CVarSave();
}

extern "C" int Port_Enh_TrainerMemoryEnabled(void) {
    static const int env = EnvOverride("PORT_TRAINER_MEMORY");
    return env >= 0 ? env : CVarGetInteger("gEnhancements.TrainerMemory", 0) != 0;
}

extern "C" void Port_Enh_SetTrainerMemoryEnabled(int on) {
    CVarSetInteger("gEnhancements.TrainerMemory", on ? 1 : 0);
    CVarSave();
}

extern "C" int Port_Enh_HueChoiceEnabled(void) {
    return Setting("PORT_HUE_CHOICE", "gEnhancements.HueChoice");
}

extern "C" void Port_Enh_SetHueChoiceEnabled(int on) {
    SetSetting("gEnhancements.HueChoice", on);
}

extern "C" int Port_Enh_MoveDescriptionsEnabled(void) {
    return Setting("PORT_MOVE_DESCRIPTIONS", "gEnhancements.MoveDescriptions");
}

extern "C" void Port_Enh_SetMoveDescriptionsEnabled(int on) {
    SetSetting("gEnhancements.MoveDescriptions", on);
}

extern "C" int Port_Enh_OrderQueueEnabled(void) {
    return Setting("PORT_ORDER_QUEUE", "gEnhancements.OrderQueue");
}

extern "C" void Port_Enh_SetOrderQueueEnabled(int on) {
    SetSetting("gEnhancements.OrderQueue", on);
}

extern "C" hu_hue_store* Port_Enh_Hues(void) {
    if (gHues != nullptr) {
        return gHues;
    }
    gHues = hu_hue_store_new();
    if (gHuePath.empty()) {
        gHuePath = std::string(Port_SramFilePath()) + ".hue";
    }
    hu_hue_io r = hu_hue_load(gHues, gHuePath.c_str());
    if (r == HU_HUE_CORRUPT || r == HU_HUE_IO_ERROR) {
        KeepDamaged(gHuePath.c_str(), (int)r);
    }
    return gHues;
}

extern "C" void Port_Enh_SaveHues(void) {
    if (gHues == nullptr || !hu_hue_dirty(gHues)) {
        return;
    }
    if (hu_hue_save(gHues, gHuePath.c_str()) != HU_HUE_OK) {
        fprintf(stderr, "[enhancements] could not write %s\n", gHuePath.c_str());
    }
}

extern "C" const char* Port_Enh_MemoryPath(void) {
    if (gMemoryPath.empty()) {
        gMemoryPath = std::string(Port_SramFilePath()) + ".memory";
    }
    return gMemoryPath.c_str();
}

extern "C" hu_tm* Port_Enh_Memory(void) {
    if (gMemory != nullptr) {
        return gMemory;
    }
    gMemory = hu_tm_new();
    const char* path = Port_Enh_MemoryPath();
    hu_tm_io r = hu_tm_load(gMemory, path);
    if (r == HU_TM_CORRUPT || r == HU_TM_IO_ERROR) {
        KeepDamaged(path, (int)r);
    }
    return gMemory;
}

extern "C" void Port_Enh_SaveMemory(void) {
    if (gMemory == nullptr || !hu_tm_dirty(gMemory)) {
        return;
    }
    if (hu_tm_save(gMemory, Port_Enh_MemoryPath()) != HU_TM_OK) {
        fprintf(stderr, "[enhancements] could not write %s\n", Port_Enh_MemoryPath());
    }
}

// ---- the summary bars' view: written by the game code each battle frame, read by the overlay when ImGui draws ----

namespace {
std::mutex gViewLock;
PortEnhBattleView gView;
std::chrono::steady_clock::time_point gViewTime;
bool gHasView = false;
} // namespace

extern "C" void Port_Enh_PublishBattle(const PortEnhBattleView* view) {
    std::lock_guard<std::mutex> lock(gViewLock);
    gView = *view;
    gViewTime = std::chrono::steady_clock::now();
    gHasView = true;
}

extern "C" int Port_Enh_LatestBattle(PortEnhBattleView* out) {
    std::lock_guard<std::mutex> lock(gViewLock);
    // the battle publishes every frame it draws its HUD; a quarter second without one means it is over (or paused
    // outside the battle scene), so the bars go away with it
    if (!gHasView || std::chrono::steady_clock::now() - gViewTime > std::chrono::milliseconds(250)) {
        return 0;
    }
    *out = gView;
    return 1;
}

// ---- the overlay font ----

namespace {
ImFont* gCardFont = nullptr;
std::string gCardFontData; // ImGui reads the TTF when it builds the atlas, so the bytes stay alive
} // namespace

extern "C" void Port_Enh_LoadFonts(void) {
    if (gCardFont != nullptr || ImGui::GetCurrentContext() == nullptr) {
        return;
    }
    // the port's own rounded font (assets/font/pop_subset.ttf: Latin, kana, the game's kanji, Hangul; OFL). Only the
    // glyphs the subset has are rasterized, so the wide ranges cost nothing for the ones it lacks.
    if (!Port::ReadAppFile("assets/font/pop_subset.ttf", gCardFontData) || gCardFontData.empty()) {
        fprintf(stderr, "[enhancements] assets/font/pop_subset.ttf missing: the summary bars use ImGui's font\n");
        return;
    }
    ImGuiIO& io = ImGui::GetIO();
    static ImVector<ImWchar> ranges;
    ImFontGlyphRangesBuilder b;
    b.AddRanges(io.Fonts->GetGlyphRangesDefault());
    b.AddRanges(io.Fonts->GetGlyphRangesJapanese());
    b.AddRanges(io.Fonts->GetGlyphRangesKorean());
    b.AddRanges(io.Fonts->GetGlyphRangesCyrillic());
    static const ImWchar extra[] = { 0x2010, 0x2027, 0x3000, 0x30FF, 0xFF00, 0xFFEF, 0 };
    b.AddRanges(extra);
    b.BuildRanges(&ranges);
    ImFontConfig cfg;
    cfg.FontDataOwnedByAtlas = false;
    cfg.OversampleH = 1;
    cfg.OversampleV = 1;
    snprintf(cfg.Name, sizeof cfg.Name, "plethron-pop");
    // rasterized large once and drawn smaller (the atlas is built once; window sizes vary)
    gCardFont = io.Fonts->AddFontFromMemoryTTF(gCardFontData.data(), (int)gCardFontData.size(), 40.0f, &cfg,
                                               ranges.Data);
}

ImFont* Port_Enh_CardFont() {
    return gCardFont != nullptr ? gCardFont : ImGui::GetFont();
}
