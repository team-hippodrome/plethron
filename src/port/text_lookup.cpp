// Runtime text translation (see text_lookup.h and translations/README.md).
#include "text_lookup.h"
#include "zero_lift.h"
#include "unicode_font.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <libultraship/libultraship.h>

extern "C" void* ZLift_Resolve(uint32_t addr);

namespace {

// ---- md5 (RFC 1321) ----
struct Md5 {
    uint32_t a = 0x67452301, b = 0xefcdab89, c = 0x98badcfe, d = 0x10325476;
    uint64_t len = 0;
    uint8_t buf[64];
    size_t n = 0;

    static uint32_t Rol(uint32_t x, int s) { return (x << s) | (x >> (32 - s)); }

    void Block(const uint8_t* p) {
        static const uint32_t K[64] = {
            0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
            0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
            0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
            0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
            0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
            0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
            0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
            0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391,
        };
        static const int S[64] = { 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
                                   5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20,
                                   4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
                                   6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21 };
        uint32_t m[16];
        for (int i = 0; i < 16; i++) {
            m[i] = p[i * 4] | (p[i * 4 + 1] << 8) | (p[i * 4 + 2] << 16) | ((uint32_t)p[i * 4 + 3] << 24);
        }
        uint32_t A = a, B = b, C = c, D = d;
        for (int i = 0; i < 64; i++) {
            uint32_t f;
            int g;
            if (i < 16) {
                f = (B & C) | (~B & D);
                g = i;
            } else if (i < 32) {
                f = (D & B) | (~D & C);
                g = (5 * i + 1) & 15;
            } else if (i < 48) {
                f = B ^ C ^ D;
                g = (3 * i + 5) & 15;
            } else {
                f = C ^ (B | ~D);
                g = (7 * i) & 15;
            }
            uint32_t t = D;
            D = C;
            C = B;
            B = B + Rol(A + f + K[i] + m[g], S[i]);
            A = t;
        }
        a += A;
        b += B;
        c += C;
        d += D;
    }

    void Update(const uint8_t* p, size_t l) {
        len += l;
        while (l > 0) {
            size_t k = std::min(l, 64 - n);
            memcpy(buf + n, p, k);
            n += k;
            p += k;
            l -= k;
            if (n == 64) {
                Block(buf);
                n = 0;
            }
        }
    }

    void Final(uint8_t out[16]) {
        uint64_t bits = len * 8;
        uint8_t pad = 0x80;
        Update(&pad, 1);
        pad = 0;
        while (n != 56) {
            Update(&pad, 1);
        }
        uint8_t l8[8];
        for (int i = 0; i < 8; i++) {
            l8[i] = (uint8_t)(bits >> (8 * i));
        }
        Update(l8, 8);
        uint32_t w[4] = { a, b, c, d };
        for (int i = 0; i < 16; i++) {
            out[i] = (uint8_t)(w[i / 4] >> (8 * (i % 4)));
        }
    }
};

const char* const kColumns[PORT_LANG_COUNT] = { "", "ja", "en", "es", "de", "pt", "ko" };
const char* const kNames[PORT_LANG_COUNT] = {
    "にほんご", "日本語", "English", "Español", "Deutsch", "Português", "한국어",
};

struct Row {
    std::vector<std::string> text; // one UTF-8 string per language (empty = falls through)
    std::vector<uint32_t> addrs;   // N64 link addresses of the original string
};
struct CopyNote {
    std::string id;
    std::string bytes; // what the copy held when noted: a later write to the buffer drops the note
};
struct Tables {
    // id -> row, per file. human.csv wins over ai.csv for a non-empty cell.
    std::unordered_map<std::string, Row> human, ai;
    // md5 of the original bytes -> ids with those bytes, in file order (ai.csv first). The first is the fallback.
    std::unordered_map<std::string, std::vector<std::string>> md5Ids;
    // host address of a string -> id, for strings bound through their addr column or noted copies
    std::unordered_map<const void*, std::string> bound;
    std::unordered_map<const void*, CopyNote> copies;
    // host address of a buffer the game formatted itself -> {bytes it held, translated UTF-8}
    std::unordered_map<const void*, std::pair<std::string, std::string>> formatted;
    std::unordered_set<std::string> missed;
    std::mutex mutex;
    FILE* missLog = nullptr;
    bool loaded = false;
};
Tables& T() {
    static Tables t;
    return t;
}
int sLanguage = PORT_LANG_ORIGINAL;

// Decodes the \xNN escapes the tables use for non-printing bytes.
std::string Unescape(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '\\' && i + 3 < s.size() + 0 && s[i + 1] == 'x' && isxdigit((uint8_t)s[i + 2]) &&
            isxdigit((uint8_t)s[i + 3])) {
            out += (char)strtol(s.substr(i + 2, 2).c_str(), nullptr, 16);
            i += 3;
        } else {
            out += s[i];
        }
    }
    return out;
}

