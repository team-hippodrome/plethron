// Western Gen 1 saves in the Japanese layout for Zero: see gb_save_convert.h.
#include "gb_save_convert.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

extern "C" {
unsigned char Species_FromInternalIndex(int index); // gb_data.c: Gen 1 internal index -> National Dex number
char* Text_CopySpeciesName(char* dst, int dex);    // gb_data.c: EUC-JP species name
char* Text_TranscodeName(char* gbDst, char* euc);  // gb_data.c: EUC-JP -> Japanese GB codes, 0x50-terminated
extern uint16_t D_80076FD4_jp[256];                // gb_data.c: Japanese GB code -> EUC-JP
}

namespace GbSaveConvert {
namespace {

constexpr uint8_t TERM = 0x50;

// Western layout (pokered's sPlayerName ... sMainDataCheckSum, sBox1 ...)
constexpr int W_NAME = 0x2598, W_MAIN = 0x25A3, W_RIVAL = 0x25F6, W_AFTER_RIVAL = 0x2601, W_CURBOX = 0x284C,
              W_SPRITES = 0x2D2C, W_PARTY = 0x2F2C, W_BOXDATA = 0x30C0, W_CHECKSUM = 0x3523;
constexpr int W_BOX_SIZE = 0x462, W_BOX_MONS = 20, W_NAME_LEN = 11, W_BOXES = 12, W_BOXES_PER_BANK = 6;
// Japanese layout (as Zero reads it: GbSaveMainData, GbSaveParty, GbSaveBox in gb_data.h, at SRAM 0x2580)
constexpr int J_NAME = 0x2598, J_MAIN = 0x259E, J_RIVAL = 0x25F1, J_AFTER_RIVAL = 0x25F7, J_CURBOX = 0x2842,
              J_SPRITES = 0x2CD5, J_PARTY = 0x2ED5, J_BOXDATA = 0x302D, J_CHECKSUM = 0x3594;
constexpr int J_BOX_SIZE = 0x566, J_BOX_MONS = 30, J_NAME_LEN = 6, J_BOXES = 8, J_BOXES_PER_BANK = 4;
// Zero reads nothing in the shifted main data past its event flag byte 0x2A93 (unk_513): beyond that the two
// layouts drift apart, so only this much is carried back.
constexpr int J_SHIFTED_END = 0x2A94;
constexpr int REC = 33, PARTY_REC = 44;

int WBoxAt(int b) { return (b < W_BOXES_PER_BANK ? 0x4000 : 0x6000) + (b % W_BOXES_PER_BANK) * W_BOX_SIZE; }
int JBoxAt(int b) { return (b < J_BOXES_PER_BANK ? 0x4000 : 0x6000) + (b % J_BOXES_PER_BANK) * J_BOX_SIZE; }

uint8_t Checksum(const uint8_t* p, int n) {
    uint8_t s = 0;
    for (int i = 0; i < n; i++) {
        s += p[i];
    }
    return s ^ 0xFF;
}

// ---- characters ----
// Japanese codes the Japanese character set leaves blank (Zero's table maps them to a full-width space), used here
// for the Latin letters it lacks: O P W X Y, then a-z.
const uint8_t kFree[] = { 0x14, 0x15, 0x16, 0x17, 0x18, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x35, 0x36,
                          0x37, 0x38, 0x39, 0x3F, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0xEC, 0xED, 0xEE, 0xF0, 0xF1, 0xF2 };

uint8_t LatinToJp(char c) {
    if (c >= 'A' && c <= 'N') return 0x60 + (c - 'A');
    if (c >= 'Q' && c <= 'V') return 0x70 + (c - 'Q');
    switch (c) {
        case 'O': return kFree[0];
        case 'P': return kFree[1];
        case 'W': return kFree[2];
        case 'X': return kFree[3];
        case 'Y': return kFree[4];
        case 'Z': return 0x79;
    }
    if (c >= 'a' && c <= 'z') return kFree[5 + (c - 'a')];
    return 0;
}

// A Western GB code as Latin text (accents dropped), 0 if it has none.
char WestToLatin(uint8_t b, bool european) {
    if (b >= 0x80 && b <= 0x99) return 'A' + (b - 0x80);
    if (b >= 0xA0 && b <= 0xB9) return 'a' + (b - 0xA0);
    if (european) {
        static const char kAccents[] = "aeeusc"; // 0xBA-0xBF: à è é ù ß ç
        if (b >= 0xBA && b <= 0xBF) return kAccents[b - 0xBA];
        static const char kUmlauts[] = "AOUaouei"; // 0xC0-0xC7: Ä Ö Ü ä ö ü ë ï
        if (b >= 0xC0 && b <= 0xC7) return kUmlauts[b - 0xC0];
    } else if (b == 0xBA) {
        return 'e'; // é
    }
    return 0;
}

uint8_t WestCharToJp(uint8_t b, bool european) {
    if (char c = WestToLatin(b, european)) return LatinToJp(c);
    if (b >= 0xF6) return b; // digits, the same codes in both
    switch (b) {
        case 0x7F: return 0x7F; // space
        case 0xEF: return 0xEF; // ♂
        case 0xF5: return 0xF5; // ♀
        case 0xE3: return 0xE3; // - (ー)
        case 0xE6: return 0xE6; // ?
        case 0xE7: return 0xE7; // !
        case 0xE8: case 0xF2: return 0xE5; // .
        case 0xF4: return 0xE8; // ,
    }
    return 0xE6;
}

// The Western code of a Japanese one, 0 if there is none (kana).
uint8_t JpCharToWest(uint8_t j) {
    for (char c = 'A'; c <= 'Z'; c++) if (LatinToJp(c) == j) return 0x80 + (c - 'A');
    for (char c = 'a'; c <= 'z'; c++) if (LatinToJp(c) == j) return 0xA0 + (c - 'a');
    if (j >= 0xF6) return j;
    switch (j) {
        case 0x7F: case 0xEF: case 0xF5: case 0xE3: case 0xE6: case 0xE7: return j;
        case 0xE5: return 0xE8;
        case 0xE8: return 0xF4;
    }
    return 0;
}

std::vector<uint8_t> Name(const uint8_t* p, int len) {
    std::vector<uint8_t> v;
    for (int i = 0; i < len && p[i] != TERM; i++) {
        v.push_back(p[i]);
    }
    return v;
}

void PutName(uint8_t* dst, const std::vector<uint8_t>& name, int len) {
    memset(dst, TERM, len);
    memcpy(dst, name.data(), std::min<size_t>(name.size(), len - 1));
}

bool IsEuropean(const Context& ctx) {
    static const uint8_t kRhydon[] = { 0x91, 0x87, 0x98, 0x83, 0x8E, 0x8D }; // RHYDON (English, Spanish, Italian)
    const auto& n = ctx.speciesNames.size() > 1 ? ctx.speciesNames[1] : std::vector<uint8_t>();
    return !(n.size() == 6 && memcmp(n.data(), kRhydon, 6) == 0);
}

std::vector<uint8_t> NameWestToJp(const Context& ctx, const std::vector<uint8_t>& w) {
    std::vector<uint8_t> j;
    for (uint8_t b : w) {
        if (j.size() == J_NAME_LEN - 1) break;
        j.push_back(WestCharToJp(b, IsEuropean(ctx)));
    }
    return j;
}

std::vector<uint8_t> JpSpeciesName(int internal) {
    char euc[32] = {};
    char gb[32] = {};
    int dex = Species_FromInternalIndex(internal);
    if (dex < 1 || dex > 151) {
        return {};
    }
    Text_CopySpeciesName(euc, dex);
    Text_TranscodeName(gb, euc);
    return Name((const uint8_t*)gb, sizeof(gb));
}

std::vector<uint8_t> WestSpeciesName(const Context& ctx, int internal) {
    return internal > 0 && internal < (int)ctx.speciesNames.size() ? ctx.speciesNames[internal] : std::vector<uint8_t>();
}

std::vector<uint8_t> NicknameWestToJp(const Context& ctx, int species, const std::vector<uint8_t>& w) {
    auto def = WestSpeciesName(ctx, species);
    if (!def.empty() && w == def) {
        auto j = JpSpeciesName(species);
        if (!j.empty()) return j;
    }
    return NameWestToJp(ctx, w);
}

// A name the Western save never had (a Pokémon Zero brought from another cartridge): Latin letters and digits
// carry over; anything else becomes the species name (nickname) or '?' (trainer).
std::vector<uint8_t> NameJpToWest(const Context& ctx, const std::vector<uint8_t>& j, int species, bool nickname) {
    if (nickname && j == JpSpeciesName(species)) {
        return WestSpeciesName(ctx, species);
    }
    std::vector<uint8_t> w;
    for (uint8_t c : j) {
        uint8_t b = JpCharToWest(c);
        if (b == 0) {
            if (nickname) return WestSpeciesName(ctx, species);
            b = 0xE6;
        }
        w.push_back(b);
    }
    return w;
}

// ---- Pokémon ----
struct Mon {
    std::vector<uint8_t> rec; // the 33-byte box record (or the 44-byte party record)
    std::vector<uint8_t> nick, ot; // Western names
    std::vector<uint8_t> jnick, jot; // Japanese names
    int box = -1;
};

// What identifies a Pokémon across Zero's moves: species, OT ID, experience, DVs, and its Japanese names.
std::string Key(const uint8_t* rec, const std::vector<uint8_t>& jnick, const std::vector<uint8_t>& jot) {
    std::string k;
    for (int off : { 0x00, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x1B, 0x1C }) k.push_back((char)rec[off]);
    k.push_back('|');
    k.append(jnick.begin(), jnick.end());
    k.push_back('|');
    k.append(jot.begin(), jot.end());
    return k;
}

void FillJpNames(Context& ctx, Mon& m) {
    m.jnick = NicknameWestToJp(ctx, m.rec[0], m.nick);
    m.jot = NameWestToJp(ctx, m.ot);
    ctx.names.insert({ Key(m.rec.data(), m.jnick, m.jot), { m.nick, m.ot } });
}

std::vector<Mon> ReadWestBoxes(Context& ctx, const std::vector<uint8_t>& w) {
    std::vector<Mon> mons;
    bool inUse = w[W_CURBOX] & 0x80;
    int cur = std::min(w[W_CURBOX] & 0x0F, W_BOXES - 1);
    for (int b = 0; b < W_BOXES; b++) {
        const uint8_t* p = b == cur ? &w[W_BOXDATA] : inUse ? &w[WBoxAt(b)] : nullptr;
        if (p == nullptr) continue;
        int n = std::min<int>(p[0], W_BOX_MONS);
        for (int i = 0; i < n; i++) {
            Mon m;
            m.rec.assign(p + 0x16 + i * REC, p + 0x16 + (i + 1) * REC);
            m.ot = Name(p + 0x2AA + i * W_NAME_LEN, W_NAME_LEN);
            m.nick = Name(p + 0x386 + i * W_NAME_LEN, W_NAME_LEN);
            m.box = b;
            FillJpNames(ctx, m);
            mons.push_back(m);
        }
    }
    return mons;
}

void WriteJpBox(uint8_t* q, const std::vector<Mon>& mons, size_t from, size_t n) {
    memset(q, 0xFF, J_BOX_SIZE);
    q[0] = (uint8_t)n;
    for (size_t i = 0; i < n; i++) {
        const Mon& m = mons[from + i];
        q[1 + i] = m.rec[0];
        memcpy(q + 0x20 + i * REC, m.rec.data(), REC);
        PutName(q + 0x3FE + i * J_NAME_LEN, m.jot, J_NAME_LEN);
        PutName(q + 0x4B2 + i * J_NAME_LEN, m.jnick, J_NAME_LEN);
    }
    q[1 + n] = 0xFF;
}

std::vector<Mon> ReadJpBox(const uint8_t* p, int box) {
    std::vector<Mon> mons;
    int n = std::min<int>(p[0], J_BOX_MONS);
    for (int i = 0; i < n; i++) {
        Mon m;
        m.rec.assign(p + 0x20 + i * REC, p + 0x20 + (i + 1) * REC);
        m.jot = Name(p + 0x3FE + i * J_NAME_LEN, J_NAME_LEN);
        m.jnick = Name(p + 0x4B2 + i * J_NAME_LEN, J_NAME_LEN);
        m.box = box;
        mons.push_back(m);
    }
    return mons;
}

// The Western names of a Pokémon read from the Japanese copy: its original ones if Zero did not change them.
void FillWestNames(const Context& ctx, Mon& m) {
    auto it = ctx.names.find(Key(m.rec.data(), m.jnick, m.jot));
    if (it != ctx.names.end()) {
        m.nick = it->second.first;
        m.ot = it->second.second;
    } else {
        m.nick = NameJpToWest(ctx, m.jnick, m.rec[0], true);
        m.ot = NameJpToWest(ctx, m.jot, m.rec[0], false);
    }
}

void WriteWestBox(uint8_t* q, const std::vector<Mon>& mons) {
    memset(q, 0, W_BOX_SIZE);
    memset(q + 1, 0xFF, W_BOX_MONS + 1);
    q[0] = (uint8_t)mons.size();
    for (size_t i = 0; i < mons.size(); i++) {
        const Mon& m = mons[i];
        q[1 + i] = m.rec[0];
        memcpy(q + 0x16 + i * REC, m.rec.data(), REC);
        PutName(q + 0x2AA + i * W_NAME_LEN, m.ot, W_NAME_LEN);
        PutName(q + 0x386 + i * W_NAME_LEN, m.nick, W_NAME_LEN);
    }
}

void JpBankChecksums(std::vector<uint8_t>& j) {
    for (int bank = 0; bank < 2; bank++) {
        int base = bank == 0 ? 0x4000 : 0x6000;
        j[base + J_BOX_SIZE * J_BOXES_PER_BANK] = Checksum(&j[base], J_BOX_SIZE * J_BOXES_PER_BANK);
        for (int i = 0; i < J_BOXES_PER_BANK; i++) {
            j[base + J_BOX_SIZE * J_BOXES_PER_BANK + 1 + i] = Checksum(&j[base + i * J_BOX_SIZE], J_BOX_SIZE);
        }
    }
}

void WestBankChecksums(std::vector<uint8_t>& w) {
    for (int bank = 0; bank < 2; bank++) {
        int base = bank == 0 ? 0x4000 : 0x6000;
        w[base + W_BOX_SIZE * W_BOXES_PER_BANK] = Checksum(&w[base], W_BOX_SIZE * W_BOXES_PER_BANK);
        for (int i = 0; i < W_BOXES_PER_BANK; i++) {
            w[base + W_BOX_SIZE * W_BOXES_PER_BANK + 1 + i] = Checksum(&w[base + i * W_BOX_SIZE], W_BOX_SIZE);
        }
    }
}

bool Changed(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b, int from, int to) {
    return memcmp(&a[from], &b[from], to - from) != 0;
}

} // namespace

bool IsWesternGen1(const std::vector<uint8_t>& rom) {
    if (rom.size() < 0x8000 || rom[0x14A] != 1) {
        return false;
    }
    const char* t = (const char*)&rom[0x134];
    return memcmp(t, "POKEMON RED", 11) == 0 || memcmp(t, "POKEMON BLUE", 12) == 0 || memcmp(t, "POKEMON YEL", 11) == 0;
}

void Init(Context& ctx, const std::vector<uint8_t>& rom) {
    // The species name table: 190 names of 10 bytes, padded with 0x50, from internal index 1 (RHYDON, RIZEROS in
    // German, RHINOFEROS in French).
    static const std::vector<std::vector<uint8_t>> kFirst = {
        { 0x91, 0x87, 0x98, 0x83, 0x8E, 0x8D },                         // RHYDON
        { 0x91, 0x88, 0x99, 0x84, 0x91, 0x8E, 0x92 },                   // RIZEROS
        { 0x91, 0x87, 0x88, 0x8D, 0x8E, 0x85, 0x84, 0x91, 0x8E, 0x92 }, // RHINOFEROS
    };
    ctx.speciesNames.clear();
    for (const auto& first : kFirst) {
        std::vector<uint8_t> pat(10, TERM);
        std::copy(first.begin(), first.end(), pat.begin());
        auto it = std::search(rom.begin(), rom.end(), pat.begin(), pat.end());
        if (it == rom.end() || rom.end() - it < 190 * 10) {
            continue;
        }
        ctx.speciesNames.assign(1, {});
        for (int i = 0; i < 190; i++) {
            ctx.speciesNames.push_back(Name(&*(it + i * 10), 10));
        }
        break;
    }
    if (ctx.speciesNames.empty()) {
        fprintf(stderr, "[gbpak] species names not found in the cartridge: nicknames are carried letter by letter\n");
    }
}

void WestToJp(Context& ctx, const std::vector<uint8_t>& w, std::vector<uint8_t>& j) {
    Port_GbCharsetExtend(D_80076FD4_jp);
    ctx.names.clear();
    j.assign(w.size(), 0xFF);
    memcpy(&j[0], &w[0], J_NAME); // bank 0 and the start of bank 1 share the layout
    PutName(&j[J_NAME], NameWestToJp(ctx, Name(&w[W_NAME], W_NAME_LEN)), J_NAME_LEN);
    memcpy(&j[J_MAIN], &w[W_MAIN], J_RIVAL - J_MAIN); // Pokédex, bag, money
    PutName(&j[J_RIVAL], NameWestToJp(ctx, Name(&w[W_RIVAL], W_NAME_LEN)), J_NAME_LEN);
    memcpy(&j[J_AFTER_RIVAL], &w[W_AFTER_RIVAL], J_SPRITES - J_AFTER_RIVAL);
    memcpy(&j[J_SPRITES], &w[W_SPRITES], J_PARTY - J_SPRITES);

    // party
    const uint8_t* wp = &w[W_PARTY];
    uint8_t* jp = &j[J_PARTY];
    int n = std::min<int>(wp[0], 6);
    memset(jp, 0xFF, J_BOXDATA - J_PARTY);
    jp[0] = (uint8_t)n;
    for (int i = 0; i < n; i++) {
        Mon m;
        m.rec.assign(wp + 8 + i * PARTY_REC, wp + 8 + (i + 1) * PARTY_REC);
        m.ot = Name(wp + 0x110 + i * W_NAME_LEN, W_NAME_LEN);
        m.nick = Name(wp + 0x152 + i * W_NAME_LEN, W_NAME_LEN);
        FillJpNames(ctx, m);
        jp[1 + i] = m.rec[0];
        memcpy(jp + 8 + i * PARTY_REC, m.rec.data(), PARTY_REC);
        PutName(jp + 0x110 + i * J_NAME_LEN, m.jot, J_NAME_LEN);
        PutName(jp + 0x134 + i * J_NAME_LEN, m.jnick, J_NAME_LEN);
    }
    jp[1 + n] = 0xFF;

    // boxes, packed in order; box 1 is the current one
    std::vector<Mon> mons = ReadWestBoxes(ctx, w);
    for (int b = 0; b < J_BOXES; b++) {
        size_t from = std::min(mons.size(), (size_t)b * J_BOX_MONS);
        size_t count = std::min(mons.size() - from, (size_t)J_BOX_MONS);
        WriteJpBox(&j[JBoxAt(b)], mons, from, count);
        if (b == 0) {
            WriteJpBox(&j[J_BOXDATA], mons, from, count);
        }
    }
    j[J_CURBOX] = 0x80;
    j[J_CHECKSUM] = Checksum(&j[J_NAME], J_CHECKSUM - J_NAME);
    JpBankChecksums(j);
    ctx.base = j;
    fprintf(stderr, "[gbpak] Western save shown in the Japanese layout: %d in the party, %zu in the boxes\n", n,
            mons.size());
}

bool JpToWest(Context& ctx, const std::vector<uint8_t>& j, std::vector<uint8_t>& w) {
    if (ctx.base.size() != j.size()) {
        ctx.base = j;
        return false;
    }
    std::vector<uint8_t> before = w;
    const std::vector<uint8_t>& b = ctx.base;
    // bytes in the shared, shifted-by-5 and shifted-by-10 parts (names are Zero's to read, not to change)
    for (int i = 0; i < J_NAME; i++) if (j[i] != b[i]) w[i] = j[i];
    for (int i = J_MAIN; i < J_RIVAL; i++) if (j[i] != b[i]) w[i + (W_MAIN - J_MAIN)] = j[i];
    for (int i = J_AFTER_RIVAL; i < J_SHIFTED_END; i++) {
        if (i != J_CURBOX && j[i] != b[i]) w[i + (W_AFTER_RIVAL - J_AFTER_RIVAL)] = j[i];
    }

    if (Changed(j, b, J_PARTY, J_BOXDATA)) {
        const uint8_t* jp = &j[J_PARTY];
        uint8_t* wp = &w[W_PARTY];
        int n = std::min<int>(jp[0], 6);
        memset(wp, 0xFF, W_BOXDATA - W_PARTY);
        wp[0] = (uint8_t)n;
        for (int i = 0; i < n; i++) {
            Mon m;
            m.rec.assign(jp + 8 + i * PARTY_REC, jp + 8 + (i + 1) * PARTY_REC);
            m.jot = Name(jp + 0x110 + i * J_NAME_LEN, J_NAME_LEN);
            m.jnick = Name(jp + 0x134 + i * J_NAME_LEN, J_NAME_LEN);
            FillWestNames(ctx, m);
            wp[1 + i] = m.rec[0];
            memcpy(wp + 8 + i * PARTY_REC, m.rec.data(), PARTY_REC);
            PutName(wp + 0x110 + i * W_NAME_LEN, m.ot, W_NAME_LEN);
            PutName(wp + 0x152 + i * W_NAME_LEN, m.nick, W_NAME_LEN);
        }
        wp[1 + n] = 0xFF;
    }

    if (Changed(j, b, J_BOXDATA, J_CHECKSUM) || Changed(j, b, 0x4000, 0x8000) || j[J_CURBOX] != b[J_CURBOX]) {
        // where each Pokémon was, by identity
        Context scratch;
        scratch.speciesNames = ctx.speciesNames;
        std::multimap<std::string, int> origin;
        for (const Mon& m : ReadWestBoxes(scratch, w)) {
            origin.insert({ Key(m.rec.data(), m.jnick, m.jot), m.box });
        }
        int jcur = std::min(j[J_CURBOX] & 0x0F, J_BOXES - 1);
        std::vector<std::vector<Mon>> boxes(W_BOXES);
        std::vector<Mon> homeless;
        for (int k = 0; k < J_BOXES; k++) {
            for (Mon& m : ReadJpBox(k == jcur ? &j[J_BOXDATA] : &j[JBoxAt(k)], k)) {
                FillWestNames(ctx, m);
                auto it = origin.find(Key(m.rec.data(), m.jnick, m.jot));
                if (it != origin.end() && boxes[it->second].size() < W_BOX_MONS) {
                    boxes[it->second].push_back(m);
                    origin.erase(it);
                } else {
                    homeless.push_back(m);
                }
            }
        }
        for (Mon& m : homeless) {
            for (auto& box : boxes) {
                if (box.size() < W_BOX_MONS) {
                    box.push_back(m);
                    break;
                }
            }
        }
        int cur = std::min(w[W_CURBOX] & 0x0F, W_BOXES - 1);
        bool others = false;
        for (int k = 0; k < W_BOXES; k++) others |= k != cur && !boxes[k].empty();
        if (others || (w[W_CURBOX] & 0x80)) {
            w[W_CURBOX] |= 0x80;
            for (int k = 0; k < W_BOXES; k++) WriteWestBox(&w[WBoxAt(k)], boxes[k]);
            WestBankChecksums(w);
        }
        WriteWestBox(&w[W_BOXDATA], boxes[cur]);
    }

    ctx.base = j;
    if (w == before) {
        return false;
    }
    w[W_CHECKSUM] = Checksum(&w[W_NAME], W_CHECKSUM - W_NAME);
    return true;
}

} // namespace GbSaveConvert

extern "C" void Port_GbCharsetExtend(uint16_t* table) {
    using namespace GbSaveConvert;
    for (char c = 'O'; c <= 'Z'; c++) {
        uint8_t code = LatinToJp(c);
        if (code < 0x60 || code > 0x79) table[code] = 0xA3C1 + (c - 'A'); // Ｏ Ｐ Ｗ Ｘ Ｙ
    }
    for (char c = 'a'; c <= 'z'; c++) table[LatinToJp(c)] = 0xA3E1 + (c - 'a'); // ａ-ｚ
}
