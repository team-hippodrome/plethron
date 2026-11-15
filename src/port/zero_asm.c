// Host versions of Stadium Zero's handwritten assembly functions (the decomp has no C for them), and stand-ins for
// the ones the port does not run yet.
#include <stdio.h>
#include <stdint.h>
#include <string.h>

static uint32_t Be32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

// Yay0 decoder (both handwritten copies: func_800093E0 in the resident boot code, func_80048010 next to the audio
// code). src: "Yay0", decompressed size, link table offset, chunk offset, then the mask words.
static void Yay0Decode(const uint8_t* src, uint8_t* dst) {
    uint32_t outSize = Be32(src + 4);
    const uint8_t* link = src + Be32(src + 8);
    const uint8_t* chunk = src + Be32(src + 12);
    const uint8_t* maskp = src + 16;
    uint32_t mask = 0;
    int bits = 0;
    uint8_t* out = dst;
    uint8_t* end = dst + outSize;

    while (out < end) {
        if (bits == 0) {
            mask = Be32(maskp);
            maskp += 4;
            bits = 32;
        }
        if (mask & 0x80000000u) {
            *out++ = *chunk++;
        } else {
            uint32_t l = ((uint32_t)link[0] << 8) | link[1];
            link += 2;
            const uint8_t* from = out - (l & 0xFFF) - 1;
            uint32_t n = l >> 12;
            n = (n == 0) ? (uint32_t)*chunk++ + 0x12 : n + 2;
            while (n-- > 0 && out < end) {
                *out++ = *from++;
            }
        }
        mask <<= 1;
        bits--;
    }
}

void func_800093E0(void* src, void* dst) {
    Yay0Decode(src, dst);
}

void func_80048010(void* src, void* dst) {
    Yay0Decode(src, dst);
}

// The audio code calls it through this data-style name.
void D_80048010(void* src, void* dst) {
    Yay0Decode(src, dst);
}

// Writes the CPU's WatchLo/WatchHi (debug watchpoint): nothing to do on the host.
void func_800093C0(uint32_t watchLo) {
    (void)watchLo;
}

// osCreatePiManager / osCreateViManager under the decomp's func_ names: the host OS layer has its own managers
// (src/port/os_devices.cpp), so nothing has to be created.
void port_osCreateViManager(int32_t pri);

void func_80054AB0(int32_t pri, void* cmdQ, void* cmdBuf, int32_t cmdMsgCnt) {
    (void)pri;
    (void)cmdQ;
    (void)cmdBuf;
    (void)cmdMsgCnt;
}

void func_8005BD80(int32_t pri) {
    port_osCreateViManager(pri);
}

// The 64DD boot path: the port has no 64DD.
void LeoBootGame(void* loadAddr) {
    fprintf(stderr, "[port] LeoBootGame(%p): no 64DD on the host\n", loadAddr);
}

// TODO(port): model callback in handwritten asm (fragment001); translate it to C.
int32_t func_81001CE0(int32_t event, void* node) {
    static int warned;
    if (!warned++) {
        fprintf(stderr, "[port] func_81001CE0 (model callback) is not ported yet\n");
    }
    (void)event;
    (void)node;
    return 0;
}

// TODO(port): the GB Player's Game Boy CPU core (handwritten asm). stadion's src/port/gb_core.c runs Stadium 1's;
// it still has to be wired to Zero's emulator context.
void GbCpu_Reset(void* emu) {
    (void)emu;
}

uint8_t GbCpu_Run(void* emu, int32_t arg1) {
    static int warned;
    if (!warned++) {
        fprintf(stderr, "[port] GbCpu_Run: the GB Player core is not wired up yet\n");
    }
    (void)emu;
    (void)arg1;
    return 0;
}