// RFC 4180 CSV: quoted fields may hold commas, newlines and doubled quotes.
std::vector<std::vector<std::string>> ParseCsv(const std::string& text) {
    std::vector<std::vector<std::string>> rows(1);
    std::string field;
    bool quoted = false;
    for (size_t i = 0; i < text.size(); i++) {
        char ch = text[i];
        if (quoted) {
            if (ch == '"') {
                if (i + 1 < text.size() && text[i + 1] == '"') {
                    field += '"';
                    i++;
                } else {
                    quoted = false;
                }
            } else {
                field += ch;
            }
        } else if (ch == '"') {
            quoted = true;
        } else if (ch == ',') {
            rows.back().push_back(field);
            field.clear();
        } else if (ch == '\n' || ch == '\r') {
            if (ch == '\r' && i + 1 < text.size() && text[i + 1] == '\n') {
                i++;
            }
            rows.back().push_back(field);
            field.clear();
            rows.emplace_back();
        } else {
            field += ch;
        }
    }
    if (!field.empty() || !rows.back().empty()) {
        rows.back().push_back(field);
    } else {
        rows.pop_back();
    }
    return rows;
}

void LoadTable(const char* name, std::unordered_map<std::string, Row>& into,
               std::unordered_map<std::string, std::vector<std::string>>& md5Ids) {
    std::string path = Ship::Context::LocateFileAcrossAppDirs(std::string("translations/") + name);
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        fprintf(stderr, "[text] %s not found (%s)\n", name, path.c_str());
        return;
    }
    std::stringstream ss;
    ss << f.rdbuf();
    // Line breaks inside quoted fields are part of the text. A checkout with CRLF line endings (git's core.autocrlf on
    // Windows) turns each into CR LF, and the CR was kept in the string and drawn as "?" at the end of every line.
    std::string text = ss.str();
    std::string lf;
    lf.reserve(text.size());
    for (size_t i = 0; i < text.size(); i++) {
        if (text[i] == '\r' && i + 1 < text.size() && text[i + 1] == '\n') {
            continue;
        }
        lf += text[i];
    }
    auto rows = ParseCsv(lf);
    if (rows.empty()) {
        return;
    }
    // Columns by header name: id,md5,addr,<languages>,note (translations/README.md)
    int col[PORT_LANG_COUNT];
    int idCol = -1, md5Col = -1, addrCol = -1;
    for (size_t c = 0; c < rows[0].size(); c++) {
        const std::string& h = rows[0][c];
        if (h == "id") idCol = (int)c;
        if (h == "md5") md5Col = (int)c;
        if (h == "addr") addrCol = (int)c;
    }
    for (int l = 0; l < PORT_LANG_COUNT; l++) {
        col[l] = -1;
        for (size_t c = 0; c < rows[0].size(); c++) {
            if (l != 0 && rows[0][c] == kColumns[l]) {
                col[l] = (int)c;
            }
        }
    }
    if (idCol < 0 || md5Col < 0) {
        fprintf(stderr, "[text] %s: no id/md5 columns, skipped\n", name);
        return;
    }
    size_t count = 0;
    for (size_t r = 1; r < rows.size(); r++) {
        const auto& row = rows[r];
        if ((int)row.size() <= std::max(idCol, md5Col) || row[idCol].empty() || row[md5Col].size() != 32) {
            continue;
        }
        Row v;
        v.text.resize(PORT_LANG_COUNT);
        for (int l = 1; l < PORT_LANG_COUNT; l++) {
            if (col[l] >= 0 && col[l] < (int)row.size()) {
                v.text[l] = Unescape(row[col[l]]);
            }
        }
        if (addrCol >= 0 && addrCol < (int)row.size()) {
            std::istringstream as(row[addrCol]);
            std::string w;
            while (as >> w) {
                v.addrs.push_back((uint32_t)strtoul(w.c_str(), nullptr, 16));
            }
        }
        auto& ids = md5Ids[row[md5Col]];
        if (std::find(ids.begin(), ids.end(), row[idCol]) == ids.end()) {
            ids.push_back(row[idCol]);
        }
        into[row[idCol]] = std::move(v);
        count++;
    }
    fprintf(stderr, "[text] %s: %zu strings\n", name, count);
}

