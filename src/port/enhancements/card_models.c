// The summary bars' 3D models (hippodrome-utils docs/specs/PARTY_SUMMARY_BARS.md, "Model"): every card shows its own
// copy of the Pokémon's battle model, animated, drawn by the game's own model renderer the way its model previews are
// (poke_icon_2.c: a preview scene with a camera, the model loaded from the model archive). Compiled with the game code.
//
// The copies live outside the game's memory pools (a host buffer each), so the battle's own allocations do not
// change. Each frame, after the battle's HUD, they are drawn into one off-screen picture of the port's (a framebuffer
// that summary_bars.cpp creates and shows: G_SETFB, libultraship's render-to-texture command), one cell per card.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "global.h"
#include "fragments/battle/battle.h"
#include "asset_archive.h"
#include "display_object_textures.h"
#include "geo_layout.h"
#include "geo_node.h"
#include "geo_render.h"
#include "anim_raw.h"
#include "gfx_rect.h"
#include "main_pool.h"
#include "matrix.h"
#include "model_renderer.h"
#include "variables.h"

#include "enhancements.h"
#include "card_models.h"

extern SpeciesInfo D_80064C5C[];
extern GfxImage* D_8005F670; // the current render target (gfx_rect.c)
extern DisplayObject* D_800AD1C0; // the preview scene's model, captured while its layout is processed
s32 PokeIcon_CapturePreviewGeoNode(s32, GraphNode*);
void* MainPool_InitState(struct MainPoolState* block, s32 size); // main_pool.c

// per model copy: its scene and model data (the copy's geo layout and the model's host tables)
#define CARD_MODEL_POOL (2 * 1024 * 1024)

typedef struct CardModel {
    u8 species; // 0 = none
    u8 loaded;
    BattleMon* own;   // the card's Pokémon
    BattleMon* shown; // whose model it shows (another one's while transformed)
    void* memory;
    MainPoolState* pool;
    GraphNode* root;
    DisplayObject* obj;
    s32 anim;  // the animation playing (-1 none)
    s32 speed; // 16.16 step
    s32 faintAnim; // the battle model's fainting animation once it starts (-1 not yet, -2 idling before it)
    u8 faintHeld;  // that animation has ended: the card keeps its last frame
} CardModel;

static CardModel sCards[2][HU_SIDE_MAX];

// The preview scene of poke_icon_2.c (PokeIcon_InitPreviewScene): a camera, a light and the model node.
static uintptr_t sSceneLayout[] = { // PORT64: geo layout slots hold addresses (one uintptr_t per original word)
    0x0C00FFFF, 0x05000000, 0x0B00001E, 0x00000000, 0x010000E0, 0x00000000,
    0x0000000A, 0x015404E2, 0x05000000, 0x0D000000, 0x05000000, 0x14000000,
    0x002D013B, 0xFFFFFF32, 0x16FFFFFF, 0x0F000001, 0x05000000, 0x1F000000,
    0x00000000, 0x00000000, 0xFF060000, 0x00640064, 0x00640000, 0x08000000,
    0 /* PokeIcon_CapturePreviewGeoNode */, 0x00000000, 0x06000000, 0x06000000, 0x06000000, 0x06000000,
    0x01000000, 0x00000000,
};

static void FreeCard(CardModel* c) {
    BattleMon* own = c->own;
    free(c->memory);
    memset(c, 0, sizeof(*c));
    c->own = own;
    c->anim = -1;
    c->faintAnim = -1;
}

void PortEnh_CardModelsFree(void) {
    s32 s, i;
    for (s = 0; s < 2; s++) {
        for (i = 0; i < HU_SIDE_MAX; i++) {
            FreeCard(&sCards[s][i]);
            sCards[s][i].own = NULL;
        }
    }
}

