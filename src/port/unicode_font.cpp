// Unicode glyphs for translated text: GNU Unifont 16-pixel bitmaps (assets/font/unifont_subset.hex, made by
// tools/font/make_unifont_subset.py) turned into IA8 textures in the style of the game's own font: white fill with a
// one-pixel dark outline, resampled to the height of the active font set. font.c draws them (Font_Printf).
// The "Text font" setting (gPort.TextFont, PORT_TEXT_FONT=pop|unifont) can pick the prototype Plethron Pop outlines
// instead (assets/font/pop_subset.ttf, tools/font/make_pop_font.py), drawn the same way; Unifont stays the default
// and covers any character Pop lacks.
// The game's own font sets 16x10 and 16x12 have no outline (plain white glyphs); only 16x16 and up are outlined. The
// "Small text" setting (gPort.SmallTextOutline, PORT_SMALL_TEXT_OUTLINE=1|0|2) picks how translated text below 16
// pixels is drawn: 1 = outlined, 0 = plain white like the game's small kana, 2 = plain white and
// one source pixel bolder (the game's small kana have two-pixel strokes; the default, Julius 2026-11-21).
#include "unicode_font.h"
#include "app_file.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <cstdlib>
#include <map>
#include <sstream>
#include <memory>
#include <mutex>
#include <thread>
#include <string>
#include <unordered_map>
#include <vector>

#include <libultraship/libultraship.h>

// a private copy, as in text3d.cpp (STBTT_STATIC)
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "third_party/stb_truetype.h"

namespace {

struct Bitmap {
    int width = 0; // 8 or 16
    uint16_t rows[16] = {};
};

struct Glyph {
    std::vector<unsigned char> tex; // IA8, texWidth x height
    int texWidth = 0;
    int drawWidth = 0;
    int advance = 0;
};

struct Font {
    bool loaded = false;
    std::unordered_map<uint32_t, Bitmap> bitmaps;
    std::map<std::pair<uint32_t, int>, std::unique_ptr<Glyph>> glyphs; // (codepoint, height); never freed
};

Font& GetFont() {
    static Font font;
    if (!font.loaded) {
        font.loaded = true;
        std::string hex, path;
        if (!Port::ReadAppFile("assets/font/unifont_subset.hex", hex, &path)) {
            fprintf(stderr, "[font] %s not found: translated text will not draw\n", path.c_str());
        }
        std::istringstream f(hex);
        std::string line;
        while (std::getline(f, line)) {
            size_t colon = line.find(':');
            if (line.empty() || line[0] == '#' || colon == std::string::npos) {
                continue;
            }
            std::string bits = line.substr(colon + 1);
            while (!bits.empty() && (bits.back() == '\r' || bits.back() == ' ')) {
                bits.pop_back();
            }
            Bitmap b;
            if (bits.size() == 32) {
                b.width = 8;
            } else if (bits.size() == 64) {
                b.width = 16;
            } else {
                continue;
            }
            int digits = b.width / 4;
            for (int y = 0; y < 16; y++) {
                b.rows[y] = (uint16_t)strtoul(bits.substr(y * digits, digits).c_str(), nullptr, 16);
            }
            font.bitmaps[(uint32_t)strtoul(line.substr(0, colon).c_str(), nullptr, 16)] = b;
        }
    }
    return font;
}

bool Pixel(const Bitmap& b, int x, int y) {
    if (x < 0 || y < 0 || x >= b.width || y >= 16) {
        return false;
    }
    return (b.rows[y] >> (b.width - 1 - x)) & 1;
}

// Below this height the game's font sets have no outline (16x10, 16x12).
constexpr int kOutlineMinHeight = 16;

enum GlyphStyle { kStylePlain = 0, kStyleOutline = 1, kStyleBold = 2 };

int StyleFor(int height) {
    if (height >= kOutlineMinHeight) {
        return kStyleOutline;
    }
    static int env = -2;
    if (env == -2) {
        const char* e = getenv("PORT_SMALL_TEXT_OUTLINE");
        env = (e == nullptr) ? -1 : std::clamp(atoi(e), 0, 2);
    }
    return (env >= 0) ? env : std::clamp(CVarGetInteger("gPort.SmallTextOutline", kStyleBold), 0, 2);
}

// The outlined glyph at Unifont scale: (width + 2) x 18 cells, 2 = fill, 1 = outline, 0 = clear. Then an area
// resample to `height` rows: each output texel's alpha is its covered fraction and its intensity the fill's share of
// that coverage, so the outline reads as dark like the game's glyphs. The plain styles keep the same size and advance
// (the layout does not move) and are all white, like the game's small sets: kStylePlain is the bare bitmap with its
// alpha raised (a one-pixel Unifont stroke covers only ~60% of a texel at 10 pixels and read as grey), kStyleBold
// also fills the cell right of and below each pixel (two-pixel strokes, as in the game's 10 and 12 pixel kana).
std::unique_ptr<Glyph> Build(const Bitmap& b, int height, int style) {
    const int sw = b.width + 2, sh = 18;
    std::vector<unsigned char> src(sw * sh, 0);
    for (int y = 0; y < sh; y++) {
        for (int x = 0; x < sw; x++) {
            if (Pixel(b, x - 1, y - 1)) {
                src[y * sw + x] = 2;
                continue;
            }
            if (style == kStyleBold) {
                if (Pixel(b, x - 2, y - 1) || Pixel(b, x - 1, y - 2) || Pixel(b, x - 2, y - 2)) {
                    src[y * sw + x] = 2;
                }
                continue;
            }
            if (style == kStylePlain) {
                continue;
            }
            for (int dy = -1; dy <= 1 && !src[y * sw + x]; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    if (Pixel(b, x - 1 + dx, y - 1 + dy)) {
                        src[y * sw + x] = 1;
                        break;
                    }
                }
            }
        }
    }
    const double scale = (double)height / sh;
    auto g = std::make_unique<Glyph>();
    g->drawWidth = std::max(1, (int)std::lround(sw * scale));
    g->texWidth = (g->drawWidth + 7) & ~7;
    // Unifont glyphs carry their own side bearing; let neighbouring outlines touch.
    g->advance = std::max(1, g->drawWidth - (int)std::lround(scale));
    g->tex.assign(g->texWidth * height, 0);
    for (int ty = 0; ty < height; ty++) {
        double y0 = ty / scale, y1 = (ty + 1) / scale;
        for (int tx = 0; tx < g->drawWidth; tx++) {
            double x0 = tx / scale, x1 = (tx + 1) / scale;
            double fill = 0, line = 0, area = (x1 - x0) * (y1 - y0);
            for (int sy = (int)y0; sy < sh && sy < y1; sy++) {
                double oy = std::min(y1, sy + 1.0) - std::max(y0, (double)sy);
                for (int sx = (int)x0; sx < sw && sx < x1; sx++) {
                    double o = oy * (std::min(x1, sx + 1.0) - std::max(x0, (double)sx));
                    if (src[sy * sw + sx] == 2) {
                        fill += o;
                    } else if (src[sy * sw + sx] == 1) {
                        line += o;
                    }
                }
            }
            double a = (fill + line) / area;
            if (style == kStylePlain) {
                a *= 1.6;
            }
            if (a <= 0) {
                continue;
            }
            int alpha = std::min(15, (int)std::lround(a * 15));
            int intensity = std::min(15, (int)std::lround(fill / (fill + line) * 15));
            g->tex[ty * g->texWidth + tx] = (unsigned char)((intensity << 4) | alpha);
        }
    }
    return g;
}


