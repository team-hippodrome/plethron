/* Stadium Zero resident code. */
/* C ported from the Stadium 1 decomp: src/jpeg_stream.c. */

#include "asset_archive.h"
#include "dma.h"
#include "jpeg_stream.h"
#include "memory_main.h"
#include "stage_loader.h"
#include "util.h"
#include "lib/ultralib/include/PR/leo.h"

// PORT: from stadion (defined below)
void Port_FixPersHeader(void* h);
void Port_FixPresJpegHeader(void* h);
extern int gPortRelocateDataBlob;
typedef enum {
    JPEG_MARKER_START_OF_FRAME = 0xC0,
    JPEG_MARKER_HUFFMAN_TABLE = 0xC4,
    JPEG_MARKER_RESTART0 = 0xD0,
    JPEG_MARKER_RESTART7 = 0xD7,
    JPEG_MARKER_START_OF_IMAGE = 0xD8,
    JPEG_MARKER_END_OF_IMAGE = 0xD9,
    JPEG_MARKER_START_OF_SCAN = 0xDA,
    JPEG_MARKER_QUANTIZATION_TABLE = 0xDB,
} JpegStreamMarker;
extern u8 D_800A94F0[0x200];
extern OSMesgQueue D_800A96F0;
extern OSMesg D_800A9708;
extern u64 njpgdspMainTextStart[];
extern u64 njpgdspMainDataStart[];

void Dma_InitializeCompletionQueue(void);
void Yay0_DecompressAndRelocate(u8* in_header, u8* memory);
void Sram_TransferChunked(u8* arg0, s32 arg1, s32 arg2, s32 arg3);
void* Jpeg_LoadDecodedImage(u8* addr, PRESJPEG* arg1, s32 side);
void* Yay0_LoadDecompressedAsset(void* arg0, PERSSZP* arg1, s32 side);
u32* Asset_CopyUncompressed(u8* arg0, s32 arg1, u32 arg2, s32 arg3);
void* Asset_LoadCompressed(u8* romStart, u8* romEnd, s32 arg2, s32 arg3);
void* Storage_LoadAssetByLbaRange(StorageLbaRange* arg0, s32 arg1);
StorageLbaRange* Storage_SetLbaRange(StorageLbaRange* arg0, s32 arg1, s32 arg2);
StorageLbaRange* Storage_SetByteRange(StorageLbaRange* arg0, u32 arg1, u32 offset, u32 size);
void Storage_PrefetchLbaRange(s32 arg0, s32 arg1, s32 arg2);
void Memmap_ClearSegmentOnBlockFree(uintptr_t block_addr, u32 addr);
u8* Asset_LoadToSegment(s32 id, u8* rom_start, u8* rom_end, s32 arg3);
MainPoolBlock* Asset_LoadToSegmentByLbaRange(s32 arg0, s32 arg1, s32 arg2, s32 arg3);
void Fragment_FreeCallback(uintptr_t base_addr, u32 addr);
void Fragment_Register(s32 arg0, Fragment* addr);
ret_func_80004454 Fragment_Load(s32 arg0, u8* romStart, u8* romEnd);
// PORT: statically linked fragments (port_fragments.c)
void Port_FragmentRemember(u8* romStart, void* block);
void Port_FragmentLoaded(u32 slot, u8* romStart);
void* Port_FragmentEntry(u8* romStart);
void* Fragment_LoadByLbaRange(s32 arg0, s32 arg1, s32 arg2);
BinArchive* BinArchive_Open(u8* romStart, u8* romEnd, s32 arg2, s32 arg3);
void* BinArchive_LoadByLbaRange(s32 arg0, s32 arg1, s32 arg2, s32 arg3);
void BinArchive_Free(void* arg0);
Fragment* BinArchive_LoadFileFromRom(BinArchive* arg0, BinArchiveFile* arg1);
void* BinArchive_LoadFileBuffered(BinArchive* arg0, BinArchiveFile* arg1);
void* BinArchive_GetFile(BinArchive* archive, s32 file_number);
void* BinArchive_GetCachedFile(BinArchive* archive, s32 file_number);

