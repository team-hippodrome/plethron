/* Stadium Zero resident code. */
/* C ported from the Stadium 1 decomp: src/input.c. */

#include "global.h"
#include "input.h"
#include "controller.h"
#include "StadionPort.h" // PORT: widescreen menu patterns (Gfx_PortDrawPatternBeside43)
extern s32 pad_D_800AC880[0x2];
extern s32 D_800AB02C;
extern Controller* D_800AB030;

void Input_ResetRepeatState(void);

/**
 * Empty input-repeat initialization hook.
 * Seen in play (2026-11-13) (tour of menus and a COM battle): ran in the main menu, the Pokédex,
 * the party screen, the Lab list, Lab sort, registration, the Lab PC, Free Battle setup, the battle
 * opening, a switch in battle, running from battle and a Pound turn.
 * @unverified Name matches Stadium 1. Comment from Stadium 1.
 */
void Input_ResetRepeatState(void) {
}

/**
 * Emits a texture rectangle for the already loaded tile at (x1, y1), width x height pixels. In copy
 * mode the right/bottom edges are inclusive and S steps 4 texels per pixel, as copy mode requires.
 * Shared tail of the Gfx_DrawTexture* helpers.
 */
void Gfx_DrawLoadedTextureRect(s32 x1, s32 y1, s32 width, s32 height, s32 mode) {
    s32 xh = (x1 + width) << 2;
    s32 yh = (y1 + height) << 2;
    s32 dsdx = 0x400;

    if (mode == G_CYC_COPY) {
        xh -= 4;
        yh -= 4;
        dsdx = 0x1000;
    }
    gSPTextureRectangle(gDisplayListHead++, x1 << 2, y1 << 2, xh, yh, G_TX_RENDERTILE, 0, 0, dsdx, 0x400);
}

void Gfx_DrawLoadedTextureRect(s32, s32, s32, s32, s32);

/** Draws an RGBA32 texture as a rectangle. */
void Gfx_DrawTextureRgba32(s32 x1, s32 y1, s32 draw_width, s32 height, u8* texture, s32 load_width, s32 texture_spread_x_mode) {
    gDPLoadTextureBlock(gDisplayListHead++, texture, G_IM_FMT_RGBA, G_IM_SIZ_32b, load_width, height, 0,
                        G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
                        G_TX_NOLOD);

    Gfx_DrawLoadedTextureRect(x1, y1, draw_width, height, texture_spread_x_mode);
}

/** Draws an RGBA16 texture as a rectangle. */
void Gfx_DrawTextureRgba16(s32 x1, s32 y1, s32 draw_width, s32 height, u8* texture, s32 load_width, s32 texture_spread_x_mode) {
    gDPLoadTextureBlock(gDisplayListHead++, texture, G_IM_FMT_RGBA, G_IM_SIZ_16b, load_width, height, 0,
                        G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
                        G_TX_NOLOD);

    Gfx_DrawLoadedTextureRect(x1, y1, draw_width, height, texture_spread_x_mode);
}

void Gfx_DrawTexturedRect(s32, s32, s32, s32, s32, s32, s32);
/**
 * Loads an RGBA16 texture and draws it with Gfx_DrawTexturedRect (inclusive corners). Same as
 * Gfx_DrawTextureRgba16 but through the clipped rectangle path. @unverified: no callers in resident code.
 */
void Gfx_DrawTextureRgba16Rect(s32 x1, s32 y1, s32 draw_width, s32 height, u8* texture, s32 load_width, s32 mode) {
    gDPLoadTextureBlock(gDisplayListHead++, texture, G_IM_FMT_RGBA, G_IM_SIZ_16b, load_width, height, 0,
                        G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
                        G_TX_NOLOD);
    if (mode == G_CYC_COPY) {
        Gfx_DrawTexturedRect(x1, y1, x1 + draw_width - 1, y1 + height - 1, 0x1000, 0x400, mode);
    } else {
        Gfx_DrawTexturedRect(x1, y1, x1 + draw_width - 1, y1 + height - 1, 0x400, 0x400, mode);
    }
}