// ---- Plethron Pop (prototype) ----

struct PopFont {
    bool loaded = false, ok = false;
    std::vector<unsigned char> data;
    stbtt_fontinfo info;
    std::map<std::pair<uint32_t, int>, std::unique_ptr<Glyph>> glyphs; // (codepoint, height); never freed
};

PopFont& GetPopFont() {
    static PopFont font;
    if (!font.loaded) {
        font.loaded = true;
        std::string bytes, path;
        Port::ReadAppFile("assets/font/pop_subset.ttf", bytes, &path);
        font.data.assign(bytes.begin(), bytes.end());
        font.ok = !font.data.empty() &&
                  stbtt_InitFont(&font.info, font.data.data(), stbtt_GetFontOffsetForIndex(font.data.data(), 0));
        if (!font.ok) {
            fprintf(stderr, "[font] %s not loaded: translated text uses Unifont\n", path.c_str());
        }
    }
    return font;
}

bool UsePop() {
    static int env = -1;
    if (env < 0) {
        const char* e = getenv("PORT_TEXT_FONT");
        env = (e == nullptr) ? 2 : (strcmp(e, "pop") == 0 ? 1 : 0);
    }
    int choice = (env != 2) ? env : (CVarGetInteger("gPort.TextFont", 0) == 1 ? 1 : 0);
    return choice == 1 && GetPopFont().ok;
}

