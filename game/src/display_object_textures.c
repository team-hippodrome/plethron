/* Stadium Zero resident code. */
/* C ported from the Stadium 1 decomp: src/display_object_textures.c. */

#include "display_object_textures.h"
#include "dma.h"
#include "geo_layout.h"
#include "jpeg_stream.h"
#include "main_pool.h"
#include "matrix.h"
#include "memory_main.h"
#include "model_renderer.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include "StadionPort.h"

// Declared here instead of in a header: the callers of these disagree on their types.
void MainPool_FinalizeAllocation(struct MemoryBlock* block);
void MainPool_ResetStateToOffset(struct MainPoolState* state, s32 size);
void* MainPool_AllocAligned(struct MainPoolState* state, s32 arg1, s32 arg2);
void* MainPool_InitState(struct MainPoolState* block, s32 size);
#include <PR/os_internal_reg.h>
#include <PR/leo.h>
// Header returned by a fragment's entry function when called with (0, 0): geo layout count at 0x03, layout list at 0x08.
// PORT: the same table is read as ModelNodeView and unk_D_86002F30 (arrays of pointer-sized slots), so the host
// layout keeps one pointer-sized slot per original word from 0x08 on.
typedef struct unk_func_80017428 {
    /* 0x00 */ char unk00[0x3];
    /* 0x03 */ u8 unk_03;
    /* 0x04 */ char unk04[2 * sizeof(void*) - 4];
    /* 0x08 */ GeoTextureBankNode** unk_08;
    /* 0x0C */ void* unk0C[2];
    /* 0x14 */ func_unk_D_800ABE10 unk_14;
} unk_func_80017428;
extern ModelLoaderThread D_800AC7A0;

#include "m2c_macros.h"

void GeoTextureBank_AdjustColors(GeoTextureBankNode* arg0, arg1_func_80010CA8 arg1);
Fragment* Jpeg_AllocAndDecodeFragment(MemoryBlock* arg0, PRESJPEG* arg1, PRESJPEG* arg2);
Fragment* Yay0_AllocAndDecompressFragment(MainPoolState* arg0, PERSSZP* arg1, PERSSZP* arg2);
Fragment* Asset_AllocAndCopyFragment(MainPoolState* arg0, PERSSZP* arg1, s32 arg2, u32 arg3);
Fragment* Fragment_LoadFromRomRange(MainPoolState* arg0, s32 start, s32 end, PERSSZP* arg3, s32 arg4);
Fragment* Fragment_LoadFromLbaRange(MainPoolState* arg0, StorageLbaRange* arg1, PERSSZP* arg2, u32 arg3);
void Fragment_RelocateAndCommit(MainPoolState* arg0, u32 arg1, Fragment* arg2);
Fragment* BinArchive_LoadFileFromRomRange(MainPoolState* arg0, BinArchive* arg1, BinArchiveFile* arg2, PERSSZP* arg3, s32 arg4);
Fragment* BinArchive_LoadFileFromLbaRange(MainPoolState* arg0, BinArchive* arg1, BinArchiveFile* arg2, PERSSZP* arg3, s32 arg4);
Fragment* BinArchive_LoadFileToPool(MainPoolState* arg0, BinArchive* arg1, s32 arg2, PERSSZP* arg3, s32 arg4);
void Fragment_ProcessGeoLayoutList(MemoryBlock* arg0, arg1_func_80019420* arg1);
void PokeIcon_LoadModelArchive(ModelLoadRequest* arg0);
void PokeIcon_LoadMultipleModels(ModelLoadRequest* arg0);
void PokeIcon_StartThread(void);
unk_D_86002F30* Model_LoadByArchiveIndex(s32 arg0);
unk_D_86002F30* Model_LoadForPokemon(BattleMon* arg0);
arg1_func_80019420* BattleAnim_LoadResourceTable(s32 arg0);
Fragment* Fragment_LoadAndRelocateFromRomRange(MainPoolState* arg0, u32 arg1, s32 arg2, s32 arg3, PERSSZP* arg4, s32 arg5);
void Port_FragmentLoaded(u32 slot, u8* romStart); // PORT: port_fragments.c

void PokeIcon_ThreadMain(UNUSED void* arg0);

/**
 * Runs `adjust` over every RGBA16 and RGBA32 texel and every palette entry of a geo texture bank (in place).
 */
