/* Stadium Zero resident code. */
/* C ported from the Stadium 1 decomp: src/memmap.c. */

#include "global.h"
#include "memmap.h"
struct MemoryMap {
    uintptr_t vaddr;
    size_t size;
};
extern struct MemoryMap gSegments[16];
extern struct MemoryMap gFragments[240];

void Memmap_SetSegmentMap(u32 id, uintptr_t vaddr, size_t size);
uintptr_t Memmap_GetSegmentBaseVaddr(u32 id);
uintptr_t Memmap_GetSegmentVaddr(u32 mask);
u32 Memmap_GetSegmentVaddrMask(u32 i, uintptr_t addr);
void Memmap_ClearSegmentMemmap(u32 id);
void Memmap_SetSegments(Gfx** gfxDl);
void Memmap_RelocateFragment(u32 id, struct Fragment* fragment);
void Memmap_SetFragmentMap(u32 id, uintptr_t vaddr, size_t size);
uintptr_t Memmap_GetFragmentBaseVaddr(u32 id);
uintptr_t Memmap_GetFragmentVaddr(void* addr);
u32 Memmap_GetFragmentVaddrMask(u32 i, uintptr_t addr);
void Memmap_ClearFragmentMemmap(u32 id);
uintptr_t Memmap_GetLoadedFragmentVaddr(uintptr_t addr);

#include <stdio.h>
#include <stdlib.h>
#include "StadionPort.h"

// PORT: from stadion (game/src/memmap.c)
/*
 * PORT: fragment headers in ROM data are big-endian. The first word after the jump is always a nop (0), which is
 * reused as the "already byte swapped" marker so this can be called from every entry point.
 */
#define PORT_FRAGMENT_SWAPPED 0x50525453
void Memmap_NormalizeFragmentHeader(struct Fragment* f) {
    if (f->inst[1] != PORT_FRAGMENT_SWAPPED) {
        f->headerSize = PORT_BE32(f->headerSize);
        f->relocOffset = PORT_BE32(f->relocOffset);
        f->sizeInRom = PORT_BE32(f->sizeInRom);
        f->sizeInRam = PORT_BE32(f->sizeInRam);
        f->inst[1] = PORT_FRAGMENT_SWAPPED;
    }
}

/*
 * PORT: virtual 32-bit address space for data blobs (models) that the game relocates into RAM and keeps several of at
 * once. All of them are linked at the same slot address (e.g. 0x8FF00000), so without relocation their addresses
 * collide. Each loaded blob gets a pseudo base here; same-slot pointers in its data are rewritten to it (see
 * Port_RelocateDataBlob) and Port_VResolve maps pseudo addresses back to host memory.
 */
#define PORT_VBASE 0x30000000u
#define PORT_VBLOBS 512
struct PortVBlob {
    u8* host;
    u32 size;
    u32 link; // slot base the blob was linked at
};
static struct PortVBlob sVBlobs[PORT_VBLOBS];

u32 Port_VAlloc(void* host, u32 size, u32 link) {
    int i;
    if (size > 0x100000) {
        return 0;
    }
    // The pool hands the same memory out again after a blob is released; drop a stale entry that starts at the same
    // address so lookups by host address (Port_VAddrOfLink) and by pseudo address cannot find the old blob.
    for (i = 0; i < PORT_VBLOBS; i++) {
        if (sVBlobs[i].host == (u8*)host) {
            sVBlobs[i].host = NULL;
        }
    }
    for (i = 0; i < PORT_VBLOBS; i++) {
        if (sVBlobs[i].host == NULL) {
            sVBlobs[i].host = host;
            sVBlobs[i].size = size;
            sVBlobs[i].link = link;
            if (PORT_GETENV_ONCE("PORT_TRACE_VBLOB"))
                fprintf(stderr, "[vblob] alloc slot %d host %p size 0x%x link 0x%08x\n", i, host, size, link);
            return PORT_VBASE + (u32)i * 0x100000;
        }
    }
    return 0;
}

