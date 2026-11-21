// Cheat settings (cheats.h): what the launcher's Cheats panel sets and the game reads.
#include "cheats.h"

#include <libultraship/bridge/consolevariablebridge.h>

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>

namespace {

const Port::CheatInfo kCheats[PORT_CHEAT_COUNT] = {
    { "InfiniteHp", "Infinite HP", "Your Pokémon take no damage and are kept at full HP (Self-Destruct and Explosion "
                                   "still faint them)" },
    { "InfinitePp", "Infinite PP", "Your moves use no PP" },
    { "NoStatus", "Status always good", "Sleep, poison, burn, freeze, paralysis and confusion are cured as soon as they "
                                        "hit your Pokémon" },
    { "NeverMiss", "Your moves never miss", "Your moves skip the accuracy roll (a Pokémon underground or in the air, "
                                            "Mist and Dream Eater on an awake target still fail)" },
    { "FoeNeverHits", "Opponent never hits", "Every move aimed at your Pokémon misses" },
    { "MoveFirst", "Always your turn first", "You act first every turn, whatever the Speed or Quick Attack" },
    { "PerfectStats", "Perfect stats", "Your Pokémon battle with perfect DVs and stat experience: Attack, Defense, "
                                       "Speed and Special as high as their level allows (HP is unchanged)" },
    { "FoeMinStats", "Opponent minimum stats", "Your opponents' Pokémon battle with zero DVs and stat experience: "
                                               "the lowest Attack, Defense, Speed and Special for their level" },
};

std::string Key(int cheat) {
    return std::string("gCheats.") + kCheats[cheat].key;
}

int Hex(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    c = (char)std::toupper((unsigned char)c);
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

} // namespace

namespace Port {

const CheatInfo& Cheat(int cheat) {
    return kCheats[cheat];
}

std::string GbCheatKey(int port) {
    return "gCheats.Gb" + std::to_string(port + 1) + ".Codes";
}

std::vector<GbCheatCode> ParseGbCheats(const std::string& text, std::vector<std::string>* errors) {
    std::vector<GbCheatCode> codes;
    size_t i = 0;
    while (i < text.size()) {
        while (i < text.size() && (std::isspace((unsigned char)text[i]) || text[i] == ',' || text[i] == ';')) i++;
        size_t start = i;
        while (i < text.size() && !std::isspace((unsigned char)text[i]) && text[i] != ',' && text[i] != ';') i++;
        if (start == i) break;
        std::string word = text.substr(start, i - start);
        std::string digits;
        for (char c : word) {
            if (c != '-') digits += c;
        }
        int v[8];
        bool ok = digits.size() == 8;
        for (int k = 0; ok && k < 8; k++) ok = (v[k] = Hex(digits[k])) >= 0;
        if (ok) {
            const int type = v[0] << 4 | v[1];
            const uint8_t value = (uint8_t)(v[2] << 4 | v[3]);
            const uint16_t address = (uint16_t)((v[6] << 12) | (v[7] << 8) | (v[4] << 4) | v[5]);
            if (type == 0x01 && address >= 0xC000 && address < 0xE000) {
                if (codes.size() < PORT_CHEAT_GB_MAX_CODES) {
                    codes.push_back({ address, value });
                    continue;
                }
                if (errors != nullptr) errors->push_back(word + " (more than " +
                                                         std::to_string(PORT_CHEAT_GB_MAX_CODES) + " codes)");
                continue;
            }
        }
        if (errors != nullptr) errors->push_back(word);
    }
    return codes;
}

void ClearCheats() {
    for (int c = 0; c < PORT_CHEAT_COUNT; c++) CVarClear(Key(c).c_str());
    for (int p = 0; p < 4; p++) CVarClear(GbCheatKey(p).c_str());
}

bool AnyCheatSet() {
    for (int c = 0; c < PORT_CHEAT_COUNT; c++) {
        if (CVarGetInteger(Key(c).c_str(), 0) & 0xF) return true;
    }
    for (int p = 0; p < 4; p++) {
        if (!ParseGbCheats(CVarGetString(GbCheatKey(p).c_str(), "")).empty()) return true;
    }
    return false;
}

} // namespace Port

// Test aid: PORT_CHEATS="InfiniteHp=1,NeverMiss=3" (key = player mask) replaces the settings for the battle cheats, and
// PORT_GB_CHEATS="1:01xxxxxx 01yyyyyy" those of one Transfer Pak, so a headless run needs no config file.
static const char* EnvValue(const char* env, const std::string& key) {
    const char* all = getenv(env);
    if (all == nullptr) return nullptr;
    const char* p = all;
    while ((p = strstr(p, key.c_str())) != nullptr) {
        const bool start = p == all || p[-1] == ',' || p[-1] == ' ';
        if (start && (p[key.size()] == '=' || p[key.size()] == ':')) return p + key.size() + 1;
        p += key.size();
    }
    return "";
}

extern "C" int Port_CheatPlayers(int cheat) {
    if (cheat < 0 || cheat >= PORT_CHEAT_COUNT) return 0;
    if (const char* v = EnvValue("PORT_CHEATS", kCheats[cheat].key)) return (int)strtol(v, nullptr, 0) & 0xF;
    return CVarGetInteger(Key(cheat).c_str(), 0) & 0xF;
}

extern "C" void Port_GbCheatsApply(uint8_t* mem, int port) {
    if (mem == nullptr || port < 0 || port > 3) return;
    // parsed once per change of the text (the Game Boy runs up to 4 frames per N64 frame)
    static std::mutex lock;
    static std::string text[4];
    static std::vector<Port::GbCheatCode> codes[4];
    static bool logged[4];
    std::lock_guard<std::mutex> guard(lock);
    const char* now = CVarGetString(Port::GbCheatKey(port).c_str(), "");
    std::string fromEnv;
    if (const char* env = getenv("PORT_GB_CHEATS")) {
        fromEnv = env[0] == '1' + port && env[1] == ':' ? env + 2 : "";
        now = fromEnv.c_str();
    }
    if (now == nullptr) now = "";
    if (text[port] != now) {
        text[port] = now;
        codes[port] = Port::ParseGbCheats(text[port]);
        logged[port] = false;
    }
    if (!logged[port] && !codes[port].empty()) {
        logged[port] = true;
        fprintf(stderr, "[cheats] Game Boy (Transfer Pak %d): %zu GameShark code(s) active\n", port + 1,
                codes[port].size());
    }
    for (const auto& c : codes[port]) mem[c.address] = c.value;
}