void GeoTextureBank_AdjustColors(GeoTextureBankNode* bank, arg1_func_80010CA8 adjust) {
    s32 i;
    s32 j;
    GeoTexture* var_s2 = bank->textures;
    GeoPalette* var_s4 = bank->palettes;

    if ((uintptr_t)var_s2 < 0x100000000ULL || (uintptr_t)var_s4 < 0x100000000ULL) {
        // PORT: from stadion: the tables of a model loaded from ROM are big-endian records (0xC bytes, 32-bit link
        // addresses), and so are the texels they point to.
        const u8* t = (const u8*)Util_ConvertAddrToVirtAddr((uintptr_t)var_s2);
        const u8* c = (const u8*)Util_ConvertAddrToVirtAddr((uintptr_t)var_s4);

        for (i = 0; t != NULL && i < bank->textureCount; i++, t += 0xC) {
            u8* tex = (u8*)Util_ConvertAddrToVirtAddr(PORT_BE32(*(const u32*)(t + 8)));
            s32 n = (s16)((t[6] << 8) | t[7]);
            if (t[0] != 0 || tex == NULL || (uintptr_t)tex < 0x100000000ULL) {
                continue;
            }
            if (t[1] == GEO_TEXTURE_SIZ_2) {
                for (j = 0; j < n; j++, tex += 2) {
                    u16 v = Color_AdjustRGB5551((u16)((tex[0] << 8) | tex[1]), adjust);
                    tex[0] = v >> 8;
                    tex[1] = v;
                }
            } else if (t[1] == GEO_TEXTURE_SIZ_3) {
                for (j = 0; j < n; j++, tex += 4) {
                    u32 v = Color_AdjustRGBA8(PORT_BE32(*(u32*)tex), adjust);
                    tex[0] = v >> 24;
                    tex[1] = v >> 16;
                    tex[2] = v >> 8;
                    tex[3] = v;
                }
            }
        }
        for (i = 0; c != NULL && i < bank->paletteCount; i++, c += 0xC) {
            u8* tex = (u8*)Util_ConvertAddrToVirtAddr(PORT_BE32(*(const u32*)(c + 4)));
            s32 n = (s32)PORT_BE32(*(const u32*)c);
            if (tex == NULL || (uintptr_t)tex < 0x100000000ULL) {
                continue;
            }
            for (j = 0; j < n; j++, tex += 2) {
                u16 v = Color_AdjustRGB5551((u16)((tex[0] << 8) | tex[1]), adjust);
                tex[0] = v >> 8;
                tex[1] = v;
            }
        }
        return;
    }

    for (i = 0; i < bank->textureCount; i++, var_s2++) {
        if (var_s2->fmt == 0) {
            if (var_s2->siz == GEO_TEXTURE_SIZ_2) {
                u16* var_s1 = (u16*)var_s2->texture;
                for (j = 0; j < var_s2->num_texels; j++, var_s1++) *var_s1 = Color_AdjustRGB5551(*var_s1, adjust);
            }
            if (var_s2->siz == GEO_TEXTURE_SIZ_3) {
                u32* var_s1 = var_s2->texture;
                for (j = 0; j < var_s2->num_texels; j++, var_s1++) *var_s1 = Color_AdjustRGBA8(*var_s1, adjust);
            }
        }
    }
    for (i = 0; i < bank->paletteCount; i++, var_s4++) {
        u16* var_s1 = var_s4->data;
        for (j = 0; j < var_s4->count; j++, var_s1++) *var_s1 = Color_AdjustRGB5551(*var_s1, adjust);
    }
}

/** Allocates a fragment and decodes a JPEG image into it. */
Fragment* Jpeg_AllocAndDecodeFragment(MemoryBlock* arg0, PRESJPEG* arg1, PRESJPEG* arg2) {
    Fragment* sp1C;
    s32 sp18;
    s32 temp_v0_2;

    sp18 = arg0->size;
    sp1C = MainPool_AllocAligned(arg0, arg2->decodedSize + 0x100, 0x10);

    if (sp1C != NULL) {
        temp_v0_2 = Jpeg_DecodeImage(sp1C, arg2->decodedSize + 0x100, (u8*)arg2 + arg2->dataOffset);
        if (temp_v0_2 == 0) {
            MainPool_ResetStateToOffset(arg0, sp18);
            sp1C = NULL;
        } else {
            MainPool_ResetStateToOffset(arg0, sp18 + temp_v0_2);
        }
    }
    return sp1C;
}

