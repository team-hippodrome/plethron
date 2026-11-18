// Translated 3D words: see text3d.h.
//
// The game's letters (e.g. fragment 035's バトル-1..6, fragment 033's かち / まけ) are F3DEX2 lists of flat-shaded
// triangles with normals in the vertex colour slot: a front and a back face, a chamfer of 45 degrees around both, and
// the side walls. Seen from the front the face is the glyph's outline pulled in by the chamfer's width, and the walls
// are the outline itself. The profile (face depth, wall depth) is read from the original glyphs, so the generated
// letters keep each word's thickness and bevel; the outlines come from Noto Sans CJK JP Bold.
//
// Layout: the word's letters fly in one part per glyph, and the parts' rest positions are fixed. The translation is
// laid out as one line centred where the original word was, at the original glyphs' height (narrowed, then scaled
// down, when it is wider than the original word allows), and each translated glyph goes to the part whose rest
// position is nearest to it, so the word still arrives in pieces from the same directions.
#include "text3d.h"
#include "text_lookup.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <libultraship/libultraship.h>

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "third_party/stb_truetype.h"
#include "third_party/earcut.hpp"

extern "C" void* Port_GfxPrepareDL(uintptr_t dl);

namespace {

struct Vec2 {
    float x, y;
};
using Contour = std::vector<Vec2>;

struct Font {
    bool tried = false;
    bool ok = false;
    std::vector<unsigned char> data;
    stbtt_fontinfo info;
    float unitsPerEm = 1000.0f;
};

Font& GetFont() {
    static Font f;
    if (f.tried) return f;
    f.tried = true;
    std::string path = Ship::Context::LocateFileAcrossAppDirs("assets/font/text3d_subset.otf");
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        fprintf(stderr, "[text3d] %s not found: 3D words stay in the original\n", path.c_str());
        return f;
    }
    f.data.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    if (!stbtt_InitFont(&f.info, f.data.data(), stbtt_GetFontOffsetForIndex(f.data.data(), 0))) {
        fprintf(stderr, "[text3d] %s: not a font\n", path.c_str());
        return f;
    }
    int x0, y0, x1, y1;
    stbtt_GetFontBoundingBox(&f.info, &x0, &y0, &x1, &y1);
    f.unitsPerEm = 1.0f / stbtt_ScaleForMappingEmToPixels(&f.info, 1.0f);
    f.ok = true;
    return f;
}

std::vector<uint32_t> DecodeUtf8(const char* s) {
    std::vector<uint32_t> out;
    const unsigned char* p = (const unsigned char*)s;
    while (*p) {
        uint32_t c = *p++;
        int extra = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : 0;
        if (extra) c &= 0x3F >> extra;
        while (extra-- > 0 && (*p & 0xC0) == 0x80) c = (c << 6) | (*p++ & 0x3F);
        out.push_back(c);
    }
    return out;
}

