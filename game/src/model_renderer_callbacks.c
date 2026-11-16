/* Stadium Zero resident code. */
/* C ported from the Stadium 1 decomp: src/model_renderer_callbacks.c. */

#include "model_renderer_callbacks.h"
#include "battle_hud.h"
#include "fragments/cartridge_check/cartridge_check.h"
#include "geo_render.h"
#include "gfx_buffer.h"
#include "matrix.h"
#include "model_vertex_draw.h"
#include "vector_math.h"

// PORT: the decomp called these without a prototype (IDO allows it; clang does not).
Gfx* Model_Draw(Gfx* gfx, StadiumModel* model, Vtx* vtx_buf);
void Model_InitSkinned(StadiumModel* model, void* normal_buf, void* mtx_seg, void* segment_src);
void Model_UpdateVertexPositions(StadiumModel* model, MtxF* mtx);
typedef struct StadiumModelSlot {
    /* 0x000 */ StadiumModel model;
    /* 0x204 */ char pad204[0x5CC];
} StadiumModelSlot;
typedef struct unk_D_800B2EA8 {
    u8 unk_00[0xA8];
} unk_D_800B2EA8;
typedef struct {
    MtxF mtx[16];   // 16 (0x10) × 0x40 = 0x400
} MtxBuffer;
extern MtxBuffer D_800AFB38[2];
extern StadiumModelSlot D_800B0338[2][3];

void ModelRenderer_CacheSlotMatrices(u8 arg0);
void ModelRenderer_ApplySlotScale(u8 index);
void ModelRenderer_RebuildSlotMatrix(u8 index);
void ModelRenderer_ClearSlotModels(s32 arg0);

/**
 * Loops over the three model slots of side `side` & 1 without doing anything (retail leftover).
 */
void ModelRenderer_UnusedSlotLoop(u32 side) {
    s32 i;
    StadiumModelSlot* slot;

    if (side < 4) {
        side &= 1;
        for (i = 0; i < 3; i++) {
            slot = &D_800B0338[side][i];
            if (slot) {  // karodo
            }
        }
        if (slot) { // karodo
        }
    }
}

extern s16 D_800AFB30[];

/**
 * Returns 1 when side `side`'s slot model has finished loading (state 3), or for an invalid side.
 * @unverified Name read from the code; the battle thumbnail slide-in waits on it.
 */
s32 ModelRenderer_IsSlotModelReady(u32 side) {
    if (side >= 4) {
        return 1;
    }
    side &= 1;
    if (D_800AFB30[side] == 3) {
        return 1;
    }
    return 0;
}

/** Saves the current matrices of a model slot. */
void ModelRenderer_CacheSlotMatrices(u8 arg0) {
    MtxF_Copy(D_800AFB38[arg0].mtx, GeoRender_GetRelativeMatrix(0));
    MtxF_Copy(D_800AFB38[arg0].mtx[1].mf, GeoRender_GetRelativeMatrix(-1));
}

/** Applies a model slot's scale to its matrix. */
void ModelRenderer_ApplySlotScale(u8 index) {
    MtxF* temp_a1;
    MtxF sp1C;

    temp_a1 = &D_800AFB38[index];
    MtxF_ExtractScale(&sp1C, &D_80060488->view.floatMatrix);
    guMtxCatF(sp1C.mf, temp_a1->mf, temp_a1->mf);
}

/** Rebuilds a model slot's matrix. */
void ModelRenderer_RebuildSlotMatrix(u8 index) {
    MtxF* temp_a0;
    
    s32 pad[2]; // FAKE: stack padding
    f32 spC0;
    f32 spBC;
    f32 spB8;
    
    MtxF sp78;
    MtxF sp38;
    Vec3f sp2C;
    
    temp_a0 = &D_800AFB38[index].mtx;
    
    MtxF_GetScaleVector(temp_a0, &sp2C);
    guScaleF(sp38.mf, sp2C.x, sp2C.y, sp2C.z);
    guMtxXFMF(temp_a0->mf, 0.0f, 0.0f, 0.0f, &spC0, &spBC, &spB8);
    guTranslateF(sp78.mf, spC0, spBC, spB8);
    guMtxCatF(sp38.mf, sp78.mf, temp_a0->mf);
}

