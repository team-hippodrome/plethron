// The save editor inside the launcher, for Zero's 32 KiB SRAM save (SaveSram, zero_sram.h): overview and repair,
// progress and GB Player unlocks, options, the 16 registered teams, the four N64 boxes, the item storage, the saved
// tournament and the Western Game Boy saves of the Transfer Paks. It edits the save the way Stadium 1's editor page
// did: a dump of the file ("base") and an edited copy ("cur"); Save rebuilds the file from base and commits every
// record that changed (EncodeEdit), keeping the previous file as .bak. The file is written in N64 byte order.
//
// Zero's names are Japanese Game Boy text. The launcher's font has no kana, so they are shown and typed in romaji
// (zero_text.h): capitals for katakana, small letters for hiragana. Japanese Game Boy saves are not edited here; Zero
// edits them itself through Organize (pj64-pokestadium-debug can edit them too).
//
// Everything is plain ImGui in the launcher's look. For the handheld layout every control is a gamepad focus target
// (Style::navItem); lists that would need a drop-down (species, moves, items) open a picker page instead, and the
// Pokémon dialog and questions replace the page, so the focus never lands on something hidden.
#include "save_editor_ui.h"
#include "stadium_save.h"
#include "zero_text.h"
#include "gamedata.h"

#include <SDL3/SDL.h>
#include <imgui_internal.h>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <tuple>

namespace Port {

using namespace stadium_save;
namespace gd = stadium_save::gamedata;

namespace {

// Launcher.swift's colours (as launcher.cpp's Theme)
constexpr uint32_t kCard = 0x1A1F27, kBorder = 0x2A313C, kText = 0xE6E9EE, kDim = 0x8B95A3, kGreen = 0x2EA043,
                   kRed = 0xE5484D, kButton = 0x2A313C, kButtonHover = 0x353D4A, kSelected = 0x23406B, kWarn = 0xD29922,
                   kWindow = 0x12161D;

ImU32 Col(uint32_t rgb, float a = 1.0f) {
    return IM_COL32((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF, (int)(a * 255));
}
ImVec4 ColV(uint32_t rgb) {
    return ImGui::ColorConvertU32ToFloat4(Col(rgb));
}

uint32_t TypeColor(const char* t) {
    static const std::map<std::string, uint32_t> colors = {
        { "Normal", 0x9A9A6E }, { "Fighting", 0xC03028 }, { "Flying", 0x8F7AE0 }, { "Poison", 0xA040A0 },
        { "Ground", 0xD6B45A }, { "Rock", 0xB8A038 },     { "Bug", 0x98A820 },    { "Ghost", 0x705898 },
        { "Fire", 0xF08030 },   { "Water", 0x6890F0 },    { "Grass", 0x5CB041 },  { "Electric", 0xE8B800 },
        { "Psychic", 0xF85888 }, { "Ice", 0x6CC8C8 },     { "Dragon", 0x7038F8 },
    };
    if (t == nullptr) return 0x666666;
    auto it = colors.find(t);
    return it != colors.end() ? it->second : 0x888888;
}

// ImGui's built-in font has Latin-1 only: the gender signs as letters, the PK and MN ligatures as their letters
// a name for the launcher's font: kana as romaji, ♂ ♀ PK MN spelled out
std::string Show(const std::string& text) {
    const std::string s = KanaToRomaji(text);
    std::string out;
    for (size_t i = 0; i < s.size(); i++) {
        if (s.compare(i, 3, "♀") == 0) out += " F", i += 2;
        else if (s.compare(i, 3, "♂") == 0) out += " M", i += 2;
        else if (s.compare(i, 4, "<PK>") == 0) out += "PK", i += 3;
        else if (s.compare(i, 4, "<MN>") == 0) out += "MN", i += 3;
        else out += s[i];
    }
    return out;
}

// A name as it is typed in a text field: the font cannot draw ♂ and ♀, so they are written <M> and <F> (like <PK>,
// <MN>); the codec accepts both spellings.
std::string FieldForm(const std::string& text) {
    const std::string s = KanaToRomaji(text);
    std::string out;
    for (size_t i = 0; i < s.size(); i++) {
        if (s.compare(i, 3, "♀") == 0) out += "<F>", i += 2;
        else if (s.compare(i, 3, "♂") == 0) out += "<M>", i += 2;
        else out += s[i];
    }
    return out;
}

// Characters the Game Boy games (and so the game's names) can hold that a keyboard may not have, for the picker:
// {text inserted, label}. The labels stay in ASCII/Latin-1 for the built-in font.
const std::vector<std::pair<std::string, std::string>>& GbSymbols() {
    static const std::vector<std::pair<std::string, std::string>> v = {
        { "<M>", "<M>  male sign" },     { "<F>", "<F>  female sign" },    { "<PK>", "<PK>  PK" },
        { "<MN>", "<MN>  MN" },          { "é", "é  e acute" },            { "×", "×  multiplication" },
        { "'", "'  apostrophe" },        { "-", "-  hyphen" },             { ".", ".  period" },
        { ",", ",  comma" },             { "/", "/  slash" },              { "?", "?  question mark" },
        { "!", "!  exclamation mark" },  { "(", "(  open parenthesis" },   { ")", ")  close parenthesis" },
        { ":", ":  colon" },             { ";", ";  semicolon" },          { "[", "[  open bracket" },
        { "]", "]  close bracket" },     { "¥", "¥  yen" },                { " ", "(space)" },
        { "à", "à" }, { "è", "è" }, { "ù", "ù" }, { "ç", "ç" }, { "ß", "ß" },
        { "ä", "ä" }, { "ö", "ö" }, { "ü", "ü" }, { "Ä", "Ä" }, { "Ö", "Ö" }, { "Ü", "Ü" },
        { "â", "â" }, { "ê", "ê" }, { "î", "î" }, { "ô", "ô" }, { "û", "û" }, { "ë", "ë" }, { "ï", "ï" },
    };
    return v;
}

// a file's name, as UTF-8
std::string FileNameOf(const std::string& path) {
    const auto u = std::filesystem::u8path(path).filename().u8string();
    return std::string(u.begin(), u.end());
}

std::string Pad(long long v, int width) {
    std::string s = std::to_string(v);
    while ((int)s.size() < width) s = "0" + s;
    return s;
}

int I(const json& v) {
    try {
        return (int)PyInt(v);
    } catch (...) {
        return 0;
    }
}

int SpeciesDex(const json& mon) {
    if (mon.is_object() && mon.contains("species")) {
        try {
            int d = pokemon::SpeciesLookup(mon["species"]);
            return d > 0 && d <= 151 ? d : 0;
        } catch (...) {
        }
    }
    return 0;
}

std::string MonName(const json& m) {
    if (m.contains("species") && m["species"].is_string()) return Show(m["species"].get<std::string>());
    return "Unknown #" + std::to_string(I(m.value("species_index", json(0))));
}

json EmptyList() {
    return json{ { "flags", 0 }, { "name", "" }, { "trainer_id", 0 }, { "entries", json::array() } };
}

// the GB Player's button choices (SaveOptionsRecord.gbPlayerButtons)
const char* const kGbButtons[5] = { "START", "L", "R", "C-Left", "C-Down" };

// a Pokémon's name when it has no nickname: Japanese in Zero's save, English in a Western Game Boy save
std::string DefaultName(int dex, bool western) {
    return dex > 0 && dex <= 151 ? (western ? gd::SPECIES[dex] : JP_SPECIES[dex]) : "";
}

const char* const kStatNames[5] = { "HP", "Attack", "Defense", "Speed", "Special" };

struct Section {
    std::string key, name;
};

} // namespace

struct SaveEditor::Impl {
    struct Gb {
        std::string label, title, kind, path, rom, name;
        json base, cur;
    };
    struct MonCtx {
        std::string title;
        json mon; // null: a new one
        json otName = "レッド";
        int otId = 0, level = 30;
        bool western = false; // a Western Game Boy save's Pokémon (English names of 10 letters)
        std::function<void(const json&)> onDone;
        std::function<void()> onRemove; // empty: no Remove button
    };
    struct MonDlg {
        MonCtx ctx;
        int tab = 0;
        std::string species;
        json nickname = "";
        int level = 30;
        std::string moves[4];
        int ppUps[4] = {};
        json otName = "レッド";
        int otId = 0;
        int dvs[4] = { 15, 15, 15, 15 };
        int se[5] = {};
        json preview;
        std::vector<std::string> warnings;
        std::string error;
    };
    struct Picker {
        std::string title;
        std::vector<std::pair<int, std::string>> items;
        int current = -1;
        std::function<void(int)> onPick;
        char filter[32] = {};
        bool focusFilter = true;
    };
    struct Confirm {
        std::string text, yes;
        std::function<void()> action;
        std::string no = "Cancel";
        std::function<void()> other; // a third button (Close without saving)
        std::string otherLabel;
    };

    bool open = false;
    std::string path, fileName;
    json base, cur;
    std::vector<Gb> gbs;
    bool dirty = false;

    std::string section = "overview";
    int slot = 0, box = 0, ibox = 0, gbTab = 1, gbBox = 0;
    std::vector<std::string> skipped; // Game Boy saves left out (Japanese ones), for the overview
    std::optional<MonDlg> dlg;
    std::optional<Picker> picker;
    std::map<ImGuiID, std::string> pendingSymbol; // a Symbols button pick, taken by its name field next frame
    std::optional<Confirm> confirm;
    std::string toast, error;
    uint64_t toastUntil = 0;
    ImGuiID focusField = 0;
    std::map<ImGuiID, std::string> editBuf;
    SaveEditorStyle style;       // the launcher's, as of the last frame
    const SaveEditorStyle* st = &style;
    std::string scrollTo; // reset the content's scroll on the next frame

    // ---- loading and saving ----

    void Load() {
        base = SaveFile::Load(path).ToDict();
        cur = base;
        for (Gb& g : gbs) {
            g.base = gb::GBSave(ReadFile(g.path)).ToDict();
            g.cur = g.base;
        }
        Changed();
    }

    void Changed() {
        dirty = cur != base;
        for (const Gb& g : gbs) dirty |= g.cur != g.base;
    }

