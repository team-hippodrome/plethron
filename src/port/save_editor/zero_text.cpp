#include "zero_text.h"
#include "stadium_save.h"

#include <algorithm>
#include <cstdio>
#include <map>
#include <utility>

namespace stadium_save {

namespace {

constexpr uint8_t TERM = 0x50, SPACE = 0x7F;

std::vector<uint32_t> Cps(const std::string& s) {
    std::vector<uint32_t> out;
    for (size_t i = 0; i < s.size();) {
        unsigned char c = (unsigned char)s[i];
        int n = c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : (c >> 3) == 30 ? 4 : 1;
        uint32_t cp = n == 1 ? c : c & (0xFF >> (n + 1));
        for (int k = 1; k < n && i + k < s.size(); k++) cp = cp << 6 | ((unsigned char)s[i + k] & 0x3F);
        out.push_back(cp);
        i += n;
    }
    return out;
}

std::string U8(uint32_t cp) {
    std::string s;
    if (cp < 0x80) s += (char)cp;
    else if (cp < 0x800) s += (char)(0xC0 | cp >> 6), s += (char)(0x80 | (cp & 0x3F));
    else if (cp < 0x10000) s += (char)(0xE0 | cp >> 12), s += (char)(0x80 | ((cp >> 6) & 0x3F)), s += (char)(0x80 | (cp & 0x3F));
    else s += (char)(0xF0 | cp >> 18), s += (char)(0x80 | ((cp >> 12) & 0x3F)), s += (char)(0x80 | ((cp >> 6) & 0x3F)), s += (char)(0x80 | (cp & 0x3F));
    return s;
}

// Zero's GB -> character table (D_80076FD4 in the ROM, read 2026-11-16): codes with a character.
struct Charset {
    uint32_t toCp[256] = {};
    std::map<uint32_t, uint8_t> fromCp;
    Charset() {
        auto put = [&](int code, const char* chars) {
            for (uint32_t cp : Cps(chars)) toCp[code++] = cp;
        };
        put(0x05, "ガギグゲゴザジズゼゾダヂヅデド");
        put(0x19, "バビブボ");
        put(0x26, "がぎぐげござじずぜぞだぢづでど");
        put(0x3A, "ばびぶべぼ");
        put(0x40, "パピプポぱぴぷぺぽ");
        put(0x60, "ＡＢＣＤＥＦＧＨＩＪＫＬＭＮぃぅＱＲＳＴＵＶぁぇぉＺ");
        put(0x7F, "　");
        put(0x80, "アイウエオカキクケコサシスセソタチツテトナニヌネノハヒフホマミムメモヤユヨラルレロワヲンッャュョィ");
        put(0xB1, "あいうえおかきくけこさしすせそたちつてとなにぬねのはひふへほまみむめもやゆよらりるれろわをんっゃゅょー。．？！、ァゥェ");
        put(0xEF, "♂");
        put(0xF4, "ォ♀０１２３４５６７８９");
        for (int b = 0; b < 256; b++)
            if (toCp[b]) fromCp[toCp[b]] = (uint8_t)b;
        // katakana the cartridges write with the hiragana glyph
        fromCp[0x30D8] = fromCp[0x3078]; // ヘ へ
        fromCp[0x30D9] = fromCp[0x3079]; // ベ べ
        fromCp[0x30DA] = fromCp[0x307A]; // ペ ぺ
        fromCp[0x30EA] = fromCp[0x308A]; // リ り
        fromCp[' '] = SPACE;
    }
};

const Charset& Set() {
    static Charset c;
    return c;
}

// romaji <-> hiragana; katakana is the same with capitals
const std::vector<std::pair<std::string, std::string>>& Syllables() {
    static const std::vector<std::pair<std::string, std::string>> v = {
        { "kya", "きゃ" }, { "kyu", "きゅ" }, { "kyo", "きょ" }, { "sha", "しゃ" }, { "shu", "しゅ" }, { "sho", "しょ" },
        { "she", "しぇ" }, { "cha", "ちゃ" }, { "chu", "ちゅ" }, { "cho", "ちょ" }, { "che", "ちぇ" }, { "nya", "にゃ" },
        { "nyu", "にゅ" }, { "nyo", "にょ" }, { "hya", "ひゃ" }, { "hyu", "ひゅ" }, { "hyo", "ひょ" }, { "mya", "みゃ" },
        { "myu", "みゅ" }, { "myo", "みょ" }, { "rya", "りゃ" }, { "ryu", "りゅ" }, { "ryo", "りょ" }, { "gya", "ぎゃ" },
        { "gyu", "ぎゅ" }, { "gyo", "ぎょ" }, { "ja", "じゃ" }, { "ju", "じゅ" }, { "jo", "じょ" }, { "je", "じぇ" },
        { "bya", "びゃ" }, { "byu", "びゅ" }, { "byo", "びょ" }, { "pya", "ぴゃ" }, { "pyu", "ぴゅ" }, { "pyo", "ぴょ" },
        { "fa", "ふぁ" }, { "fi", "ふぃ" }, { "fe", "ふぇ" }, { "fo", "ふぉ" }, { "ti", "てぃ" }, { "di", "でぃ" },
        { "tu", "とぅ" }, { "du", "どぅ" }, { "wi", "うぃ" }, { "we", "うぇ" },
        { "shi", "し" }, { "chi", "ち" }, { "tsu", "つ" }, { "dji", "ぢ" }, { "dzu", "づ" },
        { "ka", "か" }, { "ki", "き" }, { "ku", "く" }, { "ke", "け" }, { "ko", "こ" }, { "sa", "さ" }, { "su", "す" },
        { "se", "せ" }, { "so", "そ" }, { "ta", "た" }, { "te", "て" }, { "to", "と" }, { "na", "な" }, { "ni", "に" },
        { "nu", "ぬ" }, { "ne", "ね" }, { "no", "の" }, { "ha", "は" }, { "hi", "ひ" }, { "fu", "ふ" }, { "he", "へ" },
        { "ho", "ほ" }, { "ma", "ま" }, { "mi", "み" }, { "mu", "む" }, { "me", "め" }, { "mo", "も" }, { "ya", "や" },
        { "yu", "ゆ" }, { "yo", "よ" }, { "ra", "ら" }, { "ri", "り" }, { "ru", "る" }, { "re", "れ" }, { "ro", "ろ" },
        { "wa", "わ" }, { "wo", "を" }, { "ga", "が" }, { "gi", "ぎ" }, { "gu", "ぐ" }, { "ge", "げ" }, { "go", "ご" },
        { "za", "ざ" }, { "ji", "じ" }, { "zu", "ず" }, { "ze", "ぜ" }, { "zo", "ぞ" }, { "da", "だ" }, { "de", "で" },
        { "do", "ど" }, { "ba", "ば" }, { "bi", "び" }, { "bu", "ぶ" }, { "be", "べ" }, { "bo", "ぼ" }, { "pa", "ぱ" },
        { "pi", "ぴ" }, { "pu", "ぷ" }, { "pe", "ぺ" }, { "po", "ぽ" },
        { "a", "あ" }, { "i", "い" }, { "u", "う" }, { "e", "え" }, { "o", "お" },
        { "xa", "ぁ" }, { "xi", "ぃ" }, { "xu", "ぅ" }, { "xe", "ぇ" }, { "xo", "ぉ" }, { "xya", "ゃ" }, { "xyu", "ゅ" },
        { "xyo", "ょ" }, { "xtsu", "っ" },
    };
    return v;
}

bool IsKata(uint32_t cp) { return cp >= 0x30A1 && cp <= 0x30F6; }
bool IsHira(uint32_t cp) { return cp >= 0x3041 && cp <= 0x3096; }
uint32_t ToHira(uint32_t cp) { return IsKata(cp) ? cp - 0x60 : cp; }
uint32_t ToKata(uint32_t cp) { return IsHira(cp) ? cp + 0x60 : cp; }
bool Vowel(char c) { c = (char)tolower((unsigned char)c); return c == 'a' || c == 'i' || c == 'u' || c == 'e' || c == 'o'; }
std::string Lower(std::string s) { for (char& c : s) c = (char)tolower((unsigned char)c); return s; }
std::string UpperS(std::string s) { for (char& c : s) c = (char)toupper((unsigned char)c); return s; }

// full-width digits and punctuation <-> ASCII
const std::vector<std::pair<char, uint32_t>>& Wide() {
    static const std::vector<std::pair<char, uint32_t>> v = { { '?', 0xFF1F }, { '!', 0xFF01 }, { '.', 0x3002 },
                                                               { ',', 0x3001 }, { ' ', 0x3000 } };
    return v;
}

} // namespace

std::string DecodeJp(const uint8_t* d, size_t n) {
    std::vector<uint32_t> cps; // 0: a byte with no character
    size_t len = 0;
    while (len < n && d[len] != TERM) cps.push_back(Set().toCp[d[len++]]);
    // へ べ ぺ り serve katakana too: read them as katakana next to katakana (リザード, not りザード)
    for (size_t i = 0; i < cps.size(); i++) {
        const uint32_t cp = cps[i];
        if (cp != 0x3078 && cp != 0x3079 && cp != 0x307A && cp != 0x308A) continue;
        bool kata = false;
        for (size_t j = i; j-- > 0 && !kata;) {
            if (IsKata(cps[j])) kata = true;
            if (IsHira(cps[j]) && cps[j] != 0x3078 && cps[j] != 0x3079 && cps[j] != 0x307A && cps[j] != 0x308A) break;
        }
        for (size_t j = i + 1; j < cps.size() && !kata; j++) {
            if (IsKata(cps[j])) kata = true;
            if (IsHira(cps[j]) && cps[j] != 0x3078 && cps[j] != 0x3079 && cps[j] != 0x307A && cps[j] != 0x308A) break;
        }
        if (kata) cps[i] = ToKata(cp);
    }
    std::string out;
    for (size_t i = 0; i < cps.size(); i++) {
        if (cps[i]) {
            out += U8(cps[i]);
        } else {
            char buf[8];
            snprintf(buf, sizeof(buf), "<%02X>", d[i]);
            out += buf;
        }
    }
    return out;
}

std::vector<uint8_t> EncodeJp(const std::string& text, size_t size, uint8_t pad) {
    std::vector<uint8_t> out;
    std::vector<uint32_t> cps = Cps(text);
    for (size_t i = 0; i < cps.size(); i++) {
        if (cps[i] == '<') {
            size_t j = i;
            while (j < cps.size() && cps[j] != '>') j++;
            std::string g;
            for (size_t k = i + 1; k < j && k < cps.size(); k++) g += (char)cps[k];
            if (j < cps.size() && (g == "M" || g == "m")) { out.push_back(Set().fromCp.at(0x2642)); i = j; continue; }
            if (j < cps.size() && (g == "F" || g == "f")) { out.push_back(Set().fromCp.at(0x2640)); i = j; continue; }
            if (j < cps.size() && g.size() == 2 && isxdigit((unsigned char)g[0]) && isxdigit((unsigned char)g[1])) {
                out.push_back((uint8_t)std::stoi(g, nullptr, 16));
                i = j;
                continue;
            }
        }
        uint32_t cp = cps[i];
        if (cp >= '0' && cp <= '9') cp = 0xFF10 + (cp - '0');
        for (auto [a, w] : Wide())
            if (cp == (uint32_t)a) cp = w;
        if (cp == '-') cp = 0x30FC;
        auto it = Set().fromCp.find(cp);
        if (it == Set().fromCp.end()) throw Error("'" + U8(cps[i]) + "' is not in the Japanese Game Boy character set");
        out.push_back(it->second);
    }
    out.push_back(TERM);
    if (out.size() > size) {
        throw Error("'" + text + "' is too long: " + std::to_string(size - 1) + " characters at most");
    }
    out.resize(size, pad);
    return out;
}

std::string KanaToRomaji(const std::string& text) {
    std::vector<uint32_t> cps = Cps(text);
    // hiragana -> romaji, longest first
    static const std::map<std::string, std::string> toRomaji = [] {
        std::map<std::string, std::string> m;
        for (auto& [r, k] : Syllables())
            if (!m.count(k)) m[k] = r;
        return m;
    }();
    std::string out;
    for (size_t i = 0; i < cps.size();) {
        uint32_t cp = cps[i];
        const bool kata = IsKata(cp);
        auto romajiAt = [&](size_t at, size_t* used) -> std::string {
            for (size_t len : { size_t(2), size_t(1) }) {
                if (at + len > cps.size()) continue;
                std::string key;
                bool kana = true;
                for (size_t k = 0; k < len; k++) {
                    kana &= IsKata(cps[at + k]) || IsHira(cps[at + k]);
                    key += U8(ToHira(cps[at + k]));
                }
                if (!kana) continue;
                auto it = toRomaji.find(key);
                if (it != toRomaji.end()) {
                    *used = len;
                    return it->second;
                }
            }
            return "";
        };
        size_t used = 0;
        std::string r;
        if (ToHira(cp) == 0x3063 && i + 1 < cps.size()) { // っ: double the next consonant
            r = romajiAt(i + 1, &used);
            if (!r.empty() && !Vowel(r[0])) {
                r = r.substr(0, 1) + r;
                used += 1;
            } else {
                r = "xtsu";
                used = 1;
            }
        } else if (ToHira(cp) == 0x3093) { // ん
            r = "n";
            used = 1;
            if (i + 1 < cps.size()) {
                size_t u2 = 0;
                std::string next = romajiAt(i + 1, &u2);
                if (!next.empty() && (Vowel(next[0]) || next[0] == 'y' || next[0] == 'n')) r = "n'";
            }
        } else {
            r = romajiAt(i, &used);
        }
        if (!r.empty()) {
            out += kata ? UpperS(r) : r;
            i += used;
            continue;
        }
        if (cp == 0x30FC) out += "-";
        else if (cp >= 0xFF10 && cp <= 0xFF19) out += (char)('0' + (cp - 0xFF10));
        else if (cp >= 0xFF21 && cp <= 0xFF3A) out += std::string("{") + (char)('A' + (cp - 0xFF21)) + "}";
        else if (cp == 0xFF0E) out += "{.}";
        else {
            bool wide = false;
            for (auto [a, w] : Wide())
                if (cp == w) out += a, wide = true;
            if (!wide) out += U8(cp);
        }
        i++;
    }
    return out;
}

std::string RomajiToKana(const std::string& text) {
    std::string out;
    size_t i = 0;
    auto kanaOf = [](const std::string& hira, bool kata) {
        std::string s;
        for (uint32_t cp : Cps(hira)) s += U8(kata ? ToKata(cp) : cp);
        return s;
    };
    while (i < text.size()) {
        const unsigned char c = (unsigned char)text[i];
        if (c >= 0x80) { // kana or another character typed as it is
            size_t n = (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : 4;
            out += text.substr(i, n);
            i += n;
            continue;
        }
        if (c == '<') { // <M>, <F>, <XX>: left for EncodeJp
            size_t j = text.find('>', i);
            if (j != std::string::npos) {
                out += text.substr(i, j - i + 1);
                i = j + 1;
                continue;
            }
        }
        if (c == '{' && i + 2 < text.size() && text[i + 2] == '}') {
            char l = (char)toupper((unsigned char)text[i + 1]);
            if (l >= 'A' && l <= 'Z') out += U8(0xFF21 + (l - 'A'));
            else if (l == '.') out += U8(0xFF0E);
            else throw Error("{" + std::string(1, text[i + 1]) + "} is not a full-width letter");
            i += 3;
            continue;
        }
        if (c == '-') {
            out += "ー";
            i++;
            continue;
        }
        if (!isalpha(c)) {
            out += (char)c; // digits, ? ! . , and spaces: EncodeJp makes them full width
            i++;
            continue;
        }
        const bool kata = isupper(c);
        const std::string rest = Lower(text.substr(i, 4));
        // n' and n before a consonant (or the end): ん
        if (rest[0] == 'n' && (rest.size() == 1 || rest[1] == '\'' || (!Vowel(rest[1]) && rest[1] != 'y' && isalpha((unsigned char)rest[1])) ||
                               !isalpha((unsigned char)rest[1]))) {
            out += kanaOf("ん", kata);
            i += rest.size() > 1 && rest[1] == '\'' ? 2 : 1;
            continue;
        }
        // a doubled consonant: っ
        if (rest.size() > 1 && rest[0] == rest[1] && !Vowel(rest[0]) && rest[0] != 'n') {
            out += kanaOf("っ", kata);
            i++;
            continue;
        }
        bool found = false;
        for (size_t len : { size_t(4), size_t(3), size_t(2), size_t(1) }) {
            if (len > rest.size()) continue;
            const std::string key = rest.substr(0, len);
            for (auto& [r, k] : Syllables()) {
                if (r == key) {
                    out += kanaOf(k, kata);
                    i += len;
                    found = true;
                    break;
                }
            }
            if (found) break;
        }
        if (!found) throw Error("cannot read \"" + text.substr(i) + "\" as romaji (capitals: katakana, small letters: hiragana)");
    }
    return out;
}

const char* const JP_SPECIES[152] = {
    "",
    "フシギダネ",
    "フシギソウ",
    "フシギバナ",
    "ヒトカゲ",
    "リザード",
    "リザードン",
    "ゼニガメ",
    "カメール",
    "カメックス",
    "キャタピー",
    "トランセル",
    "バタフリー",
    "ビードル",
    "コクーン",
    "スピアー",
    "ポッポ",
    "ピジョン",
    "ピジョット",
    "コラッタ",
    "ラッタ",
    "オニスズメ",
    "オニドリル",
    "アーボ",
    "アーボック",
    "ピカチュウ",
    "ライチュウ",
    "サンド",
    "サンドパン",
    "ニドラン♀",
    "ニドリーナ",
    "ニドクイン",
    "ニドラン♂",
    "ニドリーノ",
    "ニドキング",
    "ピッピ",
    "ピクシー",
    "ロコン",
    "キュウコン",
    "プリン",
    "プクリン",
    "ズバット",
    "ゴルバット",
    "ナゾノクサ",
    "クサイハナ",
    "ラフレシア",
    "パラス",
    "パラセクト",
    "コンパン",
    "モルフォン",
    "ディグダ",
    "ダグトリオ",
    "ニャース",
    "ペルシアン",
    "コダック",
    "ゴルダック",
    "マンキー",
    "オコリザル",
    "ガーディ",
    "ウインディ",
    "ニョロモ",
    "ニョロゾ",
    "ニョロボン",
    "ケーシィ",
    "ユンゲラー",
    "フーディン",
    "ワンリキー",
    "ゴーリキー",
    "カイリキー",
    "マダツボミ",
    "ウツドン",
    "ウツボット",
    "メノクラゲ",
    "ドククラゲ",
    "イシツブテ",
    "ゴローン",
    "ゴローニャ",
    "ポニータ",
    "ギャロップ",
    "ヤドン",
    "ヤドラン",
    "コイル",
    "レアコイル",
    "カモネギ",
    "ドードー",
    "ドードリオ",
    "パウワウ",
    "ジュゴン",
    "ベトベター",
    "ベトベトン",
    "シェルダー",
    "パルシェン",
    "ゴース",
    "ゴースト",
    "ゲンガー",
    "イワーク",
    "スリープ",
    "スリーパー",
    "クラブ",
    "キングラー",
    "ビリリダマ",
    "マルマイン",
    "タマタマ",
    "ナッシー",
    "カラカラ",
    "ガラガラ",
    "サワムラー",
    "エビワラー",
    "ベロリンガ",
    "ドガース",
    "マタドガス",
    "サイホーン",
    "サイドン",
    "ラッキー",
    "モンジャラ",
    "ガルーラ",
    "タッツー",
    "シードラ",
    "トサキント",
    "アズマオウ",
    "ヒトデマン",
    "スターミー",
    "バリヤード",
    "ストライク",
    "ルージュラ",
    "エレブー",
    "ブーバー",
    "カイロス",
    "ケンタロス",
    "コイキング",
    "ギャラドス",
    "ラプラス",
    "メタモン",
    "イーブイ",
    "シャワーズ",
    "サンダース",
    "ブースター",
    "ポリゴン",
    "オムナイト",
    "オムスター",
    "カブト",
    "カブトプス",
    "プテラ",
    "カビゴン",
    "フリーザー",
    "サンダー",
    "ファイヤー",
    "ミニリュウ",
    "ハクリュー",
    "カイリュー",
    "ミュウツー",
    "ミュウ"
};

} // namespace stadium_save
