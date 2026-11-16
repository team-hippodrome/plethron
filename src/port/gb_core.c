// Native Game Boy / Super Game Boy core for Stadium Zero's GB Player.
//
// On the N64 the GB Player runs a hand-written MIPS emulator (fragment000: GbCpu_Reset 0x81117B20, GbCpu_Run 0x81118018)
// and two custom RSP microcodes that draw the Game Boy picture, the frame around it and the status bar into the frame
// buffer. Neither can run on the host. This file is the C core behind those two entry points (the glue that knows the
// game's types is in game/src/fragments/audio_and_gb_emulator/gb_apu_3.c) and the frame composition
// (Port_GbPlayerPresent). The decompiled GB Player code around it still loads the cartridge through the Transfer Pak,
// streams the ROM banks in, writes the save RAM back, draws the loading banner, the menus and the status bar.
//
// The core keeps the Game Boy address space where the original keeps it, in the emulator memory block (GbEmuState.mem,
// 0x27F80 bytes): VRAM at +0x8000, work RAM at +0xC000, OAM at +0xFE00, the I/O registers at +0xFF00 and HRAM / IE at
// +0xFF80. The GB Player code reads them there (LCDC, BGP, SCY), next to the emulator's own variables at +0x10100 and up
// (speed, sound time base) and the tables it shares with the Transfer Pak thread (bank and save-RAM block states).
//
// The emulated machine is a Super Game Boy, as on the N64: DMG CPU and LCD, SGB packets (palettes, attribute maps,
// masking, the border). Cartridges flagged for the Game Boy Color (Western Yellow) run in their SGB mode too: Zero's GB
// Player has no Game Boy Color emulation. Mappers: MBC1 (Japanese Red/Green/Blue), MBC3 (Pikachu, English Red/Blue),
// MBC5 (the other Western versions); Zero itself only knew the first two, through simplified bank handlers.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int8_t s8;
typedef int32_t s32;

// ---- the GB Player code this core calls ----
extern volatile u8 D_81156475; // the Transfer Pak thread is busy with a request
extern u8 D_811565A0;          // the GB Player's controller port
s32 GbPak_RequestBankLoad(s32 chan, s32 bank);
void GbApu_QueueRegisterWrite(u16 reg, u8 value, u16 time);
u8 GbMem_ReadIoRegister(u16 reg);

// ---- offsets in the emulator memory block (GbEmuState.mem) ----
#define MEM_TIME_BASE 0x10100   // s32: sound time of this Game Boy frame (GbEmu_UpdateFrame)
#define MEM_SPEED 0x101B9       // u8: Game Boy frames per N64 frame (1, 2, 4)
#define MEM_GB_PALETTES 0x12D00 // 4 palettes of 4 RGBA5551 colours (big-endian), GbEmu_ClearPalettes blanks them
#define MEM_BANK_STATE 0x16B20  // u8[0x40]: ROM bank state, 0 = loaded, 1 = not requested, 2/3 = queued
#define MEM_SRAM_STATE 0x16B60  // u8[0x20]: save RAM bank state, as above
#define MEM_SRAM_BLOCKS 0x16B80 // u8[0x80]: save RAM 256-byte blocks, 0 = written by the game (to be written back)

// ---- machine state ----
static struct {
    // CPU
    u8 a, f, b, c, d, e, h, l;
    u16 sp, pc;
    int ime, imeDelay, halted;
    u32 frameCount;
    // memory
    u8* mem;
    u8* io; // mem + 0xFF00
    const u8* rom;
    u32 romSize;
    u8* sram;
    u32 sramSize;
    int mbc;
    u32 romBank, ramBank;
    int ramEnable, mbc1Mode;
    int error; // a bank could not be loaded: GbCpu_Run stops with it
    // timer / serial
    u32 divCounter, timaCounter, serialCounter;
    // LCD
    u32 dot;
    int windowLine;
    u8 shades[144][160]; // 0-3 after BGP/OBP
    u8 bgIndex[160];     // raw BG colour index of the current line (sprite priority)
    // joypad / SGB
    u16 pad;
    int sgbPlayers, sgbPlayer;
    u8 lastP1;
    int pktState, pktBits, pktCount, pktTotal;
    u8 pkt[7 * 16];
    u16 sgbSysPal[512][4]; // from PAL_TRN (BGR555)
    u8 sgbAttr[18][20];    // palette per 8x8 cell
    u8 sgbAttrFiles[45][90];
    int sgbMask;
    u8 sgbBorderTiles[256 * 32];
    u8 frozen[144][160];
    int trnCmd, trnFrames;
    u8 trnArg;
    // the border built by the last PCT_TRN, waiting for the GB Player code (Port_GbCoreTakeBorder)
    int borderReady;
    u8 borderImage[256 * 224]; // palette index per pixel: colour | (palette << 4)
    u16 borderPal[256];        // RGBA5551
    u32 frameCycles; // CPU cycles since the frame started
    int frameDone;
    int initialised;
} G;

// ---- helpers ----
#define FZ 0x80
#define FN 0x40
#define FH 0x20
#define FC 0x10

static void SgbWriteP1(u8 v);

static void RequestInterrupt(int bit) {
    G.io[0x0F] |= (u8)(1 << bit);
}

static u16 Rgba5551(u16 bgr) {
    u16 r = bgr & 31, g = (bgr >> 5) & 31, b = (bgr >> 10) & 31;
    return (u16)((r << 11) | (g << 6) | (b << 1) | 1);
}

static void PutBe16(u8* p, u16 v) {
    p[0] = (u8)(v >> 8);
    p[1] = (u8)v;
}

static u16 GetBe16(const u8* p) {
    return (u16)((p[0] << 8) | p[1]);
}

// SGB palette p, colour c, in the GB Player's palette block (where the original keeps it and GbEmu_ClearPalettes
// clears it)
static void SetGbColor(int p, int c, u16 bgr) {
    PutBe16(G.mem + MEM_GB_PALETTES + p * 8 + c * 2, Rgba5551(bgr));
}

// ---- Transfer Pak streaming ----
// Red, Green and Blue start before their ROM is in: GbEmu_UpdateFrame streams the banks in the background
// (GbPak_QueueNextBankLoad). A bank the game switches to before it has arrived is requested ahead of the others and
// waited for, as the original bank handlers do (0x8111B780: state 1 asks for it, 2/3 waits while the Transfer Pak
// thread is busy). Banks of a fully copied cartridge (Pikachu, Western carts) are all 0.
static void EnsureLoaded(int offset, int bank, int requestId) {
    volatile u8* state = G.mem + offset + bank;
    while (*state != 0 && !G.error) {
        if (*state == 1) {
            if (GbPak_RequestBankLoad(D_811565A0, requestId) != 0) {
                G.error = 1;
            }
            return;
        }
        if (!D_81156475) {
            G.error = 1;
            return;
        }
    }
}

static void SetRomBank(u32 bank) {
    u32 banks = G.romSize >> 14;
    bank %= banks ? banks : 1;
    G.romBank = bank;
    EnsureLoaded(MEM_BANK_STATE, (int)bank, (int)bank);
}

static void SetRamBank(u32 bank) {
    u32 banks = G.sramSize >> 13;
    bank %= banks ? banks : 1;
    G.ramBank = bank;
    EnsureLoaded(MEM_SRAM_STATE, (int)bank, (int)bank | 0x80);
}

// ---- memory ----
static u8 ReadIo(u16 a);
static void WriteIo(u16 a, u8 v);

