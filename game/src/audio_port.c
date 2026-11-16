// PORT: Stadium Zero's sound player data on a 64-bit little-endian host (see audio_port.h). Adapted from
// team-hippodrome/stadion game/src/port_audio.c (Stadium 1 uses the same "N64 PtrTablesV2" sound banks).
#include "audio_port.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---- sound banks ------------------------------------------------------------------------------------------------
// Blob layout (big-endian): char name[16]; u32 flags; char wbk[12]; s32 count; u32 basenote, detune, wave_list
// (offsets from the blob); wave_list = count u32 offsets of 20-byte wave tables {u32 base (offset into the wave ROM
// area unless 0xFFxxxxxx), s32 len, u8 type, u8 flags, u16 pad, u32 loop, u32 book (offsets from the blob)}.
// A loop is an ALADPCMloop (ADPCM) or ALRawLoop (raw 16-bit); a book is s32 order, s32 npredictors, s16 book[].
#define SB_FLAGS 0x10
#define SB_COUNT 0x20
#define SB_BASENOTE 0x24
#define SB_DETUNE 0x28
#define SB_WAVELIST 0x2C

typedef struct BankCache {
    const void* blob;
    u32 hash;
    PortSoundBank* bank;
    struct BankCache* next;
} BankCache;
static BankCache* sBankCache;   // every bank built (reused when the same data is loaded at the same place again)
static BankCache* sBankCurrent; // blob -> bank now relocated there (one entry per blob address)

static u32 HashBytes(u32 h, const void* p, u32 n) {
    const u8* b = (const u8*)p;
    u32 i;
    for (i = 0; i < n; i++) {
        h = (h ^ b[i]) * 16777619u;
    }
    return h;
}

static int MarkSwapped(u32* list, u32* n, u32 max, u32 off) {
    u32 i;
    for (i = 0; i < *n; i++) {
        if (list[i] == off) {
            return 0;
        }
    }
    if (*n < max) {
        list[(*n)++] = off;
    }
    return 1;
}

PortSoundBank* PortA_Bank(const void* blob) {
    BankCache* c;
    for (c = sBankCurrent; c != NULL; c = c->next) {
        if (c->blob == blob) {
            return c->bank;
        }
    }
    return NULL;
}

static void SetCurrent(const void* blob, PortSoundBank* bank) {
    BankCache* c;
    for (c = sBankCurrent; c != NULL; c = c->next) {
        if (c->blob == blob) {
            c->bank = bank;
            return;
        }
    }
    c = calloc(1, sizeof(BankCache));
    c->blob = blob;
    c->bank = bank;
    c->next = sBankCurrent;
    sBankCurrent = c;
}