/** Clears all the models of a slot. */
void ModelRenderer_ClearSlotModels(s32 arg0) {
    s32 i;
    StadiumModelSlot* slot = &D_800B0338[arg0];

    for (i = 0; i < 3; i++) {
        slot[i].model.isActive = 0;
    }
}

extern s16 D_800AFB28;
extern unk_D_800B2EA8 D_800B3218[];
// PORT: the two skinned flame templates (Charizard's tail and Moltres; the 0x92 one is Moltres) and their tables,
// declared with their types so the lifter converts them to host data (they were bytes, left big-endian).
extern u32 D_800758D4[3];
extern Vtx_t D_800758E0[10];
extern s16 D_80075980[12];
extern u8 D_80075998[0x30]; // Gfx[6]: display list, kept in N64 form for the display-list translation
extern s16 D_800759C8[24];
extern s16 D_800759F8[44];
extern s16 D_80075A50[12];
extern ModelSegment D_80075A68;
extern Vtx_t D_80075A88[10];
extern s16 D_80075B28[12];
extern u8 D_80075B40[0x30]; // Gfx[6]
extern s16 D_80075B70[24];
extern s16 D_80075BA0[44];
extern s16 D_80075BF8[12];
extern ModelSegment D_80075C10;
// Referenced so the host links (and the lifter places) every table.
void* const gPortSkinnedTemplateTables[] = {
    D_800758D4, D_800758E0, D_80075980, D_80075998, D_800759C8, D_800759F8, D_80075A50, &D_80075A68,
    D_80075A88, D_80075B28, D_80075B40, D_80075B70, D_80075BA0, D_80075BF8, &D_80075C10,
};

/** Updates the model of the current slot. */
void ModelRenderer_UpdateSlotModel(void) {
    u8 index;
    s32 mode;

    mode = ModelRenderer_GetActiveMode();
    index = (u8)mode;
    if (mode < 2) {
        ModelRenderer_CacheSlotMatrices(index);
        ModelRenderer_RebuildSlotMatrix(index);
        ModelRenderer_ApplySlotScale(index);

        if (D_8006049C->objectType == 0x92) {
            Model_InitSkinned(&D_800B0338[index][D_800AFB28].model, D_800B3218, D_800AFB38[index].mtx, &D_80075C10);
            return;
        }
        Model_InitSkinned(&D_800B0338[index][D_800AFB28].model, D_800B3218, D_800AFB38[index].mtx, &D_80075A68);
    }
}

extern s16 D_800AF7C0[4];
extern s16 D_800AFB30[];
extern s16 D_800AFB28;

/** Updates the vertices of the current slot's model. */
void ModelRenderer_UpdateSlotVertices(void) {
    u8 index;
    s32 mode;

    mode = ModelRenderer_GetActiveMode();
    index = (u8)mode;
    if ((mode < 2) && (ModelPhase_IsIdle() == 0) && (D_800AFB30[index] >= 2)) {
        ModelRenderer_CacheSlotMatrices(index);
        ModelRenderer_RebuildSlotMatrix(index);
        ModelRenderer_ApplySlotScale(index);
        // Zero model slots are 0x7D0 bytes. PORT: indexed by type, as the other slot users do: StadiumModel is
        // larger on the host, so a 0x7D0 stride missed every slot but the first (the second flame model crashed)
        Model_UpdateVertexPositions(&D_800B0338[index][D_800AFB28].model, &D_800AFB38[index].mtx);
    }
}