// The row's text in the current language: human.csv first, then ai.csv. NULL when both cells are empty.
const char* TextOf(Tables& t, const std::string& id) {
    for (auto* table : { &t.human, &t.ai }) {
        auto it = table->find(id);
        if (it != table->end() && !it->second.text[sLanguage].empty()) {
            return it->second.text[sLanguage].c_str(); // stable: the tables are never modified after loading
        }
    }
    return nullptr;
}

const Row* RowOf(Tables& t, const std::string& id) {
    auto it = t.ai.find(id);
    if (it != t.ai.end()) return &it->second;
    it = t.human.find(id);
    return it != t.human.end() ? &it->second : nullptr;
}

// Which string `s` is. Identity first: a noted copy, or the host address of one of the rows' N64 addresses. The md5
// of the bytes is the fallback, and its first row wins when several rows share those bytes.
const std::string* IdOf(Tables& t, const char* s, const std::string& md5) {
    auto c = t.copies.find(s);
    if (c != t.copies.end()) {
        if (c->second.bytes == s) {
            return &c->second.id;
        }
        t.copies.erase(c);
    }
    auto b = t.bound.find(s);
    if (b != t.bound.end()) {
        return &b->second;
    }
    auto m = t.md5Ids.find(md5);
    if (m == t.md5Ids.end()) {
        return nullptr;
    }
    if (m->second.size() > 1) {
        for (const std::string& id : m->second) {
            const Row* row = RowOf(t, id);
            for (uint32_t a : row ? row->addrs : std::vector<uint32_t>()) {
                if (ZLift_Resolve(a) == (const void*)s) {
                    return &(t.bound[s] = id);
                }
            }
        }
    }
    return &m->second[0];
}


// Text with no translation is still EUC-JP (names from Game Boy saves, mostly): drawn in a translated language it
// is turned into UTF-8 for the Unicode font. Kana and the symbols names use are mapped; full-width Latin letters and
// digits become ASCII, so a Western cartridge's names read as typed (gb_save_convert.h).
std::string EucToUtf8(const char* s) {
    std::string out;
    auto put = [&](uint32_t cp) {
        if (cp < 0x80) {
            out += (char)cp;
        } else if (cp < 0x800) {
            out += (char)(0xC0 | (cp >> 6));
            out += (char)(0x80 | (cp & 0x3F));
        } else {
            out += (char)(0xE0 | (cp >> 12));
            out += (char)(0x80 | ((cp >> 6) & 0x3F));
            out += (char)(0x80 | (cp & 0x3F));
        }
    };
    for (const uint8_t* p = (const uint8_t*)s; *p != 0; p++) {
        if (*p < 0x80 || p[1] == 0) {
            put(*p < 0x80 ? *p : '?');
            continue;
        }
        uint16_t c = (uint16_t)((p[0] << 8) | p[1]);
        p++;
        if (c >= 0xA3B0 && c <= 0xA3FA) {
            put(c - 0xA3B0 + '0'); // ０-９ Ａ-Ｚ ａ-ｚ
        } else if (c >= 0xA4A1 && c <= 0xA4F3) {
            put(0x3041 + (c - 0xA4A1)); // hiragana
        } else if (c >= 0xA5A1 && c <= 0xA5F6) {
            put(0x30A1 + (c - 0xA5A1)); // katakana
        } else {
            switch (c) {
                case 0xA1A1: put(' '); break;
                case 0xA1A2: put(0x3001); break; // 、
                case 0xA1A3: put(0x3002); break; // 。
                case 0xA1A4: put(','); break;
                case 0xA1A5: put('.'); break;
                case 0xA1A6: put(0x30FB); break; // ・
                case 0xA1A7: put(':'); break;
                case 0xA1A9: put('?'); break;
                case 0xA1AA: put('!'); break;
                case 0xA1BC: put(0x30FC); break; // ー
                case 0xA1BD: case 0xA1DD: put('-'); break;
                case 0xA1C7: put('\''); break;
                case 0xA1BF: put('/'); break;
                case 0xA1CA: put('('); break;
                case 0xA1CB: put(')'); break;
                case 0xA1DC: put('+'); break;
                case 0xA1E1: put('='); break;
                case 0xA1E3: put('<'); break;
                case 0xA1E4: put('>'); break;
                case 0xA1F3: put('%'); break;
                case 0xA1F4: put('#'); break;
                case 0xA1F5: put('&'); break;
                case 0xA1F6: put('*'); break; // the order screen's button hints
                case 0xA1F7: put('@'); break;
                case 0xA1E9: put(0x2642); break; // ♂
                case 0xA1EA: put(0x2640); break; // ♀
                default: put('?'); break;
            }
        }
    }
    return out;
}

