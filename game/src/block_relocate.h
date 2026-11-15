/* Declarations for src/block_relocate.c. */
#ifndef BLOCK_RELOCATE_H
#define BLOCK_RELOCATE_H

#include "global.h"

#include "libnumus/player.h"

// PORT: the header is the sound player's FX block (AudioFxBlock in libnumus/player.h)
typedef AudioFxBlock RelocHeader8003A710;

typedef struct Unk8003A7AC {
    /* 0x00 */ s32 size;
    /* 0x04 */ s32 flags;
    /* 0x08 */ s32 unk08[2];
    /* 0x10 */ u8 data[1];
} Unk8003A7AC;

// block_relocate.c
void Audio_RelocateSoundBankInPlace(SoundBank* arg0); // PORT: arg0 was s32
u8* Audio_RelocateSoundBankHeader(Unk8003A7AC* arg0); // PORT: returned s32

#endif // BLOCK_RELOCATE_H