PortSoundBank* PortA_RelocateSoundBank(void* blobv, uintptr_t waveRomBase) {
    u8* blob = (u8*)blobv;
    u32* flags = (u32*)(blob + SB_FLAGS);
    s32 count, i;
    u32 waveListOff, hash;
    u32* swapped;
    u32 numSwapped = 0;
    BankCache* c;
    PortSoundBank* bank = NULL;

    if (*flags & 1) {
        return NULL; // already relocated (host order)
    }
    *flags = Port_ReadBE32(flags) | 1;
    count = (s32)Port_ReadBE32(blob + SB_COUNT);
    waveListOff = Port_ReadBE32(blob + SB_WAVELIST);
    if (count < 0 || count > 0x1000) {
        fprintf(stderr, "[audio] bad sound bank %p (count %d)\n", (void*)blob, (int)count);
        return NULL;
    }

    hash = HashBytes(2166136261u, &waveRomBase, sizeof(waveRomBase));
    hash = HashBytes(hash, blob + SB_COUNT, 16);
    for (i = 0; i < count; i++) {
        u32 off = Port_ReadBE32(blob + waveListOff + 4 * i);
        hash = HashBytes(hash, &off, 4);
        if (off != 0) {
            hash = HashBytes(hash, blob + off, 20);
        }
    }
    for (c = sBankCache; c != NULL; c = c->next) {
        if (c->blob == blob && c->hash == hash) {
            bank = c->bank;
            break;
        }
    }

    if (bank == NULL) {
        bank = calloc(1, sizeof(PortSoundBank));
        bank->count = count;
        bank->basenote = blob + Port_ReadBE32(blob + SB_BASENOTE);
        bank->detune = (f32*)(blob + Port_ReadBE32(blob + SB_DETUNE));
        bank->wave_list = calloc(count > 0 ? count : 1, sizeof(ALWaveTable*));
        for (i = 0; i < count; i++) {
            u32 off = Port_ReadBE32(blob + waveListOff + 4 * i);
            const u8* w = blob + off;
            s32 j;
            ALWaveTable* wt = NULL;
            if (off == 0) {
                continue;
            }
            for (j = 0; j < i; j++) { // wave tables shared between entries stay shared
                if (Port_ReadBE32(blob + waveListOff + 4 * j) == off) {
                    wt = bank->wave_list[j];
                    break;
                }
            }
            if (wt == NULL) {
                u32 base = Port_ReadBE32(w + 0);
                u32 loop = Port_ReadBE32(w + 12);
                u32 book = Port_ReadBE32(w + 16);
                wt = calloc(1, sizeof(ALWaveTable));
                wt->base = (u8*)(((base & 0xFF000000) != 0xFF000000) ? (uintptr_t)base + waveRomBase : (uintptr_t)base);
                wt->len = (s32)Port_ReadBE32(w + 4);
                wt->type = w[8];
                wt->flags = 1;
                wt->waveInfo.adpcmWave.loop = loop != 0 ? (ALADPCMloop*)(blob + loop) : NULL;
                wt->waveInfo.adpcmWave.book =
                    (wt->type == AL_ADPCM_WAVE && book != 0) ? (ALADPCMBook*)(blob + book) : NULL;
                if (w[9] != 0) {
                    fprintf(stderr, "[audio] bank %p wave %d already flagged (0x%x)\n", (void*)blob, (int)i, w[9]);
                }
            }
            bank->wave_list[i] = wt;
        }
        c = calloc(1, sizeof(BankCache));
        c->blob = blob;
        c->hash = hash;
        c->bank = bank;
        c->next = sBankCache;
        sBankCache = c;
    }

    // loops and books live in the (freshly loaded, big-endian) blob: byte-swap them in place
    swapped = calloc(2 * (count > 0 ? count : 1), sizeof(u32));
    for (i = 0; i < count; i++) {
        u32 off = Port_ReadBE32(blob + waveListOff + 4 * i);
        const u8* w = blob + off;
        u32 loop, book;
        if (off == 0) {
            continue;
        }
        loop = Port_ReadBE32(w + 12);
        book = Port_ReadBE32(w + 16);
        if (loop != 0 && MarkSwapped(swapped, &numSwapped, 2 * count, loop)) {
            Port_SwapWords(blob + loop, 3);
            if (w[8] == AL_ADPCM_WAVE) {
                Port_SwapHalves(blob + loop + 12, 16);
            }
        }
        if (w[8] == AL_ADPCM_WAVE && book != 0 && MarkSwapped(swapped, &numSwapped, 2 * count, book)) {
            ALADPCMBook* b = (ALADPCMBook*)(blob + book);
            Port_SwapWords(b, 2);
            if (b->order > 0 && b->order <= 16 && b->npredictors > 0 && b->npredictors <= 16) {
                Port_SwapHalves(b->book, (u32)(b->order * b->npredictors * 8));
            } else {
                fprintf(stderr, "[audio] bank %p: odd ADPCM book %d x %d\n", (void*)blob, (int)b->order,
                        (int)b->npredictors);
            }
        }
    }
    free(swapped);

    SetCurrent(blob, bank);
    return bank;
}
