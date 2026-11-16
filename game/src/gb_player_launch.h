/* Declarations for src/gb_player_launch.c. */
#ifndef GB_PLAYER_LAUNCH_H
#define GB_PLAYER_LAUNCH_H

#include "global.h"
#include "scheduler.h"

// PORT: the GB Player's two thread blocks, raw MainPool blocks (0x2208 and 0x21E0 bytes) addressed by N64 offsets in
// the original code; named here so the host layout has room for its pointer-sized fields. Offsets are the N64 ones.
typedef struct GbPlayerEmuBlock {
    /* 0x0000 */ OSThread thread; // the emulator thread (GbPlayer_EmulatorThreadEntry)
    /* 0x01B0 */ u8 stack[0x2000];
    /* 0x21B0 */ OSMesg exitMesg; // buffer of exitQueue
    /* 0x21B4 */ OSMesgQueue exitQueue; // GbEmu_RunFrame sends the exit code, GbPlayer_WaitForExit receives it
    /* 0x21CC */ void* font1Data; // file 0 of the GB Player archive (GbEmu_LoadCartridge: D_81156598)
    /* 0x21D0 */ void* font2Data; // file 1 of the GB Player archive (GbEmu_LoadCartridge: D_8115659C)
    /* 0x21D4 */ SchedClient client; // retrace client of the emulator thread
    /* 0x2200 */ GbPlayerLaunchParams params; // copied in by GbPlayer_Start, back out by GbPlayer_Stop
} GbPlayerEmuBlock; // size = 0x2208

typedef struct GbPlayerOutputBlock {
    /* 0x0000 */ OSThread thread; // the output thread (GbPlayer_OutputThreadEntry)
    /* 0x01B0 */ u8 stack[0x2000];
    /* 0x21B0 */ SchedClient client; // retrace client of the output thread
} GbPlayerOutputBlock; // size = 0x21DC (0x21E0 allocated)

// gb_player_launch.c
void GbPlayer_Start(GbPlayerLaunchParams* arg0);
void GbPlayer_Stop(GbPlayerLaunchParams* arg0);

#endif // GB_PLAYER_LAUNCH_H