void Port_VFree(void* host) {
    int i;
    for (i = 0; i < PORT_VBLOBS; i++) {
        if (sVBlobs[i].host == (u8*)host) {
            if (PORT_GETENV_ONCE("PORT_TRACE_VBLOB"))
                fprintf(stderr, "[vblob] free slot %d host %p\n", i, host);
            sVBlobs[i].host = NULL;
        }
    }
    Port_GfxFlushCache();
}

void* Port_VResolve(uintptr_t addr) {
    if (addr >= PORT_VBASE && addr < PORT_VBASE + PORT_VBLOBS * 0x100000u) {
        struct PortVBlob* b = &sVBlobs[(addr - PORT_VBASE) >> 20];
        u32 off = addr & 0xFFFFF;
        if (b->host != NULL && off < b->size) {
            return b->host + off;
        }
    }
    return NULL;
}

int Port_VIsRaw(const void* p) {
    int i;
    for (i = 0; i < PORT_VBLOBS; i++) {
        if (sVBlobs[i].host != NULL && (const u8*)p >= sVBlobs[i].host &&
            (const u8*)p < sVBlobs[i].host + sVBlobs[i].size) {
            return 1;
        }
    }
    return 0;
}

// Link address of a pseudo address (0 if it is not one of a registered blob).
u32 Port_VLinkOf(uintptr_t addr) {
    if (addr >= PORT_VBASE && addr < PORT_VBASE + PORT_VBLOBS * 0x100000u) {
        struct PortVBlob* b = &sVBlobs[(addr - PORT_VBASE) >> 20];
        u32 off = addr & 0xFFFFF;
        if (b->host != NULL && off < b->size) {
            return b->link + off;
        }
    }
    return 0;
}

// Pseudo address of a link address inside the blob that starts at 'host' (0 if the blob is not registered).
u32 Port_VAddrOfLink(void* host, u32 linkAddr) {
    int i;
    for (i = PORT_VBLOBS - 1; i >= 0; i--) {
        if (sVBlobs[i].host == (u8*)host) {
            return PORT_VBASE + (u32)i * 0x100000 + (linkAddr & 0xFFFFF);
        }
    }
    return 0;
}

// Rewrites the same-slot pointer words of a freshly loaded data blob (R_MIPS_32 entries of its relocation table) to the
// blob's pseudo base. Code relocations are skipped: the blob's MIPS code never runs here.
u32 Port_RelocateDataBlob(struct Fragment* f) {
    struct RelocTable* tbl;
    u32 link;
    u32 vbase;
    u32 i;
    u32 n;

    Memmap_NormalizeFragmentHeader(f);
    {
        // entry instruction is `j target`; the target's high bits are the slot base
        u32 insn = PORT_BE32(f->inst[0]);
        link = (((insn & 0x03FFFFFF) << 2) | 0x80000000) & 0xFFF00000;
    }
    vbase = Port_VAlloc(f, f->sizeInRam, link);
    if (vbase == 0) {
        return 0;
    }
    tbl = (struct RelocTable*)((u8*)f + f->relocOffset);
    n = PORT_BE32(tbl->nRelocations);
    for (i = 0; i < n; i++) {
        u32 reloc = PORT_BE32(tbl->relocations[i]);
        if (((reloc & 0x7F000000) >> 24) == R_MIPS_32) {
            u8* w = (u8*)f + (reloc & 0xFFFFFF);
            u32 v = Port_ReadBE32(w);
            if ((v & 0xFFF00000) == link) {
                v = vbase + (v & 0xFFFFF);
                w[0] = v >> 24;
                w[1] = v >> 16;
                w[2] = v >> 8;
                w[3] = v;
            }
        }
    }
    return vbase;
}


/*
 * Set the base virtual address and size of the specified segment memmap.
 */
void Memmap_SetSegmentMap(u32 id, uintptr_t vaddr, size_t size) {
    gSegments[id].vaddr = vaddr;
    gSegments[id].size = size;
}

/*
 * Return the base address of the specified segment number.
 */
uintptr_t Memmap_GetSegmentBaseVaddr(u32 id) {
    return gSegments[id].vaddr;
}

/*
 * Return the current virtual address of a given offset into a segment. This is
 * calculated based on the input bitmask constructed of the following values:
 *
 * Input: XYZZZZZZ
 * X: Safety nibble. Only process if 0.
 * Y: Segment number.
 * Z: Segment offset.
 */
