// Unicode glyphs for translated text: GNU Unifont 16-pixel bitmaps (assets/font/unifont_subset.hex, made by
// tools/font/make_unifont_subset.py) turned into IA8 textures in the style of the game's own font: white fill with a
// one-pixel dark outline, resampled to the height of the active font set. font.c draws them (Font_Printf).
// The "Text font" setting (gPort.TextFont, PORT_TEXT_FONT=pop|unifont) can pick the prototype Plethron Pop outlines
// instead (assets/font/pop_subset.ttf, tools/font/make_pop_font.py), drawn the same way; Unifont stays the default
// and covers any character Pop lacks.
#include "unicode_font.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <cstdlib>
#include <map>
#include <memory>
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
        std::string path = Ship::Context::LocateFileAcrossAppDirs("assets/font/unifont_subset.hex");
        std::ifstream f(path);
        if (!f) {
            fprintf(stderr, "[font] %s not found: translated text will not draw\n", path.c_str());
        }
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

// The outlined glyph at Unifont scale: (width + 2) x 18 cells, 2 = fill, 1 = outline, 0 = clear. Then an area
// resample to `height` rows: each output texel's alpha is its covered fraction and its intensity the fill's share of
// that coverage, so the outline reads as dark like the game's glyphs.
std::unique_ptr<Glyph> Build(const Bitmap& b, int height) {
    const int sw = b.width + 2, sh = 18;
    std::vector<unsigned char> src(sw * sh, 0);
    for (int y = 0; y < sh; y++) {
        for (int x = 0; x < sw; x++) {
            if (Pixel(b, x - 1, y - 1)) {
                src[y * sw + x] = 2;
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
        std::string path = Ship::Context::LocateFileAcrossAppDirs("assets/font/pop_subset.ttf");
        std::ifstream f(path, std::ios::binary);
        font.data.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
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
std::unique_ptr<Glyph> BuildPop(PopFont& font, int index, int height) {
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
    // outline: grey dilation by a disc of radius r (supersampled)
    const int rs = (int)std::lround(r * SS);
    std::vector<std::pair<int, int>> disc;
    for (int dy = -rs; dy <= rs; dy++)
        for (int dx = -rs; dx <= rs; dx++)
            if (dx * dx + dy * dy <= rs * rs + rs) disc.emplace_back(dx, dy);
    std::vector<unsigned char> lineSS(cw * ch, 0);
    for (int y = 0; y < ch; y++) {
        for (int x = 0; x < cw; x++) {
            unsigned char m = 0;
            for (auto [dx, dy] : disc) {
                int sx = x + dx, sy = y + dy;
                if (sx >= 0 && sy >= 0 && sx < cw && sy < ch) m = std::max(m, fillSS[sy * cw + sx]);
            }
            lineSS[y * cw + x] = m;
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

} // namespace

extern "C" {

int Port_Font_HasGlyph(uint32_t cp) {
    return GetFont().bitmaps.count(cp) != 0 || (UsePop() && stbtt_FindGlyphIndex(&GetPopFont().info, (int)cp) != 0);
}

const unsigned char* Port_Font_Glyph(uint32_t cp, int height, int* tex_width, int* draw_width, int* advance) {
    if (UsePop() && height > 0 && height <= 64) {
        PopFont& pop = GetPopFont();
        int index = stbtt_FindGlyphIndex(&pop.info, (int)cp);
        if (index != 0) {
            auto& slot = pop.glyphs[{ cp, height }];
            if (!slot) {
                slot = BuildPop(pop, index, height);
            }
            *tex_width = slot->texWidth;
            *draw_width = slot->drawWidth;
            *advance = slot->advance;
            return slot->tex.data();
        }
    }
    Font& font = GetFont();
    auto bit = font.bitmaps.find(cp);
    if (bit == font.bitmaps.end() || height <= 0 || height > 64) {
        return nullptr;
    }
    auto& slot = font.glyphs[{ cp, height }];
    if (!slot) {
        slot = Build(bit->second, height);
    }
    *tex_width = slot->texWidth;
    *draw_width = slot->drawWidth;
    *advance = slot->advance;
    return slot->tex.data();
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