// The glyph's contours in font units (y up), curves flattened.
std::vector<Contour> GlyphContours(Font& font, int glyph) {
    std::vector<Contour> out;
    stbtt_vertex* v = nullptr;
    int n = stbtt_GetGlyphShape(&font.info, glyph, &v);
    Contour cur;
    float px = 0, py = 0;
    auto close = [&]() {
        if (cur.size() > 1) {
            if (std::fabs(cur.front().x - cur.back().x) < 0.01f && std::fabs(cur.front().y - cur.back().y) < 0.01f)
                cur.pop_back();
            if (cur.size() >= 3) out.push_back(cur);
        }
        cur.clear();
    };
    for (int i = 0; i < n; i++) {
        const stbtt_vertex& e = v[i];
        switch (e.type) {
            case STBTT_vmove:
                close();
                cur.push_back({ (float)e.x, (float)e.y });
                break;
            case STBTT_vline:
                cur.push_back({ (float)e.x, (float)e.y });
                break;
            case STBTT_vcurve: { // quadratic: 4 segments, like the game's coarse curves
                for (int k = 1; k <= 4; k++) {
                    float t = k / 4.0f, u = 1 - t;
                    cur.push_back({ u * u * px + 2 * u * t * e.cx + t * t * e.x, u * u * py + 2 * u * t * e.cy + t * t * e.y });
                }
                break;
            }
            case STBTT_vcubic: {
                for (int k = 1; k <= 5; k++) {
                    float t = k / 5.0f, u = 1 - t;
                    cur.push_back({ u * u * u * px + 3 * u * u * t * e.cx + 3 * u * t * t * e.cx1 + t * t * t * e.x,
                                    u * u * u * py + 3 * u * u * t * e.cy + 3 * u * t * t * e.cy1 + t * t * t * e.y });
                }
                break;
            }
        }
        px = e.x;
        py = e.y;
    }
    close();
    stbtt_FreeShape(&font.info, v);
    // drop points closer than 1 font unit to the previous one (zero-length edges have no normal)
    for (Contour& c : out) {
        Contour d;
        for (const Vec2& p : c)
            if (d.empty() || std::fabs(p.x - d.back().x) + std::fabs(p.y - d.back().y) > 1.0f) d.push_back(p);
        while (d.size() > 1 && std::fabs(d.front().x - d.back().x) + std::fabs(d.front().y - d.back().y) <= 1.0f)
            d.pop_back();
        c = d;
    }
    out.erase(std::remove_if(out.begin(), out.end(), [](const Contour& c) { return c.size() < 3; }), out.end());
    return out;
}

float SignedArea(const Contour& c) {
    double a = 0;
    for (size_t i = 0, j = c.size() - 1; i < c.size(); j = i++) a += (double)c[j].x * c[i].y - (double)c[i].x * c[j].y;
    return (float)(a / 2);
}

bool Inside(const Contour& c, Vec2 p) {
    bool in = false;
    for (size_t i = 0, j = c.size() - 1; i < c.size(); j = i++) {
        if (((c[i].y > p.y) != (c[j].y > p.y)) &&
            (p.x < (c[j].x - c[i].x) * (p.y - c[i].y) / (c[j].y - c[i].y) + c[i].x))
            in = !in;
    }
    return in;
}

// Outer contours (counter-clockwise) with the holes (clockwise) directly inside them, so the interior is on the left
// of every edge.
struct Shape {
    Contour outer;
    std::vector<Contour> holes;
};

std::vector<Shape> ToShapes(std::vector<Contour> cs) {
    std::vector<int> depth(cs.size(), 0);
    for (size_t i = 0; i < cs.size(); i++)
        for (size_t j = 0; j < cs.size(); j++)
            if (i != j && Inside(cs[j], cs[i][0])) depth[i]++;
    std::vector<Shape> shapes;
    std::vector<int> shapeOf(cs.size(), -1);
    for (size_t i = 0; i < cs.size(); i++) {
        if (depth[i] % 2) continue;
        if (SignedArea(cs[i]) < 0) std::reverse(cs[i].begin(), cs[i].end());
        shapeOf[i] = (int)shapes.size();
        shapes.push_back({ cs[i], {} });
    }
    for (size_t i = 0; i < cs.size(); i++) {
        if (!(depth[i] % 2)) continue;
        if (SignedArea(cs[i]) > 0) std::reverse(cs[i].begin(), cs[i].end());
        // the innermost outer contour around it
        int best = -1, bestDepth = -1;
        for (size_t j = 0; j < cs.size(); j++)
            if (shapeOf[j] >= 0 && depth[j] == depth[i] - 1 && Inside(cs[j], cs[i][0]) && depth[j] > bestDepth) {
                best = shapeOf[j];
                bestDepth = depth[j];
            }
        if (best >= 0) shapes[best].holes.push_back(cs[i]);
    }
    return shapes;
}