/** Allocates a fragment and decompresses a Yay0 asset into it. */
Fragment* Yay0_AllocAndDecompressFragment(MainPoolState* arg0, PERSSZP* arg1, PERSSZP* arg2) {
    u8* sp1C = MainPool_AllocAligned(arg0, arg2->decompressed_size2, 0x10);

    if (sp1C != NULL) {
        Yay0_DecompressAndRelocate(arg2, sp1C);
    }
    return sp1C;
}

/** Allocates a fragment and copies an uncompressed asset into it. */
Fragment* Asset_AllocAndCopyFragment(MainPoolState* arg0, PERSSZP* arg1, s32 arg2, u32 arg3) {
    u32* sp1C;

    sp1C = MainPool_AllocAligned(arg0, arg3, 0x10);
    if (sp1C != NULL) {
        Util_Memcpy(sp1C, (u8*)arg1 + arg2, arg3 >> 2);
    }
    return sp1C;
}

/** Loads a fragment from a ROM range into a pool, decompressing it as needed. */
Fragment* Fragment_LoadFromRomRange(MainPoolState* arg0, s32 start, s32 end, PERSSZP* arg3, s32 arg4) {
    UNUSED s32 pad; // FAKE: stack padding
    u32 size = ALIGN16(end - start);
    Fragment* var_v1 = NULL;

    if (arg4 >= size) {
        Dma_WriteChunks(arg3, start, end, 0);

        // PORT: the tags and header words are big-endian
        if (PORT_BE32(((u32*)arg3->magic)[0]) == 'PERS' && (PORT_BE32(((u32*)arg3->magic)[1]) == '-SZP')) {
            Port_FixPersHeader(arg3);
            var_v1 = Yay0_AllocAndDecompressFragment(arg0, arg3, arg3);
        } else if (PORT_BE32(((u32*)arg3->magic)[0]) == 'PRES' && (PORT_BE32(((u32*)arg3->magic)[1]) == 'JPEG')) {
            Port_FixPresJpegHeader((PRESJPEG*)arg3);
            var_v1 = Jpeg_AllocAndDecodeFragment(arg0, arg3, arg3);
        } else {
            var_v1 = Asset_AllocAndCopyFragment(arg0, arg3, 0, size);
        }
    }

    return var_v1;
}

/**
 * Loads a fragment from a block-address range into a pool, decompressing it as
 * needed.
 */
Fragment* Fragment_LoadFromLbaRange(MainPoolState* arg0, StorageLbaRange* arg1, PERSSZP* arg2, u32 arg3) {
    s32 sp3C;
    Fragment* sp38;
    PERSSZP* temp_s1;

    LeoLBAToByte(arg1->startLba, arg1->lbaCount, &sp3C);
    sp3C = ALIGN16(sp3C);

    if (arg3 >= sp3C) {
        temp_s1 = arg1->dataOffset + (u8*)arg2;

        Storage_QueueDiskTransfer(0, arg2, arg1->startLba, arg1->lbaCount, &D_800AC7A0.diskQueue, 0);
        osRecvMesg(&D_800AC7A0.diskQueue, NULL, 1);

        // PORT: the tags and header words are big-endian
        if (PORT_BE32(((u32*)temp_s1->magic)[0]) == 'PERS' && (PORT_BE32(((u32*)temp_s1->magic)[1]) == '-SZP')) {
            Port_FixPersHeader(temp_s1);
            sp38 = Yay0_AllocAndDecompressFragment(arg0, arg2, temp_s1);
        } else if (PORT_BE32(((u32*)temp_s1->magic)[0]) == 'PRES' && (PORT_BE32(((u32*)temp_s1->magic)[1]) == 'JPEG')) {
            Port_FixPresJpegHeader((PRESJPEG*)temp_s1);
            sp38 = Jpeg_AllocAndDecodeFragment(arg0, arg2, temp_s1);
        } else {
            sp38 = Asset_AllocAndCopyFragment(arg0, arg2, arg1->dataOffset, arg1->size);
        }
    }

    return sp38;
}

/** Relocates a fragment's pointers and commits it to its pool. */
void Fragment_RelocateAndCommit(MainPoolState* arg0, u32 arg1, Fragment* arg2) {
    Memmap_RelocateFragment(arg1, arg2);
    Port_RelocateDataBlob(arg2); // PORT: before the relocation table is released by the reset below
    MainPool_ResetStateToOffset(arg0, ((uintptr_t)arg2 + arg2->sizeInRam) - (uintptr_t)arg0->listHeadR);
}