static u8 Read(u16 a) {
    if (a < 0x4000) {
        return G.rom[a];
    }
    if (a < 0x8000) {
        return G.rom[G.romBank * 0x4000 + (a - 0x4000)];
    }
    if (a < 0xA000) {
        return G.mem[a];
    }
    if (a < 0xC000) {
        if (!G.ramEnable || G.sramSize == 0) {
            return 0xFF;
        }
        return G.sram[G.ramBank * 0x2000 + (a - 0xA000)];
    }
    if (a < 0xE000) {
        return G.mem[a];
    }
    if (a < 0xFE00) {
        return G.mem[a - 0x2000];
    }
    if (a < 0xFEA0) {
        return G.mem[a];
    }
    if (a < 0xFF00) {
        return 0xFF;
    }
    if (a < 0xFF80) {
        return ReadIo(a);
    }
    return G.mem[a];
}

// The cartridge type. Not known at reset: a streamed cartridge starts before its bank 0 has arrived (the boot stub runs
// meanwhile), so it is read at the first bank switch, when the game runs from bank 0.
static void DetectMbc(void) {
    switch (G.rom[0x147]) {
        case 0x01: case 0x02: case 0x03: G.mbc = 1; break;
        case 0x0F: case 0x10: case 0x11: case 0x12: case 0x13: G.mbc = 3; break;
        case 0x19: case 0x1A: case 0x1B: case 0x1C: case 0x1D: case 0x1E: G.mbc = 5; break;
        default: G.mbc = 0; break;
    }
}

static void Write(u16 a, u8 v) {
    if (a < 0x8000) {
        if (G.mbc < 0) {
            DetectMbc();
        }
        if (a < 0x2000) {
            G.ramEnable = (v & 0x0F) == 0x0A;
        } else if (a < 0x4000) {
            if (G.mbc == 5) {
                if (a < 0x3000) {
                    SetRomBank((G.romBank & 0x100) | v);
                } else {
                    SetRomBank((G.romBank & 0xFF) | ((u32)(v & 1) << 8));
                }
            } else if (G.mbc == 3) {
                SetRomBank((v & 0x7F) ? (v & 0x7F) : 1);
            } else if (G.mbc == 1) {
                SetRomBank((G.romBank & 0x60) | ((v & 0x1F) ? (v & 0x1F) : 1));
            }
        } else if (a < 0x6000) {
            if (G.mbc == 5) {
                SetRamBank(v & 0x0F);
            } else if (G.mbc == 3) {
                if (v < 4) {
                    SetRamBank(v); // 8-0xC would select the clock registers, which no Pokémon cartridge has
                }
            } else if (G.mbc == 1) {
                if (G.mbc1Mode) {
                    SetRamBank(v & 3);
                } else {
                    SetRomBank((G.romBank & 0x1F) | ((u32)(v & 3) << 5));
                }
            }
        } else if (G.mbc == 1) {
            G.mbc1Mode = v & 1;
        }
        return;
    }
    if (a < 0xA000) {
        G.mem[a] = v;
    } else if (a < 0xC000) {
        if (G.ramEnable && G.sramSize != 0) {
            u32 off = G.ramBank * 0x2000 + (a - 0xA000);
            G.sram[off] = v;
            // as the original (0x8111B90C): the block goes back to the cartridge (GbPak_WriteBackDirtySramBlock)
            G.mem[MEM_SRAM_BLOCKS + (off >> 8)] = 0;
        }
    } else if (a < 0xE000) {
        G.mem[a] = v;
    } else if (a < 0xFE00) {
        G.mem[a - 0x2000] = v;
    } else if (a < 0xFEA0) {
        G.mem[a] = v;
    } else if (a < 0xFF00) {
    } else if (a < 0xFF80) {
        WriteIo(a, v);
    } else {
        G.mem[a] = v;
    }
}

static u8 ReadIo(u16 a) {
    u8 r = (u8)(a & 0x7F);
    switch (r) {
        case 0x00: { // P1
            u8 sel = G.io[0x00] & 0x30;
            u8 low = 0x0F;
            if ((sel & 0x30) == 0x30) {
                // nothing selected: with SGB multiplayer the low bits give the current controller id
                low = G.sgbPlayers > 1 ? (u8)(0x0F - G.sgbPlayer) : 0x0F;
            } else {
                u16 p = G.sgbPlayer == 0 ? G.pad : 0;
                if (!(sel & 0x10)) { // directions
                    if (p & 0x0100) low &= ~0x01; // right
                    if (p & 0x0200) low &= ~0x02; // left
                    if (p & 0x0800) low &= ~0x04; // up
                    if (p & 0x0400) low &= ~0x08; // down
                }
                if (!(sel & 0x20)) { // buttons
                    if (p & 0x8000) low &= ~0x01; // A
                    if (p & 0x4000) low &= ~0x02; // B
                    if (p & 0x2000) low &= ~0x04; // select
                    if (p & 0x1000) low &= ~0x08; // start
                }
            }
            return (u8)(0xC0 | sel | low);
        }
        case 0x02:
            return G.io[0x02] | 0x7E;
        case 0x04:
            return (u8)(G.divCounter >> 8);
        case 0x07:
            return G.io[0x07] | 0xF8;
        case 0x26:
            return GbMem_ReadIoRegister(0xFF26) | 0x70;
        case 0x0F:
            return G.io[0x0F] | 0xE0;
        case 0x41:
            return G.io[0x41] | 0x80;
        default:
            if (r >= 0x10 && r < 0x40) {
                static const u8 mask[0x30] = {
                    0x80, 0x3F, 0x00, 0xFF, 0xBF, 0xFF, 0x3F, 0x00, 0xFF, 0xBF, 0x7F, 0xFF, 0x9F, 0xFF, 0xBF, 0xFF,
                    0xFF, 0x00, 0x00, 0xBF, 0x00, 0x00, 0x70, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                };
                return G.io[r] | (r < 0x30 ? mask[r - 0x10] : 0);
            }
            if (r >= 0x4C) {
                return 0xFF;
            }
            return G.io[r];
    }
}

static void WriteIo(u16 a, u8 v) {
    u8 r = (u8)(a & 0x7F);
    switch (r) {
        case 0x00:
            G.io[0x00] = v & 0x30;
            SgbWriteP1(v & 0x30);
            return;
        case 0x02:
            G.io[0x02] = v;
            G.serialCounter = 0;
            return;
        case 0x04:
            G.divCounter = 0;
            return;
        case 0x0F:
            G.io[0x0F] = v & 0x1F;
            return;
        case 0x41:
            G.io[0x41] = (G.io[0x41] & 0x07) | (v & 0x78);
            return;
        case 0x44:
            return; // LY is read-only
        case 0x46: { // OAM DMA (instant)
            u16 src = (u16)(v << 8);
            for (int i = 0; i < 0xA0; i++) {
                G.mem[0xFE00 + i] = Read((u16)(src + i));
            }
            G.io[0x46] = v;
            return;
        }
        case 0x40:
            if ((G.io[0x40] & 0x80) && !(v & 0x80)) { // LCD off
                G.io[0x44] = 0;
                G.dot = 0;
                G.io[0x41] &= ~3;
            }
            G.io[0x40] = v;
            return;
        default:
            G.io[r] = v;
            if (r >= 0x10 && r < 0x40) {
                // as the original (0x8111BB30): the time is the frame's sound time base (0x10100, advanced by
                // GbEmu_UpdateFrame for each Game Boy frame of this N64 frame) plus the cycles so far, in units of
                // 1/112 line, divided by the speed so that the mixer plays a fast-forwarded frame in one buffer
                u32 speed = G.mem[MEM_SPEED] ? G.mem[MEM_SPEED] : 1;
                u32 base;
                memcpy(&base, G.mem + MEM_TIME_BASE, 4);
                GbApu_QueueRegisterWrite((u16)(0xFF00 | r), v, (u16)((base + G.frameCycles * 112 / 456) / speed));
            }
            return;
    }
}

