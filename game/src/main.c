/* Stadium Zero resident code. */

#include "global.h"
#include "m2c_macros.h"
#include "main.h"

M2C_UNK Storage_StartThread();                            /* extern */
M2C_UNK DpIntro_SetViMode(M2C_UNK, M2C_UNK);            /* extern */
M2C_UNK func_80001868();                            /* extern */
M2C_UNK func_80004E34();                            /* extern */
M2C_UNK func_8000803C();                            /* extern */
M2C_UNK func_8005BD80(M2C_UNK);                     /* extern */
void Audio_ThreadEntry(s32 arg0);
extern M2C_UNK D_800269A0;
extern M2C_UNK D_800809B0;
extern M2C_UNK D_80082B60;

/**
 * Idle thread: sets up the VI mode, interrupts and subsystems, starts thread 4,
 * then idles. Name from Stadium 1's Idle_ThreadEntry.
 * Seen in play (2026-11-13) ran at boot (console reset, then N64 logo, opening, title and main
 * menu).
 * @unverified Name and comment guessed from the code.
 */
void Idle_ThreadEntry(s32 arg0) {
    func_8005BD80(0xFE);
    DpIntro_SetViMode(0, 1);
    osViBlack(1);
    func_8000803C();
    Storage_StartThread();
    func_80004E34();
    osCreateThread(&D_80082B60, 4, &Audio_ThreadEntry, 0, &D_80084D10, 0x50);
    osStartThread(&D_80082B60);
    func_80001868();
    osCreateThread(&D_800809B0, 6, &D_800269A0, 0, &D_80082B60, 0x14);
    osStartThread(&D_800809B0);
    osSetThreadPri(0, 0);
loop_1:
    goto loop_1; // FAKE: m2c goto loop, an idle forever loop
}

M2C_UNK Memory_InitPools();                            /* extern */
M2C_UNK func_800093C0(M2C_UNK);                     /* extern */
extern M2C_UNK D_80080400;
extern M2C_UNK D_800809B0;

/**
 * Boot entry: initializes the OS, then creates and starts the idle thread
 * (id 1, Idle_ThreadEntry). Name from Stadium 1's Main.
 * Seen in play (2026-11-13) ran at boot (console reset, then N64 logo, opening, title and main
 * menu).
 * @unverified Name and comment guessed from the code.
 */
void Main(void) {
    osInitialize();
    func_800093C0(0);
    Memory_InitPools();
    osCreateThread(&D_80080400, 1, &Idle_ThreadEntry, 0, &D_800809B0, 0x64);
    osStartThread(&D_80080400);
}