    void Toast(const std::string& t) {
        toast = t;
        toastUntil = SDL_GetTicks() + 3000;
    }

    void Save() {
        try {
            std::vector<std::pair<std::string, Bytes>> writes;
            Bytes disk = ReadSave(path); // (a file in another byte order is rewritten in N64 order)
            if (SaveFile::FromDict(base).data != disk) {
                throw Error(fileName + " changed on disk since it was opened. Discard to load it again.");
            }
            Bytes data = EncodeEdit(base, cur);
            if (data != disk || data != ReadFile(path)) writes.push_back({ path, data });
            for (const Gb& g : gbs) {
                if (g.cur == g.base) continue;
                Bytes gdisk = ReadFile(g.path);
                if (gb::GBSave(gdisk).ToDict() != g.base) {
                    throw Error(g.name + " changed on disk since it was opened. Discard to load it again.");
                }
                Bytes gdata = gb::ApplyEdit(gdisk, g.base, g.cur);
                if (gdata != gdisk) writes.push_back({ g.path, gdata });
            }
            std::string names;
            for (auto& [p, d] : writes) { // all built first: an error above writes nothing
                WriteWithBackup(p, d);
                names += (names.empty() ? "" : " and ") + FileNameOf(p);
            }
            Load();
            Toast(writes.empty() ? "Nothing to save" : "Saved " + names + " (the previous version is kept as .bak)");
            error.clear();
        } catch (const std::exception& e) {
            error = std::string("The save could not be written: ") + e.what();
        }
    }

    void Repair() {
        try {
            SaveFile s(EncodeEdit(base, cur));
            s.Repair();
            cur = s.ToDict();
            Changed();
            Toast("Repaired. Save to keep it.");
        } catch (const std::exception& e) {
            error = e.what();
        }
    }

    // ---- record helpers (editor.html) ----

    json ListRecord(const json& rec) const {
        return IsValidRecord(rec) ? rec : EmptyList();
    }

    // edit a list record; an unreadable one starts from the game's reset state
    void EditList(json& rec, const std::function<void(json&)>& fn) {
        if (!IsValidRecord(rec)) {
            rec = EmptyList();
            rec["_footer"] = "valid";
        }
        fn(rec);
        rec.erase("count_raw");
        rec.erase("_unused_entries");
        Changed();
    }

    static bool TeamRegistered(const json& t) {
        return IsValidRecord(t) && (I(t["flags"]) & 1) && !t["entries"].empty();
    }

    std::vector<std::pair<std::string, json*>> AllRecords() {
        std::vector<std::pair<std::string, json*>> out;
        for (const RecordRef& ref : Records()) out.push_back({ ref.key(), &GetPath(cur, ref.path) });
        return out;
    }

    int RegisteredTeams() {
        int n = 0;
        for (const json& t : cur["teams"]) n += TeamRegistered(t);
        return n;
    }
    int BoxedCount() {
        int n = 0;
        for (const json& b : cur["boxes"]) n += (int)ListRecord(b)["entries"].size();
        return n;
    }
    int ItemCount() {
        int n = 0;
        for (const json& b : cur["item_boxes"]) n += (int)ListRecord(b)["entries"].size();
        return n;
    }

    // ---- widgets ----

    float S(float v) const { return v * st->scale; }
    float RowH() const { return st->handheld ? 38.0f : 24.0f; }
    bool Nav() const { return st->navItem ? st->navItem() : false; }
    void ResetFocus() const {
        if (st->resetFocus) st->resetFocus();
    }

    bool Btn(const char* label, float w = 0, float h = 0) {
        bool clicked = ImGui::Button(label, ImVec2(w > 0 ? S(w) : 0, S(h > 0 ? h : RowH())));
        return Nav() || clicked;
    }

    bool ColorBtn(const char* label, uint32_t color, float w = 0, float h = 0) {
        ImGui::PushStyleColor(ImGuiCol_Button, ColV(color));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ColV(color == kButton ? kButtonHover : color));
        bool c = Btn(label, w, h);
        ImGui::PopStyleColor(2);
        return c;
    }

    bool DangerBtn(const char* label, float w = 0) {
        ImGui::PushStyleColor(ImGuiCol_Text, ColV(0xFF8A8D));
        bool c = Btn(label, w);
        ImGui::PopStyleColor();
        return c;
    }

    // a row of buttons, the chosen one highlighted; the index clicked or -1 (launcher.cpp Choice)
    int Seg(const char* id, const std::vector<std::string>& labels, int chosen, float w = 0) {
        int clicked = -1;
        ImGui::PushID(id);
        for (int i = 0; i < (int)labels.size(); i++) {
            ImGui::PushID(i);
            if (i > 0) SameLineWrap(w > 0 ? S(w) : ImGui::CalcTextSize(labels[i].c_str()).x + S(16));
            if (ColorBtn(labels[i].c_str(), i == chosen ? kSelected : kButton, w)) clicked = i;
            ImGui::PopID();
        }
        ImGui::PopID();
        return clicked;
    }

    // SameLine when the next item of this width still fits
    void SameLineWrap(float nextW) {
        ImGui::SameLine();
        if (ImGui::GetContentRegionAvail().x < nextW) ImGui::NewLine();
    }