// ---- Super Game Boy ----
static u16 Rd16(const u8* p) {
    return (u16)(p[0] | (p[1] << 8));
}

// The 4 KB a *_TRN command transfers: the tiles the BG map shows (20x13 cells, in order), as the SGB samples the
// picture of the next frame
static void SgbVramTransfer(u8* out, int size) {
    const u8* vram = G.mem + 0x8000;
    u8 lcdc = G.io[0x40];
    u16 map = (lcdc & 0x08) ? 0x1C00 : 0x1800;
    int n = 0;
    for (int ty = 0; ty < 18 && n < size; ty++) {
        for (int tx = 0; tx < 20 && n < size; tx++) {
            u8 t = vram[map + ty * 32 + tx];
            u16 base = (lcdc & 0x10) ? (u16)(t * 16) : (u16)(0x1000 + (s8)t * 16);
            for (int k = 0; k < 16 && n < size; k++) {
                out[n++] = vram[(base + k) & 0x1FFF];
            }
        }
    }
}

static void SgbSetPal(int p0, int p1, const u8* d) {
    u16 c0 = Rd16(d + 1);
    for (int p = 0; p < 4; p++) {
        SetGbColor(p, 0, c0);
    }
    for (int i = 0; i < 3; i++) {
        SetGbColor(p0, 1 + i, Rd16(d + 3 + i * 2));
        SetGbColor(p1, 1 + i, Rd16(d + 9 + i * 2));
    }
}

static void SgbAttrBlk(const u8* d) {
    int sets = d[1];
    const u8* s = d + 2;
    for (int i = 0; i < sets && i < 18; i++, s += 6) {
        int ctrl = s[0];
        int pin = s[1] & 3, pline = (s[1] >> 2) & 3, pout = (s[1] >> 4) & 3;
        int x1 = s[2] & 31, y1 = s[3] & 31, x2 = s[4] & 31, y2 = s[5] & 31;
        if ((ctrl & 7) == 1) {
            pline = pin; // only "inside" given: the border gets it too
        }
        if ((ctrl & 7) == 4) {
            pline = pout;
        }
        for (int y = 0; y < 18; y++) {
            for (int x = 0; x < 20; x++) {
                int inside = x > x1 && x < x2 && y > y1 && y < y2;
                int onLine = !inside && x >= x1 && x <= x2 && y >= y1 && y <= y2;
                if (inside && (ctrl & 1)) {
                    G.sgbAttr[y][x] = (u8)pin;
                } else if (onLine && (ctrl & 2 || (ctrl & 7) == 1 || (ctrl & 7) == 4)) {
                    G.sgbAttr[y][x] = (u8)pline;
                } else if (!inside && !onLine && (ctrl & 4)) {
                    G.sgbAttr[y][x] = (u8)pout;
                }
            }
        }
    }
}

static void SgbAttrLin(const u8* d) {
    int n = d[1];
    for (int i = 0; i < n && i < 110; i++) {
        u8 b = d[2 + i];
        int line = b & 31, pal = (b >> 5) & 3;
        if (b & 0x80) { // horizontal
            if (line < 18)
                for (int x = 0; x < 20; x++) G.sgbAttr[line][x] = (u8)pal;
        } else if (line < 20) {
            for (int y = 0; y < 18; y++) G.sgbAttr[y][line] = (u8)pal;
        }
    }
}

static void SgbAttrDiv(const u8* d) {
    int pr = d[1] & 3, pb = (d[1] >> 2) & 3, pl = (d[1] >> 4) & 3;
    int horiz = d[1] & 0x40;
    int pos = d[2] & 31;
    for (int y = 0; y < 18; y++) {
        for (int x = 0; x < 20; x++) {
            int c = horiz ? y : x;
            G.sgbAttr[y][x] = (u8)(c < pos ? pl : c == pos ? pb : pr);
        }
    }
}

static void SgbAttrChr(const u8* d) {
    int x = d[1] % 20, y = d[2] % 18;
    int n = d[3] | (d[4] << 8);
    int vertical = d[5] & 1;
    for (int i = 0; i < n && i < 360; i++) {
        u8 b = d[6 + i / 4];
        G.sgbAttr[y][x] = (u8)((b >> (6 - 2 * (i % 4))) & 3);
        if (vertical) {
            if (++y >= 18) { y = 0; if (++x >= 20) x = 0; }
        } else if (++x >= 20) {
            x = 0;
            if (++y >= 18) y = 0;
        }
    }
}

static void SgbAttrSet(int file) {
    if (file >= 45) {
        return;
    }
    const u8* f = G.sgbAttrFiles[file];
    for (int i = 0; i < 360; i++) {
        G.sgbAttr[i / 20][i % 20] = (u8)((f[i / 4] >> (6 - 2 * (i % 4))) & 3);
    }
}

// PCT_TRN: the border's tile map (32x28 entries) and palettes 4-7. As the original (0x81118540), the border becomes a
// 256x224 picture of palette indices (colour | palette << 4) with a 256-colour RGBA5551 palette; colour 0 of each
// palette is the screen's backdrop. The GB Player code takes it over (Port_GbCoreTakeBorder) and fades it in.
static void SgbBuildBorder(const u8* trn) {
    u16 backdrop = GetBe16(G.mem + MEM_GB_PALETTES);
    for (int i = 0; i < 256; i++) {
        G.borderPal[i] = backdrop;
    }
    for (int p = 0; p < 4; p++) {
        for (int c = 1; c < 16; c++) {
            G.borderPal[(4 + p) * 16 + c] = Rgba5551(Rd16(trn + 0x800 + p * 32 + c * 2));
        }
    }
    for (int y = 0; y < 224; y++) {
        for (int x = 0; x < 256; x++) {
            const u8* m = &trn[((y / 8) * 32 + x / 8) * 2];
            u16 entry = (u16)(m[0] | (m[1] << 8));
            int tile = entry & 0xFF, pal = (entry >> 10) & 7, hf = entry & 0x4000, vf = entry & 0x8000;
            int px = hf ? 7 - (x & 7) : (x & 7), py = vf ? 7 - (y & 7) : (y & 7);
            const u8* t = &G.sgbBorderTiles[tile * 32];
            int bit = 7 - px;
            int ci = ((t[py * 2] >> bit) & 1) | (((t[py * 2 + 1] >> bit) & 1) << 1) | (((t[16 + py * 2] >> bit) & 1) << 2) |
                     (((t[17 + py * 2] >> bit) & 1) << 3);
            if (pal < 4) {
                pal = 4;
            }
            G.borderImage[y * 256 + x] = (u8)(ci | (pal << 4));
        }
    }
    G.borderReady = 1;
}