/** Loads an archive file into a pool from a ROM range. */
Fragment* BinArchive_LoadFileFromRomRange(MainPoolState* arg0, BinArchive* arg1, BinArchiveFile* arg2, PERSSZP* arg3, s32 arg4) {
    s32 temp_a1 = arg1->source + arg2->offset;

    return Fragment_LoadFromRomRange(arg0, temp_a1, arg2->size + temp_a1, arg3, arg4);
}

/** Loads an archive file into a pool from a block-address range. */
Fragment* BinArchive_LoadFileFromLbaRange(MainPoolState* arg0, BinArchive* arg1, BinArchiveFile* arg2, PERSSZP* arg3, s32 arg4) {
    StorageLbaRange sp1C;

    Storage_SetByteRange(&sp1C, arg1->source, arg2->offset, arg2->size);
    return Fragment_LoadFromLbaRange(arg0, &sp1C, arg3, arg4);
}

/** Loads an archive file into a pool, choosing the ROM or block-address path. */
Fragment* BinArchive_LoadFileToPool(MainPoolState* arg0, BinArchive* arg1, s32 arg2, PERSSZP* arg3, s32 arg4) {
    s32 pad[4]; // FAKE: stack padding
    Fragment* var_s0 = NULL;
    BinArchiveFile* sp2C = (u8*)arg1 + sizeof(BinArchive) + (arg2 * sizeof(BinArchiveFile));

    if (arg2 < arg1->num_files) {
        if (arg1->flags & 1) {
            var_s0 = (u8*)arg1 + sp2C->offset;
        } else if (!(arg1->flags & 4) && (sp2C->cached != 0)) {
            var_s0 = Port_HandleToPtr(sp2C->cached); // PORT: the 32-bit slot holds a handle (asset_archive.c)
        } else {
            if (arg1->flags & 0x80) {
                var_s0 = BinArchive_LoadFileFromLbaRange(arg0, arg1, sp2C, arg3, arg4);
            } else {
                var_s0 = BinArchive_LoadFileFromRomRange(arg0, arg1, sp2C, arg3, arg4);
            }

            if (var_s0 != NULL) {
                if ((PORT_BE32(((u32*)var_s0->magic)[0]) == 'FRAG') && (PORT_BE32(((u32*)var_s0->magic)[1]) == 'MENT')) {
                    Fragment_RelocateAndCommit(arg0, arg1->fragmentId, var_s0);
                }
                sp2C->cached = Port_PtrToHandle(var_s0);
            }
        }
    }

    return var_s0;
}