// The contour pulled `d` towards its interior (left of each edge), mitred, the mitre limited to 2.5 d.
Contour Inset(const Contour& c, float d) {
    size_t n = c.size();
    Contour out(n);
    for (size_t i = 0; i < n; i++) {
        Vec2 a = c[(i + n - 1) % n], b = c[i], e = c[(i + 1) % n];
        float ux = b.x - a.x, uy = b.y - a.y, ul = std::sqrt(ux * ux + uy * uy);
        float vx = e.x - b.x, vy = e.y - b.y, vl = std::sqrt(vx * vx + vy * vy);
        Vec2 n1 = { -uy / ul, ux / ul }, n2 = { -vy / vl, vx / vl };
        float mx = n1.x + n2.x, my = n1.y + n2.y, ml = std::sqrt(mx * mx + my * my);
        if (ml < 1e-4f) {
            out[i] = { b.x + n1.x * d, b.y + n1.y * d };
            continue;
        }
        mx /= ml;
        my /= ml;
        float cosHalf = mx * n1.x + my * n1.y;
        float len = d / std::max(cosHalf, 0.4f);
        out[i] = { b.x + mx * len, b.y + my * len };
    }
    return out;
}

struct V3 {
    float x, y, z;
};
struct MeshTri {
    V3 p[3];
    V3 n; // unit normal
};

void AddTri(std::vector<MeshTri>& out, V3 a, V3 b, V3 c, V3 n) {
    // wind counter-clockwise seen from the side the normal points to (back faces are culled, as in the originals)
    V3 u = { b.x - a.x, b.y - a.y, b.z - a.z }, v = { c.x - a.x, c.y - a.y, c.z - a.z };
    V3 g = { u.y * v.z - u.z * v.y, u.z * v.x - u.x * v.z, u.x * v.y - u.y * v.x };
    if (g.x * n.x + g.y * n.y + g.z * n.z < 0) std::swap(b, c);
    out.push_back({ { a, b, c }, n });
}

V3 Norm(float x, float y, float z) {
    float l = std::sqrt(x * x + y * y + z * z);
    return { x / l, y / l, z / l };
}

// A bevelled solid of `shape` (already in the part's local units): faces at +-zFace pulled in by `bevel`, walls
// between +-zWall on the outline.
void Extrude(const Shape& shape, float zFace, float zWall, float bevel, std::vector<MeshTri>& out) {
    std::vector<Contour> rings;
    rings.push_back(shape.outer);
    for (const Contour& h : shape.holes) rings.push_back(h);
    std::vector<Contour> inner;
    for (const Contour& r : rings) inner.push_back(Inset(r, bevel));

    // front and back faces
    std::vector<std::vector<std::array<float, 2>>> poly;
    std::vector<Vec2> flat;
    for (const Contour& r : inner) {
        poly.emplace_back();
        for (const Vec2& p : r) {
            poly.back().push_back({ p.x, p.y });
            flat.push_back(p);
        }
    }
    std::vector<uint32_t> idx = mapbox::earcut<uint32_t>(poly);
    for (size_t i = 0; i + 2 < idx.size(); i += 3) {
        Vec2 a = flat[idx[i]], b = flat[idx[i + 1]], c = flat[idx[i + 2]];
        AddTri(out, { a.x, a.y, zFace }, { b.x, b.y, zFace }, { c.x, c.y, zFace }, { 0, 0, 1 });
        AddTri(out, { a.x, a.y, -zFace }, { b.x, b.y, -zFace }, { c.x, c.y, -zFace }, { 0, 0, -1 });
    }

    // walls and the two chamfers, one flat quad per outline edge
    for (size_t r = 0; r < rings.size(); r++) {
        const Contour& o = rings[r];
        const Contour& in = inner[r];
        size_t n = o.size();
        for (size_t i = 0; i < n; i++) {
            size_t j = (i + 1) % n;
            Vec2 a = o[i], b = o[j], ai = in[i], bi = in[j];
            float ex = b.x - a.x, ey = b.y - a.y;
            V3 side = Norm(ey, -ex, 0); // outwards: interior is on the left
            AddTri(out, { a.x, a.y, -zWall }, { b.x, b.y, -zWall }, { b.x, b.y, zWall }, side);
            AddTri(out, { a.x, a.y, -zWall }, { b.x, b.y, zWall }, { a.x, a.y, zWall }, side);
            for (int s = -1; s <= 1; s += 2) {
                V3 bn = Norm(side.x, side.y, (float)s);
                AddTri(out, { a.x, a.y, s * zWall }, { b.x, b.y, s * zWall }, { bi.x, bi.y, s * zFace }, bn);
                AddTri(out, { a.x, a.y, s * zWall }, { bi.x, bi.y, s * zFace }, { ai.x, ai.y, s * zFace }, bn);
            }
        }
    }
}