// Loads card (s, i)'s copy of `mon`'s model (as PokeIcon_SetPreviewMon frames it). Returns whether it worked.
static s32 LoadCard(CardModel* c, BattleMon* mon) {
    s32 species = mon->species.id;
    Fragment* frag;
    arg1_func_80010CA8 variant;
    unk_D_86002F30* model;
    SpeciesInfo* info;

    FreeCard(c);
    if (species < 1 || species > 151 || D_800AC7A0.archives.modelArchive == NULL) {
        return 0;
    }
    frag = BinArchive_GetFile(D_800AC7A0.archives.modelArchive, species - 1);
    if (frag == NULL) {
        return 0;
    }
    c->memory = malloc(CARD_MODEL_POOL);
    if (c->memory == NULL) {
        return 0;
    }
    c->pool = MainPool_InitState((MainPoolState*)c->memory, CARD_MODEL_POOL);
    sSceneLayout[24] = (uintptr_t)PokeIcon_CapturePreviewGeoNode;
    D_800AD1C0 = NULL;
    c->root = GeoLayout_Process((MemoryBlock*)c->pool, sSceneLayout);
    c->obj = D_800AD1C0;
    Model_ComputeSizeVariant(&variant, mon);
    model = (unk_D_86002F30*)Fragment_ProcessGeoLayout((MemoryBlock*)c->pool, frag, variant);
    if (c->root == NULL || c->obj == NULL || model == NULL || model->unk_08 == NULL) {
        FreeCard(c);
        return 0;
    }
    info = &D_80064C5C[species - 1];
    c->obj->position.x = (s16)(info->unk_14 >> 0x10) >> 5;
    c->obj->position.y = (s16)(info->unk_14 >> 5) >> 6;
    c->obj->position.z = (s16)(info->unk_14 << 5) >> 5;
    c->obj->rotation = D_8006045C;
    Model_InitDisplayObject(c->obj, 0, species, model->unk_08->unk_00[0]);
    Vec3f_SetComponentsDuplicate(&c->obj->scale, info->modelScale / 100.0f, info->modelScale / 100.0f,
                                 info->modelScale / 100.0f);
    c->obj->node.renderFlags &= ~0x40;
    c->obj->node.renderFlags |= 0x80;
    GeoCamera_SetPerspective((CameraNode*)c->root->children, 30.0f, 100.0f, 12800.0f);
    if (getenv("PORT_ENH_LOG") != NULL) {
        fprintf(stderr, "[enh] card model: species %d, %u bytes\n", species, (unsigned)(uintptr_t)c->pool->listHeadL);
    }
    c->species = species;
    c->shown = mon;
    c->loaded = 1;
    c->anim = -1;
    return 1;
}

#define VALID(s, i) ((s) >= 0 && (s) <= 1 && (i) >= 0 && (i) < HU_SIDE_MAX)

void PortEnh_CardModelsLoad(s32 side, s32 card, BattleMon* mon) {
    if (!VALID(side, card) || mon == NULL) {
        return;
    }
    sCards[side][card].own = mon;
    if (sCards[side][card].loaded && sCards[side][card].shown == mon) {
        return;
    }
    LoadCard(&sCards[side][card], mon);
}

void PortEnh_CardModelsLoadFrom(s32 side, s32 card, s32 from_side, s32 from_card) {
    if (!VALID(side, card) || !VALID(from_side, from_card) || sCards[from_side][from_card].own == NULL) {
        return;
    }
    LoadCard(&sCards[side][card], sCards[from_side][from_card].own);
}

void PortEnh_CardModelsRestore(s32 side, s32 card) {
    if (VALID(side, card) && sCards[side][card].own != NULL && sCards[side][card].shown != sCards[side][card].own) {
        LoadCard(&sCards[side][card], sCards[side][card].own);
    }
}

s32 PortEnh_CardModelReady(s32 side, s32 card) {
    return VALID(side, card) && sCards[side][card].loaded && Port_Enh_CardModelFramebuffer() > 0;
}

void Model_SetMaterialColor(DisplayObject* display_object, u8 r, u8 g, u8 b, u8 alpha); // model_renderer.c

// The card's look (the spec's "Model"): its tint through the model's material colour (the game's own blend toward a
// colour, as on Onix's red flash) and its animation. A sleeping, frozen or fainting Pokémon that is in battle shows
// what its battle model does (Zero has no separate sleep pose for most species: it idles, frozen ones stand still); its
// card follows that model's animation and frame, and keeps the last one once it has left the field.
static void SetSpeed(CardModel* c, s32 speed) {
    if (c->speed != speed) {
        ModelAnim_SetSpeed(c->obj, speed);
        c->speed = speed;
    }
}

// Shows what the battle model shows: its animation, at its frame.
static void Follow(CardModel* c, const DisplayObject* battle) {
    if (c->anim != battle->transformAnim.animationId) {
        ModelAnim_SetAnimation(c->obj, battle->transformAnim.animationId);
        c->anim = battle->transformAnim.animationId;
        c->speed = 0x10000; // (binding an animation sets its speed)
    }
    SetSpeed(c, 0);
    c->obj->transformAnim.frameFixed = battle->transformAnim.frameFixed;
}

