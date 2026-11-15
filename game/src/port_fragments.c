/* PORT: not part of the decomp. Statically linked fragments (from stadion's jpeg_stream.c, adapted to Stadium Zero). */
#include <stdio.h>
#include <stdlib.h>
#include "global.h"
#include "memory_main.h"
#include "StadionPort.h"
#include "../../src/port/zero_lift.h"

// The table maps a fragment's ROM offset to the function its header jumps to (src/port/generated/
// port_fragment_entries.c, tools/zero/gen_fragment_entries.py). Loading still brings the fragment's image into memory:
// data blobs are read from it, and code fragments keep the N64's block in the pool.
struct PortFragmentEntry {
    unsigned romStart;
    unsigned textStart;
    void* entry;
    const char* name;
};
extern const struct PortFragmentEntry gPortFragmentEntries[];
static void* sPortFragmentBlock[0x100];
static unsigned sPortFragmentRom[0x100];
static unsigned sPortSlotRom[240];

static const struct PortFragmentEntry* Port_FindFragment(u8* romStart) {
    const struct PortFragmentEntry* e;
    for (e = gPortFragmentEntries; e->name != NULL; e++) {
        if (e->romStart == (unsigned)(uintptr_t)romStart) {
            return e;
        }
    }
    return NULL;
}

void Port_FragmentRemember(u8* romStart, void* block) {
    int i;
    for (i = 0; i < 0x100; i++) {
        if (sPortFragmentBlock[i] == NULL || sPortFragmentRom[i] == (unsigned)(uintptr_t)romStart) {
            sPortFragmentBlock[i] = block;
            sPortFragmentRom[i] = (unsigned)(uintptr_t)romStart;
            return;
        }
    }
}

void Port_FragmentFree(u8* romStart) {
    int i;
    for (i = 0; i < 0x100; i++) {
        if (sPortFragmentBlock[i] != NULL && sPortFragmentRom[i] == (unsigned)(uintptr_t)romStart) {
            MainPool_TryFree(sPortFragmentBlock[i]);
            sPortFragmentBlock[i] = NULL;
            return;
        }
    }
}

void* Port_FragmentEntry(u8* romStart) {
    const struct PortFragmentEntry* e = Port_FindFragment(romStart);
    return e ? e->entry : NULL;
}

unsigned Port_SlotRom(u32 slot) {
    return slot < 240 ? sPortSlotRom[slot] : 0;
}

// Called by Fragment_Load once the image is in memory: the fragment's data starts fresh, as the N64 reloads it from
// ROM on every load (C statics: Port_FragmentResetData; the asm-side data: the data lifter, src/port/zero_lift.c).
void Port_FragmentLoaded(u32 slot, u8* romStart) {
    static int sTrace = -1;
    const struct PortFragmentEntry* e = Port_FindFragment(romStart);

    if (sTrace < 0) {
        sTrace = getenv("PORT_TRACE_FRAGMENTS") != NULL;
    }
    if (sTrace) {
        fprintf(stderr, "[fragment] slot %u: %s @frame %llu\n", (unsigned)slot, e && e->name ? e->name : "?",
                (unsigned long long)Port_ViTick());
    }
    if (slot < 240) {
        sPortSlotRom[slot] = (unsigned)(uintptr_t)romStart;
    }
    if (e != NULL) {
        ZLift_LoadImage((int)(e - gPortFragmentEntries) + 1);
        Port_FragmentResetData(e->entry);
    }
}