static void SgbTransfer(int cmd, u8 arg) {
    static u8 trn[4096];
    switch (cmd) {
        case 0x0B: // PAL_TRN
            SgbVramTransfer(trn, 4096);
            for (int i = 0; i < 512; i++)
                for (int c = 0; c < 4; c++) G.sgbSysPal[i][c] = Rd16(trn + i * 8 + c * 2);
            break;
        case 0x13: // CHR_TRN
            SgbVramTransfer(trn, 4096);
            memcpy(G.sgbBorderTiles + (arg & 1) * 4096, trn, 4096);
            break;
        case 0x14: // PCT_TRN
            SgbVramTransfer(trn, 4096);
            SgbBuildBorder(trn);
            break;
        case 0x15: // ATTR_TRN
            SgbVramTransfer(trn, 45 * 90);
            memcpy(G.sgbAttrFiles, trn, sizeof(G.sgbAttrFiles));
            break;
    }
}

static void SgbCommand(void) {
    const u8* d = G.pkt;
    int cmd = d[0] >> 3;
    switch (cmd) {
        case 0x00: SgbSetPal(0, 1, d); break;
        case 0x01: SgbSetPal(2, 3, d); break;
        case 0x02: SgbSetPal(0, 3, d); break;
        case 0x03: SgbSetPal(1, 2, d); break;
        case 0x04: SgbAttrBlk(d); break;
        case 0x05: SgbAttrLin(d); break;
        case 0x06: SgbAttrDiv(d); break;
        case 0x07: SgbAttrChr(d); break;
        case 0x0A: { // PAL_SET
            for (int p = 0; p < 4; p++) {
                int idx = (d[1 + p * 2] | (d[2 + p * 2] << 8)) & 511;
                for (int c = 0; c < 4; c++) {
                    SetGbColor(p, c, G.sgbSysPal[idx][c]);
                }
            }
            for (int p = 1; p < 4; p++) { // colour 0 is shared
                memcpy(G.mem + MEM_GB_PALETTES + p * 8, G.mem + MEM_GB_PALETTES, 2);
            }
            if (d[9] & 0x80) {
                SgbAttrSet(d[9] & 0x3F);
            }
            if (d[9] & 0x40) {
                G.sgbMask = 0;
            }
            break;
        }
        case 0x0B: case 0x13: case 0x14: case 0x15:
            G.trnCmd = cmd;
            G.trnArg = d[1];
            G.trnFrames = 2; // this frame's end, then the next one
            break;
        case 0x11: // MLT_REQ
            G.sgbPlayers = (d[1] & 3) == 3 ? 4 : (d[1] & 3) + 1;
            if (G.sgbPlayers == 3) G.sgbPlayers = 2;
            G.sgbPlayer = 0;
            break;
        case 0x16: // ATTR_SET
            SgbAttrSet(d[1] & 0x3F);
            if (d[1] & 0x40) {
                G.sgbMask = 0;
            }
            break;
        case 0x17: // MASK_EN
            G.sgbMask = d[1] & 3;
            if (G.sgbMask == 1) memcpy(G.frozen, G.shades, sizeof(G.frozen));
            break;
        default:
            break; // sound, data transfer, jump, icon, ...: not needed
    }
}

static void SgbWriteP1(u8 v) {
    u8 old = G.lastP1;
    G.lastP1 = v;
    // multiplayer: the controller id advances when P15 goes from low to high
    if (G.sgbPlayers > 1 && !(old & 0x20) && (v & 0x20) && G.pktState == 0) {
        G.sgbPlayer = (G.sgbPlayer + 1) % G.sgbPlayers;
    }
    if (v == 0x00) { // reset pulse: a packet starts
        G.pktState = 1;
        G.pktBits = 0;
        if (G.pktCount == 0) {
            memset(G.pkt, 0, sizeof(G.pkt));
        }
        return;
    }
    if (G.pktState == 0 || v == 0x30 || old != 0x30) {
        return;
    }
    int bit = (v == 0x10) ? 1 : 0; // P15 low: 1, P14 low: 0
    if (G.pktBits < 128) {
        int byte = G.pktCount * 16 + G.pktBits / 8;
        if (byte < (int)sizeof(G.pkt) && bit) {
            G.pkt[byte] |= (u8)(1 << (G.pktBits & 7));
        }
        G.pktBits++;
        return;
    }
    // stop bit: the packet is complete
    G.pktState = 0;
    if (G.pktCount == 0) {
        G.pktTotal = G.pkt[0] & 7;
        if (G.pktTotal == 0) G.pktTotal = 1;
    }
    if (++G.pktCount >= G.pktTotal) {
        SgbCommand();
        G.pktCount = 0;
    }
}

// ---- LCD ----
static void RenderLine(int ly) {
    const u8* vram = G.mem + 0x8000;
    const u8* oam = G.mem + 0xFE00;
    u8 lcdc = G.io[0x40];
    u8* out = G.shades[ly];
    u8 bgp = G.io[0x47];
    if (!(lcdc & 0x01)) {
        memset(out, bgp & 3, 160);
        memset(G.bgIndex, 0, 160);
    } else {
        u8 scy = G.io[0x42], scx = G.io[0x43];
        u16 map = (lcdc & 0x08) ? 0x1C00 : 0x1800;
        int y = (ly + scy) & 255;
        for (int x = 0; x < 160; x++) {
            int bx = (x + scx) & 255;
            u8 t = vram[map + (y / 8) * 32 + bx / 8];
            u16 base = (lcdc & 0x10) ? (u16)(t * 16) : (u16)(0x1000 + (s8)t * 16);
            u8 lo = vram[base + (y & 7) * 2], hi = vram[base + (y & 7) * 2 + 1];
            int bit = 7 - (bx & 7);
            int ci = ((lo >> bit) & 1) | (((hi >> bit) & 1) << 1);
            G.bgIndex[x] = (u8)ci;
            out[x] = (bgp >> (ci * 2)) & 3;
        }
        // window
        u8 wy = G.io[0x4A], wx = G.io[0x4B];
        if ((lcdc & 0x20) && ly >= wy && wx <= 166) {
            u16 wmap = (lcdc & 0x40) ? 0x1C00 : 0x1800;
            int wl = G.windowLine;
            int drawn = 0;
            for (int x = 0; x < 160; x++) {
                int wxp = x - (wx - 7);
                if (wxp < 0) continue;
                drawn = 1;
                u8 t = vram[wmap + (wl / 8) * 32 + wxp / 8];
                u16 base = (lcdc & 0x10) ? (u16)(t * 16) : (u16)(0x1000 + (s8)t * 16);
                u8 lo = vram[base + (wl & 7) * 2], hi = vram[base + (wl & 7) * 2 + 1];
                int bit = 7 - (wxp & 7);
                int ci = ((lo >> bit) & 1) | (((hi >> bit) & 1) << 1);
                G.bgIndex[x] = (u8)ci;
                out[x] = (bgp >> (ci * 2)) & 3;
            }
            if (drawn) G.windowLine++;
        }
    }
    // sprites
    if (lcdc & 0x02) {
        int h = (lcdc & 0x04) ? 16 : 8;
        int idx[10], n = 0;
        for (int i = 0; i < 40 && n < 10; i++) {
            int sy = oam[i * 4] - 16;
            if (ly >= sy && ly < sy + h) idx[n++] = i;
        }
        // DMG priority: smaller X first, then OAM order: draw in reverse priority
        for (int a = 0; a < n; a++)
            for (int b = a + 1; b < n; b++)
                if (oam[idx[b] * 4 + 1] < oam[idx[a] * 4 + 1] ||
                    (oam[idx[b] * 4 + 1] == oam[idx[a] * 4 + 1] && idx[b] < idx[a])) {
                    int t = idx[a]; idx[a] = idx[b]; idx[b] = t;
                }
        for (int k = n - 1; k >= 0; k--) {
            const u8* s = &oam[idx[k] * 4];
            int sy = s[0] - 16, sx = s[1] - 8;
            u8 tile = s[2], attr = s[3];
            int row = ly - sy;
            if (attr & 0x40) row = h - 1 - row;
            if (h == 16) tile &= 0xFE;
            u16 base = (u16)(tile * 16 + row * 2);
            u8 lo = vram[base & 0x1FFF], hi = vram[(base + 1) & 0x1FFF];
            u8 pal = (attr & 0x10) ? G.io[0x49] : G.io[0x48];
            for (int px = 0; px < 8; px++) {
                int x = sx + px;
                if (x < 0 || x >= 160) continue;
                int bit = (attr & 0x20) ? px : 7 - px;
                int ci = ((lo >> bit) & 1) | (((hi >> bit) & 1) << 1);
                if (ci == 0) continue;
                if ((attr & 0x80) && G.bgIndex[x] != 0) continue;
                out[x] = (pal >> (ci * 2)) & 3;
            }
        }
    }
}