/** Builds the graph node tree for a fragment's geo layout. */
unk_func_80017428* Fragment_ProcessGeoLayout(MemoryBlock* arg0, Fragment* arg1, arg1_func_80010CA8 arg2) { // PORT: returns the table (was s32)
    s32 i;
    GeoTextureBankNode* temp_v0_2;
    unk_func_80017428* temp_v0;
    unk_func_80017428* (*func)(s32, s32) = (void*)arg1;

#ifdef PORT_STATIC_FRAGMENTS
    // PORT: from stadion: model blobs are data fragments whose entry is a tiny MIPS function returning a model table
    // (`lui v1,hi; addiu v1,v1,lo; ... jr ra`). It cannot run on the host, so its constant is decoded from the
    // instructions. The table and the layouts it lists are big-endian with 32-bit link addresses; a host copy of the
    // table is built in the pool.
    {
        const u8* blob = (const u8*)arg1;
        u32 tableLink = 0;
        const u8* table;
        const u8* layouts;
        u32 count;
        unk_func_80017428* host;
        GeoTextureBankNode** nodes;

        for (i = 0; i < 12 && tableLink == 0; i++) {
            u32 w = PORT_BE32(*(u32*)(blob + 0x20 + i * 4));
            if ((w >> 26) == 0x0F) { // lui
                u32 next = PORT_BE32(*(u32*)(blob + 0x24 + i * 4));
                if ((next >> 26) == 0x09 && ((next >> 21) & 31) == ((w >> 16) & 31)) { // addiu on the same register
                    tableLink = (w << 16) + (s32)(s16)(next & 0xFFFF);
                }
            }
        }
        if (tableLink == 0) {
            return NULL;
        }
        {
            u32 pseudo = Port_VAddrOfLink((void*)blob, tableLink);
            table = (const u8*)Util_ConvertAddrToVirtAddr(pseudo != 0 ? pseudo : tableLink);
        }
        if (table == NULL) {
            return NULL;
        }
        count = table[3];
        layouts = (const u8*)Util_ConvertAddrToVirtAddr(PORT_BE32(*(u32*)(table + 8)));

        host = MainPool_AllocAligned(arg0, sizeof(unk_func_80017428), 8);
        nodes = MainPool_AllocAligned(arg0, (count + 1) * sizeof(GeoTextureBankNode*), 8);
        if (host == NULL || nodes == NULL || layouts == NULL) {
            return NULL;
        }
        bzero(host, sizeof(*host));
        *(u16*)host->unk00 = (u16)((table[0] << 8) | table[1]); // model id (ModelLoadResult.modelId)
        host->unk00[2] = table[2];
        host->unk_03 = count;
        host->unk_08 = nodes;
        host->unk_14 = (func_unk_D_800ABE10)arg1;
        for (i = 0; i < count; i++) {
            temp_v0_2 = (GeoTextureBankNode*)GeoLayout_Process(arg0, (void*)(uintptr_t)PORT_BE32(*(u32*)(layouts + i * 4)));
            if (temp_v0_2 != NULL && temp_v0_2->node.type == 0xE) {
                temp_v0_2->unk_28 = (func_unk_D_800ABE10)arg1;
                if (arg2.raw != 0) {
                    GeoTextureBank_AdjustColors(temp_v0_2, arg2);
                }
            }
            nodes[i] = temp_v0_2;
        }
        return host;
    }
#endif

    temp_v0 = func(0, 0);
    temp_v0->unk_14 = (func_unk_D_800ABE10)arg1;

    for (i = 0; i < temp_v0->unk_03; i++) {
        temp_v0_2 = GeoLayout_Process(arg0, temp_v0->unk_08[i]);

        if (temp_v0_2->node.type == 0xE) {
            temp_v0_2->unk_28 = arg1;
            if (arg2.raw != 0) {
                GeoTextureBank_AdjustColors(temp_v0_2, arg2);
            }
        }
        temp_v0->unk_08[i] = temp_v0_2;
    }

    return temp_v0;
}

// PORT: from stadion: a resource table in a ROM fragment is big-endian with 8-byte records {s8 type; s16; u32 address}
// and a zero type at the end; returns the host-layout copy (pointer-sized address field), allocated from 'pool'.
arg1_func_80019420* Port_ConvertResourceTable(MainPoolState* pool, const void* data) {
    const u8* raw = (const u8*)data;
    arg1_func_80019420* out;
    s32 n = 0;
    s32 k;

    while (raw[n * 8] != 0) {
        n++;
    }
    out = MainPool_AllocAligned(pool, (n + 1) * sizeof(arg1_func_80019420), 8);
    if (out != NULL) {
        for (k = 0; k <= n; k++) {
            out[k].unk_00 = (s8)raw[k * 8];
            out[k].unk_02 = (s16)((raw[k * 8 + 2] << 8) | raw[k * 8 + 3]);
            out[k].unk_04 = (void*)(uintptr_t)PORT_BE32(*(const u32*)(raw + k * 8 + 4));
        }
    }
    return out;
}

/** Builds graph node trees for every geo layout in a list. */
void Fragment_ProcessGeoLayoutList(MemoryBlock* arg0, arg1_func_80019420* arg1) {
    while (arg1->unk_00) {
        if (arg1->unk_00 == 3) {
            arg1->unk_04 = GeoLayout_Process(arg0, arg1->unk_04);
        }

        arg1++;
    }
}

/** Loads a Pokemon icon model archive on the loader thread. */
void PokeIcon_LoadModelArchive(ModelLoadRequest* arg0) {
    UNUSED s32 pad; // FAKE: stack padding
    MemoryBlock* sp28;
    Fragment* sp24;

    sp28 = MainPool_InitState(arg0->pool, arg0->size);
    sp24 = BinArchive_LoadFileToPool(sp28, D_800AC7A0.archives.modelArchive, arg0->fileIndex, D_800AC7A0.archives.scratchBuffer, 0x28000);

    if (sp24 != NULL) {
        arg0->result = Fragment_ProcessGeoLayout(sp28, sp24, arg0->colorVariant);
    } else {
        arg0->result = 0;
    }

    arg0->size = (s32)(uintptr_t)((MainPoolState*)sp28)->listHeadL + sizeof(MainPoolState); // PORT: bytes used plus the state header (was size + 0x10)
}

