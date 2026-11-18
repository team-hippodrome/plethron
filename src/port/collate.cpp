// Unicode text sorting for the port (collate.h): a small multi-level collation in the spirit of the Unicode
// Collation Algorithm, enough for the names the game lists. It compares three levels in turn:
//   1. base letters: spaces < punctuation < symbols < digits < Latin < Greek < Cyrillic < Hangul < kana < the rest,
//      letters case-folded with their accents taken off (é = e, ß = ss, æ = ae);
//   2. accents (and the kana voicing marks), in the CLDR root order;
//   3. case (lower first), small kana and katakana;
// then the UTF-8 bytes. Language tailorings: Spanish sorts ñ as its own letter after n; Japanese sorts the long vowel
// mark ー as the vowel before it (JIS X 4061), so kana lists come out in あいうえお order.
#include "collate.h"

#include <stdint.h>
#include <string.h>

#include <vector>

#include "text_lookup.h"

namespace {

struct LatinEntry {
    char base;
    char base2;
    uint8_t rank;
    uint8_t upper;
};

struct KanaEntry {
    uint16_t plain;
    uint8_t voice;
    uint8_t flags;
    char vowel;
};

const LatinEntry kLatin[] = {
#include "collate_latin.inc"
};

const KanaEntry kKana[] = { COLLATE_KANA_TABLE };

// accent rank of a combining mark (U+0300..U+036F) in text that is already decomposed; same order as
// tools/text/gen_collate_latin.py's MARKS
const uint16_t kMarkRank[] = { 0x301, 0x300, 0x306, 0x302, 0x30C, 0x30A, 0x308, 0x30B, 0x303, 0x307,
                               0x327, 0x328, 0x304, 0x323, 0x326, 0x30F, 0x311, 0x31B, 0x335 };

enum Group : uint32_t {
    G_SPACE = 1,
    G_PUNCT,
    G_SYMBOL,
    G_DIGIT,
    G_LATIN,
    G_GREEK,
    G_CYRILLIC,
    G_HANGUL,
    G_KANA,
    G_OTHER,
};

struct Keys {
    std::vector<uint32_t> primary;
    std::vector<uint8_t> secondary;
    std::vector<uint8_t> tertiary;
};

uint32_t Decode(const unsigned char*& p) {
    uint32_t c = *p++;
    int extra = 0;
    if (c >= 0xF0) {
        c &= 0x07;
        extra = 3;
    } else if (c >= 0xE0) {
        c &= 0x0F;
        extra = 2;
    } else if (c >= 0xC0) {
        c &= 0x1F;
        extra = 1;
    } else if (c >= 0x80) {
        return 0xFFFD;
    }
    while (extra-- > 0) {
        if ((*p & 0xC0) != 0x80) {
            return 0xFFFD;
        }
        c = (c << 6) | (*p++ & 0x3F);
    }
    return c;
}

uint32_t Primary(Group g, uint32_t value) {
    return ((uint32_t)g << 24) | (value & 0xFFFFFF);
}

bool IsPunct(uint32_t c) {
    if (c < 0x80) {
        return strchr("!\"#%&'()*,-./:;?@[\\]_{}", (int)c) != nullptr;
    }
    return c == 0xA1 || c == 0xA7 || c == 0xAB || c == 0xB6 || c == 0xB7 || c == 0xBB || c == 0xBF ||
           (c >= 0x2010 && c <= 0x2027) || (c >= 0x2030 && c <= 0x205E) || (c >= 0x3001 && c <= 0x3003) ||
           (c >= 0x3008 && c <= 0x3011) || (c >= 0x3014 && c <= 0x301F) || c == 0x30FB || c == 0xFF0C ||
           c == 0xFF0E || c == 0xFF1A || c == 0xFF1B || c == 0xFF1F || c == 0xFF01;
}

void Add(Keys& k, uint32_t primary, uint8_t secondary, uint8_t tertiary) {
    k.primary.push_back(primary);
    k.secondary.push_back(secondary);
    k.tertiary.push_back(tertiary);
}

void AddLatin(Keys& k, char base, uint8_t rank, uint8_t upper, int lang) {
    uint32_t v = (uint32_t)(base - 'a') * 2;
    if (lang == PORT_LANG_ES && base == 'n' && rank == 9) { // ñ (n + tilde, rank 9) is a letter of its own
        v += 1;
        rank = 0;
    }
    Add(k, Primary(G_LATIN, v), rank, upper);
}

Keys MakeKeys(const char* s, int lang) {
    Keys k;
    char lastVowel = 0;
    for (const unsigned char* p = (const unsigned char*)s; *p != 0;) {
        uint32_t c = Decode(p);
        char vowel = 0;
        if (c >= 0x300 && c <= 0x36F) { // a combining accent: raises the last letter's secondary weight
            for (size_t i = 0; i < sizeof(kMarkRank) / sizeof(kMarkRank[0]); i++) {
                if (kMarkRank[i] == c && !k.secondary.empty()) {
                    k.secondary.back() = (uint8_t)(i + 1);
                }
            }
            if (c == 0x303 && lang == PORT_LANG_ES && !k.primary.empty() &&
                k.primary.back() == Primary(G_LATIN, ('n' - 'a') * 2)) {
                k.primary.back() += 1;
                k.secondary.back() = 0;
            }
            continue;
        }
        if (c == 0x3099 || c == 0x309A || c == 0x309B || c == 0x309C) { // kana voicing marks on their own
            if (!k.secondary.empty()) {
                k.secondary.back() = (c == 0x3099 || c == 0x309B) ? 1 : 2;
            }
            continue;
        }
        if (c >= 0xFF01 && c <= 0xFF5E) { // fullwidth ASCII sorts as ASCII, after it at the third level
            c -= 0xFEE0;
        }
        if (c == ' ' || c == 0x3000 || c == '\t' || c == '\n') {
            Add(k, Primary(G_SPACE, 0), 0, 0);
        } else if (c >= '0' && c <= '9') {
            Add(k, Primary(G_DIGIT, c - '0'), 0, 0);
        } else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
            AddLatin(k, (char)(c | 0x20), 0, (c < 'a') ? 1 : 0, lang);
        } else if (c >= 0xC0 && c < 0x250 && kLatin[c - 0xC0].base != 0) {
            const LatinEntry& e = kLatin[c - 0xC0];
            AddLatin(k, e.base, e.rank, e.upper, lang);
            if (e.base2 != 0) {
                AddLatin(k, e.base2, 0, e.upper, lang);
            }
        } else if (c >= 0x391 && c <= 0x3C9) { // Greek, case-folded
            uint32_t f = (c <= 0x3A9) ? c + 0x20 : c;
            Add(k, Primary(G_GREEK, (f == 0x3C2) ? 0x3C3 : f), 0, (c <= 0x3A9) ? 1 : 0);
        } else if (c >= 0x400 && c <= 0x44F) { // Cyrillic, case-folded
            uint32_t f = (c < 0x410) ? c + 0x50 : (c < 0x430) ? c + 0x20 : c;
            Add(k, Primary(G_CYRILLIC, f), 0, (c < 0x430) ? 1 : 0);
        } else if (c >= 0x450 && c <= 0x4FF) {
            Add(k, Primary(G_CYRILLIC, c), 0, 0);
        } else if ((c >= 0xAC00 && c <= 0xD7A3) || (c >= 0x1100 && c <= 0x11FF) || (c >= 0x3131 && c <= 0x318E)) {
            // precomposed Hangul syllables are in jamo order already (가 < 각 < 나)
            Add(k, Primary(G_HANGUL, c), 0, 0);
        } else if (c >= 0x3041 && c <= 0x30FA && kKana[c - 0x3041].plain != 0) {
            const KanaEntry& e = kKana[c - 0x3041];
            Add(k, Primary(G_KANA, e.plain), e.voice, e.flags);
            vowel = e.vowel;
        } else if (c == 0x30FC && lang == PORT_LANG_JA && lastVowel != 0) {
            // ー: the vowel it lengthens (アーボ sorts as ああほ), after that vowel written out
            static const char kVowels[] = "aiueo";
            static const uint16_t kVowelKana[] = { 0x3042, 0x3044, 0x3046, 0x3048, 0x304A };
            uint32_t kana = kVowelKana[strchr(kVowels, lastVowel) - kVowels];
            Add(k, Primary(G_KANA, kana), 0, 4);
            vowel = lastVowel;
        } else if (IsPunct(c)) {
            Add(k, Primary(G_PUNCT, c), 0, 0);
        } else if (c < 0x3000 || (c >= 0xFF00 && c < 0xFFF0)) {
            Add(k, Primary(G_SYMBOL, c), 0, 0); // ♀ (U+2640) before ♂ (U+2642)
        } else {
            Add(k, Primary(G_OTHER, c), 0, 0);
        }
        lastVowel = vowel;
    }
    return k;
}

template <typename T> int CompareLevel(const std::vector<T>& a, const std::vector<T>& b) {
    size_t n = (a.size() < b.size()) ? a.size() : b.size();
    for (size_t i = 0; i < n; i++) {
        if (a[i] != b[i]) {
            return (a[i] < b[i]) ? -1 : 1;
        }
    }
    return (a.size() == b.size()) ? 0 : (a.size() < b.size()) ? -1 : 1;
}

} // namespace

extern "C" int Port_Collate(const char* a, const char* b, int lang) {
    Keys ka = MakeKeys(a, lang);
    Keys kb = MakeKeys(b, lang);
    int r = CompareLevel(ka.primary, kb.primary);
    if (r == 0) {
        r = CompareLevel(ka.secondary, kb.secondary);
    }
    if (r == 0) {
        r = CompareLevel(ka.tertiary, kb.tertiary);
    }
    if (r == 0) {
        r = strcmp(a, b);
        r = (r > 0) - (r < 0);
    }
    return r;
}