static void SetMode(int mode) {
    u8 stat = G.io[0x41];
    G.io[0x41] = (u8)((stat & ~3) | mode);
    if ((mode == 0 && (stat & 0x08)) || (mode == 1 && (stat & 0x10)) || (mode == 2 && (stat & 0x20))) {
        RequestInterrupt(1);
    }
}

static void CheckLyc(void) {
    if (G.io[0x44] == G.io[0x45]) {
        if (!(G.io[0x41] & 0x04)) {
            G.io[0x41] |= 0x04;
            if (G.io[0x41] & 0x40) RequestInterrupt(1);
        }
    } else {
        G.io[0x41] &= ~0x04;
    }
}

static void LcdStep(u32 cycles) {
    if (!(G.io[0x40] & 0x80)) {
        // LCD off: frames still pass
        G.dot += cycles;
        if (G.dot >= 70224) {
            G.dot -= 70224;
            G.frameDone = 1;
        }
        return;
    }
    while (cycles > 0) {
        u32 step = cycles > 4 ? 4 : cycles;
        cycles -= step;
        u32 lineDot = G.dot % 456;
        int ly = G.io[0x44];
        u32 before = lineDot;
        G.dot += step;
        lineDot = G.dot % 456;
        if (ly < 144) {
            if (before < 80 && lineDot >= 80) {
                SetMode(3);
                RenderLine(ly);
            } else if (before < 252 && lineDot >= 252) {
                SetMode(0);
            }
        }
        if (G.dot >= 456) {
            G.dot -= 456;
            ly++;
            if (ly == 144) {
                SetMode(1);
                RequestInterrupt(0);
                G.frameDone = 1;
            } else if (ly > 153) {
                ly = 0;
                G.windowLine = 0;
            }
            G.io[0x44] = (u8)ly;
            if (ly < 144) SetMode(2);
            CheckLyc();
        }
    }
}

static void TimerStep(u32 cycles) {
    G.divCounter = (G.divCounter + cycles) & 0xFFFF;
    if (G.io[0x07] & 4) {
        static const u32 period[4] = { 1024, 16, 64, 256 };
        G.timaCounter += cycles;
        u32 p = period[G.io[0x07] & 3];
        while (G.timaCounter >= p) {
            G.timaCounter -= p;
            if (++G.io[0x05] == 0) {
                G.io[0x05] = G.io[0x06];
                RequestInterrupt(2);
            }
        }
    }
    // serial with the internal clock and no partner: 8 bits shift in 0xFF
    if ((G.io[0x02] & 0x81) == 0x81) {
        G.serialCounter += cycles;
        if (G.serialCounter >= 4096) {
            G.io[0x01] = 0xFF;
            G.io[0x02] &= 0x7F;
            RequestInterrupt(3);
        }
    }
}

// ---- CPU ----
#define BC ((u16)((G.b << 8) | G.c))
#define DE ((u16)((G.d << 8) | G.e))
#define HL ((u16)((G.h << 8) | G.l))
#define SET_BC(v) (G.b = (u8)((v) >> 8), G.c = (u8)(v))
#define SET_DE(v) (G.d = (u8)((v) >> 8), G.e = (u8)(v))
#define SET_HL(v) (G.h = (u8)((v) >> 8), G.l = (u8)(v))

static u8 Fetch(void) {
    return Read(G.pc++);
}
static u16 Fetch16(void) {
    u16 lo = Fetch();
    return (u16)(lo | (Fetch() << 8));
}
static void Push(u16 v) {
    Write(--G.sp, (u8)(v >> 8));
    Write(--G.sp, (u8)v);
}
static u16 Pop(void) {
    u16 lo = Read(G.sp++);
    return (u16)(lo | (Read(G.sp++) << 8));
}

static u8* Reg8(int i) {
    switch (i) {
        case 0: return &G.b;
        case 1: return &G.c;
        case 2: return &G.d;
        case 3: return &G.e;
        case 4: return &G.h;
        case 5: return &G.l;
        default: return &G.a;
    }
}
static u8 GetR(int i) {
    return i == 6 ? Read(HL) : *Reg8(i);
}
static void SetR(int i, u8 v) {
    if (i == 6) Write(HL, v);
    else *Reg8(i) = v;
}

static void Alu(int op, u8 v) {
    u8 a = G.a;
    int carry = (G.f & FC) ? 1 : 0;
    int r;
    switch (op) {
        case 0: // ADD
            r = a + v;
            G.f = (u8)(((r & 0xFF) ? 0 : FZ) | (((a & 15) + (v & 15)) > 15 ? FH : 0) | (r > 255 ? FC : 0));
            G.a = (u8)r;
            break;
        case 1: // ADC
            r = a + v + carry;
            G.f = (u8)(((r & 0xFF) ? 0 : FZ) | (((a & 15) + (v & 15) + carry) > 15 ? FH : 0) | (r > 255 ? FC : 0));
            G.a = (u8)r;
            break;
        case 2: // SUB
            r = a - v;
            G.f = (u8)(((r & 0xFF) ? 0 : FZ) | FN | ((a & 15) < (v & 15) ? FH : 0) | (r < 0 ? FC : 0));
            G.a = (u8)r;
            break;
        case 3: // SBC
            r = a - v - carry;
            G.f = (u8)(((r & 0xFF) ? 0 : FZ) | FN | (((a & 15) - (v & 15) - carry) < 0 ? FH : 0) | (r < 0 ? FC : 0));
            G.a = (u8)r;
            break;
        case 4: G.a &= v; G.f = (u8)((G.a ? 0 : FZ) | FH); break;
        case 5: G.a ^= v; G.f = (u8)(G.a ? 0 : FZ); break;
        case 6: G.a |= v; G.f = (u8)(G.a ? 0 : FZ); break;
        default: // CP
            r = a - v;
            G.f = (u8)(((r & 0xFF) ? 0 : FZ) | FN | ((a & 15) < (v & 15) ? FH : 0) | (r < 0 ? FC : 0));
            break;
    }
}

static int Cond(int cc) {
    switch (cc) {
        case 0: return !(G.f & FZ);
        case 1: return (G.f & FZ) != 0;
        case 2: return !(G.f & FC);
        default: return (G.f & FC) != 0;
    }
}