uintptr_t Memmap_GetSegmentVaddr(u32 mask) {
    if ((mask >> 0x1C) == 0) {
        u32 id = (mask & 0x0F000000) >> 0x18;
        u32 value = (mask & 0x00FFFFFF) >> 0x00;
        if (gSegments[id].vaddr != NULL) {
            mask = gSegments[id].vaddr + value;
        }
    }
    return mask;
}

/*
 * Returns the input bitmask equivalent for Memmap_GetSegmentVaddr's input. This function
 * appears to be unused.
 */
u32 Memmap_GetSegmentVaddrMask(u32 i, uintptr_t addr) {
    struct MemoryMap* entry = &gSegments[i];
    size_t diff = addr - entry->vaddr;

    if (diff >= entry->size) {
        return 0;
    }
    return (i << 0x18) | diff;
}

/*
 * Clear the memmap for the segment ID specified.
 */
void Memmap_ClearSegmentMemmap(u32 id) {
    gSegments[id].vaddr = NULL;
    gSegments[id].size = 0;
}

/*
 * Initialize the 16 segments with the segment memmap virtual addresses for the
 * display list head.
 */
void Memmap_SetSegments(Gfx** gfx_dl) {
    u32 i;
    Gfx* gfx = *gfx_dl;

    for (i = 0; i < 16; i++) {
        gSPSegment(gfx++, i, (uintptr_t)gSegments[i].vaddr); // PORT: full host pointer, not a 29-bit physical address
    }
    *gfx_dl = gfx;
}

/*
 * Perform fragment relocation on a given Fragment.
 */
void Memmap_RelocateFragment(u32 id, struct Fragment* fragment) {
    u32 is_lo_neg;
    u32* lui_refs[32];
    u32 lui_vals[32];
    u32* lui_inst_ref;
    u32* reloc_data_p;
    u32 reloc_size;
    struct RelocTable* reloc_info;
    UNUSED u32 reloc_offset;
    u32 reloc;
    u32 temp_v0_5;
    u32 i;
    u32* reg_val_p;
    UNUSED s32 pad; // FAKE: stack padding

    reloc_offset = fragment->relocOffset;
    reloc_size = fragment->sizeInRam - fragment->relocOffset;
    reloc_info = (struct RelocTable*)((uintptr_t)fragment->relocOffset + (uintptr_t)fragment);

    osInvalICache(fragment, fragment->sizeInRam);
    osInvalDCache(fragment, fragment->sizeInRam);
    Memmap_SetFragmentMap(id, (uintptr_t)fragment, fragment->sizeInRam);
#ifdef PORT_STATIC_FRAGMENTS
    // PORT: fragment code is linked into the executable, so there is nothing to relocate. The image stays as loaded
    // (data blobs are read from it; their words still hold link-time addresses).
    return;
#endif

    for (i = 0; i < reloc_info->nRelocations; i++) {
        reloc = reloc_info->relocations[i];
        reloc_data_p = (u32*)((reloc & 0xFFFFFF) + (uintptr_t)fragment);

        switch ((reloc & 0x7F000000) >> 24) {
            case R_MIPS_32:
                // Handles 32-bit address relocation, used for things such as jump tables and pointers in data.
                // Just relocate the full address.
                *reloc_data_p = Memmap_GetFragmentVaddr(*reloc_data_p);
                break;
            case R_MIPS_26:
                // Handles 26-bit address relocation, used for jumps and jals.
                // Extract the address from the target field of the J-type MIPS instruction.
                // Relocate the address and update the instruction.
                *reloc_data_p =
                    (((u32)(Memmap_GetFragmentVaddr(((*reloc_data_p * 4) & 0x0FFFFFFC) + 0x80000000) & 0x0FFFFFFF) >> 2) |
                     (*reloc_data_p & 0xFC000000));
                break;
            case R_MIPS_HI16:
                // Handles relocation for a hi/lo pair, part 1.
                // Store the reference to the LUI instruction (hi) using the `rt` register of the instruction.
                // This will be updated later in the `R_MIPS_LO16` section.
                lui_refs[(*reloc_data_p >> 0x10) & 0x1F] = reloc_data_p;
                lui_vals[(*reloc_data_p >> 0x10) & 0x1F] = *reloc_data_p;
                break;
            case R_MIPS_LO16:
                // Handles relocation for a hi/lo pair, part 2.
                // Grab the stored LUI (hi) from the `R_MIPS_HI16` section using the `rs` register of the instruction.
                // The full address is calculated, relocated, and then used to update both the LUI and lo instructions.
                // If the lo part is negative, add 1 to the LUI value.
                // Note: The lo instruction is assumed to have a signed immediate.
                lui_inst_ref = lui_refs[(*reloc_data_p >> 0x15) & 0x1F];
                reg_val_p = &lui_vals[(*reloc_data_p >> 0x15) & 0x1F];

                temp_v0_5 = Memmap_GetFragmentVaddr((*reg_val_p << 0x10) + (s16)*reloc_data_p);
                is_lo_neg = (temp_v0_5 & 0x8000) ? 1 : 0;
                *lui_inst_ref = (u32)((*lui_inst_ref & 0xFFFF0000) | ((temp_v0_5 >> 16) + is_lo_neg));
                *reloc_data_p = (u32)((*reloc_data_p & 0xFFFF0000) | (temp_v0_5 & 0xFFFF));
                break;
        }
    }
    if (reloc_size != 0) {
        bzero((void*)((uintptr_t)fragment->relocOffset + (uintptr_t)fragment), reloc_size);
    }
    osWritebackDCache(fragment, fragment->sizeInRam);
}