    bool Check(const char* label, bool* v) {
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ColV(kButton)); // (fields are darker; a box must stand out)
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ColV(kButtonHover));
        bool changed = ImGui::Checkbox(label, v);
        ImGui::PopStyleColor(2);
        if (Nav()) {
            *v = !*v;
            changed = true;
        }
        return changed;
    }

    void Label(const char* text, float width = 150) {
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(text);
        ImGui::SameLine(S(width));
    }

    void Hint(const std::string& text, uint32_t color = kDim) {
        ImGui::PushStyleColor(ImGuiCol_Text, ColV(color));
        ImGui::PushTextWrapPos(0);
        ImGui::TextUnformatted(text.c_str());
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();
    }

    void Heading(const std::string& text) {
        if (st->game) ImGui::PushFont(st->game);
        ImGui::TextUnformatted(text.c_str());
        if (st->game) ImGui::PopFont();
    }

    // A text field that commits when it loses focus (or Enter); with the gamepad, A starts typing.
    std::optional<std::string> TextField(const char* id, const std::string& value, float w, bool upper, bool digits = false) {
        ImGuiID wid = ImGui::GetID(id);
        std::string& buf = editBuf[wid];
        if (ImGui::GetActiveID() != wid) buf = value;
        if (focusField == wid) {
            ImGui::SetKeyboardFocusHere();
            focusField = 0;
        }
        char tmp[64];
        snprintf(tmp, sizeof(tmp), "%s", buf.c_str());
        ImGui::SetNextItemWidth(S(w));
        ImGuiInputTextFlags flags = (upper ? ImGuiInputTextFlags_CharsUppercase : 0) |
                                    (digits ? ImGuiInputTextFlags_CharsDecimal : 0) | ImGuiInputTextFlags_AutoSelectAll;
        ImGui::InputText(id, tmp, digits ? 8 : sizeof(tmp), flags);
        buf = tmp;
        bool commit = ImGui::IsItemDeactivatedAfterEdit();
        if (Nav()) focusField = wid;
        if (commit) return upper ? Upper(buf) : buf;
        return std::nullopt;
    }

    // A name. Zero's (Japanese, `size` - 1 kana at most) is typed in romaji: capitals for katakana, small letters for
    // hiragana; refuses what the Japanese Game Boy characters cannot write. A Western Game Boy name (western) takes
    // the Western alphabet, with a "Symbols" button for ♂ ♀ PK MN é × and the rest; the field spells ♂ ♀ as <M> <F>.
    std::optional<std::string> NameField(const char* id, const json& value, size_t size = 6, float w = 150, bool western = false) {
        const std::string current = TextOf(value);
        const ImGuiID wid = ImGui::GetID(id);
        std::optional<std::string> t = TextField(id, FieldForm(current), w, false);
        if (western) {
            ImGui::SameLine();
            ImGui::PushID(id);
            if (Btn("Symbols", 64)) {
                std::vector<std::pair<int, std::string>> items;
                for (size_t i = 0; i < GbSymbols().size(); i++) items.push_back({ (int)i, GbSymbols()[i].second });
                OpenPicker("Symbol", std::move(items), -1, [this, wid](int v) { pendingSymbol[wid] = GbSymbols()[v].first; });
            }
            ImGui::PopID();
            auto pend = pendingSymbol.find(wid);
            if (pend != pendingSymbol.end()) {
                t = current + pend->second;
                pendingSymbol.erase(pend);
            }
        } else if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Romaji: capitals for katakana (PIKACHUU), small letters for hiragana (satoshi); - is a long vowel");
        }
        if (!t) return std::nullopt;
        if (western) {
            try {
                // what the game would read back: <M> -> the sign, e.g. lower case where the alphabet has it
                std::string canon = DecodeGb(EncodeGb(*t, size).data(), size);
                error.clear();
                return canon;
            } catch (const std::exception&) {
                error = "\"" + FieldForm(*t) + "\" cannot be written in the Game Boy alphabet or is too long (" + std::to_string(size - 1) +
                        " characters at most). Letters, digits and the Symbols button's characters work; <PK>, <MN>, <M>, <F> and "
                        "<xx> (a byte in hex) can be typed.";
                return std::nullopt;
            }
        }
        try {
            const std::string kana = RomajiToKana(*t);
            std::string canon = DecodeJp(EncodeJp(kana, size, 0x50).data(), size);
            error.clear();
            return canon;
        } catch (const std::exception& e) {
            error = "\"" + *t + "\": " + e.what() + ". Names are up to " + std::to_string(size - 1) +
                    " kana, typed in romaji: capitals for katakana (PIKACHUU), small letters for hiragana (satoshi), - for a "
                    "long vowel, <M> <F> for the gender signs.";
            return std::nullopt;
        }
    }

    // a number: [-] field [+]; the new value when changed
    std::optional<long long> IntField(const char* id, long long value, long long lo, long long hi, float w = 70, long long step = 1) {
        ImGui::PushID(id);
        std::optional<long long> out;
        if (Btn("-", st->handheld ? 38 : 22)) out = value - step;
        ImGui::SameLine(0, S(2));
        if (std::optional<std::string> t = TextField("##n", std::to_string(value), w, false, true)) {
            try {
                out = std::stoll(t->empty() ? "0" : *t);
            } catch (...) {
            }
        }
        ImGui::SameLine(0, S(2));
        if (Btn("+", st->handheld ? 38 : 22)) out = value + step;
        ImGui::PopID();
        if (out) {
            out = std::clamp(*out, lo, hi);
            if (*out == value) out.reset();
        }
        return out;
    }

    // a button showing the current choice; opens the picker
    void PickButton(const char* id, const std::string& shown, float w, std::function<void()> openPicker) {
        ImGui::PushID(id);
        std::string label = shown + "  v";
        if (Btn(label.c_str(), w)) openPicker();
        ImGui::PopID();
    }

    void OpenPicker(const std::string& title, std::vector<std::pair<int, std::string>> items, int current,
                    std::function<void(int)> onPick) {
        Picker p;
        p.title = title;
        p.items = std::move(items);
        p.current = current;
        p.onPick = std::move(onPick);
        picker = std::move(p);
        ResetFocus();
    }

    static std::vector<std::pair<int, std::string>> SpeciesItems(bool withNone = false) {
        std::vector<std::pair<int, std::string>> v;
        if (withNone) v.push_back({ 0, "None" });
        for (int d = 1; d <= 151; d++) v.push_back({ d, "#" + Pad(d, 3) + " " + Show(gd::SPECIES[d]) });
        return v;
    }
    static std::vector<std::pair<int, std::string>> MoveItems() {
        std::vector<std::pair<int, std::string>> v{ { 0, "- no move -" } };
        std::vector<std::pair<int, std::string>> moves;
        for (int m = 1; m <= 165; m++) {
            const char* t = pokemon::TypeName(gd::MOVE_DATA[m].type);
            std::string s = gd::MOVES[m];
            s += std::string("   ") + (t ? t : "") + (gd::MOVE_DATA[m].power ? " " + std::to_string(gd::MOVE_DATA[m].power) : "") +
                 "  " + std::to_string(gd::MOVE_DATA[m].pp) + " PP";
            moves.push_back({ m, s });
        }
        std::sort(moves.begin(), moves.end(), [](auto& a, auto& b) { return a.second < b.second; });
        v.insert(v.end(), moves.begin(), moves.end());
        return v;
    }
    static std::vector<std::pair<int, std::string>> ItemItems() {
        std::vector<std::pair<int, std::string>> v;
        for (int i = 1; i < gd::kItemCount; i++)
            if (gd::ITEMS[i][0]) v.push_back({ i, gd::ITEMS[i] });
        return v;
    }
    static std::string ItemName(int id) {
        return id >= 0 && id < gd::kItemCount && gd::ITEMS[id][0] ? gd::ITEMS[id] : "Unknown item " + std::to_string(id);
    }

    void Ball(ImDrawList* dl, ImVec2 c, float r, const json& mon) {
        int d = SpeciesDex(mon);
        uint32_t c1 = d ? TypeColor(pokemon::TypeName(gd::BASE[d].type1)) : 0x555555;
        uint32_t c2 = d ? TypeColor(pokemon::TypeName(gd::BASE[d].type2)) : 0x555555;
        dl->PathArcTo(c, r, IM_PI, 2 * IM_PI);
        dl->PathFillConvex(Col(c1));
        dl->PathArcTo(c, r, 0, IM_PI);
        dl->PathFillConvex(Col(c2));
        dl->AddCircle(c, r, Col(0x0B0E13), 0, S(1.5f));
    }

    // a Pokémon: ball, nickname, species and level; true when clicked
    bool MonCard(const char* id, const json& m, float w) {
        ImGui::PushID(id);
        const float h = st->handheld ? 52 : 44;
        bool clicked = ImGui::InvisibleButton("card", ImVec2(S(w), S(h)));
        bool hov = ImGui::IsItemHovered();
        ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
        clicked |= Nav();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(a, b, Col(hov ? kButtonHover : kButton), S(6));
        Ball(dl, ImVec2(a.x + S(20), (a.y + b.y) / 2), S(11), m);
        const int dex = SpeciesDex(m);
        std::string nick = Show(TextOf(m["nickname"]));
        const bool plain = nick.empty() || (dex && (TextOf(m["nickname"]) == JP_SPECIES[dex] || TextOf(m["nickname"]) == gd::SPECIES[dex]));
        std::string name = plain ? MonName(m) : nick;
        std::string sub = (!plain ? MonName(m) + " · " : "") + "Lv " + std::to_string(I(m["level"]));
        const float fs = ImGui::GetFontSize();
        dl->PushClipRect(a, b, true);
        dl->AddText(ImGui::GetFont(), fs, ImVec2(a.x + S(38), a.y + (b.y - a.y) / 2 - fs - S(1)), Col(kText), name.c_str());
        dl->AddText(ImGui::GetFont(), fs, ImVec2(a.x + S(38), a.y + (b.y - a.y) / 2 + S(1)), Col(kDim), sub.c_str());
        dl->PopClipRect();
        ImGui::PopID();
        return clicked;
    }

    bool EmptyCard(const char* id, const char* label, float w, bool enabled = true) {
        ImGui::PushID(id);
        const float h = st->handheld ? 52 : 44;
        bool clicked = false;
        if (enabled) {
            ImGui::PushStyleColor(ImGuiCol_Button, ColV(kCard));
            ImGui::PushStyleColor(ImGuiCol_Border, ColV(kBorder));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, S(1));
            clicked = Btn(label, w, h);
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(2);
        } else {
            ImGui::Dummy(ImVec2(S(w), S(h)));
            ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
            ImGui::GetWindowDrawList()->AddRect(a, b, Col(kBorder, 0.5f), S(6));
        }
        ImGui::PopID();
        return clicked;
    }

    // a clickable summary card (overview)
    bool InfoCard(const char* id, const std::string& title, const std::string& value, const std::string& sub, float w) {
        ImGui::PushID(id);
        bool clicked = ImGui::InvisibleButton("c", ImVec2(S(w), S(70)));
        bool hov = ImGui::IsItemHovered();
        clicked |= Nav();
        ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(a, b, Col(hov ? kButtonHover : kButton), S(6));
        const float fs = ImGui::GetFontSize();
        dl->PushClipRect(a, b, true);
        dl->AddText(ImGui::GetFont(), fs, ImVec2(a.x + S(10), a.y + S(7)), Col(kDim), title.c_str());
        ImFont* big = st->game ? st->game : ImGui::GetFont();
        dl->AddText(big, big->FontSize * big->Scale * st->scale, ImVec2(a.x + S(10), a.y + S(24)), Col(kText), value.c_str());
        dl->AddText(ImGui::GetFont(), fs, ImVec2(a.x + S(10), a.y + S(50)), Col(kDim), sub.c_str());
        dl->PopClipRect();
        ImGui::PopID();
        return clicked;
    }

    // a grid of cells of width w: SameLine unless the row is full
    void GridNext(int i, float w) {
        if (i == 0) return;
        ImGui::SameLine();
        if (ImGui::GetContentRegionAvail().x < S(w)) ImGui::NewLine();
    }

    // ---- the Pokémon dialog (editor.html openMon) ----

    void OpenMon(MonCtx ctx) {
        MonDlg d;
        const json& m = ctx.mon;
        if (!m.is_null()) {
            int dex = SpeciesDex(m);
            d.species = dex ? gd::SPECIES[dex] : "";
            d.nickname = m["nickname"];
            d.level = I(m["level"]);
            for (int i = 0; i < 4; i++) {
                const json& mv = m["moves"][i];
                if (mv.is_null()) continue;
                int id = 0;
                try {
                    id = pokemon::MoveLookup(mv["move"]);
                } catch (...) {
                }
                d.moves[i] = id > 0 && id <= 165 ? gd::MOVES[id] : "";
                d.ppUps[i] = I(mv["pp_up"]);
            }
            d.otName = m["ot_name"];
            d.otId = I(m["ot_id"]);
            const char* dvk[4] = { "atk", "def", "spd", "spc" };
            for (int i = 0; i < 4; i++) d.dvs[i] = I(m["dvs"][dvk[i]]);
            for (int i = 0; i < 5; i++) d.se[i] = I(m["stat_exp"][pokemon::STAT_KEYS[i]]);
            d.preview = m;
        } else {
            d.level = ctx.level;
            d.otName = ctx.otName;
            d.otId = ctx.otId;
        }
        d.ctx = std::move(ctx);
        dlg = std::move(d);
        ResetFocus();
        if (!dlg->species.empty()) RefreshPreview();
    }

    json MonRequest() const {
        const MonDlg& d = *dlg;
        json moves = json::array();
        for (int i = 0; i < 4; i++)
            if (!d.moves[i].empty()) moves.push_back({ { "move", d.moves[i] }, { "pp_up", d.ppUps[i] } });
        json dvs = { { "atk", d.dvs[0] }, { "def", d.dvs[1] }, { "spd", d.dvs[2] }, { "spc", d.dvs[3] } };
        json se = json::object();
        for (int i = 0; i < 5; i++) se[pokemon::STAT_KEYS[i]] = d.se[i];
        return json{ { "species", d.species }, { "level", d.level },
                     { "nickname", TextOf(d.nickname).empty() ? json(nullptr) : d.nickname }, { "moves", moves },
                     { "ot_name", d.otName }, { "ot_id", d.otId }, { "dvs", dvs }, { "stat_exp", se },
                     { "western", d.ctx.western } };
    }

    void RefreshPreview() {
        MonDlg& d = *dlg;
        if (d.species.empty()) {
            d.preview = nullptr;
            return;
        }
        try {
            json r = MakeMonRequest(MonRequest());
            d.preview = r["mon"];
            d.warnings.clear();
            for (const json& w : r["warnings"]) d.warnings.push_back(w.get<std::string>());
            d.error.clear();
        } catch (const std::exception& e) {
            d.error = e.what();
        }
    }

    void SetSpecies(int dex) {
        MonDlg& d = *dlg;
        int old = 0;
        for (int i = 1; i <= 151; i++)
            if (d.species == gd::SPECIES[i]) old = i;
        const std::string name = gd::SPECIES[dex];
        if (TextOf(d.nickname).empty() || TextOf(d.nickname) == DefaultName(old, d.ctx.western))
            d.nickname = DefaultName(dex, d.ctx.western);
        d.species = name;
        bool any = false;
        for (const std::string& m : d.moves) any |= !m.empty();
        if (!any) StartingMoves(dex);
        RefreshPreview();
    }

    void StartingMoves(int dex) {
        MonDlg& d = *dlg;
        for (int i = 0; i < 4; i++) {
            int m = gd::BASE[dex].moves[i];
            d.moves[i] = m ? gd::MOVES[m] : "";
            d.ppUps[i] = 0;
        }
    }

    void DrawMonDialog() {
        MonDlg& d = *dlg;
        int dex = 0;
        for (int i = 1; i <= 151; i++)
            if (d.species == gd::SPECIES[i]) dex = i;
        // header: ball, name, species · level, types
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 p = ImGui::GetCursorScreenPos();
        Ball(dl, ImVec2(p.x + S(16), p.y + S(18)), S(15), dex ? json{ { "species", d.species } } : json::object());
        ImGui::SetCursorScreenPos(ImVec2(p.x + S(40), p.y));
        ImGui::BeginGroup();
        std::string title = dex ? (TextOf(d.nickname).empty() ? Show(d.species) : Show(TextOf(d.nickname)))
                                : (d.ctx.mon.is_null() ? "New Pokémon" : MonName(d.ctx.mon));
        Heading(title);
        if (dex) {
            const char* t1 = pokemon::TypeName(gd::BASE[dex].type1);
            const char* t2 = pokemon::TypeName(gd::BASE[dex].type2);
            std::string types = std::string(t1 ? t1 : "") + (t2 && t2 != t1 ? std::string(" / ") + t2 : "");
            Hint("#" + Pad(dex, 3) + " " + Show(d.species) + " · Lv " + std::to_string(d.level) + " · " + types + "   (" +
                 d.ctx.title + ")");
        } else {
            Hint(d.ctx.title);
        }
        ImGui::EndGroup();
        ImGui::Spacing();
        if (st->setDefaultFocus) st->setDefaultFocus(); // (the gamepad starts on the tabs)
        int t = Seg("tabs", { "Pokémon", "Moves", "Stats" }, d.tab, 110);
        if (t >= 0 && t != d.tab) {
            d.tab = t;
            ResetFocus();
        }
        ImGui::Separator();
        ImGui::Spacing();

        bool changed = false;
        if (d.tab == 0) {
            Label("Species");
            PickButton("species", dex ? "#" + Pad(dex, 3) + " " + Show(d.species) : "Choose a species", 240, [this, dex] {
                OpenPicker("Species", SpeciesItems(), dex, [this](int v) { SetSpecies(v); });
            });
            Label("Nickname");
            const size_t nameSize = d.ctx.western ? 11 : 6;
            if (auto v = NameField("##nick", d.nickname, nameSize, 150, d.ctx.western)) {
                d.nickname = v->empty() ? json(DefaultName(dex, d.ctx.western)) : json(*v);
                changed = true;
            }
            Label("Level (1-100)");
            if (auto v = IntField("level", d.level, 1, 100)) d.level = (int)*v, changed = true;
            Label("Original trainer");
            if (auto v = NameField("##ot", d.otName, nameSize, 150, d.ctx.western)) d.otName = *v, changed = true;
            Label("Trainer ID");
            if (auto v = IntField("otid", d.otId, 0, 65535, 80)) d.otId = (int)*v, changed = true;
            ImGui::Spacing();
            Hint(d.ctx.western ? "Names use the Game Boy alphabet: up to 10 capital letters, digits and a few symbols. Experience, "
                                 "HP and PP are filled in to match the level, as for a freshly caught Pokémon."
                               : "Names are up to 5 Japanese characters, typed in romaji (capitals: katakana, small letters: "
                                 "hiragana). Experience, HP and PP are filled in to match the level, as for a freshly caught "
                                 "Pokémon. A Pokémon from a Western cartridge keeps its English names in the save's .western "
                                 "file; changing its names or DVs here makes it a Japanese one.");
        } else if (d.tab == 1) {
            for (int i = 0; i < 4; i++) {
                ImGui::PushID(i);
                int id = 0;
                for (int m = 1; m <= 165; m++)
                    if (d.moves[i] == gd::MOVES[m]) id = m;
                PickButton("move", id ? gd::MOVES[id] : "- no move -", 190, [this, i, id] {
                    OpenPicker("Move " + std::to_string(i + 1), MoveItems(), id, [this, i](int v) {
                        dlg->moves[i] = v ? gd::MOVES[v] : "";
                        if (!v) dlg->ppUps[i] = 0;
                        RefreshPreview();
                    });
                });
                ImGui::SameLine();
                if (id) {
                    const char* tn = pokemon::TypeName(gd::MOVE_DATA[id].type);
                    ImGui::PushStyleColor(ImGuiCol_Text, ColV(TypeColor(tn)));
                    ImGui::AlignTextToFramePadding();
                    ImGui::Text("%-8s", tn ? tn : "");
                    ImGui::PopStyleColor();
                    ImGui::SameLine(S(330));
                    std::vector<std::string> ups;
                    for (int u = 0; u < 4; u++) {
                        int pp = pokemon::MaxPp(id, u);
                        ups.push_back(std::to_string(pp) + " PP" + (u ? " (+" + std::to_string(u) + ")" : ""));
                    }
                    int c = Seg("ppup", ups, d.ppUps[i], 86);
                    if (c >= 0 && c != d.ppUps[i]) d.ppUps[i] = c, changed = true;
                } else {
                    ImGui::NewLine();
                }
                ImGui::PopID();
            }
            ImGui::Spacing();
            ImGui::BeginDisabled(!dex);
            if (Btn("Use starting moves")) {
                StartingMoves(dex);
                changed = true;
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            Hint("Any move can be chosen; the game does not check whether the species can learn it. PP Ups add a fifth of the base PP each.");
        } else {
            const json* stats = d.preview.is_object() && d.preview.contains("_info") ? &d.preview["_info"]["stats"] : nullptr;
            const int hpDv = ((d.dvs[0] & 1) << 3) | ((d.dvs[1] & 1) << 2) | ((d.dvs[2] & 1) << 1) | (d.dvs[3] & 1);
            if (ImGui::BeginTable("stats", 4, ImGuiTableFlags_SizingFixedFit)) {
                ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, S(80));
                ImGui::TableSetupColumn("DV (0-15)", ImGuiTableColumnFlags_WidthFixed, S(st->handheld ? 170 : 130));
                ImGui::TableSetupColumn("Stat Exp (0-65535)", ImGuiTableColumnFlags_WidthFixed, S(st->handheld ? 200 : 160));
                ImGui::TableSetupColumn("Stat", ImGuiTableColumnFlags_WidthFixed, S(60));
                ImGui::TableHeadersRow();
                for (int k = 0; k < 5; k++) {
                    ImGui::PushID(k);
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextUnformatted(kStatNames[k]);
                    ImGui::TableNextColumn();
                    if (k == 0) {
                        ImGui::AlignTextToFramePadding();
                        ImGui::TextDisabled("%d (auto)", hpDv);
                        if (ImGui::IsItemHovered()) ImGui::SetTooltip("The HP DV comes from the other four");
                    } else if (auto v = IntField("dv", d.dvs[k - 1], 0, 15, 40)) {
                        d.dvs[k - 1] = (int)*v;
                        changed = true;
                    }
                    ImGui::TableNextColumn();
                    if (auto v = IntField("se", d.se[k], 0, 65535, 70, 256)) d.se[k] = (int)*v, changed = true;
                    ImGui::TableNextColumn();
                    ImGui::AlignTextToFramePadding();
                    if (stats) ImGui::Text("%d", I((*stats)[pokemon::STAT_KEYS[k]]));
                    else ImGui::TextUnformatted("-");
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
            if (Btn("Perfect stats")) {
                for (int& v : d.dvs) v = 15;
                for (int& v : d.se) v = 65535;
                changed = true;
            }
            ImGui::SameLine();
            if (Btn("No training")) {
                for (int& v : d.se) v = 0;
                changed = true;
            }
            ImGui::SameLine();
            Hint("DVs are Gen 1 \"individual values\"; Stat Exp is the training from battles.");
        }
        if (changed) RefreshPreview();
        ImGui::Spacing();
        if (!d.error.empty()) Hint(d.error, kRed);
        for (const std::string& w : d.warnings) Hint("! " + w, kWarn);
        ImGui::Separator();
        ImGui::Spacing();
        if (d.ctx.onRemove) {
            if (DangerBtn("Remove", 90)) {
                auto fn = d.ctx.onRemove;
                dlg.reset();
                ResetFocus();
                fn();
                return;
            }
            ImGui::SameLine();
        }
        if (Btn("Cancel", 90)) {
            dlg.reset();
            ResetFocus();
            return;
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(!dex || !d.error.empty());
        if (ColorBtn(d.ctx.mon.is_null() ? "Add" : "Done", kGreen, 90)) {
            try {
                json mon = MakeMonRequest(MonRequest())["mon"];
                auto fn = d.ctx.onDone;
                dlg.reset();
                ResetFocus();
                fn(mon);
            } catch (const std::exception& e) {
                d.error = e.what();
            }
        }
        ImGui::EndDisabled();
    }

    // ---- picker and questions (replace the page) ----

    void DrawPicker() {
        Picker& p = *picker;
        Heading(p.title);
        ImGui::SameLine();
        if (st->setDefaultFocus) st->setDefaultFocus(); // (the current choice below takes it when there is one)
        if (Btn("Cancel", 80)) {
            picker.reset();
            ResetFocus();
            return;
        }
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Search");
        ImGui::SameLine();
        if (p.focusFilter && !st->handheld) {
            ImGui::SetKeyboardFocusHere();
            p.focusFilter = false;
        }
        ImGui::SetNextItemWidth(S(220));
        ImGui::InputText("##filter", p.filter, sizeof(p.filter), ImGuiInputTextFlags_CharsUppercase);
        if (Nav()) focusField = ImGui::GetItemID();
        if (focusField == ImGui::GetItemID()) {
            ImGui::ActivateItemByID(focusField);
            focusField = 0;
        }
        ImGui::Spacing();
        ImGui::BeginChild("list", ImVec2(0, 0), false);
        ImGui::SetWindowFontScale(st->scale);
        std::string f = Upper(p.filter);
        const float w = st->handheld ? 220 : 200;
        int n = 0;
        for (auto& [id, name] : p.items) {
            if (!f.empty() && Upper(name).find(f) == std::string::npos) continue;
            ImGui::PushID(id);
            GridNext(n++, w);
            if (id == p.current && st->setDefaultFocus) st->setDefaultFocus();
            if (ColorBtn(name.c_str(), id == p.current ? kSelected : kButton, w)) {
                auto fn = p.onPick;
                int v = id;
                picker.reset();
                ResetFocus();
                ImGui::PopID();
                ImGui::EndChild();
                fn(v);
                return;
            }
            if (id == p.current && ImGui::IsWindowAppearing()) ImGui::SetScrollHereY(0.3f);
            ImGui::PopID();
        }
        if (n == 0) Hint("Nothing matches.");
        ImGui::EndChild();
    }

    void Ask(const std::string& text, const std::string& yes, std::function<void()> action) {
        confirm = Confirm{ text, yes, std::move(action) };
        ResetFocus();
    }

    void DrawConfirm() {
        Confirm& c = *confirm;
        ImGui::Spacing();
        Heading(c.text);
        ImGui::Spacing();
        if (st->setDefaultFocus) st->setDefaultFocus();
        if (Btn(c.no.c_str(), 140)) {
            confirm.reset();
            ResetFocus();
            return;
        }
        ImGui::SameLine();
        if (c.other) {
            if (Btn(c.otherLabel.c_str(), 180)) {
                auto fn = c.other;
                confirm.reset();
                ResetFocus();
                fn();
                return;
            }
            ImGui::SameLine();
        }
        if (ColorBtn(c.yes.c_str(), kSelected, 140)) {
            auto fn = c.action;
            confirm.reset();
            ResetFocus();
            fn();
        }
    }

    // ---- pages ----

    // SaveOptionsRecord.progress (Save_GetProgressFlags): high byte bit 0 Lv 50-55 tournament won, bit 1 Master Ball
    // Cup won; low byte the Lv 1-30 cups cleared (0-4)
    static constexpr int kStadiumWon = 0x100, kMasterWon = 0x200;
    static std::string ProgressLine(int p) {
        std::string s = std::to_string(p & 0xFF) + " of 4 cups";
        if (p & kMasterWon) s += ", Master Ball Cup won";
        if (p & kStadiumWon) s += ", Lv 50-55 won";
        return s;
    }
    static std::string RunTitle(const json& c) {
        const json& se = c["session"];
        int mode = I(se["mode"]);
        const char* mn = ModeName(mode);
        std::string s = mn ? mn : "Mode " + std::to_string(mode);
        int cup = I(se["cup"]);
        if (mode == 2) s += std::string(" · ") + (cup >= 0 && cup < 4 ? CUP_NAMES[cup] : "cup " + std::to_string(cup));
        s += " · battle " + std::to_string(I(se["round"]));
        return s;
    }

    void UnlockAll() {
        json& o = cur["options"];
        o["progress"] = 4 | kMasterWon | kStadiumWon;
        o["gb_player_frames"] = 2;
        o["flags"] = I(o["flags"]) | 1;
        Changed();
    }

    void Go(const std::string& sec) {
        section = sec;
        scrollTo = sec;
        ResetFocus();
    }


    void PageOverview() {
        int bad = 0, total = 0;
        for (auto& [key, rec] : AllRecords()) {
            total++;
            bad += !IsValidRecord(*rec) || rec->contains("_backup");
        }
        Heading("Overview");
        Hint("What is in this save. Pick a card to edit that part.");
        ImGui::Spacing();
        if (bad == 0) {
            Hint("This save is healthy: every part of it is readable and both copies agree.", kGreen);
        } else {
            Hint(std::to_string(bad) + " of " + std::to_string(total) +
                     " parts of this save are unreadable or out of sync. The game repairs them from their other copy, or "
                     "resets them, when it loads the save. Editing a part fixes it; Repair fixes all of them now, the way "
                     "the game would.",
                 kWarn);
            if (Btn("Repair", 100)) Repair();
        }
        ImGui::Spacing();
        const json& o = cur["options"];
        const int prog = I(o["progress"]);
        const float w = 210;
        int i = 0;
        GridNext(i++, w);
        if (InfoCard("prog", "Cups", std::to_string(prog & 0xFF) + " / 4", ProgressLine(prog), w)) Go("progress");
        GridNext(i++, w);
        if (InfoCard("teams", "Registered teams", std::to_string(RegisteredTeams()) + " / 16", "saved with Register", w)) Go("teams");
        GridNext(i++, w);
        if (InfoCard("n64", "Pokémon in N64 boxes", std::to_string(BoxedCount()), "in 4 boxes of 30", w)) Go("boxes");
        GridNext(i++, w);
        if (InfoCard("items", "Stored items", std::to_string(ItemCount()), "in 4 trainers' item storage", w)) Go("items");
        const json& c = cur["continue"];
        GridNext(i++, w);
        if (InfoCard("run", "Saved tournament", (I(c["flags"]) & 1) ? "Yes" : "None",
                     (I(c["flags"]) & 1) ? RunTitle(c) : "nothing to continue", w)) Go("run");
        for (size_t g = 0; g < gbs.size(); g++) {
            const json& gc = gbs[g].cur;
            int boxed = 0;
            for (const json& b : gc["boxes"]) boxed += (int)b.size();
            GridNext(i++, w);
            ImGui::PushID((int)g);
            if (InfoCard("gb", gbs[g].label + " · " + gbs[g].title, Show(TextOf(gc["trainer"]["name"])),
                         std::to_string(gc["party"].size()) + " in the party · " + std::to_string(boxed) + " in PC boxes", w)) {
                Go("gb" + std::to_string(g));
            }
            ImGui::PopID();
        }
        for (const std::string& sk : skipped) Hint(sk);
        ImGui::Spacing();
        ImGui::Spacing();
        Heading("Quick actions");
        if (Btn("Clear every cup and unlock both picture frames")) {
            UnlockAll();
            Toast("All four cups and the Lv 50-55 tournament cleared; Dodrio frame unlocked.");
        }
        Hint("Changes are only written when you press Save.");
    }

    void PageProgress() {
        Heading("Progress & unlocks");
        Hint("Tournament progress as the main menu reads it. Each cleared Lv 1-30 cup opens the next one.");
        json& o = cur["options"];
        const int p = I(o["progress"]);
        auto set = [this, &o](int v) {
            o["progress"] = v;
            o["flags"] = I(o["flags"]) | 1;
            Changed();
        };
        ImGui::Spacing();
        Label("Lv 1-30 cups cleared", 190);
        std::vector<std::string> cups = { "None" };
        for (const char* c : CUP_NAMES) cups.push_back(c);
        if (int k = Seg("cups", cups, std::clamp(p & 0xFF, 0, 4)); k >= 0) set((p & ~0xFF) | k);
        bool master = p & kMasterWon;
        if (Check("Master Ball Cup won", &master)) set(master ? p | kMasterWon : p & ~kMasterWon);
        bool stadium = p & kStadiumWon;
        if (Check("Lv 50-55 tournament won", &stadium)) set(stadium ? p | kStadiumWon : p & ~kStadiumWon);
        Hint("The Lv 50-55 flag is only known from the code (2026-11); the other two were seen in play.");
        ImGui::Spacing();
        ImGui::Separator();
        Heading("GB Player picture frames");
        Label("Unlocked", 190);
        if (int c = Seg("frames", { "None", "Doduo", "Dodrio" }, std::clamp(I(o["gb_player_frames"]), 0, 2)); c >= 0) {
            o["gb_player_frames"] = c;
            Changed();
        }
        Hint("Z cycles the unlocked frames in the GB Player. Clearing the Master Ball Cup unlocks one.");
    }

    void PageOptions() {
        json& o = cur["options"];
        Heading("Options");
        Hint("The settings of the game's Options menu and the GB Player's buttons.");
        ImGui::Spacing();
        Label("Sound", 180);
        if (int c = Seg("snd", { "Stereo", "Mono" }, I(o["sound"]) & 1, 90); c >= 0) o["sound"] = (I(o["sound"]) & ~1) | c, Changed();
        Label("Commentary", 180);
        if (int c = Seg("voice", { "On", "Off" }, (I(o["sound"]) >> 1) & 1, 90); c >= 0) {
            o["sound"] = (I(o["sound"]) & ~2) | (c << 1);
            Changed();
        }
        ImGui::Spacing();
        ImGui::Separator();
        Heading("GB Player buttons");
        const long long buttons = PyInt(o["gb_player_buttons"]);
        const int start = (int)((buttons >> 8) & 0xFF), select = (int)(buttons & 0xFF) + 1;
        std::vector<std::string> names(std::begin(kGbButtons), std::end(kGbButtons));
        Label("START", 180);
        if (int c = Seg("start", names, start, 80); c >= 0) o["gb_player_buttons"] = (buttons & ~0xFF00LL) | (c << 8), Changed();
        Label("SELECT", 180);
        if (int c = Seg("select", names, select, 80); c >= 0) {
            o["gb_player_buttons"] = (buttons & ~0xFFLL) | ((c - 1) & 0xFF);
            Changed();
        }
        Hint("The GB Player's コンフィグ (config). Only START -> R was checked in play.");
    }

    void PageTeams() {
        Heading("Registered teams");
        Hint("Teams saved with とうろく (Register). There are 16 slots.");
        ImGui::Spacing();
        for (int i = 0; i < 16; i++) {
            const json& s = cur["teams"][i];
            json r = ListRecord(s);
            std::string label = std::to_string(i + 1) + "  " +
                                (TeamRegistered(s) ? (TextOf(r["name"]).empty() ? "(no name)" : Show(TextOf(r["name"]))) +
                                                         " · " + std::to_string(r["entries"].size())
                                                   : std::string("empty"));
            ImGui::PushID(i);
            if (i % 4) ImGui::SameLine();
            if (ColorBtn(label.c_str(), i == slot ? kSelected : kButton, 150)) slot = i;
            ImGui::PopID();
        }
        ImGui::Spacing();
        ImGui::Separator();
        json& t = cur["teams"][slot];
        json tl = ListRecord(t);
        const int s = slot;
        Label("Trainer", 160);
        if (auto v = NameField("##tname", tl["name"])) EditTeamAt(s, [&](json& r) { r["name"] = *v; });
        ImGui::SameLine();
        ImGui::TextUnformatted("  ID");
        ImGui::SameLine();
        if (auto v = IntField("tid", I(tl["trainer_id"]), 0, 65535, 70)) EditTeamAt(s, [&](json& r) { r["trainer_id"] = *v; });
        if (TeamRegistered(t)) {
            ImGui::SameLine();
            if (DangerBtn("Delete team")) {
                Ask("Delete this team?", "Delete", [this, s] {
                    EditList(cur["teams"][s], [](json& r) {
                        for (auto it = r.begin(); it != r.end();) {
                            if (it.key().rfind("_", 0) == 0 && it.key() != "_footer") it = r.erase(it);
                            else ++it;
                        }
                        r["flags"] = 0;
                        r["name"] = "";
                        r["second_name"] = "";
                        r["trainer_id"] = 0;
                        r["entries"] = json::array();
                    });
                });
            }
        }
        ImGui::Spacing();
        MonCtx ctx;
        ctx.title = "Team " + std::to_string(s + 1);
        ctx.otName = TextOf(tl["name"]).empty() ? json("レッド") : tl["name"];
        ctx.otId = I(tl["trainer_id"]);
        const float w = 210;
        const json entries = tl["entries"];
        for (int i = 0; i < (int)entries.size(); i++) {
            GridNext(i, w);
            if (MonCard(std::to_string(i).c_str(), entries[i], w)) {
                MonCtx m = ctx;
                m.mon = entries[i];
                m.onDone = [this, s, i](const json& nm) { EditTeamAt(s, [&](json& r) { r["entries"][i] = nm; }); };
                m.onRemove = [this, s, i] { EditTeamAt(s, [&](json& r) { r["entries"].erase((size_t)i); }); };
                OpenMon(m);
            }
        }
        if (entries.size() < 6) {
            GridNext((int)entries.size(), w);
            if (EmptyCard("add", "+ Add Pokémon", w)) {
                MonCtx m = ctx;
                m.onDone = [this, s](const json& nm) {
                    EditTeamAt(s, [&](json& r) {
                        r["entries"].push_back(nm);
                        if (TextOf(r["name"]).empty()) r["name"] = "レッド";
                    });
                };
                OpenMon(m);
            }
        }
        ImGui::Spacing();
        Hint("Up to 6 Pokémon per team.");
    }

    void EditTeamAt(int s, const std::function<void(json&)>& fn) {
        json& t = cur["teams"][s];
        EditList(t, [&](json& r) {
            fn(r);
            r["flags"] = !r["entries"].empty() ? (I(r["flags"]) | 1) & ~2 : 0; // bit 0: registered
        });
    }

    void BoxCells(const char* id, const json& list, int cap, const MonCtx& ctx, bool canRemoveLast,
                  const std::function<void(const std::function<void(json&)>&)>& edit,
                  const std::function<bool(int, const json&)>& action = nullptr, float w = 190) {
        ImGui::PushID(id);
        for (int i = 0; i < cap; i++) {
            ImGui::PushID(i);
            GridNext(i, w + (action ? 34 : 0));
            ImGui::BeginGroup(); // a cell: the card, and the move arrow (or room for it)
            bool moved = false;
            if (i < (int)list.size()) {
                if (MonCard("m", list[i], w)) {
                    MonCtx m = ctx;
                    m.mon = list[i];
                    m.onDone = [edit, i](const json& nm) { edit([&](json& l) { l[i] = nm; }); };
                    if (canRemoveLast || list.size() > 1) m.onRemove = [edit, i] { edit([&](json& l) { l.erase((size_t)i); }); };
                    OpenMon(m);
                }
                if (action) {
                    ImGui::SameLine(0, S(4));
                    moved = action(i, list[i]);
                }
            } else {
                if (i == (int)list.size()) {
                    if (EmptyCard("add", "+ Add", w)) {
                        MonCtx m = ctx;
                        m.onDone = [edit](const json& nm) { edit([&](json& l) { l.push_back(nm); }); };
                        OpenMon(m);
                    }
                } else {
                    EmptyCard("x", "", w, false);
                }
                if (action) {
                    ImGui::SameLine(0, S(4));
                    ImGui::Dummy(ImVec2(S(30), S(1)));
                }
            }
            ImGui::EndGroup();
            ImGui::PopID();
            if (moved) break; // (the list changed)
        }
        ImGui::PopID();
    }

    // [<] Box n (k/20) [>]
    void BoxNav(const char* id, int count, int* cur_, const std::function<std::string(int)>& labelOf) {
        ImGui::PushID(id);
        if (Btn("<", st->handheld ? 44 : 28)) *cur_ = (*cur_ + count - 1) % count;
        ImGui::SameLine();
        ImGui::AlignTextToFramePadding();
        std::string label = labelOf(*cur_);
        ImGui::TextUnformatted(label.c_str());
        ImGui::SameLine();
        if (Btn(">", st->handheld ? 44 : 28)) *cur_ = (*cur_ + 1) % count;
        ImGui::PopID();
    }

    void PageBoxes() {
        Heading("N64 boxes");
        Hint("64のボックス: the Pokémon stored on the N64 cartridge with せいとん (Organize), 4 boxes with room for 30 each. "
             "Moving Pokémon to and from a Game Boy cartridge is done in the game.");
        ImGui::Spacing();
        json bl = ListRecord(cur["boxes"][box]);
        const int bi = box;
        auto edit = [this, bi](const std::function<void(json&)>& fn) {
            EditList(cur["boxes"][bi], [&](json& r) { fn(r["entries"]); });
        };
        MonCtx ctx;
        ctx.title = "N64 box " + std::to_string(box + 1);
        ctx.otName = bl["entries"].empty() ? json("レッド") : json(TextOf(bl["entries"][0]["ot_name"]));
        ctx.otId = bl["entries"].empty() ? 0 : I(bl["entries"][0]["ot_id"]);
        BoxNav("box", 4, &box, [this](int i) {
            return "Box " + std::to_string(i + 1) + "  (" + std::to_string(ListRecord(cur["boxes"][i])["entries"].size()) + "/30)";
        });
        if (!bl["entries"].empty()) {
            ImGui::SameLine();
            if (DangerBtn("Empty this box")) {
                Ask("Empty N64 box " + std::to_string(bi + 1) + "?", "Empty", [edit] { edit([](json& l) { l = json::array(); }); });
            }
        }
        ImGui::Spacing();
        BoxCells("n64", bl["entries"], 30, ctx, true, edit);
    }

    void ItemRows(const char* id, const json& entries, int cap, const std::function<void(const std::function<void(json&)>&)>& mutate,
                  const char* emptyText) {
        ImGui::PushID(id);
        if (entries.empty()) Hint(emptyText);
        for (int i = 0; i < (int)entries.size(); i++) {
            ImGui::PushID(i);
            int item = I(entries[i]["item"]);
            PickButton("item", ItemName(item), 200, [this, i, item, mutate] {
                OpenPicker("Item", ItemItems(), item, [i, mutate](int v) {
                    mutate([&](json& l) { l[i] = json{ { "item", v }, { "quantity", l[i]["quantity"] } }; });
                });
            });
            ImGui::SameLine();
            if (auto v = IntField("q", I(entries[i]["quantity"]), 1, 99, 40)) {
                long long q = *v;
                mutate([&](json& l) { l[i] = json{ { "item", l[i]["item"] }, { "quantity", q } }; });
            }
            ImGui::SameLine();
            if (Btn("x", st->handheld ? 38 : 24)) {
                mutate([i](json& l) { l.erase((size_t)i); });
                ImGui::PopID();
                break;
            }
            ImGui::PopID();
        }
        ImGui::BeginDisabled((int)entries.size() >= cap);
        if (Btn("+ Add item")) mutate([](json& l) { l.push_back(json{ { "item", 40 }, { "quantity", 1 } }); });
        ImGui::EndDisabled();
        ImGui::SameLine();
        Hint(std::to_string(entries.size()) + " of " + std::to_string(cap));
        ImGui::PopID();
    }

    void PageItems() {
        Heading("Item storage");
        Hint("64の カセット items: the items each trainer stored on the N64 cartridge with せいとん (Organize). The record "
             "whose name and ID match the Game Boy cartridge is the one the game shows.");
        ImGui::Spacing();
        for (int i = 0; i < 4; i++) {
            json r = ListRecord(cur["item_boxes"][i]);
            size_t n = r["entries"].size();
            std::string who = TextOf(r["name"]).empty() ? "Slot " + std::to_string(i + 1) : Show(TextOf(r["name"]));
            std::string label = who + (n ? "  " + std::to_string(n) : "");
            ImGui::PushID(i);
            if (i) ImGui::SameLine();
            if (ColorBtn(label.c_str(), i == ibox ? kSelected : kButton)) ibox = i;
            ImGui::PopID();
        }
        ImGui::Spacing();
        const int bi = ibox;
        json bl = ListRecord(cur["item_boxes"][bi]);
        auto edit = [this, bi](const std::function<void(json&)>& fn) {
            EditList(cur["item_boxes"][bi], [&](json& r) {
                fn(r);
                if (!r["entries"].empty()) r["flags"] = I(r["flags"]) | 1;
            });
        };
        Label("Trainer", 120);
        if (auto v = NameField("##owner", bl["name"])) edit([&](json& r) { r["name"] = *v; });
        ImGui::SameLine();
        ImGui::TextUnformatted("  ID");
        ImGui::SameLine();
        if (auto v = IntField("oid", I(bl["trainer_id"]), 0, 65535, 70)) edit([&](json& r) { r["trainer_id"] = *v; });
        ImGui::Spacing();
        ItemRows("rows", bl["entries"], 100, [edit](const std::function<void(json&)>& fn) {
            edit([&](json& r) {
                fn(r["entries"]);
                if (TextOf(r["name"]).empty() && !r["entries"].empty()) r["name"] = "レッド";
            });
        }, "Nothing is stored for this trainer.");
        if (!bl["entries"].empty()) {
            ImGui::SameLine();
            if (DangerBtn("Empty")) {
                Ask("Remove every item stored for this trainer?", "Empty", [this, bi] {
                    EditList(cur["item_boxes"][bi], [](json& r) {
                        r["flags"] = 0;
                        r["name"] = "";
                        r["trainer_id"] = 0;
                        r["entries"] = json::array();
                    });
                });
            }
        }
    }

    void PageRun() {
        json& c = cur["continue"];
        json& se = c["session"];
        Heading("Saved tournament");
        const bool has = (I(c["flags"]) & 1) == 1;
        auto setMons = [this](const json& list) {
            json& cc = cur["continue"];
            json p = list;
            while (p.size() < 6) p.push_back("fill:00*45");
            cc["pokemon"] = p;
            cc["count"] = list.size();
            cc["rental_mask"] = I(cc["rental_mask"]) & ((1 << list.size()) - 1);
            Changed();
        };
        if (!has) {
            Hint("ちゅうだんする (Suspend) at トーナメントをつづけますか？ saves the tournament so the main menu can continue it. "
                 "There is no saved tournament in this save.");
            if (Btn("Create a saved tournament")) {
                c["flags"] = 1;
                se["mode"] = 2;
                se["cup"] = 0;
                se["round"] = 1;
                setMons(json::array());
            }
            return;
        }
        Hint(RunTitle(c));
        ImGui::Spacing();
        Label("Tournament", 150);
        if (int k = Seg("mode", { "Lv 50-55", "Lv 1-30 cups" }, I(se["mode"]) - 1); k >= 0) se["mode"] = k + 1, se["cup"] = 0, Changed();
        if (I(se["mode"]) == 2) {
            Label("Cup", 150);
            std::vector<std::string> cups(std::begin(CUP_NAMES), std::end(CUP_NAMES));
            if (int k = Seg("cup", cups, I(se["cup"])); k >= 0) se["cup"] = k, Changed();
        }
        Label("Next battle", 150);
        if (auto v = IntField("round", I(se["round"]), 1, 8, 40)) se["round"] = *v, Changed();
        ImGui::Spacing();
        Heading("Team in this tournament");
        json mons = json::array();
        for (int i = 0; i < std::clamp(I(c["count"]), 0, 6); i++)
            if (c["pokemon"][i].is_object()) mons.push_back(c["pokemon"][i]);
        MonCtx ctx;
        ctx.title = "Saved tournament";
        ctx.otName = mons.empty() ? json("レッド") : json(TextOf(mons[0]["ot_name"]));
        ctx.otId = mons.empty() ? 0 : I(mons[0]["ot_id"]);
        BoxCells("run", mons, (int)std::min<size_t>(6, mons.size() + 1), ctx, true, [mons, setMons](const std::function<void(json&)>& fn) {
            json l = mons;
            fn(l);
            setMons(l);
        });
        if (mons.empty()) Hint("A saved tournament needs at least one Pokémon.", kWarn);
        Hint("A team from a Game Boy cartridge resumes only while the same cartridge is in the Transfer Pak (the save keeps "
             "a snapshot of it); a rental team always resumes.");
        ImGui::Spacing();
        if (DangerBtn("Delete saved tournament")) {
            Ask("Delete the saved tournament?", "Delete", [this] {
                json& cc = cur["continue"];
                for (auto it = cc.begin(); it != cc.end();) { // Save_ClearTypedRecord: the record zeroed
                    if (it.key().rfind("_raw_", 0) == 0) it = cc.erase(it);
                    else ++it;
                }
                cc["flags"] = 0;
                cc["count"] = 0;
                cc["session"] = json{ { "mode", 0 }, { "cup", 0 }, { "round", 0 }, { "opponent_history", json::array({ 0, 0, 0, 0, 0, 0, 0, 0 }) } };
                cc["pokemon"] = json::array({ "fill:00*45", "fill:00*45", "fill:00*45", "fill:00*45", "fill:00*45", "fill:00*45" });
                cc["gb_snapshot"] = "fill:00*354";
                cc["rental_mask"] = 0;
                Changed();
            });
        }
    }

    void PageGb(int gi) {
        Gb& g = gbs[gi];
        json& c = g.cur;
        json& t = c["trainer"];
        Heading(g.label + " · " + g.title);
        Hint(Show(TextOf(t["name"])) + " (ID " + Pad(I(t["id"]), 5) + ") · " + g.name);
        if (!c["valid"].get<bool>()) {
            Hint("The Game Boy game will report this save as corrupted: its checksum does not match. Saving here rewrites "
                 "the checksum.", kWarn);
        }
        ImGui::Spacing();
        int boxed = 0;
        for (const json& b : c["boxes"]) boxed += (int)b.size();
        const std::vector<std::string> tabs = { "Trainer", "Party (" + std::to_string(c["party"].size()) + ")",
                                                "PC boxes (" + std::to_string(boxed) + ")", "Items",
                                                "Pokédex (" + std::to_string(c["pokedex"]["owned"].size()) + ")" };
        if (int k = Seg("tabs", tabs, gbTab); k >= 0 && k != gbTab) gbTab = k, ResetFocus();
        ImGui::Separator();
        ImGui::Spacing();
        auto mut = [this](const std::function<void()>& fn) {
            fn();
            Changed();
        };
        MonCtx ctx;
        ctx.title = g.label;
        ctx.otName = TextOf(t["name"]);
        ctx.otId = I(t["id"]);
        ctx.level = 5;
        ctx.western = true;
        if (gbTab == 0) {
            Label("Player name", 160);
            if (auto v = NameField("##name", t["name"], 8, 150, true)) mut([&] { t["name"] = *v; });
            Label("Rival name", 160);
            if (auto v = NameField("##rival", t["rival"], 8, 150, true)) mut([&] { t["rival"] = *v; });
            Label("Trainer ID", 160);
            if (auto v = IntField("id", I(t["id"]), 0, 65535, 70)) mut([&] { t["id"] = *v; });
            Label("Money", 160);
            if (auto v = IntField("money", I(t["money"]), 0, 999999, 80, 1000)) mut([&] { t["money"] = *v; });
            if (g.kind == "yellow") {
                Label("Pikachu's friendship", 160);
                if (auto v = IntField("pika", I(t["pikachu_happiness"]), 0, 255, 40, 10)) mut([&] { t["pikachu_happiness"] = *v; });
            }
            Label("Play time", 160);
            const json& pt = t["playtime"];
            ImGui::Text("%d:%s:%s", I(pt["hours"]), Pad(I(pt["minutes"]), 2).c_str(), Pad(I(pt["seconds"]), 2).c_str());
            Hint("Changing the trainer ID makes the game treat your own Pokémon as traded ones (they obey only with the right badges).");
            ImGui::Spacing();
            Heading("Badges");
            for (int i = 0; i < 8; i++) {
                ImGui::PushID(i);
                if (i) SameLineWrap(S(130));
                bool on = I(t["badges"]) & (1 << i);
                std::string label = std::string(gb::BADGE_NAMES[i]) + " Badge";
                if (Check(label.c_str(), &on)) mut([&] { t["badges"] = on ? I(t["badges"]) | (1 << i) : I(t["badges"]) & ~(1 << i); });
                ImGui::PopID();
            }
            if (Btn("All badges")) mut([&] { t["badges"] = 255; });
            ImGui::SameLine();
            if (Btn("No badges")) mut([&] { t["badges"] = 0; });
        } else if (gbTab == 1) {
            BoxCells("party", c["party"], 6, ctx, false, [this, gi](const std::function<void(json&)>& fn) {
                fn(gbs[gi].cur["party"]);
                Changed();
            });
            Hint("Up to 6 Pokémon; the party always keeps at least one. Level and stats are recalculated when saving.");
        } else if (gbTab == 2) {
            BoxNav("box", 12, &gbBox, [&](int i) {
                return "Box " + std::to_string(i + 1) + "  (" + std::to_string(c["boxes"][i].size()) + "/20)" +
                       (i == I(c["current_box"]) ? "  · current" : "");
            });
            const int n = gbBox;
            if (!c["boxes"][n].empty()) {
                ImGui::SameLine();
                if (DangerBtn("Empty this box")) {
                    Ask("Empty PC box " + std::to_string(n + 1) + "?", "Empty", [this, gi, n] {
                        gbs[gi].cur["boxes"][n] = json::array();
                        Changed();
                    });
                }
            }
            ImGui::Spacing();
            BoxCells("box", c["boxes"][n], 20, ctx, true, [this, gi, n](const std::function<void(json&)>& fn) {
                fn(gbs[gi].cur["boxes"][n]);
                Changed();
            });
        } else if (gbTab == 3) {
            Heading("Bag");
            ItemRows("bag", c["bag"], 20, [this, gi](const std::function<void(json&)>& fn) {
                fn(gbs[gi].cur["bag"]);
                Changed();
            }, "The bag is empty.");
            ImGui::Spacing();
            Heading("PC items");
            ItemRows("pc", c["pc_items"], 50, [this, gi](const std::function<void(json&)>& fn) {
                fn(gbs[gi].cur["pc_items"]);
                Changed();
            }, "No items stored in the PC.");
        } else {
            std::set<int> owned, seen;
            for (const json& x : c["pokedex"]["owned"]) owned.insert(I(x));
            for (const json& x : c["pokedex"]["seen"]) seen.insert(I(x));
            Hint(std::to_string(owned.size()) + " caught · " + std::to_string(seen.size()) +
                 " seen · pick a Pokémon to change it (not seen -> seen -> caught)");
            if (Btn("Mark all caught")) {
                json all = json::array();
                for (int d = 1; d <= 151; d++) all.push_back(d);
                mut([&] {
                    c["pokedex"]["owned"] = all;
                    c["pokedex"]["seen"] = all;
                });
            }
            ImGui::SameLine();
            if (DangerBtn("Clear Pokédex")) {
                Ask("Clear the whole Pokédex?", "Clear", [this, gi] {
                    gbs[gi].cur["pokedex"]["owned"] = json::array();
                    gbs[gi].cur["pokedex"]["seen"] = json::array();
                    Changed();
                });
            }
            ImGui::Spacing();
            const float w = st->handheld ? 150 : 128;
            for (int d = 1; d <= 151; d++) {
                ImGui::PushID(d);
                GridNext(d - 1, w);
                std::string label = Pad(d, 3) + " " + Show(gd::SPECIES[d]);
                uint32_t col = owned.count(d) ? 0x1F4D2C : seen.count(d) ? 0x3D3A1E : kButton;
                if (ColorBtn(label.c_str(), col, w)) {
                    if (owned.count(d)) owned.erase(d), seen.erase(d);
                    else if (seen.count(d)) owned.insert(d);
                    else seen.insert(d);
                    mut([&] {
                        c["pokedex"]["owned"] = json(std::vector<int>(owned.begin(), owned.end()));
                        c["pokedex"]["seen"] = json(std::vector<int>(seen.begin(), seen.end()));
                    });
                }
                ImGui::PopID();
            }
        }
    }

    // ---- frame ----

    std::vector<Section> Sections() {
        std::vector<Section> s = { { "overview", "Overview" }, { "progress", "Progress & unlocks" }, { "options", "Options" },
                                   { "teams", "Registered teams" }, { "boxes", "N64 boxes" }, { "items", "Item storage" },
                                   { "run", "Saved tournament" } };
        for (size_t i = 0; i < gbs.size(); i++) s.push_back({ "gb" + std::to_string(i), gbs[i].label + " · " + gbs[i].title });
        return s;
    }

    std::string Count(const std::string& key) {
        if (key == "teams") return std::to_string(RegisteredTeams());
        if (key == "boxes") return std::to_string(BoxedCount());
        if (key == "items") return std::to_string(ItemCount());
        if (key == "run") return (I(cur["continue"]["flags"]) & 1) ? "1" : "";
        if (key.rfind("gb", 0) == 0) {
            const Gb& g = gbs[std::stoi(key.substr(2))];
            return g.cur != g.base ? "*" : "";
        }
        return "";
    }

    void RequestClose() {
        if (!dirty) {
            open = false;
            return;
        }
        Confirm c{ "Save the changes before closing?", "Save and close", [this] {
                      Save();
                      if (error.empty()) open = false;
                  } };
        c.other = [this] { open = false; };
        c.otherLabel = "Close without saving";
        confirm = std::move(c);
        ResetFocus();
    }

    void Draw(ImVec2 min, ImVec2 max) {
        ImDrawList* pdl = ImGui::GetWindowDrawList();
        pdl->AddRectFilled(min, max, Col(kCard), S(8));
        pdl->AddRect(min, max, Col(kBorder), S(8));
        ImGui::SetCursorScreenPos(ImVec2(min.x + S(12), min.y + S(8)));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_Header, ColV(kSelected));
        ImGui::PushStyleColor(ImGuiCol_TableHeaderBg, ColV(kButton));
        ImGui::PushStyleColor(ImGuiCol_Separator, ColV(kBorder));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ColV(kWindow));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ColV(0x0E1117));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(S(6), S(st->handheld ? 8 : 6)));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(S(8), S(st->handheld ? 9 : 4)));
        ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, S(st->handheld ? 18 : 12));
        const ImVec2 size(max.x - min.x - S(24), max.y - min.y - S(16));
        ImGui::BeginChild("save-editor", size, false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::SetWindowFontScale(st->scale);

        // header: title, file, status, Discard / Save / Close
        if (st->game) ImGui::PushFont(st->game);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("SAVE EDITOR");
        if (st->game) ImGui::PopFont();
        ImGui::SameLine();
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(ColV(kDim), "%s", fileName.c_str());
        ImGui::SameLine();
        ImGui::AlignTextToFramePadding();
        if (!toast.empty() && SDL_GetTicks() < toastUntil) ImGui::TextColored(ColV(kGreen), "%s", toast.c_str());
        else ImGui::TextColored(ColV(dirty ? kWarn : kDim), "%s", dirty ? "Unsaved changes" : "All changes saved");
        const float bw = st->handheld ? 110 : 84;
        ImGui::SameLine(size.x - S(bw * 3 + 16));
        const bool modal = dlg || picker || confirm;
        ImGui::BeginDisabled(!dirty || modal);
        if (Btn("Discard", bw)) {
            Ask("Throw away all changes since the last save?", "Discard", [this] {
                try {
                    Load();
                    error.clear();
                } catch (const std::exception& e) {
                    error = e.what();
                }
            });
        }
        ImGui::SameLine();
        if (ColorBtn("Save", dirty ? kGreen : kButton, bw)) Save();
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(modal);
        if (Btn("Close", bw)) RequestClose();
        ImGui::EndDisabled();
        if (!error.empty()) {
            Hint(error, kRed);
            ImGui::SameLine();
            if (ImGui::SmallButton("OK")) error.clear();
        }
        ImGui::Separator();

        // sections (left) and the page (right)
        const float navW = st->handheld ? 200 : 178;
        const float h = ImGui::GetContentRegionAvail().y;
        ImGui::BeginChild("sections", ImVec2(S(navW), h), false);
        ImGui::SetWindowFontScale(st->scale);
        ImGui::BeginDisabled(modal);
        for (const Section& s : Sections()) {
            ImGui::PushID(s.key.c_str());
            std::string count = Count(s.key);
            std::string label = s.name + (count.empty() ? "" : "  (" + count + ")");
            if (!modal && section == s.key && st->setDefaultFocus) st->setDefaultFocus();
            ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0, 0.5f));
            if (ColorBtn(label.c_str(), section == s.key ? kSelected : kButton, navW - 8)) Go(s.key);
            ImGui::PopStyleVar();
            ImGui::PopID();
        }
        ImGui::EndDisabled();
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::BeginChild("page", ImVec2(0, h), false);
        ImGui::SetWindowFontScale(st->scale);
        if (!scrollTo.empty()) {
            ImGui::SetScrollY(0);
            scrollTo.clear();
        }
        if (confirm) {
            DrawConfirm();
        } else if (picker) {
            DrawPicker();
        } else if (dlg) {
            DrawMonDialog();
        } else {
            ImGui::PushID(section.c_str());
            if (section == "overview") PageOverview();
            else if (section == "progress") PageProgress();
            else if (section == "options") PageOptions();
            else if (section == "teams") PageTeams();
            else if (section == "boxes") PageBoxes();
            else if (section == "items") PageItems();
            else if (section == "run") PageRun();
            else if (section.rfind("gb", 0) == 0 && std::stoi(section.substr(2)) < (int)gbs.size()) PageGb(std::stoi(section.substr(2)));
            ImGui::PopID();
        }
        ImGui::EndChild();
        ImGui::EndChild();
        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(7);

        // keyboard: Ctrl/Cmd+S saves, Escape goes back
        ImGuiIO& io = ImGui::GetIO();
        if (!io.WantTextInput) {
            if ((io.KeyCtrl || io.KeySuper) && ImGui::IsKeyPressed(ImGuiKey_S, false) && dirty && !modal) Save();
            if (!st->handheld && ImGui::IsKeyPressed(ImGuiKey_Escape, false)) Back();
        }
    }

    bool Back() {
        if (confirm) confirm.reset();
        else if (picker) picker.reset();
        else if (dlg) dlg.reset();
        else {
            RequestClose();
            return true;
        }
        ResetFocus();
        return true;
    }
};

