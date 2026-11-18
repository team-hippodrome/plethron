/* Stadium Zero fragment031_3553C0. */

#include "global.h"
#include "particle_data.h"
#include "gfx_buffer.h"
#include "m2c_macros.h"
#include "stage_loader.h" // PORT: StageContext_GetCurrentImage
#include "StadionPort.h" // PORT: Port_WideMarginN64

extern Gfx sFlameBackdropPreTextureDL[];
extern Gfx sFlameBackdropPostTextureDL[];
extern void* sFlameBackdropTextures[8]; // PORT: sized (open, it took in the fire overlay table and both texture strips)
extern u16 D_80060484;
extern GfxImage* D_8005F670; // the current render target (GfxImage_SetRenderTarget)

Gfx* FireFx_BuildFlameBackdropDisplayList(Gfx* gfx, s32 frame);

// PORT: the backdrop's two quads span x -800..800 at z 0, seen from the VS scene's camera (z 1507, 39 degrees): 1.12
// times the 4:3 picture's width, but only 0.84 of a 16:9 one, which left black bars at the sides of the fire behind the
// round title, the VS panels, the results, かち/まけ and the badge cap. In widescreen the quads are stretched by 1.2
// (x -960..960; the 16:9 view is -949..949 there), only on the screen: the same scene drawn into the 4:3 image on the
// stadium's big screen keeps the original. Nothing changes at 4:3.
#define PORT_FLAME_BACKDROP_WIDE_SCALE 1.2f

/**
 * Writes the VS scene's fire backdrop into `gfx`: two quads around a load of frame `frame`
 * (0-7) of the 32x32 RGBA16 flame strip in `sFlameBackdropTextures`. Returns the end of the
 * written list.
 * Seen in play (2026-11-14): recoloring `sFlameBackdropTextures` turned the full-screen fire
 * behind the Free Battle VS panels green, and in the tournament VS scene it turned the fire
 * on the stadium's big screen and behind the round title green.
 */
Gfx* FireFx_BuildFlameBackdropDisplayList(Gfx* gfx, s32 frame) {
    // PORT: widescreen (see PORT_FLAME_BACKDROP_WIDE_SCALE)
    s32 wide = Port_WideMarginN64() != 0 && D_8005F670 == StageContext_GetCurrentImage();

    if (wide) {
        Mtx* scale = Gfx_AllocDisplayList(sizeof(Mtx));

        guScale(scale, PORT_FLAME_BACKDROP_WIDE_SCALE, 1.0f, 1.0f);
        gSPMatrix(gfx++, scale, G_MTX_MODELVIEW | G_MTX_MUL | G_MTX_PUSH);
    }
    gSPDisplayList(gfx++, sFlameBackdropPreTextureDL);
    gDPLoadTextureBlock(gfx++, sFlameBackdropTextures[frame], G_IM_FMT_RGBA, G_IM_SIZ_16b, 32, 32, 0, G_TX_CLAMP, G_TX_CLAMP,
                        G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gSPDisplayList(gfx++, sFlameBackdropPostTextureDL);
    if (wide) {
        gSPPopMatrix(gfx++, G_MTX_MODELVIEW);
    }
    gSPEndDisplayList(gfx++);
    return gfx;
}

/**
 * Geo callback of the VS scene's fire backdrop node (fragment013, 0x82202DA8): at render
 * time (`context` 2) it rebuilds the node's display list with the flame frame picked by the
 * global frame counter, so the fire cycles through its eight frames.
 * Seen in play (2026-11-14): breakpoint hit every frame of the Free Battle and tournament
 * VS scenes.
 */
void FireFx_FlameBackdropGeoCallback(s32 context, GeoDisplayListNode* node) {
    if (context == 2) {
        node->displayList = Gfx_AllocDisplayList(sizeof(Gfx) * 12); // PORT: was 80 (10 N64 commands), +2 widescreen
        FireFx_BuildFlameBackdropDisplayList(node->displayList, D_80060484 & 7);
    }
}