static u32 ExecCB(void) {
    u8 op = Fetch();
    int r = op & 7, b = (op >> 3) & 7;
    u8 v = GetR(r);
    u32 cyc = r == 6 ? 16 : 8;
    switch (op >> 6) {
        case 0: {
            u8 res = v;
            int c = 0;
            switch (b) {
                case 0: c = v >> 7; res = (u8)((v << 1) | c); break;               // RLC
                case 1: c = v & 1; res = (u8)((v >> 1) | (c << 7)); break;         // RRC
                case 2: c = v >> 7; res = (u8)((v << 1) | ((G.f & FC) ? 1 : 0)); break; // RL
                case 3: c = v & 1; res = (u8)((v >> 1) | ((G.f & FC) ? 0x80 : 0)); break; // RR
                case 4: c = v >> 7; res = (u8)(v << 1); break;                     // SLA
                case 5: c = v & 1; res = (u8)((v >> 1) | (v & 0x80)); break;       // SRA
                case 6: c = 0; res = (u8)((v >> 4) | (v << 4)); break;             // SWAP
                default: c = v & 1; res = (u8)(v >> 1); break;                     // SRL
            }
            G.f = (u8)((res ? 0 : FZ) | (c ? FC : 0));
            SetR(r, res);
            break;
        }
        case 1: // BIT
            G.f = (u8)((G.f & FC) | FH | ((v >> b) & 1 ? 0 : FZ));
            cyc = r == 6 ? 12 : 8;
            break;
        case 2: SetR(r, (u8)(v & ~(1 << b))); break; // RES
        default: SetR(r, (u8)(v | (1 << b))); break; // SET
    }
    return cyc;
}

static u32 Step(void) {
    // interrupts
    u8 pending = G.mem[0xFFFF] & G.io[0x0F] & 0x1F;
    if (pending) {
        G.halted = 0;
        if (G.ime) {
            for (int i = 0; i < 5; i++) {
                if (pending & (1 << i)) {
                    G.ime = 0;
                    G.io[0x0F] &= (u8)~(1 << i);
                    Push(G.pc);
                    G.pc = (u16)(0x40 + i * 8);
                    return 20;
                }
            }
        }
    }
    if (G.halted) {
        return 4;
    }
    if (G.imeDelay) {
        G.imeDelay = 0;
        G.ime = 1;
    }
    u8 op = Fetch();
    switch (op) {
        case 0x00: return 4;
        case 0x01: { u16 v = Fetch16(); SET_BC(v); return 12; }
        case 0x02: Write(BC, G.a); return 8;
        case 0x03: SET_BC(BC + 1); return 8;
        case 0x07: { int c = G.a >> 7; G.a = (u8)((G.a << 1) | c); G.f = c ? FC : 0; return 4; }
        case 0x08: { u16 a = Fetch16(); Write(a, (u8)G.sp); Write((u16)(a + 1), (u8)(G.sp >> 8)); return 20; }
        case 0x09: case 0x19: case 0x29: case 0x39: {
            u16 v = op == 0x09 ? BC : op == 0x19 ? DE : op == 0x29 ? HL : G.sp;
            u32 r = (u32)HL + v;
            G.f = (u8)((G.f & FZ) | (((HL & 0xFFF) + (v & 0xFFF)) > 0xFFF ? FH : 0) | (r > 0xFFFF ? FC : 0));
            SET_HL(r);
            return 8;
        }
        case 0x0A: G.a = Read(BC); return 8;
        case 0x0B: SET_BC(BC - 1); return 8;
        case 0x0F: { int c = G.a & 1; G.a = (u8)((G.a >> 1) | (c << 7)); G.f = c ? FC : 0; return 4; }
        case 0x10: // STOP (the Super Game Boy has no speed switch)
            Fetch();
            return 4;
        case 0x11: { u16 v = Fetch16(); SET_DE(v); return 12; }
        case 0x12: Write(DE, G.a); return 8;
        case 0x13: SET_DE(DE + 1); return 8;
        case 0x17: { int c = G.a >> 7; G.a = (u8)((G.a << 1) | ((G.f & FC) ? 1 : 0)); G.f = c ? FC : 0; return 4; }
        case 0x18: { s8 e = (s8)Fetch(); G.pc = (u16)(G.pc + e); return 12; }
        case 0x1A: G.a = Read(DE); return 8;
        case 0x1B: SET_DE(DE - 1); return 8;
        case 0x1F: { int c = G.a & 1; G.a = (u8)((G.a >> 1) | ((G.f & FC) ? 0x80 : 0)); G.f = c ? FC : 0; return 4; }
        case 0x20: case 0x28: case 0x30: case 0x38: {
            s8 e = (s8)Fetch();
            if (Cond((op >> 3) & 3)) { G.pc = (u16)(G.pc + e); return 12; }
            return 8;
        }
        case 0x21: { u16 v = Fetch16(); SET_HL(v); return 12; }
        case 0x22: Write(HL, G.a); SET_HL(HL + 1); return 8;
        case 0x23: SET_HL(HL + 1); return 8;
        case 0x27: { // DAA
            int a = G.a;
            if (!(G.f & FN)) {
                if ((G.f & FC) || a > 0x99) { a += 0x60; G.f |= FC; }
                if ((G.f & FH) || (a & 0x0F) > 0x09) a += 0x06;
            } else {
                if (G.f & FC) a -= 0x60;
                if (G.f & FH) a -= 0x06;
            }
            G.a = (u8)a;
            G.f = (u8)((G.f & (FN | FC)) | (G.a ? 0 : FZ));
            return 4;
        }
        case 0x2A: G.a = Read(HL); SET_HL(HL + 1); return 8;
        case 0x2B: SET_HL(HL - 1); return 8;
        case 0x2F: G.a = (u8)~G.a; G.f |= FN | FH; return 4;
        case 0x31: G.sp = Fetch16(); return 12;
        case 0x32: Write(HL, G.a); SET_HL(HL - 1); return 8;
        case 0x33: G.sp++; return 8;
        case 0x37: G.f = (u8)((G.f & FZ) | FC); return 4;
        case 0x3A: G.a = Read(HL); SET_HL(HL - 1); return 8;
        case 0x3B: G.sp--; return 8;
        case 0x3F: G.f = (u8)((G.f & FZ) | ((G.f & FC) ? 0 : FC)); return 4;
        case 0x76: G.halted = 1; return 4;
        case 0xC0: case 0xC8: case 0xD0: case 0xD8:
            if (Cond((op >> 3) & 3)) { G.pc = Pop(); return 20; }
            return 8;
        case 0xC1: { u16 v = Pop(); SET_BC(v); return 12; }
        case 0xD1: { u16 v = Pop(); SET_DE(v); return 12; }
        case 0xE1: { u16 v = Pop(); SET_HL(v); return 12; }
        case 0xF1: { u16 v = Pop(); G.a = (u8)(v >> 8); G.f = (u8)(v & 0xF0); return 12; }
        case 0xC2: case 0xCA: case 0xD2: case 0xDA: {
            u16 a = Fetch16();
            if (Cond((op >> 3) & 3)) { G.pc = a; return 16; }
            return 12;
        }
        case 0xC3: G.pc = Fetch16(); return 16;
        case 0xC4: case 0xCC: case 0xD4: case 0xDC: {
            u16 a = Fetch16();
            if (Cond((op >> 3) & 3)) { Push(G.pc); G.pc = a; return 24; }
            return 12;
        }
        case 0xC5: Push(BC); return 16;
        case 0xD5: Push(DE); return 16;
        case 0xE5: Push(HL); return 16;
        case 0xF5: Push((u16)((G.a << 8) | G.f)); return 16;
        case 0xC6: case 0xCE: case 0xD6: case 0xDE: case 0xE6: case 0xEE: case 0xF6: case 0xFE:
            Alu((op >> 3) & 7, Fetch());
            return 8;
        case 0xC7: case 0xCF: case 0xD7: case 0xDF: case 0xE7: case 0xEF: case 0xF7: case 0xFF:
            Push(G.pc);
            G.pc = (u16)(op & 0x38);
            return 16;
        case 0xC9: G.pc = Pop(); return 16;
        case 0xCB: return ExecCB();
        case 0xCD: { u16 a = Fetch16(); Push(G.pc); G.pc = a; return 24; }
        case 0xD9: G.pc = Pop(); G.ime = 1; return 16;
        case 0xE0: Write((u16)(0xFF00 + Fetch()), G.a); return 12;
        case 0xE2: Write((u16)(0xFF00 + G.c), G.a); return 8;
        case 0xE8: {
            s8 e = (s8)Fetch();
            G.f = (u8)((((G.sp & 15) + (e & 15)) > 15 ? FH : 0) | (((G.sp & 255) + (u8)e) > 255 ? FC : 0));
            G.sp = (u16)(G.sp + e);
            return 16;
        }
        case 0xE9: G.pc = HL; return 4;
        case 0xEA: Write(Fetch16(), G.a); return 16;
        case 0xF0: G.a = Read((u16)(0xFF00 + Fetch())); return 12;
        case 0xF2: G.a = Read((u16)(0xFF00 + G.c)); return 8;
        case 0xF3: G.ime = 0; G.imeDelay = 0; return 4;
        case 0xF8: {
            s8 e = (s8)Fetch();
            G.f = (u8)((((G.sp & 15) + (e & 15)) > 15 ? FH : 0) | (((G.sp & 255) + (u8)e) > 255 ? FC : 0));
            SET_HL(G.sp + e);
            return 12;
        }
        case 0xF9: G.sp = HL; return 8;
        case 0xFA: G.a = Read(Fetch16()); return 16;
        case 0xFB: G.imeDelay = 1; return 4;
        default:
            break;
    }
    if (op >= 0x40 && op < 0x80) { // LD r,r'
        SetR((op >> 3) & 7, GetR(op & 7));
        return ((op & 7) == 6 || ((op >> 3) & 7) == 6) ? 8 : 4;
    }
    if (op >= 0x80 && op < 0xC0) {
        Alu((op >> 3) & 7, GetR(op & 7));
        return (op & 7) == 6 ? 8 : 4;
    }
    switch (op & 0xC7) {
        case 0x04: { // INC r
            int r = (op >> 3) & 7;
            u8 v = GetR(r);
            u8 res = (u8)(v + 1);
            SetR(r, res);
            G.f = (u8)((G.f & FC) | (res ? 0 : FZ) | ((v & 15) == 15 ? FH : 0));
            return r == 6 ? 12 : 4;
        }
        case 0x05: { // DEC r
            int r = (op >> 3) & 7;
            u8 v = GetR(r);
            u8 res = (u8)(v - 1);
            SetR(r, res);
            G.f = (u8)((G.f & FC) | FN | (res ? 0 : FZ) | ((v & 15) == 0 ? FH : 0));
            return r == 6 ? 12 : 4;
        }
        case 0x06: { // LD r,n
            int r = (op >> 3) & 7;
            SetR(r, Fetch());
            return r == 6 ? 12 : 8;
        }
    }
    // undefined opcodes (0xD3, 0xDB, ...): the CPU locks up; treat as NOP
    return 4;
}


