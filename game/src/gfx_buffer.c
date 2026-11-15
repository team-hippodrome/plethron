/* Stadium Zero resident code. */
/* C ported from the Stadium 1 decomp: src/gfx_buffer.c. */

#include "global.h"
#include "gfx_buffer.h"
#include "memory.h" // PORT: AllocateFunc

// PORT: the decomp called these without a prototype (IDO allows it; clang does not).
void* MainPool_AllocWithFunc(u32 size, s32 side, s32 arg, AllocateFunc func);
void MainPool_TryFree(void* arg0);
typedef struct DisplayListBuffers {
    /* 0x00 */ u32* buffers[2];  // @unverified: 'DYN0'/'DYN1' allocations in Gfx_InitDisplayListBuffers
    /* 0x08 */ uintptr_t allocEnd; // @unverified: Gfx_AllocDisplayList takes space down from here toward gDisplayListHead (PORT: uintptr_t)
    /* 0x0C */ s32 bufferSize;   // @unverified: buffer_size argument of Gfx_InitDisplayListBuffers
    /* 0x10 */ s16 current;      // @unverified: toggled by Gfx_SwapDisplayListBuffer, indexes buffers[]
} DisplayListBuffers; // size = 0x14
extern Gfx* gDisplayListHead;
extern DisplayListBuffers D_800AA158;
void Gfx_SwapDisplayListBuffer(void);

void Gfx_InitDisplayListBuffers(u32 buffer_size, s32 pool_side);
void Gfx_FreeDisplayListBuffers(void);
void Gfx_SwapDisplayListBuffer(void);
void Gfx_GetDisplayListRange(Gfx** arg0, Gfx** arg1);
void* Gfx_AllocDisplayList(s32 size);
s32 Gfx_GetDisplayListRemaining(void);

/** Allocates the two display list buffers. */
void Gfx_InitDisplayListBuffers(u32 buffer_size, s32 pool_side) {
    D_800AA158.current = 0;
    D_800AA158.bufferSize = buffer_size;
    // PORT: twice the requested size, because a host Gfx command is 16 bytes (N64: 8); as in stadion
    D_800AA158.buffers[0] = MainPool_AllocWithFunc(buffer_size * 2, pool_side, 'DYN0', NULL);
    D_800AA158.buffers[1] = MainPool_AllocWithFunc(buffer_size * 2, pool_side, 'DYN1', NULL);
    Gfx_SwapDisplayListBuffer();
}

/** Frees the display list buffers. */
void Gfx_FreeDisplayListBuffers(void) {
    MainPool_TryFree(D_800AA158.buffers[1]);
    MainPool_TryFree(D_800AA158.buffers[0]);
}

/** Switches to the other display list buffer. */
void Gfx_SwapDisplayListBuffer(void) {
    u32* buf; // PORT: index buffers[] instead of a 4-byte stride over the struct

    D_800AA158.current ^= 1;
    buf = D_800AA158.buffers[D_800AA158.current];

    D_800AA158.allocEnd = (uintptr_t)buf + D_800AA158.bufferSize * 2; // PORT: see Gfx_InitDisplayListBuffers
    gDisplayListHead = (Gfx*)buf;
}

/** Returns the start and current end of the display list buffer. */
void Gfx_GetDisplayListRange(Gfx** arg0, Gfx** arg1) {
    uintptr_t buf = (uintptr_t)D_800AA158.buffers[D_800AA158.current]; // PORT: see Gfx_SwapDisplayListBuffer
    *arg1 = (Gfx*)((uintptr_t)gDisplayListHead - buf);
    *arg0 = (Gfx*)buf;
}

/** Allocates space from the end of the display list buffer. */
void* Gfx_AllocDisplayList(s32 size) {
    uintptr_t temp_v0; // PORT: host pointers
    uintptr_t ret;

    size = ALIGN8(size);
    temp_v0 = (D_800AA158.allocEnd - size);
    ret = NULL;
    if (temp_v0 >= (uintptr_t)gDisplayListHead) {
        D_800AA158.allocEnd = temp_v0;
        ret = temp_v0;
    }
    return (void*)ret;
}

/** Returns how many bytes of the display list buffer remain. */
s32 Gfx_GetDisplayListRemaining(void) {
    return D_800AA158.allocEnd - (uintptr_t)gDisplayListHead;
}