// The em of pop_subset.ttf is the whole cell (ascent 850, descent 150), so the glyph is drawn `height` pixels tall:
// rendered 4x supersampled, given an outline by dilating the fill with a disc (1 pixel per 16 of height, at least
// one), then box-filtered down; alpha and intensity as in Build above.
std::unique_ptr<Glyph> BuildPop(PopFont& font, int index, int height, int style) {
    constexpr int SS = 4;
    const float scale = stbtt_ScaleForMappingEmToPixels(&font.info, (float)(height * SS));
    int advUnits, lsb;
    stbtt_GetGlyphHMetrics(&font.info, index, &advUnits, &lsb);
    const float r = std::max(1.0f, height / 16.0f);
    const int pad = (int)std::ceil(r);
    const int adv = std::max(1, (int)std::lround(advUnits * scale / SS));
    auto g = std::make_unique<Glyph>();
    g->drawWidth = adv + 2 * pad;
    g->texWidth = (g->drawWidth + 7) & ~7;
    g->advance = adv;
    const int cw = g->drawWidth * SS, ch = height * SS;
    std::vector<unsigned char> fillSS(cw * ch, 0);
    int x0, y0, x1, y1;
    stbtt_GetGlyphBitmapBox(&font.info, index, scale, scale, &x0, &y0, &x1, &y1);
    const int ox = pad * SS, base = (int)std::lround(height * SS * 0.85f);
    if (x1 > x0 && y1 > y0) {
        std::vector<unsigned char> bmp((x1 - x0) * (y1 - y0));
        stbtt_MakeGlyphBitmap(&font.info, bmp.data(), x1 - x0, y1 - y0, x1 - x0, scale, scale, index);
        for (int y = 0; y < y1 - y0; y++) {
            int dy = base + y0 + y;
            if (dy < 0 || dy >= ch) continue;
            for (int x = 0; x < x1 - x0; x++) {
                int dx = ox + x0 + x;
                if (dx >= 0 && dx < cw) fillSS[dy * cw + dx] = bmp[y * (x1 - x0) + x];
            }
        }
    }
    // outline: grey dilation by a disc of radius r (supersampled): each texel takes the largest fill within the disc
    // dx*dx + dy*dy <= rs*rs + rs. The disc is a stack of rows, row dy reaching `reach[dy]` either side, so the
    // dilation is the largest, over dy, of the fill row y + dy's running maximum over [x - reach, x + reach]. The
    // running maxima for each reach are built one step wider at a time (wider[k] = max(wider[k-1], x - k, x + k)).
    // Same pixels as testing the whole disc at each texel, which took 25-70 ms a glyph at 64 pixels.
    // Pop is already extra bold, so both plain styles just leave the outline out (rs 0: nothing is dilated)
    const int rs = (style == kStyleOutline) ? (int)std::lround(r * SS) : 0;
    std::vector<int> reach(2 * rs + 1);
    for (int dy = -rs; dy <= rs; dy++) {
        int w = -1;
        while ((w + 1) * (w + 1) + dy * dy <= rs * rs + rs) w++;
        reach[dy + rs] = w; // >= rs for dy = 0, so every row has at least its own texel
    }
    std::vector<unsigned char> wider((size_t)(rs + 1) * cw * ch);
    std::copy(fillSS.begin(), fillSS.end(), wider.begin());
    for (int k = 1; k <= rs; k++) {
        const unsigned char* prev = &wider[(size_t)(k - 1) * cw * ch];
        unsigned char* cur = &wider[(size_t)k * cw * ch];
        for (int y = 0; y < ch; y++) {
            const unsigned char* f = &fillSS[(size_t)y * cw];
            for (int x = 0; x < cw; x++) {
                unsigned char m = prev[(size_t)y * cw + x];
                if (x - k >= 0) m = std::max(m, f[x - k]);
                if (x + k < cw) m = std::max(m, f[x + k]);
                cur[(size_t)y * cw + x] = m;
            }
        }
    }
    std::vector<unsigned char> lineSS(cw * ch, 0);
    for (int y = 0; y < ch; y++) {
        unsigned char* out = &lineSS[(size_t)y * cw];
        for (int dy = -rs; dy <= rs; dy++) {
            const int sy = y + dy;
            if (sy < 0 || sy >= ch || reach[dy + rs] < 0) continue;
            const unsigned char* row = &wider[((size_t)reach[dy + rs] * ch + sy) * cw];
            for (int x = 0; x < cw; x++) out[x] = std::max(out[x], row[x]);
        }
    }
    g->tex.assign(g->texWidth * height, 0);
    for (int ty = 0; ty < height; ty++) {
        for (int tx = 0; tx < g->drawWidth; tx++) {
            int f = 0, o = 0;
            for (int sy = 0; sy < SS; sy++) {
                for (int sx = 0; sx < SS; sx++) {
                    int i = (ty * SS + sy) * cw + tx * SS + sx;
                    f += fillSS[i];
                    o += lineSS[i];
                }
            }
            double fill = f / (255.0 * SS * SS), all = std::max(fill, o / (255.0 * SS * SS));
            if (all <= 0) continue;
            int alpha = std::min(15, (int)std::lround(all * 15));
            int intensity = std::min(15, (int)std::lround(fill / all * 15));
            g->tex[ty * g->texWidth + tx] = (unsigned char)((intensity << 4) | alpha);
        }
    }
    return g;
}

