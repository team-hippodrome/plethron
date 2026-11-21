// Drawing helpers for the enhancements (enh_draw.h).
#include "enh_draw.h"

#include "unicode_font.h"

static int Enh_Q(f32 v) { // pixels to the RDP's 10.2 fixed point, clamped at 0
    int q = (int)(v * 4.0f + 0.5f);
    return q < 0 ? 0 : q;
}

void Enh_Begin(void) {
    gDPPipeSync(gDisplayListHead++);
    gDPSetCycleType(gDisplayListHead++, G_CYC_1CYCLE);
    gDPSetRenderMode(gDisplayListHead++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gDPSetTexturePersp(gDisplayListHead++, G_TP_NONE);
    gDPSetTextureFilter(gDisplayListHead++, G_TF_BILERP);
    gDPSetTextureLUT(gDisplayListHead++, G_TT_NONE);
    gDPSetAlphaCompare(gDisplayListHead++, G_AC_NONE);
    gSPTexture(gDisplayListHead++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON);
}

void Enh_End(void) {
    gDPPipeSync(gDisplayListHead++);
    gDPSetTexturePersp(gDisplayListHead++, G_TP_PERSP);
    gDPSetCombineMode(gDisplayListHead++, G_CC_SHADE, G_CC_SHADE);
    gDPSetRenderMode(gDisplayListHead++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
}

void Enh_Rect(f32 x, f32 y, f32 w, f32 h, u8 r, u8 g, u8 b, u8 a) {
    if (w <= 0.0f || h <= 0.0f) {
        return;
    }
    gDPPipeSync(gDisplayListHead++);
    gDPSetCombineLERP(gDisplayListHead++, 0, 0, 0, PRIMITIVE, 0, 0, 0, PRIMITIVE, 0, 0, 0, PRIMITIVE, 0, 0, 0,
                      PRIMITIVE);
    gDPSetPrimColor(gDisplayListHead++, 0, 0, r, g, b, a);
    gSPTextureRectangle(gDisplayListHead++, Enh_Q(x), Enh_Q(y), Enh_Q(x + w), Enh_Q(y + h), G_TX_RENDERTILE, 0, 0,
                        1 << 10, 1 << 10);
}

static const unsigned char* Enh_Glyph(u32* cp, int px, int* tex_width, int* draw_width, int* advance) {
    const unsigned char* tex = Port_Font_Glyph(*cp, px, tex_width, draw_width, advance);
    if (tex == NULL) {
        *cp = '?';
        tex = Port_Font_Glyph('?', px, tex_width, draw_width, advance);
    }
    return tex;
}

static int Enh_GlyphPx(f32 height) {
    int px = (int)(height * ENH_TEXT_SUPERSAMPLE + 0.5f);
    if (px > 64) px = 64; // unicode_font.cpp's largest size
    if (px < 1) px = 1;
    return px;
}

f32 Enh_TextWidth(f32 height, const char* utf8) {
    int px = Enh_GlyphPx(height);
    f32 k = (f32)px / height; // texels per pixel
    f32 w = 0.0f;
    u32 cp;
    while ((cp = Port_Utf8Next(&utf8)) != 0) {
        int tw, dw, adv;
        if (Enh_Glyph(&cp, px, &tw, &dw, &adv) != NULL) {
            w += adv / k;
        }
    }
    return w;
}

f32 Enh_Text(f32 x, f32 y, f32 height, f32 max_width, const char* utf8, u8 r, u8 g, u8 b, u8 a) {
    int px = Enh_GlyphPx(height);
    f32 k = (f32)px / height;
    f32 total = Enh_TextWidth(height, utf8);
    f32 squeeze = (max_width > 0.0f && total > max_width) ? max_width / total : 1.0f;
    f32 pen = x;
    u32 cp;

    if (utf8 == NULL || *utf8 == '\0') {
        return 0.0f;
    }
    gDPPipeSync(gDisplayListHead++);
    gDPSetCombineMode(gDisplayListHead++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gDPSetPrimColor(gDisplayListHead++, 0, 0, r, g, b, a);
    while ((cp = Port_Utf8Next(&utf8)) != 0) {
        int tw, dw, adv;
        const unsigned char* tex = Enh_Glyph(&cp, px, &tw, &dw, &adv);
        f32 gw;
        if (tex == NULL) {
            continue;
        }
        gw = dw / k * squeeze;
        if (cp != ' ' && dw > 0) {
            // the glyph texture is `px` texels tall: step k texels per pixel (and k / squeeze across)
            gDPLoadTextureBlock(gDisplayListHead++, tex, G_IM_FMT_IA, G_IM_SIZ_8b, tw, px, 0, G_TX_CLAMP, G_TX_CLAMP,
                                0, 0, 0, 0);
            gSPTextureRectangle(gDisplayListHead++, Enh_Q(pen), Enh_Q(y), Enh_Q(pen + gw), Enh_Q(y + height),
                                G_TX_RENDERTILE, 0, 0, (int)(k / squeeze * 1024.0f), (int)(k * 1024.0f));
        }
        pen += adv / k * squeeze;
    }
    return pen - x;
}