s32 Jpeg_DecodeImage(uintptr_t addr, s32 arg1, u8* arg2);
void Dma_WriteChunks(uintptr_t write_start, uintptr_t buffer_start, uintptr_t buffer_end, s32 arg3);

/** Creates the message queue used to wait for DMA completion. */
void Dma_InitializeCompletionQueue(void) {
    osCreateMesgQueue(&D_800A96F0, &D_800A9708, 1);
}

/** Decompresses a Yay0 asset into memory and relocates it. */
void Yay0_DecompressAndRelocate(u8* in_header, u8* memory) {
    UNUSED s32 pad2; // FAKE: stack padding
    PERSSZP* header = in_header;
    u32* sp2C = (u32*)header + 6;
    UNUSED s32 pad; // FAKE: stack padding
    s32 sp24 = header->decompressed_size2 - header->decompressed_size1;
    u32 var_v0;

    osInvalICache(memory, header->decompressed_size2);
    osInvalDCache(memory, header->decompressed_size2);

    Yay0_Decompress(in_header + header->header_size, memory);

    for (var_v0 = 0; var_v0 < header->relocCount; var_v0++) {
        *(u32*)(memory + sp2C[1]) = sp2C[0];
        sp2C += 2;
    }

    if (sp24 != 0) {
        bzero(memory + header->decompressed_size1, sp24);
    }

    osWritebackDCache(memory, header->decompressed_size2);
}

/** Reads data from the cartridge in chunks, waiting for each DMA to complete. */
void Sram_TransferChunked(u8* arg0, s32 arg1, s32 arg2, s32 arg3) {
    s32 var_s0;
    OSMesgQueue* mesq = &D_800A96F0;

    for (; arg2 > 0; arg2 -= var_s0) {
        if (arg2 >= 0x1000) {
            var_s0 = 0x1000;
        } else {
            var_s0 = arg2;
        }

        Storage_QueueSramTransfer(arg3, arg0, arg1, var_s0, mesq, 0);
        osRecvMesg(mesq, NULL, 1);

        arg0 += var_s0;
        arg1 += var_s0;
    }
}

/**
 * Writes a buffer to the cartridge in chunks, waiting for each DMA to
 * complete.
 */
void Dma_WriteChunks(uintptr_t write_start, uintptr_t buffer_start, uintptr_t buffer_end, s32 arg3) { // PORT: addresses are pointer-sized
    s32 chunk_size;
    s32 buffer;
    OSMesgQueue* mesq = &D_800A96F0;

    for (buffer = ALIGN16(buffer_end - buffer_start); buffer > 0; buffer -= chunk_size) {
        if (buffer >= 0x1000) {
            chunk_size = 0x1000;
        } else {
            chunk_size = buffer;
        }

        Storage_QueueRomRead(arg3, write_start, buffer_start, chunk_size, mesq, 0);
        osRecvMesg(mesq, NULL, 1);

        write_start += chunk_size;
        buffer_start += chunk_size;
    }
}

/** Allocates memory for and decodes a JPEG image. */
void* Jpeg_LoadDecodedImage(u8* addr, PRESJPEG* arg1, s32 side) {
    void* new_addr = MainPool_Alloc(arg1->decodedSize + 0x100, side);

    if (new_addr != NULL) {
        s32 size = Jpeg_DecodeImage(new_addr, arg1->decodedSize + 0x100, (uintptr_t)arg1 + arg1->dataOffset);

        if (size == 0) {
            MainPool_TryFree(new_addr);
            new_addr = NULL;
        } else if (side == 0) {
            MainPool_Realloc(new_addr, size);
        }
    }

    MainPool_TryFree(addr);

    return new_addr;
}

/** Allocates memory for and decompresses a Yay0 asset. */
void* Yay0_LoadDecompressedAsset(void* arg0, PERSSZP* arg1, s32 side) {
    void* vaddr = MainPool_Alloc(arg1->decompressed_size2, side);

    if (vaddr != NULL) {
        Yay0_DecompressAndRelocate(arg1, vaddr);
    }

    MainPool_TryFree(arg0);

    return vaddr;
}

