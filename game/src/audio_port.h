#ifndef AUDIO_PORT_H
#define AUDIO_PORT_H

// PORT: helpers for Stadium Zero's sound player data on a 64-bit little-endian host.
//
// Sound banks, songs and FX blocks are loaded from ROM as big-endian blobs whose pointer fields are 32-bit offsets
// the game relocates in place into absolute addresses. On the host the headers are byte-swapped on first use,
// offsets stay offsets inside the blob (readers add the blob's address, PORT_BLOB_AT in libnumus/player.h), and the
// parts the synthesizer reads through real pointers (wave tables) are rebuilt as host structures (audio_port.c).
// Adapted from team-hippodrome/stadion game/src/port_audio.h.

#include "global.h"
#include <PR/libaudio.h>

// Sound bank ("N64 PtrTablesV2", SoundBank in sound.h): the host-side view built by PortA_RelocateSoundBank.
typedef struct PortSoundBank {
    s32 count;
    u8* basenote;            // count bytes in the blob
    f32* detune;             // count floats in the blob (converted in place by the game)
    ALWaveTable** wave_list; // host wave tables (loops and ADPCM books stay in the blob, byte-swapped)
} PortSoundBank;
PortSoundBank* PortA_RelocateSoundBank(void* blob, uintptr_t waveRomBase); // NULL when already relocated
PortSoundBank* PortA_Bank(const void* blob);

#endif
