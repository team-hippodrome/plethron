/* Stadium Zero fragment 001 (model callbacks): Entry slot table that model data calls its callbacks through. */
#include "global.h"
#include "model_callbacks.h"
#include "gfx_buffer.h"
#include "geo_render.h"
#include "model_renderer_callbacks.h"
#include "battle_hud.h"
#include "vector_math.h"

/**
 * Handwritten jump-stub table (func_81000020 and func_81000028), not C: each 8-byte slot is
 * `j <callback>; nop`. Model data names its geo callbacks by these fixed slot addresses, so the
 * callbacks themselves can sit anywhere. Slot 0 jumps to ModelCallback_IndexedTexture; the
 * 40 slots of func_81000028 jump to callbacks in the battle fragment (for example
 * BattleAnim_AllocateReflectiveTextureDisplayList) and in this fragment (ModelCallback_TwoSegments).
 * Counted with the handwritten asm, not as a match.
 */
#ifdef NON_MATCHING
/* Handwritten jump stubs; there is no C version. */
#else
#pragma GLOBAL_ASM("asm/jp/nonmatchings/fragments/fragment001_15B1A0/func_81000020.s")
#endif

// PORT: host copies of the callback tables (see model_callbacks.h). The copies are rebuilt on every call: the callbacks
// build their display list right away, and the words they copy into it are values, not pointers into the copy.
#include "anim_raw.h"
#include "util.h"

static const u8* ModelCallback_TableBytes(void* table, int* raw) {
    uintptr_t a = (uintptr_t)table;

    if (a != 0 && a <= 0xFFFFFFFFu) {
        a = Util_ConvertAddrToVirtAddr(a);
    }
    *raw = Anim_IsRaw((const void*)a);
    return (const u8*)a;
}

DisplayListAddresses* ModelCallback_HostAddresses(DisplayListAddresses* table, s32 count) {
    static u32 sWords[4][16];
    static s32 sNext;
    u32* out = sWords[sNext++ & 3];
    int raw;
    const u8* src = ModelCallback_TableBytes(table, &raw);
    s32 i;

    if (src == NULL) {
        return NULL;
    }
    for (i = 0; i < count && i < 16; i++) {
        out[i] = Anim_ReadU32(src + i * 4, raw);
    }
    return (DisplayListAddresses*)out;
}

ModelScrollStrip* ModelCallback_HostScrollStrip(ModelScrollStrip* strip) {
    static ModelScrollStrip sStrip;
    int raw;
    const u8* src = ModelCallback_TableBytes(strip, &raw);
    s32 i;

    if (src == NULL) {
        return NULL;
    }
    sStrip.scrollSpeed = (s16)Anim_ReadU16(src, raw);
    sStrip.unk_02 = (s16)Anim_ReadU16(src + 2, raw);
    for (i = 0; i < 8; i++) { // PORT: 8, as ModelCallback_BuildScrollStripList reads them (the N64 reads past the table)
        sStrip.textures[i] = (s32)Anim_ReadU32(src + 4 + i * 4, raw);
    }
    return &sStrip;
}