/** Allocates memory and copies an uncompressed asset from the cartridge. */
u32* Asset_CopyUncompressed(u8* arg0, s32 arg1, u32 arg2, s32 arg3) {
    u32* sp1C = MainPool_Alloc(arg2, arg3);

    if (sp1C != NULL) {
        Util_Memcpy(sp1C, arg0 + arg1, arg2 >> 2);
    }

    MainPool_TryFree(arg0);

    return sp1C;
}

/** Loads an asset from a ROM range, decompressing it when it is compressed. */
void* Asset_LoadCompressed(u8* rom_start, u8* rom_end, s32 arg2, s32 arg3) {
    u32* addr;
    u32* newaddr;
    u32* ret;
    UNUSED s32 pad; // FAKE: stack padding
    int side;

    side = (arg2 == 0) ? MEMORY_POOL_RIGHT : MEMORY_POOL_LEFT;
    addr = MainPool_Alloc(0x18, side);

    if (addr != NULL) {
        Dma_WriteChunks((uintptr_t)addr, (uintptr_t)rom_start, (uintptr_t)rom_start + 0x18, arg3);
        // PORT: the tags and header words are big-endian
        if ((PORT_BE32(addr[0]) == 'PERS') && (PORT_BE32(addr[1]) == '-SZP')) {
            newaddr = MainPool_Alloc(ALIGN16(rom_end - rom_start), side);
            if (newaddr != NULL) {
                Dma_WriteChunks((uintptr_t)newaddr, (uintptr_t)rom_start, (uintptr_t)rom_end, arg3);
                Port_FixPersHeader((PERSSZP*)newaddr);
                ret = Yay0_LoadDecompressedAsset(newaddr, (PERSSZP*)newaddr, arg2);
            }
        } else if ((PORT_BE32(addr[0]) == 'PRES') && (PORT_BE32(addr[1]) == 'JPEG')) {
            newaddr = MainPool_Alloc(ALIGN16(rom_end - rom_start), side);
            if (newaddr != NULL) {
                Dma_WriteChunks((uintptr_t)newaddr, (uintptr_t)rom_start, (uintptr_t)rom_end, arg3);
                Port_FixPresJpegHeader((PRESJPEG*)newaddr);
                ret = Jpeg_LoadDecodedImage((u8*)newaddr, (PRESJPEG*)newaddr, arg2);
            }
        } else {
            newaddr = MainPool_Alloc(ALIGN16(rom_end - rom_start), arg2);
            ret = newaddr;
            if (newaddr != NULL) {
                Dma_WriteChunks((uintptr_t)ret, (uintptr_t)rom_start, (uintptr_t)rom_end, arg3);
            }
        }
        MainPool_TryFree(addr);
    }

    return ret;
}

/** Loads an asset from a range described by a storage descriptor. */
void* Storage_LoadAssetByLbaRange(StorageLbaRange* arg0, s32 arg1) {
    s32 sp44;
    u32* sp40;
    PERSSZP* temp_s0;
    s32 var_s0;
    u8* temp_v0;

    if (arg1 == 0) {
        var_s0 = 1;
    } else {
        var_s0 = 0;
    }

    LeoLBAToByte(arg0->startLba, arg0->lbaCount, &sp44);
    sp44 = ALIGN16(sp44);

    temp_v0 = MainPool_Alloc(sp44, var_s0);

    if (temp_v0 != NULL) {
        temp_s0 = arg0->dataOffset + temp_v0;

        Storage_QueueDiskTransfer(0, temp_v0, arg0->startLba, arg0->lbaCount, &D_800A96F0, 0);
        osRecvMesg(&D_800A96F0, NULL, 1);

        if (PORT_BE32(((u32*)temp_s0->magic)[0]) == 'PERS' && (PORT_BE32(((u32*)temp_s0->magic)[1]) == '-SZP')) {
            sp40 = Yay0_LoadDecompressedAsset(temp_v0, temp_s0, arg1);
        } else if (PORT_BE32(((u32*)temp_s0->magic)[0]) == 'PRES' && (PORT_BE32(((u32*)temp_s0->magic)[1]) == 'JPEG')) {
            sp40 = Jpeg_LoadDecodedImage(temp_v0, temp_s0, arg1);
        } else {
            sp40 = Asset_CopyUncompressed(temp_v0, arg0->dataOffset, arg0->size, arg1);
        }
    }

    return sp40;
}