/*
 * Set the base virtual address and size of the specified fragment memmap.
 */
void Memmap_SetFragmentMap(u32 id, uintptr_t vaddr, size_t size) {
    gFragments[id].vaddr = vaddr;
    gFragments[id].size = size;
}

/*
 * Return the base vaddress of the fragment ID specified.
 */
uintptr_t Memmap_GetFragmentBaseVaddr(u32 id) {
    return gFragments[id].vaddr;
}

/*
 * Return the current virtual address of a given offset into a fragment. This is
 * calculated based on the input bitmask constructed of the following values:
 *
 * Input: XYYZZZZZ
 * X: Safety nibble. Only process if 0. This is handled by the address range check
 *    at the start.
 * Y: Fragment number.
 * Z: Fragment offset.
 */
uintptr_t Memmap_GetFragmentVaddr(void* addr) {
    if (((uintptr_t)addr >= 0x81000000U) && ((uintptr_t)addr < 0x90000000U)) {
        u32 id = (((uintptr_t)addr & 0x0FF00000) >> 0x14) - 0x10;
        u32 value = (((uintptr_t)addr & 0x000FFFFF));

        if (gFragments[id].vaddr != NULL) {
            addr = gFragments[id].vaddr + value;
        }
    }
    return (uintptr_t)addr;
}

/*
 * Returns the input bitmask equivalent for Memmap_GetFragmentVaddr's input. This function
 * appears to be unused.
 */
u32 Memmap_GetFragmentVaddrMask(u32 i, uintptr_t addr) {
    struct MemoryMap* entry = &gFragments[i];
    size_t diff = addr - entry->vaddr;

    if (diff >= entry->size) {
        return 0;
    }
    return ((i + 0x10) << 0x14) | 0x80000000 | diff;
}

/*
 * Clear the memmap for the fragment ID specified.
 */
void Memmap_ClearFragmentMemmap(u32 id) {
    gFragments[id].vaddr = NULL;
    gFragments[id].size = 0;
}

/*
 * Given a specified address, check to see if it is located inside of a fragment
 * currently loaded. Return a calculated address.
 */
uintptr_t Memmap_GetLoadedFragmentVaddr(uintptr_t addr) {
    struct MemoryMap* fraglist = gFragments;
    int i, UNUSED j;

    for (i = 0, fraglist = gFragments; i < 240; i++, fraglist++) {
        if (addr >= fraglist->vaddr) {
            size_t diff = addr - fraglist->vaddr;
            if (diff < fraglist->size) {
                return ((i + 0x10) << 0x14) | 0x80000000 | diff;
            }
        }
    }
    return 0;
}