// ---- entry points (called from the GB Player glue, gb_apu_3.c) ----

// GbCpu_Reset: the machine after the Super Game Boy's boot ROM, and the emulator memory as the original reset leaves it
// (0x81117B20): VRAM and the emulator's line tables cleared, the four SGB palettes set to Zero's green default, every
// save RAM block marked clean, NR52/NR50/NR51 written through the sound emulation.
void Port_GbCoreReset(u8* mem, const u8* rom, u32 romSize, u8* sram, u32 sramSize) {
    memset(&G, 0, sizeof(G));
    G.mem = mem;
    G.io = mem + 0xFF00;
    G.rom = rom;
    G.romSize = romSize;
    G.sram = sram;
    G.sramSize = sramSize;
    G.mbc = -1; // read from the header at the first bank switch (DetectMbc)
    G.romBank = 1; // no load request: the banks are not streaming yet

    GbApu_QueueRegisterWrite(0xFF26, 0x00, 0x00);
    GbApu_QueueRegisterWrite(0xFF26, 0x8F, 0x10);
    GbApu_QueueRegisterWrite(0xFF24, 0x77, 0x20);
    GbApu_QueueRegisterWrite(0xFF25, 0xFF, 0x30);

    memset(mem + 0x8000, 0, 0x2000);
    memset(mem + 0x10200, 0, 0x2000);
    memset(mem + 0x12D20, 0, 0x168);
    static const u16 defPal[4] = { 0xFFF4, 0xAE24, 0x5450, 0x0240 };
    for (int p = 0; p < 4; p++) {
        for (int c = 0; c < 4; c++) {
            PutBe16(mem + MEM_GB_PALETTES + p * 8 + c * 2, defPal[c]);
        }
    }
    memset(mem + MEM_SRAM_BLOCKS, 0xFF, 0x80);
    memset(mem + 0x10100, 0, 0x100); // the emulator's flags the GB Player code reads (border, speed, flash filter)
    mem[MEM_SPEED] = 1;

    // Super Game Boy after its boot ROM
    G.a = 0x01; G.f = 0x00; G.b = 0x00; G.c = 0x14; G.d = 0x00; G.e = 0x00; G.h = 0xC0; G.l = 0x60;
    G.sp = 0xFFFE;
    G.pc = 0x0100;
    G.io[0x00] = 0x30;
    G.io[0x05] = 0; G.io[0x06] = 0; G.io[0x07] = 0;
    G.io[0x10] = 0x80; G.io[0x11] = 0xBF; G.io[0x12] = 0xF3; G.io[0x14] = 0xBF; G.io[0x16] = 0x3F;
    G.io[0x19] = 0xBF; G.io[0x1A] = 0x7F; G.io[0x1B] = 0xFF; G.io[0x1C] = 0x9F; G.io[0x1E] = 0xBF;
    G.io[0x20] = 0xFF; G.io[0x23] = 0xBF; G.io[0x24] = 0x77; G.io[0x25] = 0xF3; G.io[0x26] = 0xF0;
    G.io[0x40] = 0x91; G.io[0x47] = 0xFC; G.io[0x48] = 0xFF; G.io[0x49] = 0xFF;
    G.io[0x41] = 0x85;
    G.lastP1 = 0x30;
    G.sgbPlayers = 1;
    G.initialised = 1;
}

// The end of one Game Boy frame (VBlank).
static void FrameEnd(void) {
    if (G.trnFrames && --G.trnFrames == 0) {
        SgbTransfer(G.trnCmd, G.trnArg);
    }
}