/** Fills a storage descriptor with a block-address range. */
StorageLbaRange* Storage_SetLbaRange(StorageLbaRange* arg0, s32 arg1, s32 arg2) {
    arg0->startLba = arg1;
    arg0->lbaCount = arg2 - arg1;
    arg0->dataOffset = 0;

    LeoLBAToByte(arg0->startLba, arg0->lbaCount, &arg0->size);

    return arg0;
}

/** Fills a storage descriptor with a byte range. */
StorageLbaRange* Storage_SetByteRange(StorageLbaRange* arg0, u32 arg1, u32 offset, u32 size) {
    s32 sp2C;
    s32 sp28;
    s32 sp24;

    LeoByteToLBA(arg1, offset + 1, &sp28);
    sp28 -= 1;
    LeoByteToLBA(arg1, offset + size, &sp24);
    sp24 -= 1;

    if (sp28 > 0) {
        LeoLBAToByte(arg1, sp28, &sp2C);
    } else {
        sp2C = 0;
    }

    arg0->startLba = sp28 + arg1;
    arg0->lbaCount = (sp24 - sp28) + 1;
    arg0->dataOffset = offset - sp2C;
    arg0->size = size;

    return arg0;
}

/** Prefetches a block-address range into memory. */
void Storage_PrefetchLbaRange(s32 arg0, s32 arg1, s32 arg2) {
    StorageLbaRange sp1C;

    Storage_SetLbaRange(&sp1C, arg0, arg1);
    Storage_LoadAssetByLbaRange(&sp1C, arg2);
}

/** Clears the segment base entry when its memory block is freed. */
void Memmap_ClearSegmentOnBlockFree(uintptr_t block_addr, u32 addr) { // PORT: block address is pointer-sized
    u32 base = addr & 0xFF000000;
    u32 sp1C = addr & 0xFFFFFF;

    if ((base == 0x82000000) && (Memmap_GetSegmentBaseVaddr(sp1C) == block_addr)) {
        Memmap_ClearSegmentMemmap(sp1C);
    }
}

/** Loads an asset from a ROM range and maps it at a segment. */
u8* Asset_LoadToSegment(s32 id, u8* rom_start, u8* rom_end, s32 arg3) {
    u8* vaddr = Asset_LoadCompressed(rom_start, rom_end, arg3, 0);

    if ((vaddr != NULL) && (id > 0)) {
        Memmap_SetSegmentMap(id, vaddr, MainPool_GetBlockDist(vaddr));
        MainPool_SetFunc(vaddr, id + 0x82000000, &Memmap_ClearSegmentOnBlockFree);
    }
    return vaddr;
}

/** Loads an asset from a block-address range and maps it at a segment. */
MainPoolBlock* Asset_LoadToSegmentByLbaRange(s32 arg0, s32 arg1, s32 arg2, s32 arg3) {
    UNUSED s32 pad; // FAKE: stack padding
    StorageLbaRange sp28;
    MainPoolBlock* temp_v0 = Storage_LoadAssetByLbaRange(Storage_SetLbaRange(&sp28, arg1, arg2), arg3);

    if ((temp_v0 != NULL) && (arg0 > 0)) {
        Memmap_SetSegmentMap(arg0, temp_v0, MainPool_GetBlockDist(temp_v0));
        MainPool_SetFunc(temp_v0, arg0 + 0x82000000, Memmap_ClearSegmentOnBlockFree);
    }

    return temp_v0;
}