/** Loads several Pokemon icon models on the loader thread. */
void PokeIcon_LoadMultipleModels(ModelLoadRequest* arg0) {
    Fragment* temp_v0;
    MainPoolState* temp_s3;
    s32 var_s2;
    arg1_func_80019420** temp_s6; // PORT: list of table pointers (was s32*)
    s16* var_s0;

    temp_s6 = arg0->result;
    var_s2 = 0;
    temp_s3 = MainPool_InitState(arg0->pool, arg0->size);
    var_s0 = arg0->fileList;

    while (*var_s0 != -1) {
        temp_v0 = BinArchive_LoadFileToPool(temp_s3, D_800AC7A0.archives.resourceArchive, *var_s0, D_800AC7A0.archives.scratchBuffer, 0x28000);
        if (temp_v0 == NULL) {
            break;
        }

        {
            arg1_func_80019420* tbl = Port_ConvertResourceTable(temp_s3, temp_v0 + 1); // PORT: host-layout copy
            temp_s6[var_s2] = tbl;
            if (tbl != NULL) {
                Fragment_ProcessGeoLayoutList(temp_s3, tbl);
            }
        }
        var_s0++;
        var_s2++;
    }

    temp_s6[var_s2] = NULL;
    arg0->size = (s32)(uintptr_t)temp_s3->listHeadL + sizeof(MainPoolState); // PORT: bytes used plus the state header (was listHeadL + 1, a MainPoolBlock past)
}

/** Main loop of the thread that loads Pokemon icon models on request. */
void PokeIcon_ThreadMain(UNUSED void* arg0) {
    ModelLoadRequest* sp34;

    __osSetFpcCsr(0x01000F00);

    while (true) {
        osRecvMesg(&D_800AC7A0.requestQueue, &sp34, 1);

        switch (sp34->kind) {
            case 1:
                PokeIcon_LoadModelArchive(sp34);
                break;

            case 2:
                PokeIcon_LoadMultipleModels(sp34);
                break;
        }

        if (sp34->queue != NULL) {
            osSendMesg(sp34->queue, sp34, 1);
        }
    }
}

/** Creates the message queues and starts the icon loader thread. */
void PokeIcon_StartThread(void) {
    osCreateMesgQueue(&D_800AC7A0.requestQueue, D_800AC7A0.requestMsgs, 8);
    osCreateMesgQueue(&D_800AC7A0.diskQueue, &D_800AC7A0.diskMsg, 1);
    osCreateThread(&D_800AC7A0.thread, 7, PokeIcon_ThreadMain, NULL, D_800AC7A0.stack + sizeof(D_800AC7A0.stack), 0xA);
    osStartThread(&D_800AC7A0.thread);
}

// PORT: from stadion: twice the size: a model buffer holds the loaded model file and then the graph nodes built from
// its layouts, and those nodes are larger on the host (64-bit pointers).
#define MODEL_BUFFER_SIZE (0x3C000 * 2)

/** Allocates the framebuffers used by the icon loader. */
ModelLoadContext* PokeIcon_AllocFramebuffers(s32 arg0) { // PORT: typed (was raw offsets into an s8*)
    ModelLoadContext* ctx = MainPool_Alloc(sizeof(ModelLoadContext), 1);

    ctx->pendingLoads = 0;
    ctx->bufferIndex = 0;
    ctx->loadedModel = NULL;
    ctx->modelBuffers[0] = NULL;
    ctx->modelBuffers[1] = NULL;
    ctx->backgroundBuffer = NULL;
    ctx->colorVariant.raw = 0;
    ctx->flags = arg0;
    if (arg0 & 1) {
        ctx->modelBuffers[0] = MainPool_Alloc(MODEL_BUFFER_SIZE, 1);
        if (arg0 & 4) {
            ctx->modelBuffers[1] = MainPool_Alloc(MODEL_BUFFER_SIZE, 1);
        }
    }
    if (arg0 & 2) {
        ctx->backgroundBuffer = MainPool_Alloc(0x18000, 1);
    }
    osCreateMesgQueue(&ctx->modelQueue, &ctx->modelMessage, 2);
    osCreateMesgQueue(&ctx->backgroundQueue, &ctx->backgroundMessage, 2);
    return ctx;
}

/**
 * Seen in play (2026-11-13) (tour of menus and a COM battle): ran in the Pokédex, the party screen,
 * Lab sort, registration, the Lab PC and Free Battle setup.
 * @unverified Name was previously used only by callers of this address; the definition was still func_.
 */