extern Gfx D_800744D8[];
extern Gfx D_80074540[13]; // PORT: sized (open, the data after its G_ENDDL was lifted as commands)
extern u16 D_80060484;
/**
 * Sets the combiner and colors for a slot model by the drawn object's type: a flickering red-orange
 * by default, fixed orange for 0x92 and orange plus yellow primitive color for 0x7E.
 * @unverified Name read from the code.
 */
Gfx* ModelRenderer_SetSlotModelColor(Gfx* gfx) {
    switch (D_8006049C->objectType) {
        default:
            gSPDisplayList(gfx++, D_800744D8);
            gDPSetEnvColor(gfx++, 0xB4 - (D_80060484 % 8) * 10, 0x20, 0x00, 0x00); // #B42000 (red pulses 180 down to 110 over 8 frames)
            break;
        case 0x92:
            gSPDisplayList(gfx++, D_80074540);
            gDPSetEnvColor(gfx++, 0xFF, 0x20, 0x00, 0x00); // #FF200000
            break;
        case 0x7E:
            gSPDisplayList(gfx++, D_800744D8);
            gDPSetEnvColor(gfx++, 0xFF, 0x20, 0x00, 0x00); // #FF200000
            gDPSetPrimColor(gfx++, 0xFF, 0xFF, 0xFF, 0xFF, 0x05, 0xFF); // #FFFF05FF
            break;
    }
    return gfx;
}

Gfx* ModelRenderer_SetSlotModelColor(Gfx*);
void func_80032A6C(void);
extern Gfx D_80074410[];
extern Gfx D_800744C0[];
/**
 * Geo callback that draws the current slot model of the active side: its deformable model in modes
 * 0-1, or its segment's display list in modes 2-3, then advances to the next slot.
 * Seen in play (2026-11-14): called during the opening battle.
 * @unverified Name read from the code.
 */
Gfx* ModelRenderer_DrawSlotModel(Gfx* gfx) {
    u8 mode;
    void* vtx;

    mode = ModelRenderer_GetActiveMode();
    if (mode >= 4) {
        return gfx;
    }
    if (mode >= 2) {
        mode &= 1;
        if (D_800AFB30[mode] >= 2) {
            Gfx_AllocDisplayList(0xA0);
            gfx = ModelRenderer_SetSlotModelColor(gfx);
            {
                ModelSegment* segment = &D_800B0338[mode][D_800AFB28].model.segment;
                {    Gfx *_gfx = (Gfx *)(gfx++);    _gfx->words.w0 = 0xDB060038;    _gfx->words.w1 = (uintptr_t) segment->vertexSegment; /* PORT: was (s32) */    }
                {    Gfx *_gfx = (Gfx *)(gfx++);    _gfx->words.w0 = 0xDE000000;    _gfx->words.w1 = (uintptr_t) segment->displayList;    }
                {    Gfx *_gfx = (Gfx *)(gfx++);    _gfx->words.w0 = 0xDE000000;    _gfx->words.w1 = (uintptr_t) D_800744C0;    }
            }
            D_800AFB28++;
        }
        return gfx;
    }
    switch (D_800AFB30[mode]) {
        case 1:
            func_80032A6C();
            break;
        case 2:
        case 3:
            vtx = Gfx_AllocDisplayList(0xA0);
            {    Gfx *_gfx = (Gfx *)(gfx++);    _gfx->words.w0 = 0xDA380002;    _gfx->words.w1 = (uintptr_t) D_80074410;    }
            gfx = ModelRenderer_SetSlotModelColor(gfx);
            gfx = Model_Draw(gfx, &D_800B0338[mode][D_800AFB28].model, vtx);
            {    Gfx *_gfx = (Gfx *)(gfx++);    _gfx->words.w0 = 0xD8380002;    _gfx->words.w1 = 0x40;    }
            {    Gfx *_gfx = (Gfx *)(gfx++);    _gfx->words.w0 = 0xDE000000;    _gfx->words.w1 = (uintptr_t) D_800744C0;    }
            break;
    }
    D_800AFB28++;
    return gfx;
}
