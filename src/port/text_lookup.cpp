// Runtime text translation (see text_lookup.h and translations/README.md).
#include "text_lookup.h"

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

const char* const kColumns[PORT_LANG_COUNT] = { "", "ja", "en", "es", "de", "pt", "ko", "ru", "ar", "hi" };
const char* const kNames[PORT_LANG_COUNT] = {
    "にほんご", "日本語", "English", "Español", "Deutsch", "Português", "한국어", "Русский", "العربية", "हिन्दी",
};

struct Tables {
    // md5 -> one UTF-8 string per language (empty = falls through)
    std::unordered_map<std::string, std::vector<std::string>> human, ai;
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

void LoadTable(const char* name, std::unordered_map<std::string, std::vector<std::string>>& into) {
    std::string path = Ship::Context::LocateFileAcrossAppDirs(std::string("translations/") + name);
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        fprintf(stderr, "[text] %s not found (%s)\n", name, path.c_str());
        return;
    }
    std::stringstream ss;
    ss << f.rdbuf();
    auto rows = ParseCsv(ss.str());
    if (rows.empty()) {
        return;
    }
    int col[PORT_LANG_COUNT];
    for (int l = 0; l < PORT_LANG_COUNT; l++) {
        col[l] = -1;
        for (size_t c = 0; c < rows[0].size(); c++) {
            if (l != 0 && rows[0][c] == kColumns[l]) {
                col[l] = (int)c;
            }
        }
    }
    for (size_t r = 1; r < rows.size(); r++) {
        const auto& row = rows[r];
        if (row.empty() || row[0].size() != 32) {
            continue;
        }
        std::vector<std::string> v(PORT_LANG_COUNT);
        for (int l = 1; l < PORT_LANG_COUNT; l++) {
            if (col[l] >= 0 && col[l] < (int)row.size()) {
                v[l] = Unescape(row[col[l]]);
            }
        }
        into[row[0]] = std::move(v);
    }
    fprintf(stderr, "[text] %s: %zu strings\n", name, into.size());
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
    fputc('\n', t.missLog);
    fflush(t.missLog);
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
    LoadTable("human.csv", t.human);
    LoadTable("ai.csv", t.ai);
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
    // Debug aid: PORT_LANGUAGE=<column> (en, ja, ...) starts in that language without the language select screen.
    static bool sEnvChecked = false;
    if (!sEnvChecked) {
        sEnvChecked = true;
        if (const char* env = getenv("PORT_LANGUAGE")) {
            for (int l = 1; l < PORT_LANG_COUNT; l++) {
                if (strcmp(env, kColumns[l]) == 0) {
                    Port_Text_SetLanguage(l);
                }
            }
        }
    }
    return sLanguage;
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
    std::string key(hex);
    for (auto* table : { &t.human, &t.ai }) {
        auto it = table->find(key);
        if (it != table->end() && !it->second[sLanguage].empty()) {
            return it->second[sLanguage].c_str(); // stable: the tables are never modified after loading
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
            }
        }
    }
    const char* tfmt = Port_Text_Lookup(fmt);
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

int Port_Text_Utf8ToGameEuc(const char* in, unsigned char* out, size_t cap) {
    static const char kPunct[] = " !\"#%&'()*+,-./:;<=>?@[]_";
    static const uint16_t kPunctJis[] = { 0xA1A1, 0xA1AA, 0xA1C9, 0xA1F4, 0xA1F3, 0xA1F5, 0xA1C7, 0xA1CA, 0xA1CB,
                                          0xA1F6, 0xA1DC, 0xA1A4, 0xA1DD, 0xA1A5, 0xA1BF, 0xA1A7, 0xA1A8, 0xA1E3,
                                          0xA1E1, 0xA1E4, 0xA1A9, 0xA1F7, 0xA1CE, 0xA1CF, 0xA1B2 };
    size_t n = 0;
    const uint8_t* p = (const uint8_t*)in;
    while (*p != 0 && n + 2 < cap) {
        uint32_t cp;
        int len;
        if (*p < 0x80) {
            cp = *p;
            len = 1;
        } else if ((*p & 0xE0) == 0xC0) {
            cp = *p & 0x1F;
            len = 2;
        } else if ((*p & 0xF0) == 0xE0) {
            cp = *p & 0x0F;
            len = 3;
        } else {
            cp = *p & 0x07;
            len = 4;
        }
        for (int i = 1; i < len; i++) {
            if ((p[i] & 0xC0) != 0x80) {
                len = i;
                break;
            }
            cp = (cp << 6) | (p[i] & 0x3F);
        }
        p += len;
        uint16_t jis = 0xA1A9; // full-width '?'
        if (cp == '\n' || cp == '\t') {
            out[n++] = (uint8_t)cp;
            continue;
        } else if (cp >= '0' && cp <= '9') {
            jis = 0xA3B0 + (cp - '0');
        } else if (cp >= 'A' && cp <= 'Z') {
            jis = 0xA3C1 + (cp - 'A');
        } else if (cp >= 'a' && cp <= 'z') {
            jis = 0xA3E1 + (cp - 'a');
        } else if (cp < 0x80 && strchr(kPunct, (int)cp) != nullptr) {
            jis = kPunctJis[strchr(kPunct, (int)cp) - kPunct];
        } else if (cp >= 0x3041 && cp <= 0x3093) {
            jis = 0xA4A1 + (cp - 0x3041); // hiragana
        } else if (cp >= 0x30A1 && cp <= 0x30F6) {
            jis = 0xA5A1 + (cp - 0x30A1); // katakana
        } else if (cp == 0x30FC) {
            jis = 0xA1BC;
        } else if (cp == 0xE9) { // é (Pokémon): plain e for now
            jis = 0xA3E5;
        }
        out[n++] = (uint8_t)(jis >> 8);
        out[n++] = (uint8_t)jis;
    }
    out[n] = 0;
    return (int)n;
}

} // extern "C"