SaveEditor::SaveEditor() : mImpl(std::make_unique<Impl>()) {}
SaveEditor::~SaveEditor() = default;

bool SaveEditor::Open(const std::string& save, const std::vector<SaveEditorGb>& gbs, int gbSection, std::string* error) {
    Impl& m = *mImpl;
    Impl fresh;
    try {
        fresh.path = save;
        fresh.fileName = FileNameOf(save);
        for (const SaveEditorGb& g : gbs) {
            std::error_code ec;
            if (g.save.empty() || !std::filesystem::exists(std::filesystem::u8path(g.save), ec)) continue;
            Impl::Gb e;
            e.label = g.label;
            e.path = g.save;
            e.rom = g.rom;
            e.name = FileNameOf(g.save);
            json info = gb::GameInfo(g.rom, g.save);
            e.title = info["title"].get<std::string>();
            e.kind = info["kind"].get<std::string>();
            if (info["japanese"].get<bool>()) { // the Japanese layout is not modelled here (gb::GBSave is the Western one)
                fresh.skipped.push_back(g.label + "'s Japanese Game Boy save is not edited here: Organize in the game "
                                        "moves its Pokémon and items, and pj64-pokestadium-debug edits it.");
                continue;
            }
            try {
                gb::GBSave(ReadFile(g.save)); // a Game Boy save that is not one (wrong size) is left out
            } catch (const std::exception&) {
                continue;
            }
            fresh.gbs.push_back(e);
        }
        fresh.Load();
    } catch (const std::exception& e) {
        if (error != nullptr) *error = e.what();
        return false;
    }
    m.open = true;
    m.path = fresh.path;
    m.fileName = fresh.fileName;
    m.gbs = std::move(fresh.gbs);
    m.skipped = std::move(fresh.skipped);
    m.base = std::move(fresh.base);
    m.cur = std::move(fresh.cur);
    m.dirty = false;
    m.dlg.reset();
    m.picker.reset();
    m.confirm.reset();
    m.error.clear();
    m.toast.clear();
    m.section = "overview";
    if (gbSection >= 0) {
        for (size_t i = 0; i < m.gbs.size(); i++) // the Game Boy save of that slot
            if (m.gbs[i].path == gbs[gbSection].save) m.section = "gb" + std::to_string(i);
    }
    m.scrollTo = m.section;
    return true;
}

bool SaveEditor::IsOpen() const {
    return mImpl->open;
}

void SaveEditor::Close() {
    mImpl->open = false;
}

void SaveEditor::Draw(const SaveEditorStyle& style, ImVec2 min, ImVec2 max) {
    mImpl->style = style;
    mImpl->Draw(min, max);
}

bool SaveEditor::Back() {
    if (!mImpl->open) return false;
    return mImpl->Back();
}

bool SaveEditor::HasUnsavedChanges() const {
    return mImpl->open && mImpl->dirty;
}

} // namespace Port