/** Draws an IA8 texture as a rectangle. */
void Gfx_DrawTextureIa8(s32 x1, s32 y1, s32 draw_width, s32 height, u8* texture, s32 load_width, s32 texture_spread_x_mode) {
    gDPLoadTextureBlock(gDisplayListHead++, texture, G_IM_FMT_IA, G_IM_SIZ_8b, load_width, height, 0, G_TX_NOMIRROR | G_TX_CLAMP,
                        G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    Gfx_DrawLoadedTextureRect(x1, y1, draw_width, height, texture_spread_x_mode);
}

/** Draws an IA16 texture as a rectangle. */
void Gfx_DrawTextureIa16(s32 x1, s32 y1, s32 draw_width, s32 height, u8* texture, s32 load_width, s32 texture_spread_x_mode) {
    gDPLoadTextureBlock(gDisplayListHead++, texture, G_IM_FMT_IA, G_IM_SIZ_16b, load_width, height, 0, G_TX_NOMIRROR | G_TX_CLAMP,
                        G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    Gfx_DrawLoadedTextureRect(x1, y1, draw_width, height, texture_spread_x_mode);
}

/** Draws an I4 texture as a rectangle. */
void Gfx_DrawTextureI4(s32 x1, s32 y1, s32 draw_width, s32 height, u8* texture, s32 load_width, s32 texture_spread_x_mode) {
    gDPLoadTextureBlock_4b(gDisplayListHead++, texture, G_IM_FMT_I, load_width, height, 0, G_TX_NOMIRROR | G_TX_CLAMP,
                           G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);

    Gfx_DrawLoadedTextureRect(x1, y1, draw_width, height, texture_spread_x_mode);
}

/** Draws an I8 texture as a rectangle. */
void Gfx_DrawTextureI8(s32 x1, s32 y1, s32 draw_width, s32 height, u8* texture, s32 load_width, s32 texture_spread_x_mode) {
    gDPLoadTextureBlock(gDisplayListHead++, texture, G_IM_FMT_I, G_IM_SIZ_8b, load_width, height, 0, G_TX_NOMIRROR | G_TX_CLAMP,
                        G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);

    Gfx_DrawLoadedTextureRect(x1, y1, draw_width, height, texture_spread_x_mode);
}

/** Draws an I16 texture as a rectangle. */
void Gfx_DrawTextureI16(s32 x1, s32 y1, s32 draw_width, s32 height, u8* texture, s32 load_width, s32 texture_spread_x_mode) {
    gDPLoadTextureBlock(gDisplayListHead++, texture, G_IM_FMT_I, G_IM_SIZ_16b, load_width, height, 0, G_TX_NOMIRROR | G_TX_CLAMP,
                        G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);

    Gfx_DrawLoadedTextureRect(x1, y1, draw_width, height, texture_spread_x_mode);
}


/** Draws an RGBA16 texture combined with an IA8 texture as a rectangle. */
void Gfx_DrawTextureRgba16Ia8(s32 x1, s32 y1, s32 draw_width, s32 height, u8* texture, u8* texture_multi, s32 load_width, s32 texture_x_spread_mode) {
    gDPLoadTextureBlock(gDisplayListHead++, texture, G_IM_FMT_RGBA, G_IM_SIZ_16b, load_width, height, 0,
                        G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
                        G_TX_NOLOD);
    gDPLoadMultiBlock(gDisplayListHead++, texture_multi, 0x0100, 1, G_IM_FMT_I, G_IM_SIZ_8b, load_width, height, 0,
                      G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
                      G_TX_NOLOD);

    Gfx_DrawLoadedTextureRect(x1, y1, draw_width, height, texture_x_spread_mode);
}

extern u8 D_02000000[];

// PORT: only the parts of the pattern beside the 4:3 frame (Gfx_PortDrawPatternRowsBeside43)
static s32 sPortPatternBesideOnly = 0;

/**
 * PORT: widescreen. The menus' 128-pixel I8 patterns (Gfx_DrawPatternRows here, Ui_DrawScreenBackgroundRows in
 * gb_save.c) are drawn over the 640-wide N64 screen only; in a 16:9 picture this continues the band at rows y..y+h
 * into both sides, to the picture's edges, with the tile already loaded (128 texels a period, texel 0 at x 0, clamped
 * or wrapping). Only plain texture rectangles: the left side is drawn at x 0 and moved left by the margin
 * (gDPPortAnchorLeft), the right side past x 640 (gDPPortNoCut); both are drawn past the 2D screen's 4:3 lock while
 * the menu on top stays in it. Nothing at 4:3. PORT_NO_WIDE_MENU_BG=1: off.
 */
void Gfx_PortDrawPatternBeside43(s32 y, s32 h) {
    s32 margin; // quarter pixels of the 640-wide screen
    s32 p;
    s32 next;

    if (PORT_GETENV_ONCE("PORT_NO_WIDE_MENU_BG") != NULL) {
        return;
    }
    margin = (s32)(Port_WideMarginN64Exact() * 8.0f + 0.5f);
    if (margin == 0) {
        return;
    }
    // left: picture x p (quarter pixels, -margin..0) is drawn at x p + margin, split where the tile repeats
    gDPPortAnchorLeft(gDisplayListHead++);
    for (p = -margin; p < 0; p = next) {
        next = (p - (((p % 512) + 512) % 512)) + 512; // the next multiple of 128 pixels
        if (next > 0) {
            next = 0;
        }
        gSPTextureRectangle(gDisplayListHead++, p + margin, y << 2, next + margin, (y + h) << 2, G_TX_RENDERTILE,
                            (((p % 512) + 512) % 512) << 3, 0, 1 << 10, 1 << 10);
    }
    // right: x 640..640 + margin, where the N64 screen ended
    gDPPortNoCut(gDisplayListHead++);
    for (p = 640 << 2; p < (640 << 2) + margin; p = next) {
        next = p - (p % 512) + 512;
        if (next > (640 << 2) + margin) {
            next = (640 << 2) + margin;
        }
        gSPTextureRectangle(gDisplayListHead++, p, y << 2, next, (y + h) << 2, G_TX_RENDERTILE, (p % 512) << 3, 0,
                            1 << 10, 1 << 10);
    }
    gDPPortAnchorEnd(gDisplayListHead++);
}

/**
 * Draws one 16-pixel-high band of the 128x128 I8 pattern texture at segment 0x02 across the whole
 * 640-wide screen at row y, picking the texture rows by y mod 128.
 */
void Gfx_DrawPatternBand(s32 y) {
    s32 x;

    gDPLoadTextureBlock(gDisplayListHead++, D_02000000 + (y % 128) * 128, G_IM_FMT_I, G_IM_SIZ_8b, 128, 16, 0,
                        G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
                        G_TX_NOLOD);
    for (x = 0; x < 640 && !sPortPatternBesideOnly; x += 128) {
        gSPTextureRectangle(gDisplayListHead++, x << 2, y << 2, (x + 128) << 2, (y + 16) << 2, G_TX_RENDERTILE, 0, 0,
                            0x400, 0x400);
    }
    Gfx_PortDrawPatternBeside43(y, 16); // PORT: widescreen
}

/**
 * Fills screen rows start..end (rounded to even) with the segment-0x02 I8 pattern tinted by
 * (r, g, b) through the environment color. The Pokédex menu calls it for its full-screen background
 * (0..480, color 50,100,162). @unverified: background not stubbed in play.
 */
void Gfx_DrawPatternRows(s32 start, s32 end, s32 r, s32 g, s32 b) {
    s32 y;

    gDPPipeSync(gDisplayListHead++);
    gDPSetCycleType(gDisplayListHead++, G_CYC_1CYCLE);
    gDPSetRenderMode(gDisplayListHead++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
    gDPSetCombineLERP(gDisplayListHead++, TEXEL0, 0, ENVIRONMENT, 0, TEXEL0, 0, ENVIRONMENT, 0, TEXEL0, 0, ENVIRONMENT, 0,
                      TEXEL0, 0, ENVIRONMENT, 0);
    gDPSetScissor(gDisplayListHead++, G_SC_NON_INTERLACE, 0, 0, 640, 480);
    gDPSetTexturePersp(gDisplayListHead++, G_TP_NONE);
    gDPSetEnvColor(gDisplayListHead++, r, g, b, 255);
    start &= ~1;
    end = (end + 1) & ~1;
    for (y = start; y < end; y += 16) {
        Gfx_DrawPatternBand(y);
    }
    gDPPipeSync(gDisplayListHead++);
    gDPSetTexturePersp(gDisplayListHead++, G_TP_PERSP);
}

/**
 * PORT: widescreen. Only the parts of Gfx_DrawPatternRows' pattern beside the 4:3 frame, for a screen that draws the
 * pattern on its first frames and then keeps it in the frame buffer (the order screen): the port clears the margins
 * beside the 4:3 frame at the start of every frame (libultraship interpreter.cpp GfxDpSetColorImage), so they are drawn
 * again each frame. Nothing at 4:3.
 */
void Gfx_PortDrawPatternRowsBeside43(s32 start, s32 end, s32 r, s32 g, s32 b) {
    if (Port_WideMarginN64() == 0) {
        return;
    }
    sPortPatternBesideOnly = 1;
    Gfx_DrawPatternRows(start, end, r, g, b);
    sPortPatternBesideOnly = 0;
}

/**
 * Draws the pattern texture over a rectangle at most 16 rows high, splitting it into 128-pixel
 * texture rectangles aligned to the pattern.
 */
void Gfx_DrawPatternSpan(s32 x, s32 y, s32 width, s32 height) {
    s32 n;

    gDPLoadTextureBlock(gDisplayListHead++, D_02000000 + (y % 128) * 128, G_IM_FMT_I, G_IM_SIZ_8b, 128, 16, 0,
                        G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
                        G_TX_NOLOD);
    if (x % 128 != 0) {
        UNUSED s32 pad; // FAKE: stack padding
        n = 128 - x % 128;
        gSPTextureRectangle(gDisplayListHead++, x << 2, y << 2, (x + n) << 2, (y + height) << 2, G_TX_RENDERTILE,
                            (x % 128) << 5, 0, 0x400, 0x400);
        width -= n;
        x += n;
    }
    while (width >= 128) {
        gSPTextureRectangle(gDisplayListHead++, x << 2, y << 2, (x + 128) << 2, (y + height) << 2, G_TX_RENDERTILE, 0,
                            0, 0x400, 0x400);
        width -= 128;
        x += 128;
    }
    if (width != 0) {
        gSPTextureRectangle(gDisplayListHead++, x << 2, y << 2, (x + width) << 2, (y + height) << 2, G_TX_RENDERTILE,
                            0, 0, 0x400, 0x400);
    }
}

/**
 * Fills a rectangle with the segment-0x02 I8 pattern tinted by (r, g, b), in 16-row strips aligned
 * to the pattern. @unverified: no callers found in resident code.
 */
void Gfx_DrawPatternRect(s32 x, s32 y, s32 width, s32 height, s32 r, s32 g, s32 b) {
    UNUSED s32 pad[7]; // FAKE: stack padding
    s32 s;
    s32 n;

    gDPPipeSync(gDisplayListHead++);
    gDPSetCycleType(gDisplayListHead++, G_CYC_1CYCLE);
    gDPSetRenderMode(gDisplayListHead++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
    gDPSetCombineLERP(gDisplayListHead++, TEXEL0, 0, ENVIRONMENT, 0, TEXEL0, 0, ENVIRONMENT, 0, TEXEL0, 0, ENVIRONMENT, 0,
                      TEXEL0, 0, ENVIRONMENT, 0);
    gDPSetScissor(gDisplayListHead++, G_SC_NON_INTERLACE, 0, 0, 640, 480);
    gDPSetTexturePersp(gDisplayListHead++, G_TP_NONE);
    gDPSetEnvColor(gDisplayListHead++, r, g, b, 255);
    s = y % 16;
    if (s != 0) {
        n = 16 - s;
        if (height < n) {
            n = height;
        }
        Gfx_DrawPatternSpan(x, y, width, n);
        y += n;
        height -= n;
    }
    while (height >= 16) {
        Gfx_DrawPatternSpan(x, y, width, 16);
        height -= 16;
        y += 16;
    }
    if (height != 0) {
        Gfx_DrawPatternSpan(x, y, width, height);
    }
    gDPPipeSync(gDisplayListHead++);
    gDPSetTexturePersp(gDisplayListHead++, G_TP_PERSP);
}