bool HasHighBytes(const char* s) {
    for (const uint8_t* p = (const uint8_t*)s; *p != 0; p++) {
        if (*p >= 0x80) return true;
    }
    return false;
}

void LogMiss(const std::string& key, const char* s) {
    Tables& t = T();
    if (!t.missed.insert(key).second) {
        return;
    }
    if (t.missLog == nullptr) {
        std::string path = Ship::Context::GetPathRelativeToAppDirectory("text_misses.csv");
        t.missLog = fopen(path.c_str(), "a");
        if (t.missLog == nullptr) {
            return;
        }
    }
    fprintf(t.missLog, "%s,%s,", key.c_str(), kColumns[sLanguage]);
    for (const uint8_t* p = (const uint8_t*)s; *p != 0; p++) {
        fprintf(t.missLog, "%02x", *p);
    }
    fprintf(t.missLog, ",%08X\n", ZLift_N64Addr(s)); // where the text lives on the N64 (0 when not lifted data)
    fflush(t.missLog);
}

static FILE* sOverflowLog;
static std::unordered_set<std::string> sOverflowed;

extern "C" void Port_Text_LogOverflow(const char* utf8, int width, int avail) {
    if (utf8 == nullptr) {
        return;
    }
    std::string line(utf8, strcspn(utf8, "\n"));
    std::lock_guard<std::mutex> lock(T().mutex);
    if (!sOverflowed.insert(std::string(kColumns[sLanguage]) + ":" + line).second) {
        return;
    }
    if (sOverflowLog == nullptr) {
        std::string path = Ship::Context::GetPathRelativeToAppDirectory("text_overflows.csv");
        sOverflowLog = fopen(path.c_str(), "a");
        if (sOverflowLog == nullptr) {
            return;
        }
    }
    std::string quoted;
    for (char c : line) {
        quoted += (c == '"') ? std::string("\"\"") : std::string(1, c);
    }
    fprintf(sOverflowLog, "%s,%d,%d,\"%s\"\n", kColumns[sLanguage], width, avail, quoted.c_str());
    fflush(sOverflowLog);
}

// ---- printf with translated pieces ----
enum ArgKind { ARG_NONE, ARG_INT, ARG_LONG, ARG_DOUBLE, ARG_PTR };
struct Arg {
    ArgKind kind = ARG_NONE;
    long long i = 0;
    double d = 0;
    const void* p = nullptr;
    bool isString = false;
};
struct Spec {
    size_t start, end; // whole spec in the format, including '%'
    int pos;           // 1-based argument index, or 0 for '%%'
    char conv;
    int widthArg, precArg; // 1-based index of a '*' argument, or 0
};