/** Callback run when a fragment's memory block is freed. */
void Fragment_FreeCallback(uintptr_t base_addr, u32 addr) { // PORT: block address is pointer-sized
    u32 sp1C;

    Port_VFree((void*)base_addr);
    u32 temp_a0;

    if ((addr & 0xFF000000) == 0x81000000) {
        sp1C = addr & 0xFFFFFF;
        if (Memmap_GetFragmentBaseVaddr(sp1C) == base_addr) {
            Memmap_ClearFragmentMemmap(sp1C);
        }
    }
}

// PORT: set while registering a data blob from an archive, so its pointers are moved into the virtual address space
// before the relocation table (which lies past sizeInRam) is released by the shrink below.
int gPortRelocateDataBlob;

// PORT: callers compute a fragment's slot id from a fragment symbol's address ((&D_8F900000 & 0x0FF00000) >> 20) -
// 0x10, which is a host address here. Take it from the image instead: its entry `j target` is linked in its slot.
static s32 Port_FragmentSlotOf(Fragment* f, s32 fallback) {
    u32 insn = PORT_BE32(f->inst[0]);
    u32 link = (((insn & 0x03FFFFFF) << 2) | 0x80000000) & 0xFFF00000;

    if ((insn >> 26) == 2 && link >= 0x81000000 && link < 0x90000000) {
        return ((link & 0x0FF00000) >> 0x14) - 0x10;
    }
    return fallback;
}

/** Registers a loaded fragment so it is freed with its block. */
void Fragment_Register(s32 arg0, Fragment* addr) {
    UNUSED s32 pad; // FAKE: stack padding
    u32 sp20;

    Memmap_NormalizeFragmentHeader(addr); // PORT: the header words are big-endian
    arg0 = Port_FragmentSlotOf(addr, arg0);
    sp20 = MainPool_GetBlockDist(addr);

    MainPool_SetFunc(addr, arg0 + 0x81000000, &Fragment_FreeCallback);

    if (sp20 < addr->sizeInRam) {
        MainPool_Realloc(addr, addr->sizeInRam);
        Memmap_RelocateFragment(arg0, addr);
        if (gPortRelocateDataBlob) {
            Port_RelocateDataBlob(addr);
        }
    } else {
        Memmap_RelocateFragment(arg0, addr);
        if (gPortRelocateDataBlob) {
            Port_RelocateDataBlob(addr);
        }
        MainPool_Realloc(addr, addr->sizeInRam);
    }
}

/** Loads a fragment from a ROM range and registers it. */
ret_func_80004454 Fragment_Load(s32 arg0, u8* rom_start, u8* rom_end) {
    void* addr = Asset_LoadCompressed(rom_start, rom_end, 0, 0);

    if (addr != NULL) {
        arg0 = Port_FragmentSlotOf((Fragment*)addr, arg0); // PORT
        Fragment_Register(arg0, (Fragment*)addr);
    }
#ifdef PORT_STATIC_FRAGMENTS
    // PORT: fragment code is statically linked (port_fragments.c): return the C entry, not the loaded image
    Port_FragmentRemember(rom_start, addr);
    Port_FragmentLoaded(arg0, rom_start);
    return (ret_func_80004454)Port_FragmentEntry(rom_start);
#else
    return addr;
#endif
}

/** Loads a fragment from a block-address range and registers it. */
void* Fragment_LoadByLbaRange(s32 arg0, s32 arg1, s32 arg2) {
    Fragment* sp2C;
    StorageLbaRange sp20;

    sp2C = Storage_LoadAssetByLbaRange(Storage_SetLbaRange(&sp20, arg1, arg2), 0);
    if (sp2C != NULL) {
        Fragment_Register(arg0, sp2C);
    }
    return sp2C;
}

// PORT: header words of PERS-SZP / PRESJPEG assets are big-endian; swap them in place once, right after the file is
// read (from stadion).
void Port_FixPersHeader(void* h) {
    u32* w = (u32*)h;
    w[2] = PORT_BE32(w[2]);
    w[3] = PORT_BE32(w[3]);
    w[4] = PORT_BE32(w[4]);
    w[5] = PORT_BE32(w[5]);
}

void Port_FixPresJpegHeader(void* h) {
    u32* w = (u32*)h;
    w[2] = PORT_BE32(w[2]);
    w[3] = PORT_BE32(w[3]);
}

