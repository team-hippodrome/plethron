/* Stadium Zero resident code. */

#include "global.h"
#include "m2c_macros.h"
#include "gb_player_launch.h"

uintptr_t func_80002C80(uintptr_t); // PORT: Util_ConvertAddrToVirtAddr
s32 SchedClient_WaitMessage(SchedClient*);
void Sched_AddClient(SchedClient*);
extern GbPlayerOutputBlock* D_800AAF44; // PORT: the output thread block (was s32)
extern M2C_UNK D_81106830;

/**
 * Second GB Player thread (id 9, started by GbPlayer_EmulatorThreadEntry): each retrace, unless the console
 * is about to reset, calls the emulator fragment's output routine (Stadium 1 name GbEmu_UpdateAudioOrBlankVideo).
 * @unverified: what the fragment routine outputs is from Stadium 1.
 */
void GbPlayer_OutputThreadEntry(void* arg0) { // PORT: OSThread entry signature (was s32)
    void (*temp_s2)(GbPlayerOutputBlock*);

    temp_s2 = (void (*)(GbPlayerOutputBlock*))func_80002C80((uintptr_t)&D_81106830);
    __osSetFpcCsr(0x01000F00);
    osCreateMesgQueue(&D_800AAF44->client.queue, D_800AAF44->client.mesgBuf, 1);
    Sched_AddClient(&D_800AAF44->client);
loop_1:
    SchedClient_WaitMessage(&D_800AAF44->client);
    if (D_800A9710.preNmiFrames /* PORT: was the raw N64 offset 0xA38 */ >= 21) {
        goto loop_1; // FAKE: goto loop
    }
    temp_s2(D_800AAF44);
    goto loop_1; // FAKE: goto loop
}

uintptr_t func_80002C80(uintptr_t); // PORT: Util_ConvertAddrToVirtAddr
s32 SchedClient_WaitMessage(SchedClient*);
void Sched_AddClient(SchedClient*);
extern GbPlayerEmuBlock* D_800AAF40; // PORT: the emulator thread block (was s32)
extern GbPlayerOutputBlock* D_800AAF44; // PORT: the output thread block (was s32)
extern M2C_UNK D_811008DC;
extern M2C_UNK D_81100C84;
/**
 * Main GB Player thread (id 8, started by GbPlayer_Start): runs the emulator fragment's init, starts the
 * output thread, then runs one emulator step per retrace until the console resets.
 */
void GbPlayer_EmulatorThreadEntry(void* arg0) { // PORT: OSThread entry signature (was s32)
    void (*temp_s1)(GbPlayerEmuBlock*);
    void (*temp_s2)(GbPlayerEmuBlock*);

    temp_s1 = (void (*)(GbPlayerEmuBlock*))func_80002C80((uintptr_t)&D_811008DC);
    temp_s2 = (void (*)(GbPlayerEmuBlock*))func_80002C80((uintptr_t)&D_81100C84);
    __osSetFpcCsr(0x01000F00);
    osCreateMesgQueue(&D_800AAF40->client.queue, D_800AAF40->client.mesgBuf, 1);
    Sched_AddClient(&D_800AAF40->client);
    temp_s1(D_800AAF40);
    osStartThread(&D_800AAF44->thread);
loop_1:
    SchedClient_WaitMessage(&D_800AAF40->client);
    if (D_800A9710.preNmiFrames /* PORT: was the raw N64 offset 0xA38 */ >= 21) {
        goto loop_1; // FAKE: goto loop
    }
    temp_s2(D_800AAF40);
    goto loop_1; // FAKE: goto loop
}

/*
 * GbPlayer_Start steps, in the order the code does them:
 *   1. push a memory-pool state tagged 'GBEM' and allocate two blocks:
 *      D_800AAF40 (0x2208 bytes, the emulator thread's data) and D_800AAF44
 *      (0x21E0 bytes, the audio thread's data);
 *   2. load the emulator fragment (ROM 0x80C00..0xDA9B0, linked at
 *      0x81100000) and open the font archive at ROM 0x2B2C80..0x2E0BE0,
 *      storing its files 0 and 1 at offsets 0x21CC and 0x21D0 of D_800AAF40;
 *   3. save the 8-byte input state *arg0 at offset 0x2200 of D_800AAF40
 *      (GbPlayer_Stop copies it back);
 *   4. create a one-slot message queue at D_800AAF40 + 0x21B4 with its
 *      buffer at +0x21B0, then create the audio thread (id 9, priority 17,
 *      GbPlayer_OutputThreadEntry) and the emulator thread (id 8, priority 15,
 *      GbPlayer_EmulatorThreadEntry), each with its stack top at its block + 0x21B0;
 *   5. record the previous GbPlayer_SetPollMask(0) value in D_800AAF48 and
 *      start the emulator thread.
 *
 * Same sequence as GbPlayer_Start in the Stadium 1 decomp; Zero's block
 * sizes and layout differ (0x2208 here, 0x2210 there).
 */