// Parses the conversions of `fmt`. Positional (%n$) and sequential forms may not be mixed, as in POSIX.
std::vector<Spec> ParseFormat(const char* fmt) {
    std::vector<Spec> specs;
    int next = 1;
    for (size_t i = 0; fmt[i] != 0; i++) {
        if (fmt[i] != '%') {
            continue;
        }
        Spec s{};
        s.start = i;
        size_t j = i + 1;
        if (fmt[j] == '%') {
            s.end = j + 1;
            s.conv = '%';
            specs.push_back(s);
            i = j;
            continue;
        }
        int pos = 0;
        size_t k = j;
        while (isdigit((uint8_t)fmt[k])) {
            pos = pos * 10 + (fmt[k] - '0');
            k++;
        }
        if (fmt[k] == '$' && pos > 0) {
            j = k + 1;
        } else {
            pos = 0;
        }
        while (strchr("-+ #0'", fmt[j]) && fmt[j] != 0) {
            j++;
        }
        if (fmt[j] == '*') {
            s.widthArg = next++;
            j++;
        } else {
            while (isdigit((uint8_t)fmt[j])) {
                j++;
            }
        }
        if (fmt[j] == '.') {
            j++;
            if (fmt[j] == '*') {
                s.precArg = next++;
                j++;
            } else {
                while (isdigit((uint8_t)fmt[j])) {
                    j++;
                }
            }
        }
        while (fmt[j] == 'h' || fmt[j] == 'l' || fmt[j] == 'L' || fmt[j] == 'z' || fmt[j] == 'j' || fmt[j] == 't') {
            j++;
        }
        if (fmt[j] == 0) {
            break;
        }
        s.conv = fmt[j];
        s.end = j + 1;
        s.pos = pos ? pos : next++;
        specs.push_back(s);
        i = j;
    }
    return specs;
}

bool HasLong(const char* fmt, const Spec& s) {
    for (size_t i = s.start; i < s.end; i++) {
        if (fmt[i] == 'l' || fmt[i] == 'j' || fmt[i] == 'z' || fmt[i] == 't') {
            return true;
        }
    }
    return false;
}

std::string FormatOne(const char* fmt, const Spec& s, const std::vector<Arg>& args) {
    // Rebuild the spec without the positional part and with '*' resolved.
    std::string spec = "%";
    size_t j = s.start + 1;
    size_t k = j;
    while (isdigit((uint8_t)fmt[k])) {
        k++;
    }
    if (fmt[k] == '$') {
        j = k + 1;
    }
    for (; j < s.end; j++) {
        if (fmt[j] == '*') {
            int idx = (spec.find('.') != std::string::npos) ? s.precArg : s.widthArg;
            spec += std::to_string(idx > 0 && idx <= (int)args.size() ? (int)args[idx - 1].i : 0);
        } else {
            spec += fmt[j];
        }
    }
    const Arg& a = args[s.pos - 1];
    char buf[512];
    switch (s.conv) {
        case 's':
            snprintf(buf, sizeof(buf), spec.c_str(), a.p ? (const char*)a.p : "");
            break;
        case 'p':
            snprintf(buf, sizeof(buf), spec.c_str(), a.p);
            break;
        case 'f': case 'F': case 'e': case 'E': case 'g': case 'G':
            snprintf(buf, sizeof(buf), spec.c_str(), a.d);
            break;
        default:
            if (a.kind == ARG_LONG) {
                snprintf(buf, sizeof(buf), spec.c_str(), (long)a.i);
            } else {
                snprintf(buf, sizeof(buf), spec.c_str(), (int)a.i);
            }
            break;
    }
    return buf;
}

} // namespace