void PokeIcon_OpenModelArchives(void) {
    D_800AC7A0.archives.modelArchive = BinArchive_Open((u8*)0x500000, NULL, 1, 1); // PORT: typed fields (were aliases D_800AD1A4..AC)
    D_800AC7A0.archives.resourceArchive = BinArchive_Open((u8*)0x4A8000, NULL, 1, 1);
    D_800AC7A0.archives.scratchBuffer = MainPool_Alloc(0x28000, 1);
}

/** Sends a model load request to the icon loader thread. */
s32 PokeIcon_RequestFrameLoad(ModelLoadContext* arg0, u16 arg1, arg1_func_80010CA8 arg2) {
    s32 sp1C;
    ModelLoadRequest* sp18;
    s32 var_v1;

    sp1C = 0;
    if ((arg0->flags & 1) && !(arg0->pendingLoads & 1)) {
        sp18 = Util_Malloc(sizeof(ModelLoadRequest));
        if (sp18 != NULL) {
            var_v1 = arg0->bufferIndex;
            if (arg0->flags & 4) {
                var_v1 ^= 1;
            }

            sp18->kind = 1;
            sp18->fileIndex = arg1 - 1;
            sp18->queue = &arg0->modelQueue;
            sp18->result = 0;
            sp18->fileList = 0;
            sp18->pool = arg0->modelBuffers[var_v1];
            sp18->size = MODEL_BUFFER_SIZE; // PORT: see MODEL_BUFFER_SIZE
            sp18->colorVariant = arg2;

            if ((arg0->loadedModel != NULL) && (arg0->loadedModel->modelId == arg1) && (arg0->colorVariant.raw == sp18->colorVariant.raw)) {
                osSendMesg(sp18->queue, sp18, 1);
            } else {
                osSendMesg(&D_800AC7A0.requestQueue, sp18, 1);
            }

            arg0->pendingLoads |= 1;
            sp1C = 1;
        }
    }

    return sp1C;
}

/** Sends a background model load request to the icon loader thread. */
s32 PokeIcon_RequestBackgroundLoad(ModelLoadContext* arg0, s16* arg1, void* arg2) { // PORT: pointers (were s32)
    s32 sp1C = 0;
    ModelLoadRequest* temp_v0;

    if ((arg0->flags & 2) && !(arg0->pendingLoads & 2)) {
        temp_v0 = Util_Malloc(sizeof(ModelLoadRequest));
        if (temp_v0 != NULL) {
            temp_v0->kind = 2;
            temp_v0->fileIndex = 0;
            temp_v0->queue = &arg0->backgroundQueue;
            temp_v0->result = arg2;
            temp_v0->fileList = arg1;
            temp_v0->pool = arg0->backgroundBuffer;
            temp_v0->size = 0x18000;

            osSendMesg(&D_800AC7A0.requestQueue, temp_v0, 1);

            sp1C = 1;
            arg0->pendingLoads |= 2;
        }
    }

    return sp1C;
}

/** Applies a load result received from the icon loader thread. */
s32 PokeIcon_ApplyLoadResult(ModelLoadContext* arg0, ModelLoadRequest* arg1) {
    s32 sp1C = 0;

    switch (arg1->kind) {
        case 1:
            arg0->pendingLoads &= ~1;
            if (arg1->result != 0) {
                if (arg0->flags & 4) {
                    arg0->bufferIndex ^= 1;
                }
                arg0->loadedModel = arg1->result;
                arg0->modelSize = arg1->size;
                arg0->colorVariant = arg1->colorVariant;
            }
            sp1C = 1;
            break;

        case 2:
            arg0->pendingLoads &= ~2;
            arg0->loadedBackground = arg1->result;
            arg0->backgroundSize = arg1->size;
            sp1C = 1;
            break;
    }

    Util_Free(arg1);
    return sp1C;
}

/** Checks whether the icon loader has finished a requested model. */
s32 PokeIcon_PollFrameLoad(ModelLoadContext* arg0) {
    ModelLoadRequest* sp1C;
    s32 sp18 = 0;

    if (osRecvMesg(&arg0->modelQueue, &sp1C, 0) != -1) {
        sp18 = PokeIcon_ApplyLoadResult(arg0, sp1C);
    }

    return sp18;
}

/**
 * Seen in play (2026-11-13) (tour of menus and a COM battle): ran in the battle opening, a switch
 * in battle and a Pound turn.
 * @unverified Name was previously used only by callers of this address; the definition was still func_.
 */