// The glyph cache is shared with Port_Font_PrewarmAsync's worker: lookups hold sGlyphLock, builds run outside it (a
// glyph both build at once is built twice and the first one kept; the textures never move or go away).
std::mutex sGlyphLock;

template <typename Cache, typename BuildFn>
const Glyph* CachedGlyph(Cache& cache, uint32_t cp, int height, BuildFn build) {
    {
        std::lock_guard<std::mutex> lock(sGlyphLock);
        auto it = cache.find({ cp, height });
        if (it != cache.end()) {
            return it->second.get();
        }
    }
    std::unique_ptr<Glyph> built = build();
    std::lock_guard<std::mutex> lock(sGlyphLock);
    auto& slot = cache[{ cp, height }];
    if (!slot) {
        slot = std::move(built);
    }
    return slot.get();
}

const unsigned char* FindGlyph(uint32_t cp, int height, bool pop, int* tex_width, int* draw_width, int* advance) {
    const Glyph* g = nullptr;
    const int style = StyleFor(height);
    const int key = height + 1000 * style; // the cache keeps every look, so the setting can change while running
    if (pop && height > 0 && height <= 64) {
        PopFont& font = GetPopFont();
        int index = stbtt_FindGlyphIndex(&font.info, (int)cp);
        if (index != 0) {
            g = CachedGlyph(font.glyphs, cp, key, [&] { return BuildPop(font, index, height, style); });
        }
    }
    if (g == nullptr) {
        Font& font = GetFont();
        auto bit = font.bitmaps.find(cp);
        if (bit == font.bitmaps.end() || height <= 0 || height > 64) {
            return nullptr;
        }
        g = CachedGlyph(font.glyphs, cp, key, [&] { return Build(bit->second, height, style); });
    }
    *tex_width = g->texWidth;
    *draw_width = g->drawWidth;
    *advance = g->advance;
    return g->tex.data();
}

} // namespace

extern "C" {

int Port_Font_HasGlyph(uint32_t cp) {
    return GetFont().bitmaps.count(cp) != 0 || (UsePop() && stbtt_FindGlyphIndex(&GetPopFont().info, (int)cp) != 0);
}

const unsigned char* Port_Font_Glyph(uint32_t cp, int height, int* tex_width, int* draw_width, int* advance) {
    return FindGlyph(cp, height, UsePop(), tex_width, draw_width, advance);
}

void Port_Font_PrewarmAsync(const char* utf8, int height) {
    if (utf8 == nullptr || height <= 0 || height > 64) {
        return;
    }
    const bool pop = UsePop(); // (loads the fonts here, before the worker reads them)
    GetFont();
    std::vector<uint32_t> cps;
    for (uint32_t cp; (cp = Port_Utf8Next(&utf8)) != 0;) {
        if (cp != ' ' && cp != '\n' && std::find(cps.begin(), cps.end(), cp) == cps.end()) {
            cps.push_back(cp);
        }
    }
    cps.push_back('?'); // the stand-in for a missing glyph
    std::thread([cps = std::move(cps), height, pop]() {
        int tw, dw, adv;
        for (uint32_t cp : cps) {
            FindGlyph(cp, height, pop, &tw, &dw, &adv);
        }
    }).detach();
}

uint32_t Port_Utf8Next(const char** s) {
    const unsigned char* p = (const unsigned char*)*s;
    uint32_t c = *p;
    int extra = 0;
    if (c == 0) {
        return 0;
    }
    if (c >= 0xF0) {
        c &= 0x07, extra = 3;
    } else if (c >= 0xE0) {
        c &= 0x0F, extra = 2;
    } else if (c >= 0xC0) {
        c &= 0x1F, extra = 1;
    } else if (c >= 0x80) {
        *s += 1;
        return 0xFFFD; // stray continuation byte
    }
    p++;
    for (int i = 0; i < extra; i++, p++) {
        if ((*p & 0xC0) != 0x80) {
            *s = (const char*)p;
            return 0xFFFD;
        }
        c = (c << 6) | (*p & 0x3F);
    }
    *s = (const char*)p;
    return c;
}

} // extern "C"