extern u8 D_81100000[];
extern u8 D_00080C00[];
extern u8 D_000DA9B0[];
extern u8 D_002B2C80[];
extern u8 D_002E0BE0[];
extern u8 D_800AAF48;
void MainPool_PushState(u32 tag);
void* MainPool_Alloc(u32 size, s32 side);
M2C_UNK Fragment_Load(s32 id, u8* romStart, u8* romEnd);
void* BinArchive_Open(u8* romStart, u8* romEnd, s32 arg2, s32 arg3);
void* BinArchive_GetFile(void* archive, s32 index);
u8 func_8000C610(u8);

/**
 * Starts a GB Player session: allocates both thread blocks, loads the emulator
 * fragment and fonts, copies the session parameters and starts the emulator thread.
 * Seen in play (2026-11-13) ran each time the GB Player started (main menu GB, Yes), twice in one
 * session.
 * @unverified Name was previously used only by callers of this address; the definition was still func_. Comment from Stadium 1.
 */
void GbPlayer_Start(GbPlayerLaunchParams* arg0) {
    void* temp_v0;

    MainPool_PushState('GBEM');
    D_800AAF40 = MainPool_Alloc(sizeof(GbPlayerEmuBlock), 0); // PORT: host sizes (0x2208, 0x21E0 on the N64)
    D_800AAF44 = MainPool_Alloc(sizeof(GbPlayerOutputBlock), 0);
    Fragment_Load(((((uintptr_t)D_81100000) & 0x0FF00000) >> 20) - 16, D_00080C00, D_000DA9B0);
    temp_v0 = BinArchive_Open(D_002B2C80, D_002E0BE0, 1, 1);
    D_800AAF40->font1Data = BinArchive_GetFile(temp_v0, 0);
    D_800AAF40->font2Data = BinArchive_GetFile(temp_v0, 1);
    D_800AAF40->params = *arg0;
    osCreateMesgQueue(&D_800AAF40->exitQueue, &D_800AAF40->exitMesg, 1);
    osCreateThread(&D_800AAF44->thread, 9, GbPlayer_OutputThreadEntry, 0,
                   D_800AAF44->stack + sizeof(D_800AAF44->stack), 17);
    osCreateThread(&D_800AAF40->thread, 8, GbPlayer_EmulatorThreadEntry, 0,
                   D_800AAF40->stack + sizeof(D_800AAF40->stack), 15);
    D_800AAF48 = func_8000C610(0);
    osStartThread(&D_800AAF40->thread);
}

extern GbPlayerEmuBlock* D_800AAF40; // PORT: was s32

/**
 * Blocks until the GB Player threads post their exit message and returns it. State_GbPlayer passes a
 * nonzero result to func_800268E4 with the active port.
 */
s32 GbPlayer_WaitForExit(void) {
    OSMesg sp1C; // PORT: OSMesg-sized (was s32)

    osRecvMesg(&D_800AAF40->exitQueue, &sp1C, 1);
    return (s32)(intptr_t)sp1C;
}

extern u8 D_800AAF48;
void Display_ApplyPendingVideoMode(void);
void func_80004EF0(SchedClient*); // PORT: Sched_RemoveClient
void func_80002620(s32);

/**
 * Ends the session: copies the session parameters back (button bindings), restores
 * the pak poll mask and destroys both threads.
 * Seen in play (2026-11-13) ran when quitting the GB Player (C-Up menu, Quit game, Yes) back to the
 * main menu.
 * @unverified Name matches Stadium 1. Comment from Stadium 1.
 */
void GbPlayer_Stop(GbPlayerLaunchParams* arg0) {
    *arg0 = D_800AAF40->params;
    func_8000C610(D_800AAF48);
    osViBlack(1);
    Display_ApplyPendingVideoMode();
    func_80004EF0(&D_800AAF40->client);
    func_80004EF0(&D_800AAF44->client);
    osDestroyThread(&D_800AAF40->thread);
    osDestroyThread(&D_800AAF44->thread);
    func_80002620('GBEM');
}