// PORT: an archive's file table keeps the loaded copy of each file in a 32-bit word: host pointers go through handles.
#define PORT_HANDLE_MAX 0x10000
static void* sPortHandlePtrs[PORT_HANDLE_MAX];
static u32 sPortHandleCount = 1;

u32 Port_PtrToHandle(void* p) {
    u32 i;
    if (p == NULL) {
        return 0;
    }
    for (i = 1; i < sPortHandleCount; i++) {
        if (sPortHandlePtrs[i] == p) {
            return i;
        }
    }
    if (sPortHandleCount >= PORT_HANDLE_MAX) {
        return 0;
    }
    sPortHandlePtrs[sPortHandleCount] = p;
    return sPortHandleCount++;
}

void* Port_HandleToPtr(u32 h) {
    return (h != 0 && h < sPortHandleCount) ? sPortHandlePtrs[h] : NULL;
}

// PORT: archive headers and file tables are big-endian words (the first word holds two u16: flags, fragment id).
static void BinArchive_SwapHeader(BinArchive* a) {
    u8* b = (u8*)a;
    u16 flags = (u16)((b[0] << 8) | b[1]);
    u16 id = (u16)((b[2] << 8) | b[3]);
    a->flags = flags;
    a->fragmentId = id;
    Port_SwapWords((u32*)a + 1, 3);
}

static void BinArchive_SwapTable(BinArchive* a) {
    Port_SwapWords((u32*)a + 4, a->num_files * 4); // header already swapped by the caller
}

/** Loads a bin archive's header from a ROM range. */
BinArchive* BinArchive_Open(u8* rom_start, u8* rom_end, s32 arg2, s32 arg3) {
    BinArchive* ret = NULL;
    u32 sp28;
    u32 sp28_2;
    u16* newaddr;

    switch (arg2) {
        case 0:
            newaddr = Asset_LoadCompressed(rom_start, rom_end, 0, 0);
            ret = newaddr;
            if (newaddr != NULL) {
                BinArchive_SwapHeader(ret);
                BinArchive_SwapTable(ret);
                newaddr[0] |= 1;
            }
            break;

        case 1:
            newaddr = Asset_LoadCompressed(rom_start, rom_start + 0x20, 0, 0);
            ret = newaddr;
            if (newaddr != NULL) {
                BinArchive_SwapHeader(ret);
                sp28 = (ret->num_files * 0x10) + 0x10;
                MainPool_TryFree(ret);
                newaddr = Asset_LoadCompressed(rom_start, rom_start + sp28, 0, 0);
                ret = newaddr;
                if (newaddr != NULL) {
                    BinArchive_SwapHeader(ret);
                    BinArchive_SwapTable(ret);
                    *newaddr |= 2;
                    if (arg3 == 1) {
                        *newaddr |= 4;
                    }
                }
            }
            break;

        case 2:
            newaddr = Asset_LoadCompressed(rom_start, rom_start + 0x20, 1, 0);
            ret = newaddr;
            if (newaddr != NULL) {
                BinArchive_SwapHeader(ret);
                sp28_2 = (ret->num_files * 0x10) + 0x10;
                MainPool_TryFree(ret);
                newaddr = Asset_LoadCompressed(rom_start, rom_start + sp28_2, 1, 0);
                ret = newaddr;
                if (newaddr != NULL) {
                    BinArchive_SwapHeader(ret);
                    BinArchive_SwapTable(ret);
                }
                if ((newaddr != NULL) && (arg3 == 1)) {
                    *newaddr |= 4;
                }
            }
            break;
    }

    if (ret != NULL) {
        ret->source = (uintptr_t)rom_start; // PORT: ROM offsets are small absolute addresses
    }

    return ret;
}

