/* Stadium Zero fragment000_80C00. */

#include "global.h"
#include "audio_and_gb_emulator.h"

/**
 * Puts the emulated Game Boy CPU in its post-boot-ROM state: registers (SP 0xFFFE, PC 0x0100), the
 * sound registers NR52 0x8F, NR50 0x77 and NR51 0xFF through GbApu_QueueRegisterWrite, and zeroes a
 * block of emulator memory.
 * Seen in play (2026-11-14) ran once when the GB Player started with JP Green, from
 * GbEmu_LoadCartridge, before Green's boot logo.
 */
#ifdef NON_MATCHING
// PORT: the original is hand-written MIPS. The host runs a C core (src/port/gb_core.c); this is its side that knows
// the GB Player's types. As the original: the machine and the emulator memory reset, PC 0x100, and the frame around the
// screen set to Zero's default (frame set 0, whose image D_811565F0 holds its palette indices doubled) with its
// 22-colour palette.
#include "StadionPort.h"

extern u8 D_811565F0[];
extern u8 D_811645F0[];
extern u8 D_811725F0[];
extern u8 D_81135178[];
extern u8 D_81135378[];
extern u8 D_8113556C[];
extern u8 D_811565A5;
extern u8 D_81154C4F;

void GbCpu_Reset(GbEmuState* emu) {
    s32 i;

    Port_GbCoreReset(emu->mem, emu->rom, emu->romSize, emu->sram, emu->sramSize);
    emu->gbPc = 0x100;
    emu->unk_06 = 0;
    bzero(emu->unk_44, 0x800);
    bzero(emu->unk_44 + 0xE800, 0x800);
    for (i = 0; i < 0xE000; i++) {
        emu->unk_44[0x800 + i] = D_811565F0[i] >> 1;
    }
    bzero(emu->mem + 0x14800, 0x80);
    bcopy(D_81135178, emu->mem + 0x14800, 0x16 * 2);
}
#else
#pragma GLOBAL_ASM("asm/jp/nonmatchings/fragments/fragment000_80C00/func_81117B20.s")
#endif

/**
 * The Game Boy CPU interpreter: runs the emulated LR35902 until it has to return to
 * GbEmu_UpdateFrame. Bank switches call GbPak_RequestBankLoadCurrentPort from inside it.
 * Seen in play (2026-11-14) ran from GbEmu_UpdateFrame while Green booted and played its intro.
 */
#ifdef NON_MATCHING
// PORT: the host side of the original (see GbCpu_Reset). A border the game sent (PCT_TRN) lands in the back image
// unk_48 and starts the frame change, as at 0x811184F4-0x811187C8: emulator byte 0x1019D = 1 fades the frame out
// (0x101A1 = 0x40 | n, the RSP task's fade), swaps unk_44/unk_48, takes the palette from +0x27D80 into +0x14800, and
// fades back in (0x1019D = 2). GbEmu_UpdateFrame starts the same change when Z picks another frame set. The first
// border also becomes frame set 0 (0x101C9 is set, which lets Z switch sets) and lets the status bar slide down
// (D_811565A5).
u8 GbCpu_Run(GbEmuState* emu, s32 arg1) {
    u8* mem = emu->mem;
    u8* image;
    u8* src;
    u8* pal;

    (void)arg1;
    if (mem[0x1019D] == 0 && Port_GbCoreTakeBorder(emu->unk_48 + 0x800, mem + 0x27D80)) {
        mem[0x1019D] = 1;
    }
    if (mem[0x1019D] == 2) {
        mem[0x101A1]--;
        if ((mem[0x101A1] & 0x3F) == 0) {
            mem[0x1019D] = 0;
        }
    } else if (mem[0x1019D] != 0) {
        mem[0x101A1] = (mem[0x101A1] + 1) | 0x40;
        if (mem[0x101A1] >= 0x7C) {
            image = emu->unk_44;
            emu->unk_44 = emu->unk_48;
            emu->unk_48 = image;
            bcopy(mem + 0x27D80, mem + 0x14800, 0x200);
            if (D_811565A5 == 0x20) {
                D_811565A5 = 0x1F;
            }
            if (mem[0x101C9] == 0) {
                mem[0x101C9] = 0xFF;
                bcopy(emu->unk_44 + 0x800, D_811565F0, 0xE000);
                bcopy(mem + 0x14800, D_81135178, 0x200);
                if (D_81154C4F != 0) {
                    if (D_81154C4F == 1) {
                        src = D_811645F0;
                        pal = D_81135378;
                    } else {
                        src = D_811725F0;
                        pal = D_8113556C;
                    }
                    bcopy(src, emu->unk_44 + 0x800, 0xE000);
                    bcopy(pal, mem + 0x14800, 0x200);
                }
            }
            mem[0x1019D] = 2;
        }
    } else {
        mem[0x101A1] = 0;
    }
    return Port_GbCoreRun(&emu->gbPc, (u16)emu->buttons) != 0;
}
#else
#pragma GLOBAL_ASM("asm/jp/nonmatchings/fragments/fragment000_80C00/func_81118018.s")
#endif