// What the original glyph list draws: its vertices' extent and the depth of its faces and walls.
struct Profile {
    bool ok = false;
    float x0 = 1e9f, x1 = -1e9f, y0 = 1e9f, y1 = -1e9f;
    float zFace = 0, zWall = 0;
};

Profile ReadProfile(uint32_t link) {
    Profile p;
    const Gfx* g = (const Gfx*)Port_GfxPrepareDL(link);
    if (g == nullptr || (uintptr_t)g == link) return p;
    std::vector<float> zs;
    for (int i = 0; i < 4096; i++) {
        uint32_t op = (uint32_t)((g[i].words.w0 >> 24) & 0xFF);
        if (op == G_ENDDL) break;
        if (op != G_VTX) continue;
        int n = (int)((g[i].words.w0 >> 12) & 0xFF);
        const Vtx* v = (const Vtx*)g[i].words.w1;
        if (v == nullptr || (uintptr_t)v < 0x100000000ull) continue;
        for (int k = 0; k < n; k++) {
            float x = v[k].v.ob[0], y = v[k].v.ob[1], z = v[k].v.ob[2];
            p.x0 = std::min(p.x0, x);
            p.x1 = std::max(p.x1, x);
            p.y0 = std::min(p.y0, y);
            p.y1 = std::max(p.y1, y);
            zs.push_back(std::fabs(z));
            p.ok = true;
        }
    }
    if (!p.ok) return p;
    std::sort(zs.begin(), zs.end());
    p.zFace = zs.back();
    p.zWall = 0;
    for (float z : zs)
        if (z < p.zFace - 0.5f) p.zWall = z; // the largest depth below the face: the walls' edge
    if (p.zWall <= 0) p.zWall = p.zFace * 0.8f;
    return p;
}

struct Built {
    std::vector<Vtx> vtx;
    std::vector<Gfx> gfx;
};

// A display list for `tris`: the originals' geometry mode, then triangles 10 at a time.
std::unique_ptr<Built> Emit(const std::vector<MeshTri>& tris) {
    auto b = std::make_unique<Built>();
    b->vtx.reserve(tris.size() * 3);
    for (const MeshTri& t : tris) {
        for (const V3& p : t.p) {
            Vtx v = {};
            v.n.ob[0] = (short)std::lround(p.x);
            v.n.ob[1] = (short)std::lround(p.y);
            v.n.ob[2] = (short)std::lround(p.z);
            v.n.n[0] = (signed char)std::lround(t.n.x * 120);
            v.n.n[1] = (signed char)std::lround(t.n.y * 120);
            v.n.n[2] = (signed char)std::lround(t.n.z * 120);
            v.n.a = 0xFF;
            b->vtx.push_back(v);
        }
    }
    size_t batches = (tris.size() + 9) / 10;
    b->gfx.resize(2 + batches * 6 + 1);
    Gfx* g = b->gfx.data();
    gSPSetGeometryMode(g++, G_CULL_BACK);
    for (size_t t = 0; t < tris.size(); t += 10) {
        int n = (int)std::min<size_t>(10, tris.size() - t);
        __gSPVertex(g++, &b->vtx[t * 3], n * 3, 0);
        for (int k = 0; k < n; k += 2) {
            if (k + 1 < n)
                gSP2Triangles(g++, k * 3, k * 3 + 1, k * 3 + 2, 0, k * 3 + 3, k * 3 + 4, k * 3 + 5, 0);
            else
                gSP1Triangle(g++, k * 3, k * 3 + 1, k * 3 + 2, 0);
        }
    }
    gSPEndDisplayList(g++);
    b->gfx.resize(g - b->gfx.data());
    return b;
}

