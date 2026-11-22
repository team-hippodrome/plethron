// Unicode glyphs for translated text, built by hippodrome-utils' text_font.h (shared with Stadion and Diaulos) in the
// style of the game's own font: white fill with a one-pixel dark outline, resampled to the height of the active font
// set. font.c draws them (Font_Printf). The fonts come from hippodrome-utils/assets/font, packed into plethron.o2r
// at assets/font/ (tools/make_o2r.py); a copy next to the executable wins (app_file.h):
//   - GNU Unifont 16-pixel bitmaps (unifont_subset.hex): the default, and the fallback for any character Pop lacks.
//   - the prototype Plethron Pop outlines (pop_subset.ttf), picked by the "Text font" setting (gPort.TextFont,
//     PORT_TEXT_FONT=pop|unifont).
// The game's own font sets 16x10 and 16x12 have no outline (plain white glyphs); only 16x16 and up are outlined. The
// "Small text" setting (gPort.SmallTextOutline, PORT_SMALL_TEXT_OUTLINE=1|0|2) picks how translated text below 16
// pixels is drawn: 1 = outlined, 0 = plain white like the game's small kana, 2 = plain white and
// one source pixel bolder (the game's small kana have two-pixel strokes; the default, Julius 2026-11-21).
// This file keeps what is Plethron's own: the settings, reading the files and the glyph cache.
#include "unicode_font.h"
#include "app_file.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <libultraship/libultraship.h>

#include "hippodrome/text_font.h"

namespace {

// A built glyph; the texture never moves or goes away.
struct Glyph {
    hu_glyph g = {};
    ~Glyph() { hu_glyph_free(&g); }
};
using GlyphCache = std::map<std::pair<uint32_t, int>, std::unique_ptr<Glyph>>; // (codepoint, height + 1000 * style)

struct Font {
    bool loaded = false;
    hu_unifont* font = nullptr;
    GlyphCache glyphs;
};

Font& GetFont() {
    static Font font;
    if (!font.loaded) {
        font.loaded = true;
        std::string hex, path;
        if (!Port::ReadAppFile("assets/font/unifont_subset.hex", hex, &path)) {
            fprintf(stderr, "[font] %s not found: translated text will not draw\n", path.c_str());
        }
        font.font = hu_unifont_parse(hex.data(), hex.size());
    }
    return font;
}

hu_glyph_style StyleFor(int height) {
    static int env = -2;
    if (env == -2) {
        const char* e = getenv("PORT_SMALL_TEXT_OUTLINE");
        env = (e == nullptr) ? -1 : std::clamp(atoi(e), 0, 2);
    }
    const int small = (env >= 0) ? env : std::clamp(CVarGetInteger("gPort.SmallTextOutline", HU_GLYPH_BOLD), 0, 2);
    return hu_glyph_style_for(height, (hu_glyph_style)small);
}

struct PopFont {
    bool loaded = false;
    hu_pop* font = nullptr;
    GlyphCache glyphs;
};

PopFont& GetPopFont() {
    static PopFont font;
    if (!font.loaded) {
        font.loaded = true;
        std::string bytes, path;
        Port::ReadAppFile("assets/font/pop_subset.ttf", bytes, &path);
        font.font = bytes.empty() ? nullptr : hu_pop_load(bytes.data(), bytes.size());
        if (font.font == nullptr) {
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
    return choice == 1 && GetPopFont().font != nullptr;
}

// The glyph cache is shared with Port_Font_PrewarmAsync's worker: lookups hold sGlyphLock, builds run outside it (a
// glyph both build at once is built twice and the first one kept; the textures never move or go away).
std::mutex sGlyphLock;

template <typename BuildFn> const Glyph* CachedGlyph(GlyphCache& cache, uint32_t cp, int key, BuildFn build) {
    {
        std::lock_guard<std::mutex> lock(sGlyphLock);
        auto it = cache.find({ cp, key });
        if (it != cache.end()) {
            return it->second.get();
        }
    }
    auto built = std::make_unique<Glyph>();
    if (!build(&built->g)) {
        return nullptr;
    }
    std::lock_guard<std::mutex> lock(sGlyphLock);
    auto& slot = cache[{ cp, key }];
    if (!slot) {
        slot = std::move(built);
    }
    return slot.get();
}

const unsigned char* FindGlyph(uint32_t cp, int height, bool pop, int* tex_width, int* draw_width, int* advance) {
    if (height <= 0 || height > HU_GLYPH_MAX_HEIGHT) {
        return nullptr;
    }
    const Glyph* g = nullptr;
    const hu_glyph_style style = StyleFor(height);
    const int key = height + 1000 * style; // the cache keeps every look, so the setting can change while running
    if (pop) {
        PopFont& font = GetPopFont();
        if (hu_pop_has(font.font, cp)) {
            g = CachedGlyph(font.glyphs, cp, key,
                            [&](hu_glyph* out) { return hu_pop_glyph(font.font, cp, height, style, out); });
        }
    }
    if (g == nullptr) {
        Font& font = GetFont();
        if (!hu_unifont_has(font.font, cp)) {
            return nullptr;
        }
        g = CachedGlyph(font.glyphs, cp, key,
                        [&](hu_glyph* out) { return hu_unifont_glyph(font.font, cp, height, style, out); });
        if (g == nullptr) {
            return nullptr;
        }
    }
    *tex_width = g->g.tex_width;
    *draw_width = g->g.draw_width;
    *advance = g->g.advance;
    return g->g.tex;
}

} // namespace

extern "C" {

int Port_Font_HasGlyph(uint32_t cp) {
    return hu_unifont_has(GetFont().font, cp) || (UsePop() && hu_pop_has(GetPopFont().font, cp));
}

const unsigned char* Port_Font_Glyph(uint32_t cp, int height, int* tex_width, int* draw_width, int* advance) {
    return FindGlyph(cp, height, UsePop(), tex_width, draw_width, advance);
}

void Port_Font_PrewarmAsync(const char* utf8, int height) {
    if (utf8 == nullptr || height <= 0 || height > HU_GLYPH_MAX_HEIGHT) {
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
    return hu_utf8_next(s);
}

} // extern "C"