// Debugging aid for test runs: PORT_GB_INPUT="frame:Button[+Button] ..." presses Game Boy buttons for 6 frames at the
// given Game Boy frame (counted from the reset), independent of how the N64 side paces the frames. Buttons: A B Select
// Start Up Down Left Right.
static u16 ScriptedInput(u32 frame) {
    static const char* script = (const char*)1;
    if (script == (const char*)1) {
        script = getenv("PORT_GB_INPUT");
    }
    if (script == NULL) {
        return 0;
    }
    static const struct { const char* name; u16 mask; } names[] = {
        { "A", 0x8000 }, { "B", 0x4000 }, { "Select", 0x2000 }, { "Start", 0x1000 },
        { "Up", 0x0800 }, { "Down", 0x0400 }, { "Left", 0x0200 }, { "Right", 0x0100 },
    };
    u16 pad = 0;
    const char* p = script;
    while (*p) {
        char* end;
        unsigned long at = strtoul(p, &end, 10);
        p = end;
        if (*p != ':') {
            break;
        }
        p++;
        u16 mask = 0;
        while (*p && *p != ' ') {
            size_t n = strcspn(p, "+ ");
            for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
                if (strlen(names[i].name) == n && strncmp(p, names[i].name, n) == 0) {
                    mask |= names[i].mask;
                }
            }
            p += n;
            if (*p == '+') {
                p++;
            }
        }
        while (*p == ' ') {
            p++;
        }
        if (frame >= at && frame < at + 6) {
            pad |= mask;
        }
    }
    return pad;
}

// GbCpu_Run: one Game Boy frame (GbEmu_UpdateFrame calls it 1, 2 or 4 times per N64 frame). pc is GbEmuState.gbPc:
// GbEmu_LoadCartridge sets it to 0 to start the boot stub of a streamed cartridge, and GbEmu_UpdateFrame watches it
// reach 0x100. buttons: N64 buttons, START and Z already mapped to the Game Boy's Start and Select. Returns non-zero
// when a ROM bank could not be loaded (the original's 0x8000 | error).
s32 Port_GbCoreRun(u16* pc, u16 buttons) {
    if (!G.initialised) {
        return 0x8000;
    }
    if (*pc != G.pc) {
        G.pc = *pc;
        G.halted = 0;
    }
    G.pad = buttons | ScriptedInput(G.frameCount++);
    G.error = 0;
    G.frameDone = 0;
    G.frameCycles = 0;
    for (u32 guard = 0; !G.frameDone && !G.error && guard < 200000; guard++) {
        u32 c = Step();
        G.frameCycles += c;
        TimerStep(c);
        LcdStep(c);
    }
    FrameEnd();
    static int trace = -1;
    if (trace < 0) trace = getenv("PORT_TRACE_GB") != NULL;
    static u32 frames;
    if (trace && (frames++ % 30) == 0) {
        fprintf(stderr, "[gb] frame %u pc %04X bank %u lcdc %02X ly %u ie %02X if %02X ime %d halt %d err %d\n", frames,
                G.pc, G.romBank, G.io[0x40], G.io[0x44], G.mem[0xFFFF], G.io[0x0F], G.ime, G.halted, G.error);
    }
    *pc = G.pc;
    return G.error ? 0x8001 : 0;
}

// Hands over the border of the last PCT_TRN: image = 256x224 palette indices, palette = 256 RGBA5551 colours in N64
// byte order (where the original leaves them: the back image buffer and emulator memory +0x27D80). Returns 0 when there
// is no new border.
s32 Port_GbCoreTakeBorder(u8* image, u8* palette) {
    if (!G.borderReady) {
        return 0;
    }
    G.borderReady = 0;
    memcpy(image, G.borderImage, sizeof(G.borderImage));
    for (int i = 0; i < 256; i++) {
        PutBe16(palette + i * 2, G.borderPal[i]);
    }
    return 1;
}

// ---- frame composition (the job of the GB Player's RSP microcodes) ----
static u16 Darken(u16 c, int amount) {
    if (amount <= 0) {
        return c;
    }
    int k = 63 - (amount > 63 ? 63 : amount);
    return (u16)(((((c >> 11) & 31) * k / 63) << 11) | ((((c >> 6) & 31) * k / 63) << 6) |
                 ((((c >> 1) & 31) * k / 63) << 1) | 1);
}

static void PutPx(u16* fb, int x, int y, u16 c) {
    fb[y * 320 + x] = (u16)((c >> 8) | (c << 8)); // frame buffers hold N64 byte order
}

// Draws one GB Player frame into a 320x240 frame buffer (RGBA5551, N64 byte order), as the RSP task of GbEmu_UpdateFrame:
// - the frame image (256x224 palette indices; frameImage = GbEmuState.unk_44 + 0x800, the game's Super Game Boy border
//   or one of Zero's own frames) with its 256-colour palette (framePal: emulator memory +0x14800, N64 byte order), at
//   (14, 8): the VI scales the picture by 0.888 (GbEmu_LoadCartridge), so 284 frame buffer columns fill the screen,
// - the Game Boy screen at (48, 40) inside it, coloured with the SGB palettes (emulator memory +0x12D00) per 8x8 cell,
// - the status strip (320x16 RGBA5551, N64 byte order, 0 = not drawn) at line stripY (the task's byte 0x15: it slides
//   down by 16 lines once the game's border has come in).
// fade is the task's byte 0x1F: bits 6 and 7 pick the frame and the Game Boy screen, bits 0-5 how far they are
// darkened (0x3F black). GbEmu_RunFrame fades both in from black (0xFF to 0xC0), darkens the Game Boy screen behind its
// menus (0xBF), and the emulator fades the frame out and in while it changes (0x40 | n).
void Port_GbPlayerPresent(void* frameBuffer, const u8* frameImage, const u8* framePal, const void* statusStrip,
                          int stripY, int fade) {
    u16* fb = (u16*)frameBuffer;
    if (fb == NULL) {
        return;
    }
    const int ox = 14, oy = 8;
    int amount = fade & 0x3F;
    int fadeFrame = (fade & 0x40) ? amount : 0;
    int fadeScreen = (fade & 0x80) ? amount : 0;
    for (int y = 0; y < 240; y++) {
        for (int x = 0; x < 320; x++) {
            fb[y * 320 + x] = 0;
        }
    }
    if (!G.initialised || G.mem == NULL) {
        return;
    }
    const u8* gbPal = G.mem + MEM_GB_PALETTES;
    int lcdOn = (G.io[0x40] & 0x80) != 0;
    for (int y = 0; y < 224; y++) {
        for (int x = 0; x < 256; x++) {
            int gx = x - 48, gy = y - 40;
            u16 c;
            if (gx >= 0 && gx < 160 && gy >= 0 && gy < 144) {
                int pal = G.sgbAttr[gy / 8][gx / 8] & 3;
                int shade;
                if (G.sgbMask == 2) {
                    c = 0x0001; // black
                } else if (G.sgbMask == 3) {
                    c = (u16)(GetBe16(gbPal) | 1); // colour 0
                } else {
                    shade = G.sgbMask == 1 ? G.frozen[gy][gx] : G.shades[gy][gx];
                    if (!lcdOn) {
                        shade = 0;
                    }
                    c = (u16)(GetBe16(gbPal + pal * 8 + shade * 2) | 1);
                }
                c = Darken(c, fadeScreen);
            } else {
                u8 ci = frameImage != NULL ? frameImage[y * 256 + x] : 0;
                c = framePal != NULL ? (u16)(GetBe16(framePal + ci * 2) | 1) : 0x0001;
                c = Darken(c, fadeFrame);
            }
            PutPx(fb, ox + x, oy + y, c);
        }
    }
    if (statusStrip != NULL && stripY >= 0) {
        const u16* s = (const u16*)statusStrip;
        for (int y = 0; y < 16 && stripY + y < 240; y++) {
            for (int x = 0; x < 320; x++) {
                u16 v = s[y * 320 + x];
                if (v != 0) {
                    fb[(stripY + y) * 320 + x] = v;
                }
            }
        }
    }
}