std::unordered_map<uint32_t, Gfx*> gReplacements;

// An original part moved sideways (the round number after a longer word): its list drawn under a translation.
struct Follower {
    Mtx mtx;
    Gfx gfx[4];
    uint32_t link;
};
std::unordered_map<uint32_t, Follower*> gFollowerOf;
std::vector<std::unique_ptr<Follower>> gFollowers, gRetiredFollowers;
std::vector<std::unique_ptr<Built>> gLive, gRetired;
bool gAny = false;

bool LogOn() {
    static const bool v = getenv("PORT_TEXT3D_LOG") != nullptr;
    return v;
}

} // namespace

extern "C" {

int Port_Text3D_Enabled(void) {
    return Port_Text_GetLanguage() != PORT_LANG_ORIGINAL;
}

void Port_Text3D_Build(const char* id, const Port3DTextSlot* slots, int count, float maxGrow,
                       const Port3DTextSlot* followers, int followerCount) {
    if (!Port_Text3D_Enabled() || count <= 0) return;
    const char* text = Port_Text_LookupId(id);
    if (text == nullptr || *text == '\0') return; // empty cell: the original models
    Font& font = GetFont();
    if (!font.ok) return;
    if (gReplacements.empty()) { // frames that used the retired ones are long drawn
        gRetired.clear();
        gRetiredFollowers.clear();
    }

    // the original word: each glyph's extent in model space, its profile in its own part's units
    std::vector<Profile> prof(count);
    float left = 1e9f, right = -1e9f, bottom = 1e9f, top = -1e9f, height = 0;
    for (int i = 0; i < count; i++) {
        prof[i] = ReadProfile(slots[i].dl);
        if (!prof[i].ok) {
            fprintf(stderr, "[text3d] %s: glyph list 0x%08X not readable, keeping the original\n", id, slots[i].dl);
            return;
        }
        const Port3DTextSlot& s = slots[i];
        left = std::min(left, s.x + s.scaleX * prof[i].x0);
        right = std::max(right, s.x + s.scaleX * prof[i].x1);
        bottom = std::min(bottom, s.y + s.scaleY * prof[i].y0);
        top = std::max(top, s.y + s.scaleY * prof[i].y1);
        height = std::max(height, s.scaleY * (prof[i].y1 - prof[i].y0));
    }

    // lay the translation out in font units on one line, a little looser than the font (the originals are spaced)
    std::vector<uint32_t> cps = DecodeUtf8(text);
    struct Placed {
        std::vector<Shape> shapes;
        float x0, x1; // ink extent, font units, after placement
        float y0, y1;
    };
    std::vector<Placed> glyphs;
    float pen = 0, inkL = 1e9f, inkR = -1e9f, inkB = 1e9f, inkT = -1e9f;
    const float track = 0.06f * font.unitsPerEm; // Latin letters a little apart, as the kana are
    for (uint32_t cp : cps) {
        int gi = stbtt_FindGlyphIndex(&font.info, (int)cp);
        int adv, lsb;
        stbtt_GetGlyphHMetrics(&font.info, gi, &adv, &lsb);
        std::vector<Contour> cs = gi ? GlyphContours(font, gi) : std::vector<Contour>();
        if (gi == 0 && cp != ' ') fprintf(stderr, "[text3d] %s: U+%04X is not in text3d_subset.otf\n", id, cp);
        if (!cs.empty()) {
            Placed p = { {}, 1e9f, -1e9f, 1e9f, -1e9f };
            for (Contour& c : cs)
                for (Vec2& v : c) {
                    v.x += pen;
                    p.x0 = std::min(p.x0, v.x);
                    p.x1 = std::max(p.x1, v.x);
                    p.y0 = std::min(p.y0, v.y);
                    p.y1 = std::max(p.y1, v.y);
                }
            p.shapes = ToShapes(cs);
            inkL = std::min(inkL, p.x0);
            inkR = std::max(inkR, p.x1);
            inkB = std::min(inkB, p.y0);
            inkT = std::max(inkT, p.y1);
            glyphs.push_back(std::move(p));
        }
        pen += adv + (cp < 0x2E80 ? track : 0); // CJK and hangul are set solid, like the kana
    }
    if (glyphs.empty()) return;

    const float em = font.unitsPerEm;
    float sx = 1, uniform = 1, width, avail;
    float ux, uy, baseline, originX;
    float followerShift = 0;
    std::vector<Profile> fprof(followerCount > 0 ? followerCount : 0);
    if (followerCount > 0) {
        // The word is followed by parts that stay (the round title's dash and number): the word takes the number's
        // height, and the dash and number move right by what the word grew, the whole line staying centred.
        float fl = 1e9f, fr = -1e9f, fb = 1e9f, ft = -1e9f, fh = 0;
        for (int i = 0; i < followerCount; i++) {
            fprof[i] = ReadProfile(followers[i].dl);
            if (!fprof[i].ok) {
                fprintf(stderr, "[text3d] %s: list 0x%08X not readable, keeping the original\n", id, followers[i].dl);
                return;
            }
            const Port3DTextSlot& f = followers[i];
            fl = std::min(fl, f.x + f.scaleX * fprof[i].x0);
            fr = std::max(fr, f.x + f.scaleX * fprof[i].x1);
            fb = std::min(fb, f.y + f.scaleY * fprof[i].y0);
            ft = std::max(ft, f.y + f.scaleY * fprof[i].y1);
            fh = std::max(fh, f.scaleY * (fprof[i].y1 - fprof[i].y0));
        }
        float unit = fh / (inkT - inkB);
        width = (inkR - inkL) * unit;
        float gap = fl - right, fw = fr - fl;
        avail = (fr - left) * maxGrow - gap - fw; // room for the word
        if (width > avail) { // narrow the letters a little first, then scale the word down
            sx = avail / width;
            if (sx < 0.75f) {
                uniform = sx / 0.75f;
                sx = 0.75f;
            }
            width = avail;
        }
        ux = unit * uniform * sx;
        uy = unit * uniform;
        float total = width + gap + fw;
        float lineLeft = (left + fr) / 2 - total / 2;
        originX = lineLeft - inkL * ux;
        followerShift = (lineLeft + width + gap) - fl;
        baseline = (fb + ft) / 2 - (inkB + inkT) / 2 * uy; // ink centred on the number
    } else {
        // size: kana fill about 0.88 em, so the em box gets the original glyphs' height over that; then fit the width
        float unit = (height / 0.88f) / em; // model units per font unit
        width = (inkR - inkL) * unit;
        avail = (right - left) * maxGrow;
        if (width > avail) {
            sx = avail / width;
            if (sx < 0.75f) {
                uniform = sx / 0.75f;
                sx = 0.75f;
            }
        }
        ux = unit * uniform * sx;
        uy = unit * uniform;
        float cx = (left + right) / 2, cy = (bottom + top) / 2;
        baseline = cy - 0.38f * em * uy; // the em box (descender -0.12 em, ascender 0.88 em) centred on the word
        originX = cx - (inkL + inkR) / 2 * ux;
    }

    // each glyph goes to the part nearest to its centre
    std::vector<std::vector<int>> owned(count);
    for (size_t g = 0; g < glyphs.size(); g++) {
        float gx = originX + (glyphs[g].x0 + glyphs[g].x1) / 2 * ux;
        int best = 0;
        float bestD = 1e30f;
        for (int i = 0; i < count; i++) {
            float sxc = slots[i].x + slots[i].scaleX * (prof[i].x0 + prof[i].x1) / 2;
            float d = std::fabs(sxc - gx);
            if (d < bestD) {
                bestD = d;
                best = i;
            }
        }
        owned[best].push_back((int)g);
    }

    int built = 0;
    for (int i = 0; i < count; i++) {
        const Port3DTextSlot& s = slots[i];
        const Profile& p = prof[i];
        float bevel = p.zFace - p.zWall;
        std::vector<MeshTri> tris;
        for (int g : owned[i]) {
            for (const Shape& sh : glyphs[g].shapes) {
                // font units -> model space -> this part's units
                auto local = [&](Contour c) {
                    for (Vec2& v : c) {
                        float mx = originX + v.x * ux, my = baseline + v.y * uy;
                        v = { (mx - s.x) / s.scaleX, (my - s.y) / s.scaleY };
                    }
                    return c;
                };
                Shape ls = { local(sh.outer), {} };
                for (const Contour& h : sh.holes) ls.holes.push_back(local(h));
                // the chamfer shrinks with the letters when the word had to be scaled down
                float b = bevel * std::min(uniform, 1.0f);
                Extrude(ls, p.zWall + b, p.zWall, b, tris);
            }
        }
        std::unique_ptr<Built> out = Emit(tris);
        gReplacements[s.dl] = out->gfx.data();
        gLive.push_back(std::move(out));
        gAny = true;
        built += (int)tris.size();
        if (LogOn())
            fprintf(stderr, "[text3d] %s part %d (0x%08X at %.0f,%.0f): %zu glyphs, %zu triangles\n", id, i, s.dl, s.x,
                    s.y, owned[i].size(), tris.size());
    }
    for (int i = 0; i < followerCount && followerShift != 0; i++) {
        auto w = std::make_unique<Follower>();
        w->link = followers[i].dl;
        guTranslate(&w->mtx, followerShift / followers[i].scaleX, 0, 0);
        Gfx* g = w->gfx;
        gSPMatrix(g++, &w->mtx, G_MTX_MODELVIEW | G_MTX_MUL | G_MTX_PUSH);
        __gSPDisplayList(g++, nullptr); // the original list, refreshed in Port_Text3D_Replacement
        gSPPopMatrix(g++, G_MTX_MODELVIEW);
        gSPEndDisplayList(g++);
        gFollowerOf[w->link] = w.get();
        gReplacements[w->link] = w->gfx;
        gFollowers.push_back(std::move(w));
        gAny = true;
    }
    if (LogOn())
        fprintf(stderr, "[text3d] %s \"%s\": %d triangles, width %.0f of %.0f, narrowed %.2f, scaled %.2f, shift %.0f\n",
                id, text, built, width, avail, sx, uniform, followerShift);
}

void* Port_Text3D_Replacement(uintptr_t dl) {
    if (!gAny || dl >= 0x100000000ull) return nullptr;
    auto it = gReplacements.find((uint32_t)dl);
    if (it == gReplacements.end()) return nullptr;
    auto f = gFollowerOf.find((uint32_t)dl);
    if (f != gFollowerOf.end()) {
        // the translated original list can be retired with the translation cache: take the current one each time
        void* orig = Port_GfxPrepareDL(dl);
        if (orig == nullptr || (uintptr_t)orig == dl) return nullptr;
        __gSPDisplayList(&f->second->gfx[1], orig);
    }
    return it->second;
}

void Port_Text3D_Clear(void) {
    gReplacements.clear();
    gFollowerOf.clear();
    gAny = false;
    for (auto& f : gFollowers) gRetiredFollowers.push_back(std::move(f));
    gFollowers.clear();
    for (auto& b : gLive) gRetired.push_back(std::move(b));
    gLive.clear();
}

} // extern "C"
