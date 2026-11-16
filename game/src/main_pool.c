/* Stadium Zero resident code. */
/* C ported from the Stadium 1 decomp: src/memory.c. */

#include "global.h"
#include "main_pool.h"
#include "memory_main.h"

// Declared here instead of in a header: the callers of these disagree on their types.
void MainPool_FinalizeAllocation(struct MemoryBlock* block);
void MainPool_ResetState(struct MainPoolState* state);
void MainPool_ResetStateToOffset(struct MainPoolState* state, s32 size);
void* MainPool_AllocAligned(struct MainPoolState* state, s32 arg1, s32 arg2);
void* MainPool_InitState(struct MainPoolState* block, s32 size);

void* MainPool_AllocState(u32 size, s32 side);
void MainPool_FinalizeAllocation(struct MemoryBlock* block);
void* MainPool_InitState(struct MainPoolState* block, s32 size);
void* MainPool_AllocAligned(struct MainPoolState* state, s32 arg1, s32 arg2);
void MainPool_ResetStateToOffset(struct MainPoolState* state, s32 size);
void MainPool_ResetState(struct MainPoolState* state);

/** Allocates a main pool state from one side of the pool. */
void* MainPool_AllocState(u32 size, s32 side) {
    MainPoolBlock* block;
    void* ptr = NULL;

    size = ALIGN4(size);
    ptr = 0;
    block = MainPool_Alloc(size, side);
    if (block != NULL) {
        ptr = MainPool_InitState(block, size);
    }
    return ptr;
}

// PORT: the arena helpers below view the same memory as MainPoolState and MemoryBlock, which only lines up with
// 32-bit pointers (freeSpace == next, listHeadL == size). They are rewritten against MainPoolState with
// pointer-width arithmetic, as in stadion. In this state listHeadL is the number of bytes handed out (not a pointer),
// listHeadR is the base, prev is the bump pointer.

/** Finalizes a block allocation, aligning its size. */
void MainPool_FinalizeAllocation(struct MemoryBlock* block) {
    struct MainPoolState* state = (struct MainPoolState*)block;
    s32 size = (s32)ALIGN16((uintptr_t)state->listHeadL + sizeof(struct MainPoolState));

    MainPool_Realloc(block, size);
    state->freeSpace = size - sizeof(struct MainPoolState);
}

/** Initializes a main pool state over a block of memory. */
void* MainPool_InitState(struct MainPoolState* block, s32 size) {
    void* data = (u8*)block + sizeof(struct MainPoolState); // PORT: header is 0x20 bytes with 64-bit pointers (was 0x10)

    block->listHeadL = 0;
    block->freeSpace = (s32)((size & ~3) - sizeof(struct MainPoolState)); // this doesnt match an ALIGN4 macro or whatnot
    block->listHeadR = data;
    block->prev = data;
    return block;
}

/** Allocates aligned memory from a pool state. */
void* MainPool_AllocAligned(struct MainPoolState* state, s32 arg1, s32 arg2) {
    uintptr_t prev = (uintptr_t)state->prev;
    uintptr_t aligned;
    uintptr_t ret = 0;
    s32 used;

    if (arg2 > 0) {
        aligned = (prev + (uintptr_t)arg2 - 1) & ~((uintptr_t)arg2 - 1);
    } else {
        aligned = prev;
    }
    used = (s32)(aligned - prev) + arg1;
    if (used > 0) {
        uintptr_t newUsed = (uintptr_t)state->listHeadL + (uintptr_t)used;
        if ((uintptr_t)(u32)state->freeSpace >= newUsed) {
            ret = aligned;
            state->prev = (void*)(prev + (uintptr_t)used);
            state->listHeadL = (void*)newUsed;
        }
    }
    return (void*)ret;
}

/** Resets a pool state, keeping the first part of its allocations. */
void MainPool_ResetStateToOffset(struct MainPoolState* state, s32 size) {
    if ((s32)state->freeSpace >= size) {
        state->listHeadL = (void*)(uintptr_t)size;
        state->prev = (void*)((uintptr_t)state->listHeadR + (uintptr_t)size);
    }
}

/** Frees every allocation of a pool state. */
void MainPool_ResetState(struct MainPoolState* state) {
    void* mem = (u8*)((uintptr_t)state + sizeof(struct MainPoolState));

    state->listHeadL = 0;
    state->listHeadR = mem;
    state->prev = mem;
}