s32 PokeIcon_PollBackgroundLoad(ModelLoadContext* arg0) {
    ModelLoadRequest* sp1C;
    s32 sp18 = 0;

    if (osRecvMesg(&arg0->backgroundQueue, (OSMesg*)&sp1C, 0) != -1) {
        sp18 = PokeIcon_ApplyLoadResult(arg0, sp1C);
    }

    return sp18;
}

/**
 * Blocks until the next model load request of `ctx` completes (queue at +0x24) and applies it.
 * Returns PokeIcon_ApplyLoadResult's result. Callers wait on it before reusing a slot.
 */
s32 PokeIcon_WaitFrameLoad(ModelLoadContext* ctx) {
    ModelLoadRequest* req;

    osRecvMesg(&ctx->modelQueue, (OSMesg*)&req, OS_MESG_BLOCK);
    return PokeIcon_ApplyLoadResult(ctx, req);
}

/** Blocks until the icon loader finishes a background load. */
void PokeIcon_WaitBackgroundLoad(ModelLoadContext* arg0) { // PORT: typed (was s32)
    ModelLoadRequest* sp1C;

    osRecvMesg(&arg0->backgroundQueue, (OSMesg*)&sp1C, 1);
    PokeIcon_ApplyLoadResult(arg0, sp1C);
}

/** Loads a model by its archive index. */
unk_D_86002F30* Model_LoadByArchiveIndex(s32 arg0) {
    Fragment* sp2C;
    arg1_func_80010CA8 sp28;
    unk_D_86002F30* sp24 = NULL;
    MemoryBlock* sp20;

    sp2C = BinArchive_GetFile(D_800AC7A0.archives.modelArchive, arg0 - 1);
    if (sp2C != NULL) {
        sp20 = MainPool_AllocState(MainPool_GetAvailable(), 0);
        sp28.raw = 0;
        sp24 = Fragment_ProcessGeoLayout(sp20, sp2C, sp28);
        MainPool_FinalizeAllocation(sp20);
    }

    return sp24;
}

/** Loads the model for a Pokemon. */
unk_D_86002F30* Model_LoadForPokemon(BattleMon* arg0) {
    Fragment* sp2C;
    arg1_func_80010CA8 sp28;
    unk_D_86002F30* sp24 = NULL;
    MemoryBlock* sp20;

    sp2C = BinArchive_GetFile(D_800AC7A0.archives.modelArchive, arg0->species.id - 1);
    if (sp2C != NULL) {
        sp20 = MainPool_AllocState(MainPool_GetAvailable(), 0);
        Model_ComputeSizeVariant(&sp28, arg0);
        sp24 = Fragment_ProcessGeoLayout(sp20, sp2C, sp28);
        MainPool_FinalizeAllocation(sp20);
    }
    return sp24;
}

/** Loads a battle animation resource table. */
arg1_func_80019420* BattleAnim_LoadResourceTable(s32 arg0) {
    Fragment* sp24;
    arg1_func_80019420* sp20;
    MemoryBlock* sp1C;

    sp20 = NULL;
    sp24 = BinArchive_GetFile(D_800AC7A0.archives.resourceArchive, arg0);
    if (sp24 != NULL) {
        sp1C = MainPool_AllocState(MainPool_GetAvailable(), 0);
        sp20 = Port_ConvertResourceTable(sp1C, sp24->data); // PORT: host-layout copy
        if (sp20 != NULL) {
            Fragment_ProcessGeoLayoutList(sp1C, sp20);
        }
        MainPool_FinalizeAllocation(sp1C);
    }
    return sp20;
}

/** Loads a fragment from a ROM range and relocates it. */
Fragment* Fragment_LoadAndRelocateFromRomRange(MainPoolState* arg0, u32 arg1, s32 arg2, s32 arg3, PERSSZP* arg4, s32 arg5) {
    Fragment* sp24 = Fragment_LoadFromRomRange(arg0, arg2, arg3, arg4, arg5);

    if (sp24 != NULL) {
        Fragment_RelocateAndCommit(arg0, arg1, sp24);
#ifdef PORT_STATIC_FRAGMENTS
        // PORT: as Fragment_Load does, lift the fragment's data afresh (the Pokédex habitat screen's Kanto map model
        // was read from never-filled storage and crashed)
        Port_FragmentLoaded(arg1, (u8*)(uintptr_t)arg2);
#endif
    }
    return sp24;
}