/** Loads a bin archive from a block-address range. */
void* BinArchive_LoadByLbaRange(s32 arg0, s32 arg1, s32 arg2, s32 arg3) {
    BinArchive* var_a0;
    StorageLbaRange sp20;

    switch (arg2) {
        default:
            break;

        case 0:
            Storage_SetLbaRange(&sp20, arg0, arg1);
            var_a0 = Storage_LoadAssetByLbaRange(&sp20, 0);
            if (var_a0 != NULL) {
                MainPool_Realloc(var_a0, var_a0->total_size);
                var_a0->flags |= 1;
            }
            break;

        case 1:
            Storage_SetLbaRange(&sp20, arg0, arg0 + 1);
            var_a0 = Storage_LoadAssetByLbaRange(&sp20, 0);
            if (var_a0 != NULL) {
                MainPool_Realloc(var_a0, (var_a0->num_files * sizeof(BinArchiveFile)) + sizeof(BinArchive));
                var_a0->flags |= 2;
                if (arg3 == 1) {
                    var_a0->flags |= 4;
                }
            }
            break;

        case 2:
            Storage_SetLbaRange(&sp20, arg0, arg0 + 1);
            var_a0 = Storage_LoadAssetByLbaRange(&sp20, 1);
            if (arg3 == 1) {
                var_a0->flags |= 4;
            }
            break;
    }

    if (var_a0 != NULL) {
        var_a0->flags |= 0x80;
        var_a0->source = arg0;
    }

    return var_a0;
}

/** Frees a bin archive. */
void BinArchive_Free(void* arg0) {
    MainPool_TryFree(arg0);
}

/** Loads one archive file directly from the ROM. */
Fragment* BinArchive_LoadFileFromRom(BinArchive* arg0, BinArchiveFile* arg1) {
    u8* temp_a0 = (u8*)(uintptr_t)arg0->source + arg1->offset;

    return Asset_LoadCompressed(temp_a0, arg1->size + temp_a0, 0, 0);
}

/** Loads one archive file through the buffered storage path. */
void* BinArchive_LoadFileBuffered(BinArchive* arg0, BinArchiveFile* arg1) {
    StorageLbaRange sp1C;

    Storage_SetByteRange(&sp1C, arg0->source, arg1->offset, arg1->size);
    return Storage_LoadAssetByLbaRange(&sp1C, 0);
}

/** Returns the loaded copy of an archive file, loading it if needed. */
void* BinArchive_GetFile(BinArchive* archive, s32 file_number) {
    Fragment* var_a2 = NULL;
    UNUSED s32 pad; // FAKE: stack padding
    BinArchiveFile* sp18 = (u8*)archive + sizeof(BinArchive) + (file_number * sizeof(BinArchiveFile));

    if (archive == NULL) {
        return NULL; // PORT: archive failed to open
    }

    if (file_number < archive->num_files) {
        if (archive->flags & 1) {
            var_a2 = (u8*)archive + sp18->offset;
        } else if (!(archive->flags & 4) && (sp18->cached != 0)) {
            var_a2 = Port_HandleToPtr(sp18->cached); // PORT: a handle, see Port_PtrToHandle
        } else {
            if (archive->flags & 0x80) {
                var_a2 = BinArchive_LoadFileBuffered(archive, sp18);
            } else {
                var_a2 = BinArchive_LoadFileFromRom(archive, sp18);
            }

            if (var_a2 != NULL) {
                if ((PORT_BE32(((u32*)var_a2->magic)[0]) == 'FRAG') && (PORT_BE32(((u32*)var_a2->magic)[1]) == 'MENT')) {
                    gPortRelocateDataBlob = 1; // PORT: a data blob: its pointers move to a pseudo address range
                    Fragment_Register(archive->fragmentId, var_a2);
                    gPortRelocateDataBlob = 0;
                }

                sp18->cached = Port_PtrToHandle(var_a2);
            }
        }
    }

    return var_a2;
}

/** Returns the cached copy of an archive file, if there is one. */
void* BinArchive_GetCachedFile(BinArchive* archive, s32 file_number) {
    BinArchiveFile* sp18 = (u8*)archive + sizeof(BinArchive) + (file_number * sizeof(BinArchiveFile));
    void* var_v1 = NULL;

    if (file_number < archive->num_files) {
        var_v1 = Port_HandleToPtr(sp18->cached);
    }
    return var_v1;
}