static void SetLook(CardModel* c, const hu_card* card, const DisplayObject* battle) {
    hu_model_look look = hu_card_model_look(card, PORT_ENH_FAINT_FRAMES);
    const s32 follows = battle != NULL && c->shown == c->own && battle->objectType == c->species &&
                        battle->transformAnim.animationId >= 0;
    hu_rgb tint = look.tint;
    f32 tint_amount = look.tint_amount;
    s32 speed = (s32)(look.anim_speed * 0x10000);
    u8 amount;

    if (look.anim != HU_ANIM_FAINT) {
        c->faintAnim = -1;
        c->faintHeld = 0;
    }
    if (look.anim == HU_ANIM_FAINT && !c->faintHeld) {
        // The battle model idles a while (the HP bar, the message), plays its fainting animation, then turns away.
        // The card follows it until that animation ends, then holds; grey from then on.
        s32 id = follows ? battle->transformAnim.animationId : -1;
        if (c->faintAnim == -1 && id == 0) {
            c->faintAnim = -2; // idling after the blow (an animation before that is the blow's own)
        } else if (c->faintAnim == -2 && id > 0) {
            c->faintAnim = id;
        }
        if (id < 0 || (c->faintAnim >= 0 && id != c->faintAnim)) {
            // it ended (or the battle model is gone): hold. Zero's fainting animation is a dizzy sway that ends
            // standing again (the Pokémon then turns away), so the card keeps its middle, the dizziest pose.
            c->faintHeld = 1;
            if (c->faintAnim >= 0 && c->anim == c->faintAnim && c->obj->transformAnim.data != NULL) {
                c->obj->transformAnim.frameFixed = (s32)(ANIM_END(c->obj->transformAnim.data) / 2) << 16;
            }
        } else {
            Follow(c, battle);
        }
    } else if ((look.anim == HU_ANIM_SLEEP || look.anim == HU_ANIM_FROZEN) && follows) {
        Follow(c, battle); // in battle: its battle model (Zero's sleepers mostly idle, frozen ones stand still)
    } else if (look.anim == HU_ANIM_FROZEN || look.anim == HU_ANIM_HOLD || look.anim == HU_ANIM_FAINT) {
        SetSpeed(c, 0); // frozen on the bench, not picked, or fainted: the pose it has, held
    } else {
        // idle (status: half speed; a sleeper on the bench idles, as Zero's sleeping battle models do)
        if (look.anim == HU_ANIM_SLEEP) speed = 0x10000;
        if (c->anim != 0) {
            ModelAnim_SetAnimation(c->obj, 0);
            ModelAnim_SetFrame(c->obj, 0);
            c->anim = 0;
        }
        SetSpeed(c, speed);
    }
    if (look.anim == HU_ANIM_FAINT) {
        const hu_rgb grey = { 0x80, 0x80, 0x80 };
        tint = grey;
        tint_amount = c->faintHeld ? 0.7f : 0.0f;
    }
    amount = (tint_amount <= 0.0f) ? 0 : (tint_amount >= 1.0f) ? 0xFF : (u8)(tint_amount * 0xFF);
    Model_SetMaterialColor(c->obj, tint.r, tint.g, tint.b, amount);
}

void PortEnh_CardModelsDraw(const hu_side sides[2], const s32 active[2], DisplayObject* const battle[2]) {
    s32 fb = Port_Enh_CardModelFramebuffer();
    s32 s, i;

    if (fb <= 0) {
        return;
    }
    // the port's off-screen picture: one cell per card (row = side), cleared to the cards' colour
    gDPPipeSync(gDisplayListHead++);
    gDisplayListHead->words.w0 = (u32)0x21 << 24; // G_SETFB (libultraship): draw into framebuffer `fb`
    gDisplayListHead->words.w1 = (uintptr_t)fb;
    gDisplayListHead++;
    gDPSetCycleType(gDisplayListHead++, G_CYC_FILL);
    gDPSetRenderMode(gDisplayListHead++, G_RM_NOOP, G_RM_NOOP2);
    gDPSetScissor(gDisplayListHead++, G_SC_NON_INTERLACE, 0, 0, PORT_ENH_CARD_ATLAS_W, PORT_ENH_CARD_ATLAS_H);
    // the colour of the card's model square (summary_bars.cpp), as RGBA5551 twice
    gDPSetFillColor(gDisplayListHead++, (GPACK_RGBA5551(0x30, 0x3C, 0x60, 1) << 16) | GPACK_RGBA5551(0x30, 0x3C, 0x60, 1));
    gDPFillRectangle(gDisplayListHead++, 0, 0, PORT_ENH_CARD_ATLAS_W - 1, PORT_ENH_CARD_ATLAS_H - 1);
    gDPPipeSync(gDisplayListHead++);
    gDPSetCycleType(gDisplayListHead++, G_CYC_1CYCLE);
    for (s = 0; s < 2; s++) {
        for (i = 0; i < sides[s].count && i < HU_SIDE_MAX; i++) {
            CardModel* c = &sCards[s][i];
            if (!c->loaded) continue;
            SetLook(c, &sides[s].cards[i], (i == active[s]) ? battle[s] : NULL);
            GeoCamera_SetViewport((CameraNode*)c->root->children, i * PORT_ENH_CARD_CELL, s * PORT_ENH_CARD_CELL,
                                  PORT_ENH_CARD_CELL, PORT_ENH_CARD_CELL);
            GeoCamera_SetPerspective((CameraNode*)c->root->children, 30.0f, 100.0f, 12800.0f);
            Geo_RenderRootNode(c->root);
        }
    }
    gDPPipeSync(gDisplayListHead++);
    gDisplayListHead->words.w0 = (u32)0x22 << 24; // G_RESETFB: back to the game's picture
    gDisplayListHead->words.w1 = 0;
    gDisplayListHead++;
    // the game's own scissor again: the portraits are drawn after this (the atlas's height clipped the COM's away)
    if (D_8005F670 != NULL) {
        Gfx_ApplyScissorRect(&gDisplayListHead);
    }
}
