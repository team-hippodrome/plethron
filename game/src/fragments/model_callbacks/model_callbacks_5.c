/* Stadium Zero fragment 001 (model callbacks): Base-plus-optional-layer segment callback (Starmie, Magneton, Dugtrio...). */
#include "global.h"
#include "model_callbacks.h"
#include "gfx_buffer.h"
#include "geo_render.h"
#include "model_renderer_callbacks.h"
#include "battle_hud.h"
#include "vector_math.h"

/**
 * Builds a display list that calls segment 0 only while textureMode is 0, then segment 1.
 */
void ModelCallback_BuildBaseAndDefaultModeList(Gfx* gfx, DisplayListAddresses* addresses) {
    if (D_8006049C->textureMode == 0) {
        gSPDisplayList(gfx++, addresses->segments[0]);
    }
    gSPDisplayList(gfx++, addresses->segments[1]);
    gSPEndDisplayList(gfx++);
}

/**
 * Phase 2: builds ModelCallback_BuildBaseAndDefaultModeList. Used by Weedle, Diglett, Dugtrio, Magnemite, Magneton, Voltorb,
 * Starmie and Kabuto model data; hit for Starmie in Free Battle.
 */
void ModelCallback_BaseAndDefaultModeSegments(s32 phase, DisplayListState* state) {
    Gfx* gfx;
    DisplayListAddresses* addresses;

    if (phase == 2) {
        addresses = ModelCallback_HostAddresses(state->addresses, 16); // PORT: big-endian table in model data
        gfx = Gfx_AllocDisplayList(sizeof(Gfx) * 10); // PORT: was 80 (10 N64 commands)
        state->gfx = gfx;
        ModelCallback_BuildBaseAndDefaultModeList(gfx, addresses);
    }
}