extern "C" {

void Port_Text_Md5Hex(const void* data, size_t len, char hex[33]) {
    Md5 m;
    uint8_t d[16];
    m.Update((const uint8_t*)data, len);
    m.Final(d);
    for (int i = 0; i < 16; i++) {
        snprintf(hex + i * 2, 3, "%02x", d[i]);
    }
}

void Port_Text_Init(void) {
    Tables& t = T();
    std::lock_guard<std::mutex> lock(t.mutex);
    if (t.loaded) {
        return;
    }
    t.loaded = true;
    LoadTable("ai.csv", t.ai, t.md5Ids);
    LoadTable("human.csv", t.human, t.md5Ids);
}

void Port_Text_SetLanguage(int lang) {
    if (lang >= 0 && lang < PORT_LANG_COUNT) {
        sLanguage = lang;
        if (lang != PORT_LANG_ORIGINAL) {
            Port_Text_Init();
        }
    }
}

int Port_Text_GetLanguage(void) {
    // Debug aid: PORT_LANGUAGE=<column> (en, ja, ...) starts in that language without the language select screen;
    // PORT_LANGUAGE=kana starts in the original text (as on the N64: tools/screen_diff compares with ares).
    static bool sEnvChecked = false;
    if (!sEnvChecked) {
        sEnvChecked = true;
        if (const char* env = getenv("PORT_LANGUAGE")) {
            if (strcmp(env, "kana") == 0) {
                Port_Text_SetLanguage(PORT_LANG_ORIGINAL);
            }
            for (int l = 1; l < PORT_LANG_COUNT; l++) {
                if (strcmp(env, kColumns[l]) == 0) {
                    Port_Text_SetLanguage(l);
                }
            }
        }
    }
    return sLanguage;
}

int Port_Text_LanguageForced(void) {
    Port_Text_GetLanguage();
    return getenv("PORT_LANGUAGE") != nullptr;
}

// The language select screen remembers the last choice (and whether the AI languages were unlocked) in the port's
// settings, so the cursor starts there on the next boot.
int Port_Text_SavedLanguage(void) {
    int lang = CVarGetInteger("gPort.Language", PORT_LANG_ORIGINAL);
    return (lang >= 0 && lang < PORT_LANG_COUNT) ? lang : PORT_LANG_ORIGINAL;
}

void Port_Text_SaveLanguage(int lang) {
    CVarSetInteger("gPort.Language", lang);
    CVarSave();
}

int Port_Subtitles_Enabled(void) {
    return CVarGetInteger("gPort.Subtitles", 1) != 0;
}

int Port_PokedexNameOrder_Enabled(void) {
    return CVarGetInteger("gPort.PokedexNameOrder", 1) != 0;
}

void Port_Subtitles_SetEnabled(int on) {
    CVarSetInteger("gPort.Subtitles", on ? 1 : 0);
    CVarSave();
}

const char* Port_Text_LanguageName(int lang) {
    return (lang >= 0 && lang < PORT_LANG_COUNT) ? kNames[lang] : "";
}

const char* Port_Text_Lookup(const char* s) {
    if (sLanguage == PORT_LANG_ORIGINAL || s == nullptr || *s == 0) {
        return nullptr;
    }
    char hex[33];
    Port_Text_Md5Hex(s, strlen(s), hex);
    Tables& t = T();
    std::lock_guard<std::mutex> lock(t.mutex);
    auto f = t.formatted.find(s);
    if (f != t.formatted.end()) {
        if (f->second.first == s) {
            return f->second.second.c_str();
        }
        t.formatted.erase(f);
    }
    std::string key(hex);
    if (const std::string* id = IdOf(t, s, key)) {
        if (const char* text = TextOf(t, *id)) {
            return text;
        }
    }
    // Strings with no letters to translate (numbers, punctuation, single ASCII symbols) are not worth logging.
    bool wordy = false;
    for (const uint8_t* p = (const uint8_t*)s; *p != 0; p++) {
        if (*p == '%') { // a conversion's letters are not words
            p++;
            while (*p != 0 && strchr("0123456789$-+ #.*hlLzjt", *p)) {
                p++;
            }
            if (*p == 0) {
                break;
            }
            continue;
        }
        if (*p >= 0x80 || isalpha(*p)) {
            wordy = true;
            break;
        }
    }
    if (wordy) {
        LogMiss(key, s);
    }
    return nullptr;
}

const char* Port_Text_LookupId(const char* id) {
    if (sLanguage == PORT_LANG_ORIGINAL || id == nullptr) {
        return nullptr;
    }
    Tables& t = T();
    std::lock_guard<std::mutex> lock(t.mutex);
    return TextOf(t, id);
}

void Port_Text_NoteCopy(const char* dst, const char* src) {
    if (sLanguage == PORT_LANG_ORIGINAL || dst == nullptr || src == nullptr || *src == 0) {
        return;
    }
    char hex[33];
    Port_Text_Md5Hex(src, strlen(src), hex);
    Tables& t = T();
    std::lock_guard<std::mutex> lock(t.mutex);
    if (const std::string* id = IdOf(t, src, hex)) {
        t.copies[dst] = CopyNote{ *id, std::string(dst) };
    } else {
        t.copies.erase(dst);
    }
}

void Port_Text_NoteFormatted(const char* dst, const char* fmt, va_list ap) {
    if (sLanguage == PORT_LANG_ORIGINAL || dst == nullptr || fmt == nullptr) {
        return;
    }
    char out[0x200];
    Port_Text_Vformat(out, sizeof(out), fmt, ap);
    Tables& t = T();
    std::lock_guard<std::mutex> lock(t.mutex);
    t.formatted[dst] = { std::string(dst), std::string(out) };
}

void Port_Text_NoteSame(const char* dst, const char* src) {
    if (sLanguage == PORT_LANG_ORIGINAL || dst == nullptr || src == nullptr) {
        return;
    }
    const char* u = Port_Text_Utf8Of(src);
    Tables& t = T();
    std::lock_guard<std::mutex> lock(t.mutex);
    t.formatted[dst] = { std::string(dst), std::string(u ? u : "") };
}

const char* Port_Text_Utf8Of(const char* s) {
    if (sLanguage == PORT_LANG_ORIGINAL || s == nullptr) {
        return nullptr;
    }
    if (*s == 0) {
        return "";
    }
    if (const char* tr = Port_Text_Lookup(s)) {
        return tr;
    }
    std::string u = HasHighBytes(s) ? EucToUtf8(s) : std::string(s);
    Tables& t = T();
    std::lock_guard<std::mutex> lock(t.mutex);
    auto& slot = t.formatted[s];
    slot = { std::string(s), u };
    return slot.second.c_str();
}

int Port_Text_SelfTest(void) {
    int failed = 0;
    auto check = [&](const char* what, const char* got, const char* want) {
        bool ok = got != nullptr && strcmp(got, want) == 0;
        printf("%s %s: %s\n", ok ? "ok  " : "FAIL", what, got ? got : "(null)");
        failed += !ok;
    };
    Port_Text_SetLanguage(PORT_LANG_EN);
    check("id HAUNTER_NAME", Port_Text_LookupId("HAUNTER_NAME"), "Haunter");
    check("id GHOST_TYPE_NAME", Port_Text_LookupId("GHOST_TYPE_NAME"), "Ghost");
    // the same kana bytes at two addresses: told apart by address
    const char* haunter = (const char*)ZLift_Resolve(0x8007A240);
    const char* ghost = (const char*)ZLift_Resolve(0x8007A810);
    check("addr 8007A240", Port_Text_Lookup(haunter), "Haunter");
    check("addr 8007A810", Port_Text_Lookup(ghost), "Ghost");
    // a copy keeps its source's id; an unknown buffer with the same bytes falls back to the first md5 row
    char copy[32], other[32];
    strcpy(copy, ghost);
    Port_Text_NoteCopy(copy, ghost);
    strcpy(other, ghost);
    check("copy of 8007A810", Port_Text_Lookup(copy), "Ghost");
    check("md5 fallback", Port_Text_Lookup(other), "Haunter");
    // a unique string by md5
    const char* bulbasaurEntry = (const char*)ZLift_Resolve(0x80060A30);
    const char* got = Port_Text_Lookup(bulbasaurEntry);
    if (got == nullptr && bulbasaurEntry != nullptr) {
        char hex[33];
        Port_Text_Md5Hex(bulbasaurEntry, strlen(bulbasaurEntry), hex);
        printf("     80060A30 -> %p md5 %s first bytes %02x %02x %02x %02x\n", (const void*)bulbasaurEntry, hex,
               (uint8_t)bulbasaurEntry[0], (uint8_t)bulbasaurEntry[1], (uint8_t)bulbasaurEntry[2], (uint8_t)bulbasaurEntry[3]);
    }
    printf("%s BULBASAUR_DEX_ENTRY: %s\n", got ? "ok  " : "FAIL", got ? got : "(null)");
    failed += got == nullptr;
    // every character of every translation has a glyph (tools/font/make_unifont_subset.py)
    std::string missing;
    for (auto* table : { &T().ai, &T().human }) {
        for (auto& [id, row] : *table) {
            for (int l = 1; l < PORT_LANG_COUNT; l++) {
                const char* p = row.text[l].c_str();
                while (uint32_t cp = Port_Utf8Next(&p)) {
                    if (cp >= 0x20 && cp != 0xAD && !Port_Font_HasGlyph(cp) && missing.size() < 200) {
                        missing += id + " ";
                        break;
                    }
                }
            }
        }
    }
    printf("%s glyphs for every translated character%s%s\n", missing.empty() ? "ok  " : "FAIL",
           missing.empty() ? "" : ", missing in: ", missing.c_str());
    failed += !missing.empty();
    Port_Text_SetLanguage(PORT_LANG_ORIGINAL);
    printf("%s\n", failed ? "text selftest FAILED" : "text selftest passed");
    return failed ? 1 : 0;
}

int Port_Text_Vformat(char* out, size_t cap, const char* fmt, va_list ap) {
    if (sLanguage == PORT_LANG_ORIGINAL) {
        int n = vsnprintf(out, cap, fmt, ap);
        return n < 0 ? 0 : (n >= (int)cap ? (int)cap - 1 : n);
    }
    // The original format decides the argument types; the translated one may reorder them with %n$.
    std::vector<Spec> orig = ParseFormat(fmt);
    int argc = 0;
    for (const Spec& s : orig) {
        argc = std::max({ argc, s.pos, s.widthArg, s.precArg });
    }
    std::vector<Arg> args(argc);
    for (const Spec& s : orig) {
        if (s.widthArg) {
            args[s.widthArg - 1].kind = ARG_INT;
        }
        if (s.precArg) {
            args[s.precArg - 1].kind = ARG_INT;
        }
        if (s.conv == '%') {
            continue;
        }
        Arg& a = args[s.pos - 1];
        switch (s.conv) {
            case 's': a.kind = ARG_PTR; a.isString = true; break;
            case 'p': a.kind = ARG_PTR; break;
            case 'f': case 'F': case 'e': case 'E': case 'g': case 'G': a.kind = ARG_DOUBLE; break;
            default: a.kind = HasLong(fmt, s) ? ARG_LONG : ARG_INT; break;
        }
    }
    std::vector<std::string> converted; // EUC-JP arguments with no translation, as UTF-8 (kept alive here)
    converted.reserve(args.size());
    for (Arg& a : args) {
        switch (a.kind) {
            case ARG_LONG: a.i = va_arg(ap, long); break;
            case ARG_DOUBLE: a.d = va_arg(ap, double); break;
            case ARG_PTR: a.p = va_arg(ap, const void*); break;
            default: a.i = va_arg(ap, int); break;
        }
        if (a.isString && a.p != nullptr) {
            const char* tr = Port_Text_Lookup((const char*)a.p);
            if (tr != nullptr) {
                a.p = tr;
            } else if (HasHighBytes((const char*)a.p)) {
                converted.push_back(EucToUtf8((const char*)a.p));
                a.p = converted.back().c_str();
            }
        }
    }
    const char* tfmt = Port_Text_Lookup(fmt);
    std::string fmtUtf8;
    if (tfmt == nullptr && HasHighBytes(fmt)) {
        fmtUtf8 = EucToUtf8(fmt);
        tfmt = fmtUtf8.c_str();
    }
    const char* use = tfmt ? tfmt : fmt;
    std::vector<Spec> specs = tfmt ? ParseFormat(tfmt) : orig;
    std::string res;
    size_t at = 0;
    for (const Spec& s : specs) {
        res.append(use + at, s.start - at);
        if (s.conv == '%') {
            res += '%';
        } else if (s.pos >= 1 && s.pos <= argc && args[s.pos - 1].kind != ARG_NONE) {
            res += FormatOne(use, s, args);
        }
        at = s.end;
    }
    res += use + at;
    size_t n = std::min(res.size(), cap - 1);
    memcpy(out, res.data(), n);
    out[n] = 0;
    return (int)n;
}


} // extern "C"
